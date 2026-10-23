#include "ContainerPreview/NativeWorld.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstring>
namespace preview {
namespace {
template<class T>T Read(void const* p,std::size_t offset){T value;std::memcpy(&value,static_cast<std::byte const*>(p)+offset,sizeof value);return value;}
using Getter=void*(*)(void*);
struct StringView {char const* data;std::size_t size;};
void* ActorSource(NativeProfile const& p,void* actor,void* level){
    // Same read-only Dimension route as ClientInstance::getRegion (5DAEBB0).
    // Actor+456 is the dimension weak reference, NOT a GameMode object.
    // Its owner remains alive inside the synchronous level/client callback.
    auto dimension=Read<void*>(actor,456);auto control=Read<void*>(actor,464);
    if(!dimension || !control)return nullptr;
    auto fn=p.Virtual(dimension,96);if(!fn)return nullptr;
    auto source=reinterpret_cast<Getter>(fn)(dimension);
    return p.Table(source,0xE8554E0) && Read<void*>(source,32)==level?source:nullptr;
}
int DimensionId(NativeProfile const& p,void* source){
    int id=INT_MIN;p.Function<void*(*)(void*,int*)>(0x2D7DDF0)(source,&id);return id;
}
}
Target DecodeHit(Target context,void const* hit,ActorIdentity entity,bool player) noexcept{
    __try {
        if(!hit)return {};
        int type=Read<int>(hit,24);
        if(type==0){context.kind=TargetKind::Block;context.x=Read<int>(hit,32);context.y=Read<int>(hit,36);context.z=Read<int>(hit,40);return context;}
        if(type==1 && !player && entity.unique && entity.runtime && entity.unique!=UINT64_MAX && entity.runtime!=UINT64_MAX){
            context.kind=TargetKind::Entity;context.entity=entity.unique;context.entityRuntime=entity.runtime;return context;
        }
        return {};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
void* ReadClientSceneStack(void* client) noexcept{
    __try {
        if(!client)return nullptr;
        // Exact client getter 5DC6A60 (v233) borrows this owner and checks the
        // NonOwnerPointer validity at stack+8. Nothing is retained or released.
        unsigned owner{};
        switch(CurrentVersion()){
            // Exact45 getterD646D0; no global stack or newer-field fallback.
            case GameVersion::V45:owner=1480;break;
            case GameVersion::V51:case GameVersion::V52:owner=1488;break;
            default:return nullptr;
        }
        auto stack=Read<void*>(client,owner);if(!stack)return nullptr;
        auto validity=Read<std::uint8_t const*>(stack,8);
        return validity && *validity?stack:nullptr;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
bool NativeHud(NativeProfile const& p,void* client) noexcept{
    __try {
        if(!p.Table(client,0xE9731B0) || !p.Function<bool(*)(void*)>(0x5DC0C80)(client))return false;
        // Equivalent per-client branch of 5DC6430, without calling that shared
        // hook site. A missing client scene is not replaced by a guessed global
        // scene: absence is simply ineligible. Both virtual calls stay guarded.
        //45 D643B0 uses376/57E6D0, whose reverse walk skips native640
        // scenes.45 slot424 is a boolean and must never be called as a getter.
        auto stack=ReadClientSceneStack(client);auto top=p.Virtual(stack,CurrentVersion()==GameVersion::V45?376:424);if(!top)return false;
        auto scene=reinterpret_cast<Getter>(top)(stack);auto fn=p.Virtual(scene,456);
        StringView name{"hud_screen",10};return fn && reinterpret_cast<bool(*)(void*,StringView const*)>(fn)(scene,&name);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
ClientObservation ObserveClient(NativeProfile const& p,void* client) noexcept{
    __try {
        if(!NativeHud(p,client))return {};
        auto level=p.Function<Getter>(0x5DC2630)(client);auto actor=p.Function<Getter>(0x5DAED30)(client);
        if(!p.Table(level,0xE7ADAB0) || !actor)return {};
        auto identity=ReadIdentity(actor);if(!identity.local || identity.server)return {};
        auto source=p.Function<Getter>(0x5DAEBB0)(client);
        if(!p.Table(source,0xE8554E0) || Read<void*>(source,32)!=level)return {};
        int dimension=DimensionId(p,source);if(dimension==INT_MIN)return {};
        auto hit=p.Function<Getter>(0x117D580)(level);if(!hit)return {};
        ActorIdentity entity{};bool player=false;
        if(Read<int>(hit,24)==1){
            auto target=p.Function<Getter>(0x3529DB0)(hit);if(!target)return {};
            auto flags=Read<std::uint8_t>(target,528);
            // Mob bit2 is not Player bit1. Broaden only the exact approved
            // Hive definition; preserve the old conservative read-only set.
            player=(flags&1)!=0 || ((flags&2)!=0 && !IsHiveDeathCrate(target));
            if(!player)entity=ReadIdentity(target);
        }
        Target context{reinterpret_cast<std::uintptr_t>(level),identity.unique,identity.runtime,dimension};
        return {DecodeHit(context,hit,entity,player),identity,true};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
ServerObservation ObserveServer(NativeProfile const& p,void* level,PreviewWork const& work) noexcept{
    __try {
        if(!p.Table(level,0xE83B2B0) || Read<std::uint8_t>(level,568))return {};
        auto actor=p.Function<void*(*)(void*,std::uint64_t)>(0x1177360)(level,work.local.unique);
        auto identity=ReadIdentity(actor);if(!SameIntegratedPlayer(work.local,identity))return {};
        ServerObservation result{identity,{},true};
        auto source=ActorSource(p,actor,level);
        if(!source || DimensionId(p,source)!=work.request.target.dimension)return result;
        auto const& target=work.request.target;
        if(target.kind==TargetKind::Block){
            BlockPosition pos{target.x,target.y,target.z};
            auto block=p.Function<void*(*)(void*,BlockPosition const*)>(0x2D80310)(source,&pos);
            result.contents=InspectBlock(block,p.Base(),pos);
        }else if(target.kind==TargetKind::Entity){
            auto entity=p.Function<void*(*)(void*,std::uint64_t,bool)>(0x1176930)(level,target.entity,false);
            auto id=ReadIdentity(entity);
            if(!entity || id.unique!=target.entity || id.runtime!=target.entityRuntime || (Read<std::uint8_t>(entity,528)&2))return result;
            auto entitySource=ActorSource(p,entity,level);
            if(!entitySource || DimensionId(p,entitySource)!=target.dimension)return result;
            auto component=ReadComponent(entity,0x1779E2C7,512);
            result.contents=InspectEntityComponent(reinterpret_cast<void const*>(component));
        }
        return result;
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
}
