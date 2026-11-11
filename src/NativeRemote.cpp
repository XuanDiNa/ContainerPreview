#include "ContainerPreview/NativeRemote.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstring>
#include <atomic>
#include <cmath>
namespace preview {
namespace {
void Report(RemoteCheck* check,RemoteReason reason,std::uint64_t observed=0){if(check)*check={reason,observed};}
bool Refuse(RemoteCheck* check,RemoteReason reason,std::uint64_t observed=0){Report(check,reason,observed);return false;}
}
bool InNativeRegistry(RegistryOps ops,void* client,bool(*operation)(void*),void* state,RemoteCheck* check) noexcept {
    Report(check,RemoteReason::Pass);
    if(!ops.get || !ops.enter || !ops.leave || !client || !operation)return Refuse(check,RemoteReason::RegistryContract);
    alignas(8) std::array<std::byte,16> weak{};
    alignas(8) std::array<std::byte,24> scope{};
    bool entered=false,result=false;
    __try {
        __try {
            ops.get(client,weak.data());
            NativePair registry{};std::memcpy(&registry,weak.data(),sizeof registry);
            int strong{},weakCount{};
            if(registry.control){
                std::memcpy(&strong,static_cast<std::byte const*>(registry.control)+8,sizeof strong);
                std::memcpy(&weakCount,static_cast<std::byte const*>(registry.control)+12,sizeof weakCount);
            }
            bool live=registry.object && registry.control && strong>0 && weakCount>0;
            if(!registry.object || !registry.control)Report(check,RemoteReason::RegistryMissing);
            else if(!live)Report(check,RemoteReason::RegistryExpired);
            ops.enter(scope.data(),weak.data());entered=true;
            if(live){result=operation(state);if(!result)Report(check,RemoteReason::RegistryRefused);}
        }__finally {if(entered)ops.leave(scope.data());}
    }__except(EXCEPTION_EXECUTE_HANDLER){return Refuse(check,RemoteReason::RegistryFault);}
    return result;
}
namespace {
template<class T>T At(void const* p,unsigned o){T v;std::memcpy(&v,static_cast<std::byte const*>(p)+o,sizeof v);return v;}
// Physical45 data bindings only. This does not admit the executable or provide
// callable addresses: the complete separate runtime profile remains required.
unsigned DataRva(unsigned key){
    if(CurrentVersion()!=GameVersion::V45)return Rva(key);
    switch(key){
        case 0xE9731B0:return 0xE774DB0;
        case 0xE76E2D0:return 0xE759890;
        case 0xE76EAD0:return 0xE75A090;
        case 0xE75E2D0:return 0xE749B70;
        case 0xE75E2F0:return 0xE749B90;
        case 0xE760DA0:return 0xE74AD20;
        case 0xE78D0F0:return 0xE76AC60;
        case 0xE7AF110:return 0xE79FE50;
        case 0xE6AACE0:return 0xE694AB0;
        case 0xE7BE6B0:return 0xE7AA500;
        case 0xE7BE730:return 0xE7AA580;
        case 0xE85A040:return 0xE92E240;
        case 0xE85A160:return 0xE8BCF20;
        case 0xE85A240:return 0xE92E360;
        case 0xE8F3920:return 0xE8BCE00;
        case 0xE8F3A40:return 0xE8BD0B0;
        case 0xE6D6060:return 0xE749DE0;
        case 0xE9AA770:return 0xEB1D820;
        case 0xE9A02E0:return 0xE970910;
        case 0xE8F7A40:return 0xE74B340;
        case 0xE827560:return 0xE81A090;
        case 0xE827600:return 0xE81A130;
        case 0xE6D6CD0:return 0xE698240;
        case 0xE6AF850:return 0xE69D4E0;
        case 0xE7E4B90:return 0xEA442D0;
        default:return 0;
    }
}
bool Table(void const* p,std::uintptr_t b,unsigned rva){auto physical=DataRva(rva);return p && b && physical && At<std::uintptr_t>(p,0)==b+physical;}
std::uint64_t TableLabel(std::uintptr_t value,std::uintptr_t b){return value>=b && value-b<0x12C02000?value-b:0;}
bool CheckedTable(void const* p,std::uintptr_t b,unsigned rva,RemoteCheck* check,RemoteReason reason){
    if(!p || !b)return Refuse(check,reason);
    auto actual=At<std::uintptr_t>(p,0);
    auto physical=DataRva(rva);
    return (physical && actual==b+physical) || Refuse(check,reason,TableLabel(actual,b));
}
bool LocalIdentifier(void const* id){
    auto kind=At<std::uint32_t>(id,168);
    return kind==4 || (kind==3 && At<std::uint8_t>(id,16)<=2);
}
bool SameLocalIdentifier(void const* a,void const* c){
    if(!LocalIdentifier(a) || !LocalIdentifier(c) || At<std::uint32_t>(a,168)!=At<std::uint32_t>(c,168))return false;
    if(At<std::uint32_t>(a,168)==4)return true;
    auto variant=At<std::uint8_t>(a,16);
    return variant==At<std::uint8_t>(c,16) && (!variant || At<std::uint64_t>(a,0)==At<std::uint64_t>(c,0)) &&
        (variant!=2 || At<std::uint64_t>(a,8)==At<std::uint64_t>(c,8));
}
bool ActiveLocalSession(void const* network,void const* id,std::uintptr_t b,RemoteCheck* check){
    //16AF460 uses the adjusted network+32 vector160/168/176 and supplies
    // its connected identifier to both D92690 and the registered handler.
    // D7E970's own peer112 is the OTHER endpoint, never an equality source.
    // This native session registration is transport-independent: it also
    // owns kind3 friend connections with no in-process local peer.
    auto first=At<std::uintptr_t>(network,192),last=At<std::uintptr_t>(network,200),limit=At<std::uintptr_t>(network,208);
    if(!first || first%8 || last%8 || limit%8 || last<first || limit<last || last-first>16*8)
        return Refuse(check,RemoteReason::SessionVector);
    void const* found{};void const* transport{};void const* controller{};void const* control{};void const* config{};
    for(auto p=first;p<last;p+=8){
        auto session=At<void const*>(reinterpret_cast<void const*>(p),0);
        if(!session)return Refuse(check,RemoteReason::SessionMissing);
        if(!SameLocalIdentifier(session,id))continue;
        if(found)return Refuse(check,RemoteReason::SessionAmbiguous);
        if(At<std::uint8_t>(session,409) || !At<void const*>(session,280))return Refuse(check,RemoteReason::SessionInactive);
        // D92690/16AF120/16C4500: independently owned controller contains a
        // second immutable copy of this same connected ID, not the own ID.
        controller=At<void const*>(session,416);control=At<void const*>(session,424);
        if(!Table(control,b,0xE6AACE0) || At<int>(control,8)<=0 || At<int>(control,12)<=0 ||
           reinterpret_cast<std::uintptr_t>(controller)!=reinterpret_cast<std::uintptr_t>(control)+16 ||
           !Table(controller,b,0xE7BE6B0))return Refuse(check,RemoteReason::SessionOwner);
        config=At<void const*>(controller,8);
        if(!Table(config,b,0xE7BE730))return Refuse(check,RemoteReason::SessionOwner);
        if(!SameLocalIdentifier(static_cast<std::byte const*>(config)+48,id))return Refuse(check,RemoteReason::SessionIdentity);
        found=session;transport=At<void const*>(session,280);
    }
    if(!found)return Refuse(check,RemoteReason::SessionMissing);
    // Borrowed values never escape this synchronous decoder. Refuse observed
    // vector/session retirement; do not call or retain any decoded session.
    if(first!=At<std::uintptr_t>(network,192) || last!=At<std::uintptr_t>(network,200) || limit!=At<std::uintptr_t>(network,208) ||
       At<std::uint8_t>(found,409) || At<void const*>(found,280)!=transport ||
       At<void const*>(found,416)!=controller || At<void const*>(found,424)!=control ||
       At<int>(control,8)<=0 || At<int>(control,12)<=0 || At<void const*>(controller,8)!=config ||
       !SameLocalIdentifier(found,id) || !SameLocalIdentifier(static_cast<std::byte const*>(config)+48,id))
        return Refuse(check,RemoteReason::SessionChanged);
    return true;
}
bool Chest(void const* p,std::uintptr_t b,RemoteCheck* check){
    if(!CheckedTable(p,b,0xE85A040,check,RemoteReason::ChestTable))return false;
    auto old=CurrentVersion()==GameVersion::V45;
    auto container=At<std::uintptr_t>(p,old?40:48);
    if(container!=b+DataRva(0xE85A160))return Refuse(check,RemoteReason::ChestContainerTable,TableLabel(container,b));
    auto pairing=At<std::uintptr_t>(p,old?272:280);
    if(pairing!=b+DataRva(0xE85A240))return Refuse(check,RemoteReason::ChestPairTable,TableLabel(pairing,b));
    auto type=At<std::uint8_t>(p,20);
    return type==2 || Refuse(check,RemoteReason::ChestType,type);
}
}
bool InvokeRemoteUse(RemoteUseEntry entry,void* mode,BlockPosition const* pos,unsigned char face) noexcept {
    if(!entry || !mode || !pos || face>5)return false;
    __try {entry(mode,pos,face,0,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool BackgroundInputAllowed(void const* player,bool shiftHeld,RemoteCheck* check) noexcept {
    Report(check,RemoteReason::Pass);
    if(shiftHeld)return Refuse(check,RemoteReason::ShiftHeld);
    __try {
        // Exact2834920(player+8), called by259D320 before native use:
        // component018B1887, stride100, byte0/bit1. Use the bounded existing
        // traversal and refuse unavailable input instead of assuming standing.
        auto input=ReadComponent(player,0x018B1887,100);
        if(!input)return Refuse(check,RemoteReason::InputUnavailable);
        if(At<std::uint8_t>(reinterpret_cast<void const*>(input),0)&1)return Refuse(check,RemoteReason::Sneaking,1);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return Refuse(check,RemoteReason::ReadFault);}
}
bool DecodeRemoteUse(void const* mode,void const* player,void const* hit,void const* hand,std::uintptr_t b,Target const& t,int gameType,RemoteUse& use,RemoteCheck* check,bool heldProtection) noexcept {
    Report(check,RemoteReason::Pass);
    __try {
        // 209D80 resolves the current game type, including native default5.
        // 1E1570 allocates272 bytes only for survival0, otherwise200 bytes.
        // Never call the derived wrapper on a base allocation (it reads200).
        if(gameType<0 || gameType>2)return Refuse(check,RemoteReason::GameType,static_cast<unsigned>(gameType));
        if(!CheckedTable(mode,b,gameType==0?0xE827600:0xE827560,check,RemoteReason::ModeTable))return false;
        if(!player)return Refuse(check,RemoteReason::PlayerMissing);
        if(At<void const*>(mode,8)!=player)return Refuse(check,RemoteReason::ModePlayer);
        if(At<void const*>(player,2720)!=mode)return Refuse(check,RemoteReason::PlayerMode);
        if(At<std::uintptr_t>(player,472)!=t.world)return Refuse(check,RemoteReason::World);
        if(t.kind!=TargetKind::Block)return Refuse(check,RemoteReason::Target);
        if(!hit)return Refuse(check,RemoteReason::HitMissing);
        auto hitType=At<int>(hit,24);if(hitType!=0)return Refuse(check,RemoteReason::HitType,static_cast<unsigned>(hitType));
        if(At<int>(hit,32)!=t.x || At<int>(hit,36)!=t.y || At<int>(hit,40)!=t.z)return Refuse(check,RemoteReason::HitPosition);
        auto hitFace=At<std::uint8_t>(hit,28);if(hitFace>5)return Refuse(check,RemoteReason::HitFace,hitFace);
        if(!hand)return Refuse(check,RemoteReason::HandMissing);
        if(!CheckedTable(hand,b,0xE6D6CD0,check,RemoteReason::HandTable))return false;
        auto count=At<std::uint8_t>(hand,34),valid=At<std::uint8_t>(hand,35);
        auto flags=static_cast<std::uint64_t>(count)|(static_cast<std::uint64_t>(valid)<<8);
        if(valid!=1)return Refuse(check,RemoteReason::HandInvalid,flags);
        if(count && heldProtection){
            auto weak=At<void const*>(hand,8);auto item=weak?At<void const*>(weak,0):nullptr;
            if(!item)return Refuse(check,RemoteReason::HandOccupied,flags);
            auto table=At<void const*>(item,0);
            // Native144 is the fertilizer-special block path used before
            // ItemStack::useOn. Only its exact false leaf is admitted here.
            // v1000 must enter our guarded adjustment seam. The caller also
            // verifies144 against the immutable disk vtable before dispatch.
            auto old=CurrentVersion()==GameVersion::V45;
            //45 native3D07DC0's Item tables bind20DAE10 at1000, which
            //dispatches1016.903CB0 is a similar unrelated thunk, rejected.
            if(!table || At<std::uintptr_t>(table,144)!=b+(old?0x76FB0:Rva(0x76C20)) || At<std::uintptr_t>(table,1000)!=b+(old?0x20DAE10:Rva(0x24C92E0)))
                return Refuse(check,RemoteReason::HeldItemUnsupported);
            use={hitFace,gameType==0?0x25A0810u:0x259BC60u,reinterpret_cast<std::uintptr_t>(item)};return true;
        }
        //2481B10/2484320 leave validity35=1 for a native empty stack.
        // Reproduce only2486D00's canonical empty tail. Calling that method
        // would also mutate Item/global name caches; no such call is made.
        if(count || At<std::uintptr_t>(hand,24) || At<std::uint16_t>(hand,32) ||
           At<std::uintptr_t>(hand,16) || At<std::uint64_t>(hand,72) || At<std::uint64_t>(hand,104))
            return Refuse(check,RemoteReason::HandOccupied,flags);
        auto item=At<void const*>(hand,8);
        if(item && At<std::uintptr_t>(item,0))return Refuse(check,RemoteReason::HandOccupied,flags);
        use={At<std::uint8_t>(hit,28),gameType==0?0x25A0810u:0x259BC60u};return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return Refuse(check,RemoteReason::ReadFault);}
}
bool ReleaseModelWeak(NativePair& pair,std::uintptr_t b,void(*destroy)(void*)) noexcept {
    __try {
        if(!pair.object && !pair.control)return true;
        auto control=pair.control;
        if(!destroy || !Table(control,b,0xE6D6060) || reinterpret_cast<std::uintptr_t>(pair.object)!=reinterpret_cast<std::uintptr_t>(control)+16 || At<int>(control,12)<=0)return false;
        // 6772D10 returns a weak pair. This is reference-count maintenance,
        // never an inventory write; use its exact native v8 deletion contract.
        pair={};
        auto count=reinterpret_cast<int*>(static_cast<std::byte*>(control)+12);
        if(std::atomic_ref<int>(*count).fetch_sub(1)==1)destroy(control);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool CreatorMatches(void const* functor,void const* factory,void const* name,void const* player,
    void const* position,void const* entity,void const* output,std::uintptr_t b,RemoteRequest const& r,std::uint64_t returnedEntity) noexcept {
    __try {
        auto client=reinterpret_cast<void const*>(r.client);auto const& t=r.preview.target;
        bool proxy=r.hiveProxy && !r.integrated && t.kind==TargetKind::Entity && r.slots==27 && r.blockType==0 &&
            t.entity && t.entityRuntime && returnedEntity && returnedEntity!=UINT64_MAX && returnedEntity!=t.unique;
        bool block=!r.hiveProxy && t.kind==TargetKind::Block && returnedEntity==UINT64_MAX &&
            ((r.blockType==2 && (r.slots==27 || r.slots==54)) || (r.blockType==23 && r.slots==27));
        auto old=CurrentVersion()==GameVersion::V45;
        if((!proxy && !block) || !Table(client,b,0xE9731B0) ||
           !Table(functor,b,0xE9A02E0) || At<std::uint8_t>(functor,8)!=(proxy?2:r.blockType) || !factory || At<void const*>(client,old?512:520)!=factory ||
           !player || reinterpret_cast<std::uintptr_t>(player)!=r.player || At<void const*>(player,3440)!=client || At<std::uintptr_t>(player,472)!=t.world ||
           At<void const*>(player,1440) || At<void const*>(player,1448) || At<void const*>(factory,24)!=client)return false;
        auto validity=At<std::uint8_t const*>(factory,8);if(!validity || *validity!=1)return false;
        void const* stack{};
        if(At<std::uint8_t>(client,old?1648:1656)!=0 || At<std::uint8_t>(factory,old?80:64)!=0)stack=At<void const*>(client,old?1480:1488);
        else{
            auto game=At<void const*>(client,old?416:424);if(!Table(game,b,0xE76E2D0))return false;
            stack=At<void const*>(game,old?7240:7536);
        }
        if(!Table(stack,b,0xE8F7A40))return false;
        validity=At<std::uint8_t const*>(stack,8);if(!validity || *validity!=1)return false;
        if(!name || !position || !entity || !output ||
           At<std::uint64_t>(entity,0)!=returnedEntity ||
           (!proxy && (At<int>(position,0)!=t.x || At<int>(position,4)!=t.y || At<int>(position,8)!=t.z)))return false;
        // 538040 passes its mapped creator metadata, not necessarily the
        // screen lookup key. Attribution is the exact callback/type, caller,
        // native deferred envelope and live owners, never guessed string data.
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool MatchesRemoteCreator(void const* functor,void const* factory,void const* metadata,void const* player,
    void const* position,void const* entity,void const* output,std::uintptr_t b,RemoteRequest const& r,std::uint64_t returnedEntity) noexcept {
    return CurrentVersion()!=GameVersion::V45 && CreatorMatches(functor,factory,metadata,player,position,entity,output,b,r,returnedEntity);
}
bool MatchesRemoteCreator45(void const* functor,void const* factory,void const* game,void const* client,
    void const* factoryView,void const* metadata,void const* player,void const* position,void const* entity,
    void const* output,std::uintptr_t b,RemoteRequest const& r,std::uint64_t returnedEntity) noexcept {
    __try {
        // Exact574C40 supplies factory32, factory24 and borrowed factory+40.
        // Check all ten-argument owner edges before accepting a headless result.
        if(CurrentVersion()!=GameVersion::V45 || !factory || !client || reinterpret_cast<std::uintptr_t>(client)!=r.client ||
           !Table(game,b,0xE76E2D0) || At<void const*>(factory,32)!=game || At<void const*>(factory,24)!=client ||
           At<void const*>(client,416)!=game || factoryView!=static_cast<std::byte const*>(factory)+40)return false;
        return CreatorMatches(functor,factory,metadata,player,position,entity,output,b,r,returnedEntity);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::optional<RemoteRoute> DecodeRemoteRoute(void const* client,std::uintptr_t b,RemoteCheck* check) noexcept {
    Report(check,RemoteReason::Pass);
    __try {
        if(!CheckedTable(client,b,0xE9731B0,check,RemoteReason::ClientTable))return {};
        auto old=CurrentVersion()==GameVersion::V45;
        auto game=At<void const*>(client,old?416:424);
        if(!CheckedTable(game,b,0xE76E2D0,check,RemoteReason::GameTable))return {};
        auto gameTable=At<std::uintptr_t>(game,48);
        if(gameTable!=b+DataRva(0xE76EAD0)){Report(check,RemoteReason::GameInterface,TableLabel(gameTable,b));return {};}
        //45 D6D790 -> game48/v80 ->963830 reads adjusted+1984, hence
        // root2032. Neither unadjusted1984 nor newer root2080 is valid.
        auto network=At<void const*>(game,old?2032:2080);
        if(!CheckedTable(network,b,0xE75E2D0,check,RemoteReason::NetworkTable))return {};
        auto networkTable=At<std::uintptr_t>(network,32);
        if(networkTable!=b+DataRva(0xE75E2F0)){Report(check,RemoteReason::NetworkInterface,TableLabel(networkTable,b));return {};}
        auto peer=At<void const*>(network,216),control=At<void const*>(network,224);
        bool integrated=peer || control;
        if(integrated){
            //475130 allocates this exact strong owner;474DA0 installs it.
            if(!Table(control,b,0xE760DA0) || At<int>(control,8)<=0 || At<int>(control,12)<=0 ||
               reinterpret_cast<std::uintptr_t>(peer)!=reinterpret_cast<std::uintptr_t>(control)+16 ||
               !Table(peer,b,0xE78D0F0) || At<void const*>(peer,8)!=static_cast<std::byte const*>(network)+32){
                Report(check,RemoteReason::LocalPeer);return {};
            }
        }
        auto registration=At<void const*>(network,472);if(!registration){Report(check,RemoteReason::Registration);return {};}
        auto handler=At<void const*>(registration,0);
        if(!CheckedTable(handler,b,0xE7AF110,check,RemoteReason::HandlerTable))return {};
        if(At<void const*>(handler,88)!=client){Report(check,RemoteReason::HandlerClient);return {};}
        //45 exact14F1C40 sets936 and copies the connected176-byte ID648.
        auto established=At<std::uint8_t>(handler,old?936:960);
        if(established!=1){Report(check,RemoteReason::Established,established);return {};}
        auto type=At<std::uint32_t>(handler,old?816:840);
        if(integrated){
            auto ownId=static_cast<std::byte const*>(peer)+112;
            auto connectedId=static_cast<std::byte const*>(handler)+(old?648:672);
            if(!LocalIdentifier(ownId) || !LocalIdentifier(connectedId)){
                Report(check,RemoteReason::LocalControl,(static_cast<std::uint64_t>(At<std::uint32_t>(peer,280))<<32)|type);return {};
            }
            if(!ActiveLocalSession(network,connectedId,b,check))return {};
        }else if(type==3){
            // Friend invitations can use an address identifier without a
            // local peer. The same generic16AF460 registration still binds
            // handler, active session and owned controller to one identity.
            // Keep this a remote route; kind4 remains local-only and no GUID
            // field is read from this union variant.
            auto connectedId=static_cast<std::byte const*>(handler)+(old?648:672);
            if(!LocalIdentifier(connectedId)){Report(check,RemoteReason::IdentifierType,type);return {};}
            if(!ActiveLocalSession(network,connectedId,b,check))return {};
        }else{
            if(type!=0){Report(check,RemoteReason::IdentifierType,type);return {};}
            if(At<std::uint64_t>(handler,old?672:696)==UINT64_MAX){Report(check,RemoteReason::GuidMissing);return {};}
        }
        return RemoteRoute{reinterpret_cast<std::uintptr_t>(client),reinterpret_cast<std::uintptr_t>(network),reinterpret_cast<std::uintptr_t>(handler),integrated};
    }__except(EXCEPTION_EXECUTE_HANDLER){Report(check,RemoteReason::ReadFault);return {};}
}
std::optional<RemoteShape> DecodeRemoteChest(void const* block,std::uintptr_t b,BlockPosition pos,RemoteCheck* check) noexcept {
    Report(check,RemoteReason::Pass);
    __try {
        auto old=CurrentVersion()==GameVersion::V45;auto partnerOffset=old?760:768;
        if(Table(block,b,0xE8F3920)){
            if(At<std::uintptr_t>(block,old?40:48)!=b+DataRva(0xE85A160) || At<std::uintptr_t>(block,old?272:280)!=b+DataRva(0xE8F3A40) ||
               At<std::uint8_t>(block,20)!=23 || At<int>(block,8)!=pos.x || At<int>(block,12)!=pos.y || At<int>(block,16)!=pos.z || At<void const*>(block,partnerOffset)){
                Report(check,RemoteReason::ChestType);return {};
            }
            return RemoteShape{27,23};
        }
        if(!Chest(block,b,check))return {};
        if(At<int>(block,8)!=pos.x || At<int>(block,12)!=pos.y || At<int>(block,16)!=pos.z){Report(check,RemoteReason::ChestPosition);return {};}
        auto partner=At<void const*>(block,partnerOffset);
        if(!partner)return RemoteShape{27,2};
        if(partner==block){Report(check,RemoteReason::ChestPartner);return {};}
        if(!Chest(partner,b,check))return {};
        if(At<void const*>(partner,partnerOffset)!=block){Report(check,RemoteReason::ChestPartner);return {};}
        if(At<int>(partner,12)!=pos.y){Report(check,RemoteReason::ChestPartnerY);return {};}
        auto lead=old?724:732;
        if(((At<std::uint8_t>(partner,lead)^At<std::uint8_t>(block,lead))&1)==0){Report(check,RemoteReason::ChestLead);return {};}
        auto dx=static_cast<std::int64_t>(At<int>(partner,8))-pos.x,dz=static_cast<std::int64_t>(At<int>(partner,16))-pos.z;
        if(!((dx==0 && (dz==1 || dz==-1)) || (dz==0 && (dx==1 || dx==-1)))){Report(check,RemoteReason::ChestAdjacent);return {};}
        return RemoteShape{54,2};
    }__except(EXCEPTION_EXECUTE_HANDLER){Report(check,RemoteReason::ReadFault);return {};}
}
bool BackgroundEligible(bool integrated,PreviewWork const& w,RemoteShape shape,RemoteRequest const* owned) noexcept {
    if(!((shape.blockType==2 && (shape.slots==27 || shape.slots==54)) || (shape.blockType==23 && shape.slots==27)))return false;
    if(!integrated)return true;
    if(!w.paired)return false;
    if(w.backgroundType==shape.blockType)return true;
    return owned && owned->integrated && owned->owner && owned->client && owned->player &&
        owned->preview.target==w.request.target && owned->preview.generation==w.request.generation &&
        owned->blockType==shape.blockType && owned->slots==shape.slots;
}
std::optional<RemoteModel> DecodeRemoteModel(void const* player,std::uintptr_t b,Target const& t,unsigned blockType,std::uint64_t entity) noexcept {
    __try {
        bool proxy=t.kind==TargetKind::Entity && blockType==0 && entity && entity!=UINT64_MAX && entity!=t.unique;
        bool block=t.kind==TargetKind::Block && (blockType==2 || blockType==23) && entity==UINT64_MAX;
        if((!proxy && !block) || !player || At<std::uintptr_t>(player,472)!=t.world || !At<void const*>(player,1456))return {};
        auto control=At<void const*>(player,1448),model=At<void const*>(player,1440);
        if(!Table(control,b,0xE6D6060) || At<int>(control,8)<=0 || At<int>(control,12)<=0 ||
           reinterpret_cast<std::uintptr_t>(model)!=reinterpret_cast<std::uintptr_t>(control)+16 || !Table(model,b,0xE9AA770) ||
           At<void const*>(model,48)!=player || At<std::uint8_t>(model,81)!=0 || At<int>(model,392)!=(proxy?0:t.x) ||
           At<int>(model,396)!=(proxy?0:t.y) || At<int>(model,400)!=(proxy?0:t.z) || At<std::uint64_t>(model,408)!=entity ||
           At<std::uint8_t>(model,416)!=blockType || At<std::uint8_t>(model,424)!=blockType)return {};
        return RemoteModel{reinterpret_cast<std::uintptr_t>(model),reinterpret_cast<std::uintptr_t>(control),At<std::uint8_t>(model,80)};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
bool DecodeRemoteEntityUse(void const* mode,void const* player,void const* hit,void const* hand,void const* actor,std::uintptr_t b,Target const& t,int gameType,RemoteCheck* check,bool heldProtection) noexcept {
    Report(check,RemoteReason::Pass);
    __try {
        if(gameType<0 || gameType>2)return Refuse(check,RemoteReason::GameType,static_cast<unsigned>(gameType));
        if(!CheckedTable(mode,b,gameType==0?0xE827600:0xE827560,check,RemoteReason::ModeTable))return false;
        if(!player || At<void const*>(mode,8)!=player || At<void const*>(player,2720)!=mode)return Refuse(check,RemoteReason::ModePlayer);
        if(t.kind!=TargetKind::Entity || !t.entity || !t.entityRuntime || !IsHiveDeathCrate(actor))return Refuse(check,RemoteReason::Target);
        if(At<std::uintptr_t>(player,472)!=t.world || At<std::uintptr_t>(actor,472)!=t.world)return Refuse(check,RemoteReason::World);
        auto identity=ReadIdentity(actor);
        if(identity.unique!=t.entity || identity.runtime!=t.entityRuntime || t.entity==t.unique)return Refuse(check,RemoteReason::Target);
        if(!hit || At<int>(hit,24)!=1)return Refuse(check,RemoteReason::HitType);
        for(unsigned o=44;o<=52;o+=4)if(!std::isfinite(At<float>(hit,o)))return Refuse(check,RemoteReason::HitPosition);
        if(!CheckedTable(hand,b,0xE6D6CD0,check,RemoteReason::HandTable))return false;
        if(At<std::uint8_t>(hand,35)!=1)return Refuse(check,RemoteReason::HandInvalid);
        if(At<std::uint8_t>(hand,34) && heldProtection){
            auto weak=At<void const*>(hand,8);
            if(!weak || !At<std::uintptr_t>(weak,0))return Refuse(check,RemoteReason::HandOccupied);
            // Only this exact native implementation has the audited planner.
            // Caller must verify its immutable v928 and install the full set.
            return Table(actor,b,0xE6AF850) || Refuse(check,RemoteReason::HeldItemUnsupported);
        }
        // Canonical empty tail only; never call the mutating empty predicate.
        if(At<std::uint8_t>(hand,34) || At<std::uintptr_t>(hand,24) || At<std::uint16_t>(hand,32) ||
           At<std::uintptr_t>(hand,16) || At<std::uint64_t>(hand,72) || At<std::uint64_t>(hand,104))return Refuse(check,RemoteReason::HandOccupied);
        auto item=At<void const*>(hand,8);
        return !item || !At<std::uintptr_t>(item,0) || Refuse(check,RemoteReason::HandOccupied);
    }__except(EXCEPTION_EXECUTE_HANDLER){return Refuse(check,RemoteReason::ReadFault);}
}
bool InvokeRemoteEntityUse(RemoteEntityUseEntry entry,void* mode,void* actor,void const* hitVector) noexcept {
    if(!entry || !mode || !actor || !hitVector)return false;
    __try {entry(mode,actor,hitVector,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool MatchesEntityOpenAction(void const* output,void const* actor,void const* player,std::uintptr_t b) noexcept {
    __try {
        //45 native18FB230 installs this callback only with flag96==0;
        //2FB8530 executes callback32/v16 and destroys32/v8 then title0.
        if(CurrentVersion()==GameVersion::V45)
            return b && output && actor && player && At<std::uintptr_t>(output,32)==b+0xE7B2C50 &&
                At<void const*>(output,40)==player && At<void const*>(output,48)==actor &&
                At<std::uint8_t>(output,96)==0;
        return b && output && actor && player && At<std::uintptr_t>(output,8)==b+Rva(0xE7CB810) &&
            At<void const*>(output,16)==actor && At<void const*>(output,24)==player &&
            At<std::uint8_t>(output,72)==1;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool RetireEntityAction(void* output,void(*destroy)(void*),void(*freeTitle)(void*)) noexcept {
    if(!output)return false;
    unsigned callback{},size{};
    switch(CurrentVersion()){
        case GameVersion::V45:callback=32;size=104;break;
        case GameVersion::V51:case GameVersion::V52:callback=8;size=80;break;
        default:return false;
    }
    bool ok=false;
    __try {
        __try {
            if(!freeTitle || (At<std::uintptr_t>(output,callback) && !destroy))return false;
            if(At<std::uintptr_t>(output,callback))destroy(static_cast<std::byte*>(output)+callback);
            freeTitle(output);ok=true;
        }__finally {std::memset(output,0,size);}
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return ok;
}
bool MatchesHiveProxyOwner(void const* actor,std::uintptr_t b,Target const& t,std::uint64_t returned) noexcept {
    __try {
        if(!actor || !returned || returned==UINT64_MAX || returned==t.unique || t.kind!=TargetKind::Entity ||
           !Table(actor,b,0xE7E4B90) || (At<std::uint8_t>(actor,528)&1) || At<std::uintptr_t>(actor,472)!=t.world)return false;
        auto id=ReadIdentity(actor);return id.unique==returned && id.runtime && id.runtime!=UINT64_MAX;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool MatchesRemoteConsistency(RemoteSession const& session,void const* stack,std::uintptr_t b,std::uintptr_t caller,std::uint32_t owner) noexcept {
    __try {
        // Only475EF30's model/screen consistency query, not general UI logic.
        // Exact per-client scene owners:45+1480;51/52+1488.
        if(!b || caller!=b+Rva(0x475FC80) || !owner ||
           (session.Phase()!=RemotePhase::Active && session.Phase()!=RemotePhase::Closing))return false;
        auto const& r=session.RequestValue();auto client=reinterpret_cast<void const*>(r.client);
        auto player=reinterpret_cast<void const*>(r.player);
        if(owner!=r.owner || !Table(client,b,0xE9731B0) || !player ||
           At<void const*>(player,3440)!=client || At<void const*>(client,CurrentVersion()==GameVersion::V45?1480:1488)!=stack || !Table(stack,b,0xE8F7A40))return false;
        auto validity=At<std::uint8_t const*>(stack,8);
        if(!validity || *validity!=1 || session.Id()<1 || session.Id()>100)return false;
        auto model=DecodeRemoteModel(player,b,r.preview.target,r.blockType,session.Entity());
        return model && model->model==session.Model() && model->control==session.Control() && model->id==session.Id();
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
