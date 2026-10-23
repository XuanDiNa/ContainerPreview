#include "ContainerPreview/DiagnosticTrace.h"
#include "ContainerPreview/NativeState.h"
#include "ContainerPreview/NativeAddress.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <sstream>
namespace preview {
namespace {
template<class T>T At(void const* object,std::size_t offset){T value;std::memcpy(&value,static_cast<std::byte const*>(object)+offset,sizeof value);return value;}
// IDA19E5F70 selects DEB6534F with176-byte elements;2BA7EE0 builds
// the canonical identifier from namespace0 and name32. Never invoke those
// mutating native initializers or log arbitrary strings from the component.
bool IdentifierPart(void const* component,unsigned offset,std::array<char,96>& out){
    auto p=static_cast<std::byte const*>(component)+offset;
    auto size=At<std::uint64_t>(p,16),capacity=At<std::uint64_t>(p,24);
    if(!size || size>=out.size() || capacity<size || capacity>4096 || (capacity<16 && capacity!=15))return false;
    auto data=capacity<16?reinterpret_cast<char const*>(p):At<char const*>(p,0);
    if(!data)return false;
    std::memcpy(out.data(),data,static_cast<std::size_t>(size)+1);
    if(out[size])return false;
    for(std::size_t i=0;i<size;++i)if(out[i]<0x21 || out[i]>0x7E)return false;
    return true;
}
DiagnosticActorKind ActorKind(void const* actor) noexcept{
    __try{
        auto component=ReadComponent(actor,0xDEB6534F,176);
        if(!component)return DiagnosticActorKind::Unknown;
        std::array<char,96> nameSpace{},name{};
        if(!IdentifierPart(reinterpret_cast<void const*>(component),0,nameSpace) ||
           !IdentifierPart(reinterpret_cast<void const*>(component),32,name))return DiagnosticActorKind::Unknown;
        if(!std::strcmp(nameSpace.data(),"hivesky") && !std::strcmp(name.data(),"death_crate"))return DiagnosticActorKind::DeathCrate;
        if(!std::strcmp(nameSpace.data(),"minecraft") && !std::strcmp(name.data(),"chest_minecart"))return DiagnosticActorKind::ChestMinecart;
        return DiagnosticActorKind::Other;
    }__except(EXCEPTION_EXECUTE_HANDLER){return DiagnosticActorKind::Unknown;}
}
char const* ActorKindName(DiagnosticActorKind kind){
    switch(kind){case DiagnosticActorKind::Other:return "other";case DiagnosticActorKind::DeathCrate:return "death-crate";
        case DiagnosticActorKind::Player:return "player";case DiagnosticActorKind::ChestMinecart:return "chest-minecart";default:return "unknown";}
}
struct Context {std::uint64_t call{},client{};unsigned depth{},clientDepth{};};
thread_local Context context;
std::atomic<std::uint64_t> nextCall{1};
thread_local void const* previewIconStack{};
thread_local TraceRecord* iconObservation{};
thread_local void const* observedIconStack{};
thread_local TraceRecord const* entityFactoryFrame{};
unsigned DiagnosticRva(unsigned key){
    // Physical45 data only; runtime admission still requires its complete
    // separate image profile and exact entry guards.
    if(CurrentVersion()!=GameVersion::V45)return Rva(key);
    switch(key){
        case 0xE9731B0:return 0xE774DB0;
        case 0xE9B1870:case 0xE9A6FD0:return 0xE975ED0;
        case 0xE99FEB0:return 0xE9704E0;
        case 0xE6D8360:return 0xE6C41B0;
        case 0xEB3CA70:return 0xEB1C8E0;
        case 0xE6D6060:return 0xE749DE0;
        case 0xE9AA770:return 0xEB1D820;
        case 0xE6D66B0:return 0xE6C2BF0;
        case 0xE7B0290:return 0xE7A0FE0;
        case 0xE7B0D90:return 0xE7A1A90;
        case 0xE837C90:return 0xE834D50;
        case 0xE897540:return 0xE9DBAE0;
        case 0xE827560:return 0xE81A090;
        case 0xE827600:return 0xE81A130;
        case 0xE7ADAB0:return 0xE79E800;
        default:return 0;
    }
}
bool Table(void const* object,std::uintptr_t base,std::uint32_t table){auto physical=DiagnosticRva(table);return object && base && physical && At<std::uintptr_t>(object,0)==base+physical;}
void Client(TraceRecord& r,void const* client,std::uintptr_t base){if(Table(client,base,0xE9731B0))r.client=reinterpret_cast<std::uintptr_t>(client);}
void Position(TraceRecord& r,void const* position){r.x=At<int>(position,0);r.y=At<int>(position,4);r.z=At<int>(position,8);r.fields|=PositionFields;}
char const* Name(TraceStage s){switch(s){
    case TraceStage::Client:return "client";case TraceStage::Open:return "open";
    case TraceStage::DeferredOpen:return "deferred-open";case TraceStage::Scene:return "scene";
    case TraceStage::Factory:return "factory";case TraceStage::Content:return "content";
    case TraceStage::Slot:return "slot";case TraceStage::ClosePacket:return "close-packet";
    case TraceStage::ClosePlayer:return "close-player";
    case TraceStage::ModelBinding:return "model-binding";
    case TraceStage::GuiIcon:return "gui-icon";
    case TraceStage::BlockUse:return "block-use";
    case TraceStage::ModelApply:return "model-apply";
    case TraceStage::ScreenBuild:return "screen-build";
    case TraceStage::ControllerDestroy:return "controller-destroy";
    case TraceStage::RemoteGate:return "remote-gate";
    case TraceStage::EntityUse:return "entity-use";
    case TraceStage::EntityFactory:return "entity-factory";
    case TraceStage::EntityResolve:return "entity-resolve";
    }return "unknown";
}
}
DiagnosticScope::DiagnosticScope(TraceStage stage,std::uintptr_t client) noexcept:
    previousCall_(context.call),previousClient_(context.client),call_(nextCall.fetch_add(1,std::memory_order_relaxed)),
    previousDepth_(context.depth),previousClientDepth_(context.clientDepth),stage_(stage){
    context.call=call_;++context.depth;
    if(stage==TraceStage::Client){context.client=client;++context.clientDepth;}
}
DiagnosticScope::~DiagnosticScope(){context={previousCall_,previousClient_,previousDepth_,previousClientDepth_};}
void DiagnosticScope::Stamp(TraceRecord& r) const noexcept{
    r.stage=stage_;r.call=call_;r.parent=previousCall_;r.depth=previousDepth_+1;
    r.clientDepth=context.clientDepth;r.scopedClient=context.client;
}
bool DiagnosticBuffer::Push(TraceRecord r) noexcept{
    try{
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock || size_==records_.size() || accepted_==8192){++dropped_;return false;}
        r.sequence=++accepted_;records_[(head_+size_)%records_.size()]=r;++size_;return true;
    }catch(...){++dropped_;return false;}
}
std::size_t DiagnosticBuffer::Drain(std::span<TraceRecord> output) noexcept{
    try{
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock)return 0;
        auto count=std::min(size_,output.size());
        for(std::size_t i=0;i<count;++i)output[i]=records_[(head_+i)%records_.size()];
        head_=(head_+count)%records_.size();size_-=count;return count;
    }catch(...){return 0;}
}
TraceRecord CapturePacket(TraceStage stage,void const* handler,void const* packet,std::uintptr_t base) noexcept{
    TraceRecord r{};r.stage=stage;
    __try{
        std::uint32_t table{};
        switch(stage){case TraceStage::Open:table=0xE7B0D90;break;case TraceStage::Content:table=0xE837C90;break;
            case TraceStage::Slot:table=0xE6D66B0;break;case TraceStage::ClosePacket:table=0xE897540;break;default:return r;}
        if(!Table(packet,base,table))return r;
        r.container=At<std::uint8_t>(packet,48);r.fields|=PacketFields;
        if(handler)Client(r,At<void const*>(handler,88),base);
        if(stage==TraceStage::Open){
            r.type=At<std::uint8_t>(packet,49);Position(r,static_cast<std::byte const*>(packet)+52);
            r.entity=At<std::uint64_t>(packet,64);r.fields|=EntityField;
        }else if(stage==TraceStage::Content){
            auto begin=At<std::uintptr_t>(packet,160),end=At<std::uintptr_t>(packet,168),capacity=At<std::uintptr_t>(packet,176);
            if(begin<=end && end<=capacity && (!begin?end==0 && capacity==0:true) && (end-begin)%96==0 && (end-begin)/96<=4096){
                r.count=static_cast<int>((end-begin)/96);r.fields|=CountField;
            }
        }else if(stage==TraceStage::Slot){
            auto slot=At<unsigned>(packet,52);if(slot<=4096){r.slot=static_cast<int>(slot);r.fields|=SlotField;}
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CapturePlayer(void const* player,std::uintptr_t base) noexcept{
    TraceRecord r{};if(!player)return r;
    __try{
        r.player=reinterpret_cast<std::uintptr_t>(player);r.world=At<std::uintptr_t>(player,472);
        auto control=At<void const*>(player,1448);if(!control || At<int>(control,8)<=0)return r;
        auto model=At<void const*>(player,1440);r.model=reinterpret_cast<std::uintptr_t>(model);
        if(!Table(model,base,0xE9AA770))return r;
        r.modelId=At<std::uint8_t>(model,80);r.modelType=At<std::uint8_t>(model,81);r.fields|=ModelFields;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureDeferred(void const* capture,std::uintptr_t base) noexcept{
    TraceRecord r{};
    __try{
        if(!Table(capture,base,0xE7B0290))return r;
        r=CapturePlayer(At<void const*>(capture,88),base);
        r.container=At<std::uint8_t>(capture,56);r.type=At<std::uint8_t>(capture,57);r.fields|=PacketFields;
        Position(r,static_cast<std::byte const*>(capture)+60);r.entity=At<std::uint64_t>(capture,72);r.fields|=EntityField;
        r.mayOpen=At<std::uint8_t>(capture,120);r.fields|=MayOpenField;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureScene(void const* listener,void const* variant,std::uintptr_t base) noexcept{
    TraceRecord r{};
    __try{
        if(!variant)return r;
        r.event=At<std::uint8_t>(variant,376);r.fields|=SceneField;
        // Other event variants have different layouts; never inspect payloads.
        if(r.event!=16 && r.event!=18)return r;
        if(listener)Client(r,At<void const*>(listener,56),base);
        if(r.event==16){
            auto payload=(At<std::uint8_t>(variant,48)&1)?At<void const*>(variant,0):variant;
            if(!payload)return r;
            r.type=At<std::uint8_t>(payload,24);Position(r,static_cast<std::byte const*>(payload)+28);
            r.entity=At<std::uint64_t>(payload,40);r.fields|=EntityField;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureFactory(void const* player,void const* position,void const* blockType,std::uintptr_t base) noexcept{
    auto r=CapturePlayer(player,base);
    __try{if(position)Position(r,position);if(blockType)r.type=At<std::uint8_t>(blockType,0);}
    __except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord ObserveModelBinding(TracePhase phase,TraceRecord r,void const* player,void const* pair,std::uintptr_t base) noexcept{
    // The setter consumes its input shared_ptr ownership. Its pair must only
    // be borrowed before the call; post-call observations reuse those values.
    auto current=CapturePlayer(player,base);
    r.player=current.player;r.world=current.world;r.model=current.model;
    r.modelId=current.modelId;r.modelType=current.modelType;
    r.fields=(r.fields&~ModelFields)|(current.fields&(ModelFields|ReadFault));
    if(phase!=TracePhase::Before)return r;
    r.inputModel=0;r.inputId=r.inputType=-1;r.fields&=~(BindingField|InputModelFields);
    __try{
        if(!pair)return r;
        auto model=At<void const*>(pair,0);auto control=At<void const*>(pair,8);
        r.inputModel=reinterpret_cast<std::uintptr_t>(model);r.fields|=BindingField;
        if(!control || At<int>(control,8)<=0 || !Table(model,base,0xE9AA770))return r;
        r.inputId=At<std::uint8_t>(model,80);r.inputType=At<std::uint8_t>(model,81);r.fields|=InputModelFields;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureBlockUse(void const* gameMode,void const* position,unsigned char arg3,unsigned char arg4,unsigned char arg5,std::uintptr_t base) noexcept{
    TraceRecord r{};r.stage=TraceStage::BlockUse;r.useArgs={arg3,arg4,arg5};r.fields|=UseFields;
    __try{
        if(!gameMode)return r;
        // 259BC60 borrows GameMode+8; native Player+2720 must name it back.
        // A shared thread label alone is deliberately not an ownership proof.
        auto player=At<void const*>(gameMode,8);
        if(!player || At<void const*>(player,2720)!=gameMode)return r;
        auto current=CapturePlayer(player,base);current.stage=r.stage;
        current.useArgs=r.useArgs;current.fields|=UseFields;r=current;
        if(position)Position(r,position);
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureEntityFactory(void const* player,void const* entity,std::uintptr_t base) noexcept{
    TraceRecord r{};r.stage=TraceStage::EntityFactory;
    __try{
        if(!player)return r;
        auto client=At<void const*>(player,3440),world=At<void const*>(player,472);
        if(!Table(client,base,0xE9731B0) || !Table(world,base,0xE7ADAB0) || !entity)return r;
        auto id=At<std::uint64_t>(entity,0);if(!id || id==UINT64_MAX)return r;
        r=CapturePlayer(player,base);r.stage=TraceStage::EntityFactory;
        r.client=reinterpret_cast<std::uintptr_t>(client);r.entity=id;
        r.fields|=EntityFactoryFields|EntityField;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureEntityResolution(TraceRecord const& factory,void const* level,std::uint64_t requested,void const* actor,std::uintptr_t) noexcept{
    TraceRecord r{};r.stage=TraceStage::EntityResolve;
    if((factory.fields&(EntityFactoryFields|ReadFault))!=EntityFactoryFields ||
       !factory.client || !factory.player || !factory.world || factory.world!=reinterpret_cast<std::uintptr_t>(level) ||
       !requested || requested==UINT64_MAX || requested!=factory.entity)return r;
    r=factory;r.stage=TraceStage::EntityResolve;r.requestedEntity=requested;
    r.entity=r.entityRuntime=r.actor=r.actorTable=0;r.fields&=~EntityField;
    r.actorKind=DiagnosticActorKind::Unknown;r.targetPlayer=r.actorFlags=-1;
    __try{
        if(!actor)return r;
        r.actor=reinterpret_cast<std::uintptr_t>(actor);r.actorTable=At<std::uintptr_t>(actor,0);
        r.actorFlags=At<std::uint8_t>(actor,528);r.targetPlayer=(r.actorFlags&1)?1:0;r.fields|=EntityUseFields;
        if(r.targetPlayer){r.actorKind=DiagnosticActorKind::Player;return r;}
        auto identity=ReadIdentity(actor);
        if(identity.unique && identity.runtime){r.entity=identity.unique;r.entityRuntime=identity.runtime;r.fields|=EntityField;}
        r.actorKind=ActorKind(actor);
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
EntityFactoryScope::EntityFactoryScope(TraceRecord const& r) noexcept:previous_(entityFactoryFrame){
    entityFactoryFrame=(r.fields&(EntityFactoryFields|ReadFault))==EntityFactoryFields && r.client && r.player && r.world && r.entity && r.entity!=UINT64_MAX?&r:nullptr;
}
EntityFactoryScope::~EntityFactoryScope(){entityFactoryFrame=previous_;}
TraceRecord const* EntityFactoryScope::Current() noexcept{return entityFactoryFrame;}
TraceRecord ObserveEntityUse(TracePhase phase,TraceRecord r,void const* gameMode,void const* actor,std::uintptr_t arg4,std::uint64_t result,std::uintptr_t base) noexcept{
    if(phase==TracePhase::After){
        // Original interaction may destroy the actor or change player/mode.
        // Preserve the before snapshot and never dereference arguments here.
        r.interactionResult=result;r.fields|=NativeResultField;return r;
    }
    r={};r.stage=TraceStage::EntityUse;r.interactionArg=arg4;
    if(phase!=TracePhase::Before)return r;
    __try{
        if(!Table(gameMode,base,0xE827560) && !Table(gameMode,base,0xE827600))return r;
        auto player=At<void const*>(gameMode,8);
        if(!player || At<void const*>(player,2720)!=gameMode)return r;
        r=CapturePlayer(player,base);r.stage=TraceStage::EntityUse;r.interactionArg=arg4;
        if(!actor)return r;
        auto table=At<std::uintptr_t>(actor,0);auto flags=At<std::uint8_t>(actor,528);
        r.actor=reinterpret_cast<std::uintptr_t>(actor);r.actorTable=table;
        // Exact Player constructor1DC620 sets0x01. Mob constructor2073990
        // sets0x02, including nonplayer custom actors: not a player predicate.
        r.actorFlags=flags;r.targetPlayer=(flags&1)?1:0;r.fields|=EntityUseFields;
        // Player targets are labelled only; no player identity/inventory probe.
        if(r.targetPlayer){r.actorKind=DiagnosticActorKind::Player;return r;}
        auto identity=ReadIdentity(actor);
        if(identity.unique && identity.runtime){r.entity=identity.unique;r.entityRuntime=identity.runtime;r.fields|=EntityField;}
        r.actorKind=ActorKind(actor);
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureModelApply(void const* model,unsigned slot,std::uintptr_t base) noexcept{
    TraceRecord r{};r.stage=TraceStage::ModelApply;r.model=reinterpret_cast<std::uintptr_t>(model);
    __try{
        // This is the setter's borrowed `this`, not a stored/locked weak pair.
        if(!Table(model,base,0xE9AA770))return r;
        r.modelId=At<std::uint8_t>(model,80);r.modelType=At<std::uint8_t>(model,81);r.fields|=ModelFields;
        if(slot<=4096){r.slot=static_cast<int>(slot);r.fields|=SlotField;}
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord CaptureController(TraceStage stage,void const* controller,std::uintptr_t base) noexcept{
    TraceRecord r{};r.stage=stage;r.controller=reinterpret_cast<std::uintptr_t>(controller);
    std::uint32_t table{};
    if(stage==TraceStage::ScreenBuild)table=0xE9B1870;
    else if(stage==TraceStage::ControllerDestroy)table=0xE9A6FD0;
    else return r;
    __try{
        // 6C2B670 installs E9B1870. Its destructor switches to E9A6FD0
        // before entering base destructor4DF7B50.4800/4808 owns an adapter,
        // NOT the standard model. Do not interpret other controller subclasses.
        if(!Table(controller,base,table))return r;
        r.fields|=ControllerFields;
        auto state=At<void const*>(controller,3400);
        if(state){
            auto old=CurrentVersion()==GameVersion::V45;
            auto validity=At<void const*>(state,old?64:72);
            if(validity && At<std::uint8_t>(validity,0)==1)Client(r,At<void const*>(state,old?80:88),base);
        }
        auto adapterControl=At<void const*>(controller,4808);
        if(!Table(adapterControl,base,0xE6D8360) || At<int>(adapterControl,8)<=0)return r;
        auto adapter=At<void const*>(controller,4800);r.adapter=reinterpret_cast<std::uintptr_t>(adapter);
        if(!Table(adapter,base,0xEB3CA70))return r;
        r.fields|=AdapterFields;
        // C542130 and its base33E3B90 copy the same weak model into24/32 and
        // 184/192. Borrow only matching live owners in this synchronous native
        // callback. Never lock/release them or decode the adapter as inventory.
        auto model=At<void const*>(adapter,184),control=At<void const*>(adapter,192);
        if(model!=At<void const*>(adapter,24) || control!=At<void const*>(adapter,32) ||
           !Table(control,base,0xE6D6060) || At<int>(control,8)<=0)return r;
        r.model=reinterpret_cast<std::uintptr_t>(model);
        if(!Table(model,base,0xE9AA770))return r;
        r.modelId=At<std::uint8_t>(model,80);r.modelType=At<std::uint8_t>(model,81);r.fields|=ModelFields;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
TraceRecord ObserveScreenBuild(TracePhase phase,TraceRecord r,void const* inputPair,void const* outputPair,std::uintptr_t base) noexcept{
    if(phase==TracePhase::Before){
        auto caller=r.caller;r={};r.stage=TraceStage::ScreenBuild;r.caller=caller;
        __try{
            if(!inputPair)return r;
            auto control=At<void const*>(inputPair,8);
            // Exact inline allocation from67721D0, still owned by native caller.
            if(!Table(control,base,0xE99FEB0) || At<int>(control,8)<=0)return r;
            auto current=CaptureController(TraceStage::ScreenBuild,At<void const*>(inputPair,0),base);
            current.caller=caller;return current;
        }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    }else if(phase==TracePhase::After && (r.fields&ControllerFields)){
        r.screen=0;r.fields&=~ScreenResultField;
        __try{
            // 4F0340 consumes inputPair's strong reference. Never read it or the
            // controller after return. Output pair belongs to the native caller;
            // copy its opaque first word only, without dereferencing the scene.
            if(!outputPair)return r;
            r.screen=At<std::uintptr_t>(outputPair,0);r.fields|=ScreenResultField;
        }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    }
    return r;
}
std::string FormatTrace(TraceRecord const& r){
    std::ostringstream o;o<<"Native trace: seq="<<r.sequence<<" tick="<<r.time<<" stage="<<Name(r.stage)
        <<" phase="<<(r.phase==TracePhase::Before?"before":r.phase==TracePhase::After?"after":"sample")
        <<" owner="<<r.owner<<" call="<<r.call<<" parent="<<r.parent<<" depth="<<r.depth<<" clientDepth="<<r.clientDepth
        <<std::hex<<" scopedClient=0x"<<r.scopedClient<<" client=0x"<<r.client<<" world=0x"<<r.world<<" player=0x"<<r.player<<" model=0x"<<r.model
        <<std::dec<<" fields="<<r.fields<<" id="<<r.container<<" type="<<r.type<<" pos="<<r.x<<','<<r.y<<','<<r.z
        <<" entity="<<r.entity<<" count="<<r.count<<" slot="<<r.slot<<" event="<<r.event<<" modelId="<<r.modelId
        <<" modelType="<<r.modelType<<" mayOpen="<<r.mayOpen<<" held="<<r.held<<" foreground="<<r.foreground
        <<std::hex<<" inputModel=0x"<<r.inputModel<<" caller=0x"<<r.caller
        <<std::dec<<" inputId="<<r.inputId<<" inputType="<<r.inputType;
    if(r.stage==TraceStage::BlockUse)o<<" useArgs="<<r.useArgs[0]<<','<<r.useArgs[1]<<','<<r.useArgs[2];
    if(r.stage==TraceStage::EntityUse || r.stage==TraceStage::EntityResolve)
        o<<" runtime="<<r.entityRuntime<<" targetPlayer="<<r.targetPlayer<<" actorFlags="<<r.actorFlags
            <<" actorKind="<<ActorKindName(r.actorKind)
            <<std::hex<<" actor=0x"<<r.actor<<" actorTable=0x"<<r.actorTable
            <<" interactionArg=0x"<<r.interactionArg<<" nativeResult=0x"<<r.interactionResult<<std::dec;
    if(r.stage==TraceStage::EntityResolve)o<<" requestedEntity="<<r.requestedEntity;
    if(r.stage==TraceStage::RemoteGate){
        auto const& g=r.remoteGate;
        o<<" gate="<<RemoteGateName(g.gate)<<" reason="<<RemoteReasonName(g.check.reason)
            <<" remotePhase="<<g.phase<<" remoteAction="<<g.action<<" generation="<<g.generation
            <<std::hex<<" observed=0x"<<g.check.observed<<std::dec;
    }
    if(r.stage==TraceStage::ScreenBuild || r.stage==TraceStage::ControllerDestroy)
        o<<std::hex<<" controller=0x"<<r.controller<<" screen=0x"<<r.screen<<" adapter=0x"<<r.adapter;
    if(r.stage==TraceStage::GuiIcon){
        o<<" preview="<<r.previewIcon<<" mode="<<r.renderMode
            <<" texturePresent="<<r.texturePresent<<" textureStrong="<<r.textureStrong<<" rendererPath="<<r.rendererPath
            <<" tessStarted="<<r.tessStarted<<" tessBusy="<<r.tessBusy<<" tessUv="<<r.tessUv
            <<" resolves="<<r.iconResolves<<" fallback="<<r.iconFallback<<" frame="<<r.iconFrame<<" animated="<<r.iconAnimated
            <<" uv="<<r.uv[0]<<','<<r.uv[1]<<','<<r.uv[2]<<','<<r.uv[3]
            <<" dimensions="<<r.atlasDimensions[0]<<','<<r.atlasDimensions[1]<<','<<r.atlasDimensions[2]<<','<<r.atlasDimensions[3]
            <<std::hex<<" renderer=0x"<<r.renderer<<" icon=0x"<<r.icon;
    }
    return o.str();
}
PreviewIconScope::PreviewIconScope(void const* stack) noexcept:previous_(previewIconStack){previewIconStack=stack;}
PreviewIconScope::~PreviewIconScope(){previewIconStack=previous_;}
bool PreviewIconScope::Matches(void const* stack) noexcept{return stack && stack==previewIconStack;}
IconObservationScope::IconObservationScope(TraceRecord& r,void const* stack) noexcept:
    previous_(iconObservation),previousStack_(observedIconStack){iconObservation=&r;observedIconStack=stack;}
IconObservationScope::~IconObservationScope(){iconObservation=previous_;observedIconStack=previousStack_;}
void IconObservationScope::Resolved(void const* icon,unsigned frame,unsigned animated,std::uintptr_t base,void const* stack) noexcept{
    if(!iconObservation || stack!=observedIconStack)return;
    auto& r=*iconObservation;++r.iconResolves;r.icon=reinterpret_cast<std::uintptr_t>(icon);
    r.iconFrame=static_cast<int>(frame);r.iconAnimated=static_cast<int>(animated);
    r.fields&=~IconResultFields;r.uv={};r.atlasDimensions={};r.iconFallback=-1;
    if(!icon)return;
    __try{
        // Exact1B87180/1B872A0 ->55AEF10/1EB7350 descriptor layout.
        std::array<float,4> uv{};std::array<unsigned,4> dimensions{};
        for(unsigned i=0;i<4;++i){uv[i]=At<float>(icon,4+i*4);dimensions[i]=At<std::uint16_t>(icon,20+i*2);}
        r.uv=uv;r.atlasDimensions=dimensions;
        auto emptyIcon=Rva(0x11D52E58),missingIcon=Rva(0x11E450E0);
        r.iconFallback=base && emptyIcon && r.icon==base+emptyIcon?1:base && missingIcon && r.icon==base+missingIcon?2:0;
        r.fields|=IconResultFields;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
}
bool IconSampleBudget::Admit(bool preview) noexcept{
    auto& counter=preview?preview_:normal_;auto value=counter.load(std::memory_order_relaxed);
    while(value<32){if(counter.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return true;}
    return false;
}
TraceRecord CaptureGuiIcon(void const* renderer,void const* actorContext,unsigned mode) noexcept{
    TraceRecord r{};r.stage=TraceStage::GuiIcon;r.renderer=reinterpret_cast<std::uintptr_t>(renderer);r.renderMode=static_cast<int>(mode);
    __try{
        if(!renderer || !actorContext)return r;
        r.texturePresent=At<std::uintptr_t>(renderer,240)!=0;
        auto control=At<void const*>(renderer,248);r.textureStrong=control?At<int>(control,8):0;
        r.rendererPath=At<std::uint8_t>(renderer,664);
        auto screen=At<void const*>(actorContext,40);if(!screen)return r;
        auto tess=At<void const*>(screen,184);if(!tess)return r;
        bool old=CurrentVersion()==GameVersion::V45;
        r.tessStarted=At<std::uint8_t>(tess,old?656:672);r.tessBusy=At<std::uint8_t>(tess,old?581:597);r.tessUv=At<std::uint8_t>(tess,old?388:404);
        r.fields|=GuiRenderFields;
    }__except(EXCEPTION_EXECUTE_HANDLER){r.fields|=ReadFault;}
    return r;
}
}
