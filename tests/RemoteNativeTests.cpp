#include <string_view>
#include "ContainerPreview/NativeAddress.h"
#include "ContainerPreview/NativeRemote.h"
#include "ContainerPreview/HeldUse.h"
#include "EntityFixture.h"
#include <cstring>
#include <iostream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
using namespace preview;
namespace {
std::uint32_t FixtureRva(std::uint32_t key){auto value=Rva(key);return value?value:key;}
int checks{},failures{};
void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}
template<class T>void Put(void* p,unsigned o,T v){std::memcpy(static_cast<std::byte*>(p)+o,&v,sizeof v);}
constexpr std::uintptr_t base=0x140000000;
struct InteractionInputFixture {
    std::array<std::byte,32> player{};
    std::array<std::byte,128> registry{};
    std::array<std::byte,32> node{};
    std::array<std::byte,88> pool{};
    std::array<std::uint32_t,2048> sparse{};
    std::array<std::byte,12800> input{};
    std::uintptr_t bucket{},sparsePage{},dataPage{};
    InteractionInputFixture(){
        Put(player.data(),16,registry.data());Put(player.data(),24,std::uint32_t{17});
        Put(registry.data(),72,&bucket);Put(registry.data(),80,&bucket+1);
        Put(registry.data(),104,node.data());Put(registry.data(),112,node.data()+32);
        Put(node.data(),0,UINT64_MAX);Put(node.data(),8,std::uint32_t{0x018B1887});Put(node.data(),16,pool.data());
        sparse.fill(UINT32_MAX);sparse[17]=5;
        sparsePage=reinterpret_cast<std::uintptr_t>(sparse.data());dataPage=reinterpret_cast<std::uintptr_t>(input.data());
        Put(pool.data(),8,&sparsePage);Put(pool.data(),16,&sparsePage+1);Put(pool.data(),80,&dataPage);
    }
};
std::array<std::byte,152> EmptyHand(){
    // Exact2481B10 constructor followed by2484320: a valid empty stack
    // has validity35=1 and pickup36=1, not an all-zero object.
    std::array<std::byte,152> hand{};
    Put(hand.data(),0,base+FixtureRva(0xE6D6CD0));hand[35]=std::byte{1};hand[36]=std::byte{1};
    return hand;
}
unsigned weakDestroyed{};void* destroyedControl{};
void WeakDestroy(void* p){++weakDestroyed;destroyedControl=p;}
int registryStage{},registryCalls{};bool registryResult=true,registryBad{},registryMissing{};
std::array<int,4> registryControl{0,0,1,2};int registryObject{};
void* RegistryGet(void* client,void* out){
    registryBad|=registryStage!=0 || client!=reinterpret_cast<void*>(400);registryStage=1;
    *static_cast<NativePair*>(out)=registryMissing?NativePair{}:NativePair{&registryObject,registryControl.data()};return out;
}
void* RegistryEnter(void* out,void*){registryBad|=registryStage!=1;registryStage=2;return out;}
void RegistryLeave(void*){registryBad|=registryStage!=3 && registryStage!=2;registryStage=4;}
bool RegistryOperation(void*){++registryCalls;registryBad|=registryStage!=2;registryStage=3;return registryResult;}
bool RegistryFault(void*){++registryCalls;registryStage=3;RaiseException(0xE0420001,0,0,nullptr);return true;}
unsigned useCalls{};bool useBad{},useThrow{},useEscaped{};
std::uint64_t ProtectedNativeUse(void* mode,void const* p,unsigned char face,unsigned char arg4,unsigned char arg5){
    ++useCalls;auto pos=static_cast<BlockPosition const*>(p);
    useBad|=mode!=reinterpret_cast<void*>(400) || !pos || pos->x!=10 || pos->y!=20 || pos->z!=30 || face!=3 || arg4 || arg5;
    useBad|=!HeldUseScope::ProtectsSecondary(base+FixtureRva(0x25B55D6));
    if(useThrow)RaiseException(0xE0420024,0,0,nullptr);
    return 0; // Native denial is not a fault, nor proof of accepted content.
}
bool CatchUseEscape(BlockPosition const* pos){
    __try{return InvokeRemoteUse(ProtectedNativeUse,reinterpret_cast<void*>(400),pos,3);}
    __except(EXCEPTION_EXECUTE_HANDLER){useEscaped=true;return false;}
}
unsigned entityCalls{};bool entityThrow{},entityBad{};std::array<float,3> entityHit{1.f,2.f,3.f};
unsigned actionDestroyed{},actionFreed{},actionOffset=8;bool actionBad{},actionFault{};void* actionAddress{};
void ActionDestroy(void* p){++actionDestroyed;actionBad|=p!=static_cast<std::byte*>(actionAddress)+actionOffset;if(actionFault)RaiseException(0xE0420027,0,0,nullptr);}
void ActionFree(void* p){++actionFreed;actionBad|=p!=actionAddress || actionDestroyed!=1;}
std::uint64_t EntityDispatch(void* mode,void* actor,void const* hit,std::uintptr_t flags){
    ++entityCalls;entityBad|=mode!=reinterpret_cast<void*>(400) || actor!=reinterpret_cast<void*>(500) || hit!=entityHit.data() || flags!=0;
    if(entityThrow)RaiseException(0xE0420026,0,0,nullptr);return 0;
}
struct RouteFixture {
    std::array<std::byte,432> client{};
    std::array<std::byte,2088> game{};
    std::array<std::byte,480> network{};
    std::array<std::byte,968> handler{};
    void* registered{};
    RouteFixture(){
        Put(client.data(),0,base+FixtureRva(0xE9731B0));Put(client.data(),424,game.data());
        Put(game.data(),0,base+FixtureRva(0xE76E2D0));Put(game.data(),48,base+FixtureRva(0xE76EAD0));Put(game.data(),2080,network.data());
        Put(network.data(),0,base+FixtureRva(0xE75E2D0));Put(network.data(),32,base+FixtureRva(0xE75E2F0));
        registered=handler.data();Put(network.data(),472,&registered);
        Put(handler.data(),0,base+FixtureRva(0xE7AF110));Put(handler.data(),88,client.data());
        Put(handler.data(),960,std::uint8_t{1});Put(handler.data(),840,std::uint32_t{0});Put(handler.data(),696,UINT64_C(12345));
    }
};
struct LocalRouteFixture : RouteFixture {
    alignas(8) std::array<std::byte,392> peerControl{};
    alignas(8) std::array<std::byte,432> session{};
    alignas(8) std::array<std::byte,40> transportControl{};
    alignas(8) std::array<std::byte,72> controllerControl{};
    alignas(8) std::array<std::byte,224> config{};
    std::array<void*,2> sessions{};
    void* peer=peerControl.data()+16;
    void* transport=transportControl.data()+16;
    void* controller=controllerControl.data()+16;
    LocalRouteFixture(){
        Put(peerControl.data(),0,base+FixtureRva(0xE760DA0));Put(peerControl.data(),8,int{1});Put(peerControl.data(),12,int{1});
        Put(peer,0,base+FixtureRva(0xE78D0F0));Put(peer,8,network.data()+32);
        Put(network.data(),216,peer);Put(network.data(),224,peerControl.data());
        // D7E970 deliberately supplies the OPPOSITE endpoint to16AF460.
        // Own address0x1111 is NOT the registered connected address0x2222.
        Put(peer,280,std::uint32_t{3});Put(peer,128,std::uint8_t{1});Put(peer,112,UINT64_C(0x1111));
        Connected(3,1);
        sessions[0]=session.data();Vector(1);
        Put(session.data(),280,transport);Put(session.data(),288,transportControl.data());
        Put(transportControl.data(),0,base+FixtureRva(0xE78DE20));Put(transportControl.data(),8,int{1});Put(transportControl.data(),12,int{1});
        Put(transport,0,base+FixtureRva(0xE7BE6E0));
        Put(session.data(),416,controller);Put(session.data(),424,controllerControl.data());
        Put(controllerControl.data(),0,base+FixtureRva(0xE6AACE0));Put(controllerControl.data(),8,int{1});Put(controllerControl.data(),12,int{1});
        Put(controller,0,base+FixtureRva(0xE7BE6B0));Put(controller,8,config.data());Put(config.data(),0,base+FixtureRva(0xE7BE730));
    }
    void Vector(unsigned n){
        Put(network.data(),192,sessions.data());Put(network.data(),200,sessions.data()+n);Put(network.data(),208,sessions.data()+2);
    }
    void Connected(std::uint32_t kind,std::uint8_t variant){
        // Literal connected identity in handler, registered session and its
        // owned controller config. No production helper computes expectations.
        for(auto id:{handler.data()+672,session.data(),config.data()+48}){
            Put(id,168,kind);Put(id,16,variant);Put(id,0,UINT64_C(0x2222));Put(id,8,UINT64_C(0x3333));
        }
    }
};
void Chest(void* p,int x){
    Put(p,0,base+FixtureRva(0xE85A040));Put(p,48,base+FixtureRva(0xE85A160));Put(p,280,base+FixtureRva(0xE85A240));
    Put(p,8,x);Put(p,12,int{64});Put(p,16,int{-4});Put(p,20,std::uint8_t{2});
}
}
int main(int argc,char** argv){
    if(argc>1 && std::string_view(argv[1])=="51")native_detail::active.store(GameVersion::V51);
    // Names the break: ignoring crouch before reserving an operation creates
    // an unresolved Pending tombstone even though no container was opened.
    {
        InteractionInputFixture input;RemoteCheck why{};RemoteSession session;
        RemoteRequest request{{{100,200,300,0,10,20,30,TargetKind::Block},7,1000},1,400,500,27,2};
        auto admit=[&](bool shift,std::uint64_t now){
            std::optional<RemoteRequest> desired;
            if(BackgroundInputAllowed(input.player.data(),shift,&why))desired=request;
            return session.Step(desired,now,true);
        };
        Check(admit(true,1000)==RemoteAction::None && session.Phase()==RemotePhase::Idle && why.reason==RemoteReason::ShiftHeld,"Shift before Alt does not reserve a native open");
        input.input[500]=std::byte{1};
        Check(admit(false,1001)==RemoteAction::None && session.Phase()==RemotePhase::Idle && why.reason==RemoteReason::Sneaking,"native crouch still pauses after physical Shift release");
        Check(admit(false,5000)==RemoteAction::None && session.Phase()==RemotePhase::Idle,"holding crouch beyond timeout never creates a late-response tombstone");
        input.input[500]=std::byte{0};
        Check(admit(false,5001)==RemoteAction::Open && why.reason==RemoteReason::Pass,"standing resumes the same Alt hold without opening and closing a UI");
        Check(admit(false,9002)==RemoteAction::None && session.Phase()==RemotePhase::Pending,"an actually submitted request still has no timeout retry");
        RemoteCheck check{};auto saved=input.input;
        input.input[500]=std::byte{0xFE};input.input[502]=std::byte{4};saved=input.input;
        Check(BackgroundInputAllowed(input.player.data(),false,&check) && input.input==saved,"other input bits do not masquerade as crouch and input remains unchanged");
        input.input[500]=std::byte{0xFF};
        Check(!BackgroundInputAllowed(input.player.data(),false,&check) && check.reason==RemoteReason::Sneaking,"crouch bit remains blocked with other input flags present");
        input.sparse[17]=UINT32_MAX;
        Check(!BackgroundInputAllowed(input.player.data(),false,&check) && check.reason==RemoteReason::InputUnavailable,"missing native input membership fails closed before dispatch");
        Check(!BackgroundInputAllowed(nullptr,false,&check) && check.reason==RemoteReason::InputUnavailable,"missing player cannot authorize a new open");
        Check(!BackgroundInputAllowed(reinterpret_cast<void*>(1),false,&check),"unreadable input never escapes or authorizes native interaction");
        Check(!BackgroundInputAllowed(reinterpret_cast<void*>(1),true,&check) && check.reason==RemoteReason::ShiftHeld,"physical Shift pauses without dereferencing a native player");
    }
    std::array<std::byte,80> action{};actionAddress=action.data();
    Put(action.data(),8,base+FixtureRva(0xE7CB810));Put(action.data(),16,reinterpret_cast<void*>(500));Put(action.data(),24,reinterpret_cast<void*>(400));action[72]=std::byte{1};
    auto openAction=[&]{return MatchesEntityOpenAction(action.data(),reinterpret_cast<void*>(500),reinterpret_cast<void*>(400),base);};
    Check(openAction(),"exact native container action binds current aimed actor and local player");
    auto actionBytes=action;
    Put(action.data(),8,base+FixtureRva(0xE7CB7F8));Check(!openAction(),"dye action is refused before native callback execution");
    Put(action.data(),8,base+FixtureRva(0xE7CB810));Put(action.data(),16,reinterpret_cast<void*>(501));Check(!openAction(),"action cannot substitute another actor");
    Put(action.data(),16,reinterpret_cast<void*>(500));Put(action.data(),24,reinterpret_cast<void*>(401));Check(!openAction(),"action cannot substitute another player");
    action=actionBytes;action[72]=std::byte{0};Check(!openAction(),"nonprimary action is not container authorization");action=actionBytes;
    Check(!MatchesEntityOpenAction(nullptr,reinterpret_cast<void*>(500),reinterpret_cast<void*>(400),base),"absent action cannot authorize interaction");
    Check(RetireEntityAction(action.data(),ActionDestroy,ActionFree) && actionDestroyed==1 && actionFreed==1 && !actionBad && action==std::array<std::byte,80>{},"rejected action retires native callback then title and returns exact empty storage");
    Check(RetireEntityAction(action.data(),nullptr,ActionFree) && actionDestroyed==1 && actionFreed==2,"empty action does not need a callback destructor");
    action=actionBytes;
    Check(!RetireEntityAction(action.data(),nullptr,ActionFree) && action==std::array<std::byte,80>{},"unverified callback cannot execute and storage is cancelled on retirement failure");
    action=actionBytes;actionFault=true;
    Check(!RetireEntityAction(action.data(),ActionDestroy,ActionFree) && action==std::array<std::byte,80>{},"native callback retirement fault stays below scopes and cancels output");actionFault=false;
    Check(InvokeRemoteEntityUse(EntityDispatch,reinterpret_cast<void*>(400),reinterpret_cast<void*>(500),entityHit.data()) && entityCalls==1 && !entityBad,"entity dispatch preserves borrowed vector and native zero interaction flags; denial is not ACK");
    entityThrow=true;Check(!InvokeRemoteEntityUse(EntityDispatch,reinterpret_cast<void*>(400),reinterpret_cast<void*>(500),entityHit.data()) && entityCalls==2,"entity native faults return below caller-owned scopes");entityThrow=false;
    Check(!InvokeRemoteEntityUse(nullptr,reinterpret_cast<void*>(400),reinterpret_cast<void*>(500),entityHit.data()) && !InvokeRemoteEntityUse(EntityDispatch,nullptr,reinterpret_cast<void*>(500),entityHit.data()) && entityCalls==2,"incomplete entity dispatch never calls native operation");
    BlockPosition guardedPos{10,20,30};
    {
        HeldUseScope guard(base,reinterpret_cast<void*>(400),true);
        Check(InvokeRemoteUse(ProtectedNativeUse,reinterpret_cast<void*>(400),&guardedPos,3) && useCalls==1 && !useBad,"guarded native dispatch preserves face position main-hand arguments and denial result semantics");
        useThrow=true;
        Check(!CatchUseEscape(&guardedPos) && !useEscaped && useCalls==2 && !useBad,"native fault is caught below caller TLS scopes rather than escaping past their destructors");
        useThrow=false;
        Check(!InvokeRemoteUse(nullptr,reinterpret_cast<void*>(400),&guardedPos,3) && !InvokeRemoteUse(ProtectedNativeUse,nullptr,&guardedPos,3) && !InvokeRemoteUse(ProtectedNativeUse,reinterpret_cast<void*>(400),nullptr,3) && !InvokeRemoteUse(ProtectedNativeUse,reinterpret_cast<void*>(400),&guardedPos,6) && useCalls==2,"missing native dispatch prerequisites never execute a callback");
    }
    Check(!HeldUseScope::ProtectsSecondary(base+FixtureRva(0x25B55D6)),"normal unwinding after a native fault retires held-use protection");
    // Wrong transport/registration must not turn working integrated preview
    // into automatic interaction. Fixtures use literal exact-image layouts.
    RouteFixture f;
    auto route=DecodeRemoteRoute(f.client.data(),base);
    Check(route && route->client==reinterpret_cast<std::uintptr_t>(f.client.data()) && route->handler==reinterpret_cast<std::uintptr_t>(f.handler.data()),"current established GUID handler admits the remote route");
    Put(f.network.data(),216,reinterpret_cast<void*>(0x1000));
    Check(!DecodeRemoteRoute(f.client.data(),base),"local peer presence refuses remote interaction even with a stale GUID handler");
    Put(f.network.data(),216,static_cast<void*>(nullptr));Put(f.network.data(),224,reinterpret_cast<void*>(0x1000));
    Check(!DecodeRemoteRoute(f.client.data(),base),"local peer control cannot be ignored");
    Put(f.network.data(),224,static_cast<void*>(nullptr));Put(f.handler.data(),840,std::uint32_t{2});
    Check(!DecodeRemoteRoute(f.client.data(),base),"non-GUID transport is not classified as remote");
    Put(f.handler.data(),840,std::uint32_t{0});Put(f.handler.data(),960,std::uint8_t{0});
    Check(!DecodeRemoteRoute(f.client.data(),base),"unestablished handler cannot authorize an operation");
    Put(f.handler.data(),960,std::uint8_t{1});Put(f.handler.data(),88,reinterpret_cast<void*>(0x1000));
    Check(!DecodeRemoteRoute(f.client.data(),base),"foreign client handler is rejected");
    Put(f.handler.data(),88,f.client.data());f.registered=nullptr;
    Check(!DecodeRemoteRoute(f.client.data(),base),"unregistered handler is rejected");f.registered=f.handler.data();
    Put(f.game.data(),48,base+FixtureRva(0xE76E2D0));
    Check(!DecodeRemoteRoute(f.client.data(),base),"wrong adjusted game interface is rejected");Put(f.game.data(),48,base+FixtureRva(0xE76EAD0));
    Check(!DecodeRemoteRoute(reinterpret_cast<void*>(1),base) && !DecodeRemoteRoute(nullptr,base),"invalid route memory fails closed");
    {
        LocalRouteFixture local;RemoteCheck why{};
        auto inspect=[&]{return DecodeRemoteRoute(local.client.data(),base,&why);};
        auto localRoute=inspect();
        Check(localRoute && localRoute->integrated && why.reason==RemoteReason::Pass,"distinct own and opposite kind3 addresses use the matching active registered session");
        auto savedPeer=local.peerControl;auto savedSession=local.session;
        auto savedHandler=local.handler;auto savedConfig=local.config;
        Check(inspect().has_value() && savedPeer==local.peerControl && savedSession==local.session &&
              savedHandler==local.handler && savedConfig==local.config,"session admission is read only for all native owners");
        local.Vector(0);
        Check(!inspect() && std::strcmp(RemoteReasonName(why.reason),"local-session-missing")==0 && why.observed==0,"matching handler never suffices without an active session or leaks identity in diagnostics");
        Put(local.peer,112,UINT64_C(0x2222));
        Check(!inspect(),"even equal own and opposite identity never substitutes for session ownership");Put(local.peer,112,UINT64_C(0x1111));
        local.Vector(1);Put(local.session.data(),409,std::uint8_t{1});
        Check(!inspect(),"closed registered session cannot authorize background use");Put(local.session.data(),409,std::uint8_t{0});
        Put(local.session.data(),280,static_cast<void*>(nullptr));
        Check(!inspect(),"session without transport is not active");Put(local.session.data(),280,local.transport);
        Put(local.session.data(),0,UINT64_C(0x9999));
        Check(!inspect(),"session connected address must match registered handler not own endpoint");Put(local.session.data(),0,UINT64_C(0x2222));
        Put(local.session.data(),168,std::uint32_t{4});
        Check(!inspect(),"session kind must match connected handler");Put(local.session.data(),168,std::uint32_t{3});
        Put(local.session.data(),16,std::uint8_t{2});
        Check(!inspect(),"session address variant must match connected handler");Put(local.session.data(),16,std::uint8_t{1});
        local.Connected(3,2);
        Check(inspect().has_value(),"connected sixteen byte identity admits both matching active halves");
        Put(local.session.data(),8,UINT64_C(0x3334));
        Check(!inspect(),"upper active half mismatch cannot use a connected session");Put(local.session.data(),8,UINT64_C(0x3333));
        Put(local.config.data(),48,UINT64_C(0x2223));
        Check(!inspect(),"owned controller config must name this same connected session");Put(local.config.data(),48,UINT64_C(0x2222));
        Put(local.controllerControl.data(),8,int{0});
        Check(!inspect(),"expired session controller cannot authorize use");Put(local.controllerControl.data(),8,int{1});
        Put(local.controllerControl.data(),12,int{0});
        Check(!inspect(),"invalid weak count also rejects session controller");Put(local.controllerControl.data(),12,int{1});
        Put(local.session.data(),424,static_cast<void*>(nullptr));
        Check(!inspect() && why.reason==RemoteReason::SessionOwner,"missing session control refuses before accessing its counters");Put(local.session.data(),424,local.controllerControl.data());
        Put(local.session.data(),416,static_cast<std::byte*>(local.controller)+8);
        Check(!inspect(),"controller object must belong to exact in place strong control");Put(local.session.data(),416,local.controller);
        Put(local.controllerControl.data(),0,base+FixtureRva(0xE6AACE8));
        Check(!inspect(),"foreign controller implementation fails closed");Put(local.controllerControl.data(),0,base+FixtureRva(0xE6AACE0));
        Put(local.controller,0,base+FixtureRva(0xE7BE6B8));
        Check(!inspect(),"foreign controller object fails closed");Put(local.controller,0,base+FixtureRva(0xE7BE6B0));
        Put(local.config.data(),0,base+FixtureRva(0xE7BE738));
        Check(!inspect(),"foreign controller config fails closed");Put(local.config.data(),0,base+FixtureRva(0xE7BE730));
        Put(local.controller,8,static_cast<void*>(nullptr));
        Check(!inspect(),"missing owned config cannot authorize use");Put(local.controller,8,local.config.data());
        local.Connected(3,3);
        Check(!inspect(),"unknown connected address variant fails closed");local.Connected(3,255);
        Check(!inspect(),"uninitialized connected address variant fails closed");
        local.Connected(3,0);
        Put(local.session.data(),0,UINT64_C(0x5555));Put(local.config.data(),48,UINT64_C(0x6666));
        Check(inspect().has_value(),"variant zero ignores inactive connected payload across all three owners");
        local.Connected(4,255);
        Check(inspect().has_value(),"native kind4 connected session ignores inactive union and variant storage");
        local.Connected(2,0);
        Check(!inspect(),"registered nonlocal transport cannot acquire integrated privileges");
        local.Connected(3,1);
        Put(local.peer,280,std::uint32_t{4});
        Check(inspect().has_value(),"own and opposite native kinds are checked separately not equated");Put(local.peer,280,std::uint32_t{3});
        Put(local.peer,280,std::uint32_t{5});
        Check(!inspect(),"unproven own peer kind is refused independently of opposite session");Put(local.peer,280,std::uint32_t{3});
        Put(local.peer,128,std::uint8_t{3});
        Check(!inspect(),"unknown own address variant is refused without comparing opposite addresses");Put(local.peer,128,std::uint8_t{1});
        Put(local.peer,8,static_cast<void*>(nullptr));
        Check(!inspect() && why.reason==RemoteReason::LocalPeer,"active session never bypasses reciprocal local network ownership");Put(local.peer,8,local.network.data()+32);
        Put(local.peerControl.data(),8,int{0});
        Check(!inspect() && why.reason==RemoteReason::LocalPeer,"active session never bypasses expired local peer");Put(local.peerControl.data(),8,int{1});
        Put(local.handler.data(),88,reinterpret_cast<void*>(123));
        Check(!inspect() && why.reason==RemoteReason::HandlerClient,"active session never bypasses current handler client");Put(local.handler.data(),88,local.client.data());
        Put(local.handler.data(),960,std::uint8_t{0});
        Check(!inspect() && why.reason==RemoteReason::Established,"active session never bypasses established state");Put(local.handler.data(),960,std::uint8_t{1});
        local.sessions[1]=local.session.data();local.Vector(2);
        Check(!inspect(),"duplicate matching session ownership is ambiguous and refused");
        local.sessions[0]=nullptr;local.Vector(1);
        Check(!inspect(),"null session entry fails closed");
        local.sessions[0]=reinterpret_cast<void*>(1);
        Check(!inspect() && why.reason==RemoteReason::ReadFault,"unreadable registered session faults closed");
        local.sessions[0]=local.session.data();
        Put(local.network.data(),200,reinterpret_cast<std::uintptr_t>(local.sessions.data())+1);
        Check(!inspect(),"unaligned vector endpoint is refused before session traversal");
        Put(local.network.data(),200,reinterpret_cast<std::uintptr_t>(local.sessions.data())-8);
        Check(!inspect(),"inverted vector range is refused");
        Put(local.network.data(),200,reinterpret_cast<std::uintptr_t>(local.sessions.data())+8*17);
        Put(local.network.data(),208,reinterpret_cast<std::uintptr_t>(local.sessions.data())+8*17);
        Check(!inspect(),"session traversal is bounded before any out of range pointer read");
        local.Vector(1);
        Put(local.network.data(),216,static_cast<void*>(nullptr));Put(local.network.data(),224,static_cast<void*>(nullptr));
        auto connectedRoute=inspect();
        Check(connectedRoute && !connectedRoute->integrated,"connected kind3 with an independently verified session remains remote without a local peer");
    }
    // Names the break: the GUID-only remote branch rejects an established
    // friend-invitation connection before native use. Address kind3 must use
    // the registered active session, never a GUID fallback or local labeling.
    {
        LocalRouteFixture remote;RemoteCheck why{};
        Put(remote.network.data(),216,static_cast<void*>(nullptr));Put(remote.network.data(),224,static_cast<void*>(nullptr));
        auto inspect=[&]{return DecodeRemoteRoute(remote.client.data(),base,&why);};
        for(auto variant:{std::uint8_t{0},std::uint8_t{1},std::uint8_t{2}}){
            remote.Connected(3,variant);auto accepted=inspect();
            Check(accepted && !accepted->integrated && accepted->client==reinterpret_cast<std::uintptr_t>(remote.client.data()) &&
                  accepted->handler==reinterpret_cast<std::uintptr_t>(remote.handler.data()) && why.reason==RemoteReason::Pass,
                  "friend address variants0/1/2 admit only the registered active remote route");
        }
        auto beforeClient=remote.client;auto beforeGame=remote.game;auto beforeNetwork=remote.network;
        auto beforeHandler=remote.handler;auto beforeSession=remote.session;auto beforeOwner=remote.controllerControl;auto beforeConfig=remote.config;
        Check(inspect().has_value() && remote.client==beforeClient && remote.game==beforeGame && remote.network==beforeNetwork &&
              remote.handler==beforeHandler && remote.session==beforeSession && remote.controllerControl==beforeOwner && remote.config==beforeConfig,
              "friend admission does not modify client network identity session or ownership");
        remote.Vector(0);Check(!inspect() && why.reason==RemoteReason::SessionMissing,"friend handler alone cannot authorize use without an active session");remote.Vector(1);
        Put(remote.session.data(),409,std::uint8_t{1});Check(!inspect() && why.reason==RemoteReason::SessionInactive,"closed friend session is refused");Put(remote.session.data(),409,std::uint8_t{0});
        Put(remote.session.data(),280,static_cast<void*>(nullptr));Check(!inspect() && why.reason==RemoteReason::SessionInactive,"friend session requires a live transport");Put(remote.session.data(),280,remote.transport);
        Put(remote.session.data(),8,UINT64_C(0x3334));Check(!inspect() && why.reason==RemoteReason::SessionMissing,"friend active address mismatch cannot borrow another session");Put(remote.session.data(),8,UINT64_C(0x3333));
        Put(remote.config.data(),48,UINT64_C(0x9999));Check(!inspect() && why.reason==RemoteReason::SessionIdentity,"friend session controller must repeat the connected identity");Put(remote.config.data(),48,UINT64_C(0x2222));
        Put(remote.controllerControl.data(),8,int{0});Check(!inspect() && why.reason==RemoteReason::SessionOwner,"expired friend session owner cannot authorize use");Put(remote.controllerControl.data(),8,int{1});
        Put(remote.session.data(),424,static_cast<void*>(nullptr));Check(!inspect() && why.reason==RemoteReason::SessionOwner,"missing friend session owner fails closed");Put(remote.session.data(),424,remote.controllerControl.data());
        remote.sessions[1]=remote.session.data();remote.Vector(2);Check(!inspect() && why.reason==RemoteReason::SessionAmbiguous,"duplicate friend session attribution is refused");remote.Vector(1);
        Put(remote.handler.data(),960,std::uint8_t{0});Check(!inspect() && why.reason==RemoteReason::Established,"friend session does not bypass connection establishment");Put(remote.handler.data(),960,std::uint8_t{1});
        Put(remote.handler.data(),88,reinterpret_cast<void*>(1));Check(!inspect() && why.reason==RemoteReason::HandlerClient,"friend session does not bypass current client registration");Put(remote.handler.data(),88,remote.client.data());
        remote.Connected(3,3);Check(!inspect() && why.reason==RemoteReason::IdentifierType && why.observed==3,"unknown friend address variant is refused without logging its payload");
        remote.Connected(4,0);Check(!inspect() && why.reason==RemoteReason::IdentifierType,"local-only kind4 cannot become a friend connection");
        remote.Connected(2,0);Check(!inspect() && why.reason==RemoteReason::IdentifierType,"unverified remote transport kind remains refused");
        remote.Connected(3,1);Check(inspect().has_value(),"restored valid friend session clears prior refusal diagnostics");
    }
    std::array<std::byte,776> chest{},partner{};Chest(chest.data(),10);Chest(partner.data(),11);
    auto shape=DecodeRemoteChest(chest.data(),base,{10,64,-4});
    Check(shape && shape->slots==27 && shape->blockType==2,"ordinary chest shape is admitted without reading unsynchronized client contents");
    Put(chest.data(),256,UINT64_C(123));
    Check(DecodeRemoteChest(chest.data(),base,{10,64,-4}).has_value(),"unopened loot remains a normal server-validated open, not a guessed empty inventory");
    Put(chest.data(),768,partner.data());Put(partner.data(),768,chest.data());Put(chest.data(),732,std::uint8_t{1});
    shape=DecodeRemoteChest(chest.data(),base,{10,64,-4});
    Check(shape && shape->slots==54,"reciprocal adjacent double chest admits 54 server slots");
    Put(partner.data(),732,std::uint8_t{1});
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4}),"ambiguous double-chest order is rejected");Put(partner.data(),732,std::uint8_t{0});
    Put(partner.data(),8,int{12});
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4}),"nonadjacent chest relationship is rejected");Put(partner.data(),8,int{11});
    Put(partner.data(),768,static_cast<void*>(nullptr));
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4}),"one-sided partner relationship is rejected");Put(chest.data(),768,static_cast<void*>(nullptr));
    Check(!DecodeRemoteChest(chest.data(),base,{9,64,-4}),"wrong hit position is rejected");
    Put(chest.data(),20,std::uint8_t{25});
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4}),"unknown block type cannot borrow a chest layout");
    std::array<std::byte,776> ender{};
    Put(ender.data(),0,base+FixtureRva(0xE8F3920));Put(ender.data(),48,base+FixtureRva(0xE85A160));Put(ender.data(),280,base+FixtureRva(0xE8F3A40));
    Put(ender.data(),8,int{10});Put(ender.data(),12,int{64});Put(ender.data(),16,int{-4});Put(ender.data(),20,std::uint8_t{23});
    auto es=DecodeRemoteChest(ender.data(),base,{10,64,-4});
    Check(es && es->slots==27 && es->blockType==23,"verified Ender Chest shape requests only the current player's 27 slots");
    Put(ender.data(),768,chest.data());Check(!DecodeRemoteChest(ender.data(),base,{10,64,-4}),"Ender cannot claim an ordinary chest partner");Put(ender.data(),768,static_cast<void*>(nullptr));
    Put(ender.data(),20,std::uint8_t{2});Check(!DecodeRemoteChest(ender.data(),base,{10,64,-4}),"Ender tables with an ordinary chest type fail closed");
    std::array<std::byte,1464> player{};std::array<std::byte,448> allocation{};
    auto model=allocation.data()+16;Target t{100,200,300,0,10,64,-4,TargetKind::Block};
    int publisher{};
    Put(player.data(),472,reinterpret_cast<void*>(100));Put(player.data(),1440,model);Put(player.data(),1448,allocation.data());Put(player.data(),1456,&publisher);
    Put(allocation.data(),0,base+FixtureRva(0xE6D6060));Put(allocation.data(),8,int{1});Put(allocation.data(),12,int{2});
    Put(model,0,base+FixtureRva(0xE9AA770));Put(model,48,player.data());Put(model,80,std::uint8_t{2});Put(model,81,std::uint8_t{0});
    Put(model,392,int{10});Put(model,396,int{64});Put(model,400,int{-4});Put(model,408,UINT64_MAX);
    Put(model,416,std::uint8_t{2});Put(model,424,std::uint8_t{2});
    auto current=DecodeRemoteModel(player.data(),base,t);
    auto entityTarget=t;entityTarget.kind=TargetKind::Entity;entityTarget.entity=900;entityTarget.entityRuntime=901;
    Put(model,392,int{0});Put(model,396,int{0});Put(model,400,int{0});Put(model,408,UINT64_C(999));Put(model,416,std::uint8_t{0});Put(model,424,std::uint8_t{0});
    Check(DecodeRemoteModel(player.data(),base,entityTarget,0,999).has_value(),"entity model uses received dynamic owner not aimed actor UID");
    Check(!DecodeRemoteModel(player.data(),base,entityTarget,0,900) && !DecodeRemoteModel(player.data(),base,entityTarget,0,0) && !DecodeRemoteModel(player.data(),base,entityTarget,0,UINT64_MAX),"proxy model cannot borrow wrong zero or sentinel owner");
    Put(model,392,int{1});Check(!DecodeRemoteModel(player.data(),base,entityTarget,0,999),"entity model requires native zero block fields");
    Put(model,392,int{10});Put(model,396,int{64});Put(model,400,int{-4});Put(model,408,UINT64_MAX);Put(model,416,std::uint8_t{2});Put(model,424,std::uint8_t{2});
    Check(current && current->model==reinterpret_cast<std::uintptr_t>(model) && current->control==reinterpret_cast<std::uintptr_t>(allocation.data()) && current->id==2,"model is borrowed only from the exact current strong player owner");
    Put(model,416,std::uint8_t{23});Put(model,424,std::uint8_t{23});
    Check(DecodeRemoteModel(player.data(),base,t,23).has_value(),"own Ender model preserves its exact block type and standard type-zero protocol");
    Check(!DecodeRemoteModel(player.data(),base,t,2),"an Ender model cannot satisfy an ordinary chest session");
    Put(model,424,std::uint8_t{2});Check(!DecodeRemoteModel(player.data(),base,t,23),"constructor and container source block types must agree");
    Put(model,416,std::uint8_t{2});
    Put(model,80,std::uint8_t{255});current=DecodeRemoteModel(player.data(),base,t);
    Check(current && current->id==255,"unaccepted ID is reported faithfully and must be rejected by session acceptance");
    Put(model,48,static_cast<void*>(nullptr));
    Check(!DecodeRemoteModel(player.data(),base,t),"model owned by another player is rejected");Put(model,48,player.data());
    Put(model,408,UINT64_C(44));
    Check(!DecodeRemoteModel(player.data(),base,t),"entity model cannot impersonate a chest");Put(model,408,UINT64_MAX);
    Put(model,392,int{9});Check(!DecodeRemoteModel(player.data(),base,t),"foreign target model is rejected");Put(model,392,int{10});
    Put(allocation.data(),8,int{0});Check(!DecodeRemoteModel(player.data(),base,t),"expired strong owner is rejected");Put(allocation.data(),8,int{1});
    Put(player.data(),1456,static_cast<void*>(nullptr));Check(!DecodeRemoteModel(player.data(),base,t),"missing native model publisher refuses operation admission");
    NativePair weak{model,allocation.data()};Put(allocation.data(),12,int{2});
    Check(ReleaseModelWeak(weak,base,WeakDestroy) && !weak.object && !weak.control && weakDestroyed==0 && *reinterpret_cast<int*>(allocation.data()+12)==1 && *reinterpret_cast<int*>(allocation.data()+8)==1,"returned model weak reference is consumed once without touching strong ownership");
    Check(ReleaseModelWeak(weak,base,WeakDestroy) && weakDestroyed==0,"empty weak result is a no-op and cannot release twice");
    weak={model,allocation.data()};Put(allocation.data(),8,int{0});Put(allocation.data(),12,int{1});
    Check(ReleaseModelWeak(weak,base,WeakDestroy) && weakDestroyed==1 && destroyedControl==allocation.data(),"last weak reference invokes the verified control deletion boundary once");
    weak={model,allocation.data()};Put(allocation.data(),0,base+FixtureRva(0xE6D8360));Put(allocation.data(),12,int{2});
    Check(!ReleaseModelWeak(weak,base,WeakDestroy) && weak.control && weakDestroyed==1 && *reinterpret_cast<int*>(allocation.data()+12)==2,"unknown adapter control is never treated as a model weak reference");
    std::array<std::byte,1664> creatorClient{};std::array<std::byte,7544> creatorGame{};std::array<std::byte,72> factory{};
    std::array<std::byte,24> scene{};std::array<std::byte,16> functor{};std::uint8_t live=1;
    Put(creatorClient.data(),0,base+FixtureRva(0xE9731B0));Put(creatorClient.data(),424,creatorGame.data());Put(creatorClient.data(),520,factory.data());
    Put(creatorClient.data(),1488,scene.data());Put(creatorGame.data(),0,base+FixtureRva(0xE76E2D0));Put(creatorGame.data(),7536,scene.data());
    Put(factory.data(),8,&live);Put(factory.data(),24,creatorClient.data());
    Put(scene.data(),0,base+FixtureRva(0xE8F7A40));Put(scene.data(),8,&live);
    Put(functor.data(),0,base+FixtureRva(0xE9A02E0));Put(functor.data(),8,std::uint8_t{2});
    std::array<std::byte,3456> creatorPlayer{};Put(creatorPlayer.data(),3440,creatorClient.data());Put(creatorPlayer.data(),472,reinterpret_cast<void*>(100));
    RemoteRequest request{{t,7,1000},1,reinterpret_cast<std::uintptr_t>(creatorClient.data()),reinterpret_cast<std::uintptr_t>(creatorPlayer.data()),27,2};
    BlockPosition position{10,64,-4};std::uint64_t entity=UINT64_MAX;NativePair output{};
    std::string name="chest.small_chest_screen";
    auto matches=[&]{return MatchesRemoteCreator(functor.data(),factory.data(),&name,creatorPlayer.data(),&position,&entity,&output,base,request);};
    Check(matches(),"exact creator and global concrete scene stack admit headless model creation");
    request.blockType=23;Put(functor.data(),8,std::uint8_t{23});
    Check(matches(),"exact Ender creator is eligible for the same model-only factory");
    Put(functor.data(),8,std::uint8_t{2});Check(!matches(),"ordinary creator cannot consume an Ender request");
    request.blockType=2;
    Put(creatorClient.data(),1656,std::uint8_t{1});Check(matches(),"verified per-client concrete stack branch also admits headless creation");
    Put(scene.data(),0,base+FixtureRva(0xE8F7A48));Check(!matches(),"unproven scene-stack implementation never receives a suppressed scene");Put(scene.data(),0,base+FixtureRva(0xE8F7A40));
    name="opaque native creator metadata";Check(matches(),"native creator metadata stays opaque instead of being guessed to equal the factory lookup key");
    output.object=reinterpret_cast<void*>(1);Check(matches(),"native creator output is unconstructed storage and its prior bytes are never interpreted");output={};
    live=0;Check(!matches(),"dangling scene or factory owner cannot admit creation");live=1;
    entity=123;Check(!matches(),"entity path is never suppressed by the block-only creator");entity=UINT64_MAX;
    position.x++;Check(!matches(),"creator target must match the exact pending position");position.x--;
    Put(creatorPlayer.data(),1440,reinterpret_cast<void*>(1));Check(!matches(),"existing user model is never overwritten by the background factory");
    Put(creatorPlayer.data(),1440,static_cast<void*>(nullptr));
    auto proxyRequest=request;proxyRequest.preview.target=entityTarget;proxyRequest.blockType=0;proxyRequest.hiveProxy=true;
    Put(functor.data(),8,std::uint8_t{2});entity=999;
    Check(MatchesRemoteCreator(functor.data(),factory.data(),&name,creatorPlayer.data(),&position,&entity,&output,base,proxyRequest,999),"same native chest scene callback admits exact pending proxy returned owner");
    Check(!MatchesRemoteCreator(functor.data(),factory.data(),&name,creatorPlayer.data(),&position,&entity,&output,base,proxyRequest,998),"creator cannot substitute another proxy than deferred envelope");
    proxyRequest.hiveProxy=false;Check(!MatchesRemoteCreator(functor.data(),factory.data(),&name,creatorPlayer.data(),&position,&entity,&output,base,proxyRequest,999),"generic entity scene remains untouched");
    entity=UINT64_MAX;
    std::array<std::byte,208> mode{};std::array<std::byte,136> hit{};auto hand=EmptyHand();
    Put(mode.data(),0,base+FixtureRva(0xE827600));Put(mode.data(),8,creatorPlayer.data());Put(creatorPlayer.data(),2720,mode.data());
    Put(hit.data(),24,int{0});Put(hit.data(),28,std::uint8_t{3});Put(hit.data(),32,int{10});Put(hit.data(),36,int{64});Put(hit.data(),40,int{-4});
    RemoteUse use{255,0};int gameType=0;
    {
        EntityFixture aimed;aimed.Definition("hivesky","death_crate");Put(aimed.actor.data(),528,std::uint8_t{2});Put(aimed.actor.data(),472,UINT64_C(100));
        auto et=t;et.unique=800;et.kind=TargetKind::Entity;et.entity=200;et.entityRuntime=300;
        Put(hit.data(),24,int{1});Put(hit.data(),44,1.f);Put(hit.data(),48,2.f);Put(hit.data(),52,3.f);
        auto dec=[&]{return DecodeRemoteEntityUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),aimed.actor.data(),base,et,gameType);};
        Check(dec(),"exact empty-hand entity use admits normal survival native path");
        hand[34]=std::byte{1};Check(!dec(),"death-crate use refuses held items without mutating hand");hand=EmptyHand();
        std::uintptr_t heldItem=base+FixtureRva(0x123000);Put(hand.data(),8,&heldItem);hand[34]=std::byte{32};
        Put(aimed.actor.data(),0,base+FixtureRva(0xE6AF850));
        auto heldBytes=hand;
        Check(DecodeRemoteEntityUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),aimed.actor.data(),base,et,gameType,nullptr,true) && hand==heldBytes,"guarded exact death-crate use admits held stack without writing it");
        Check(!dec(),"held death-crate use requires explicit entity protection");
        Put(aimed.actor.data(),0,base+FixtureRva(0xE6AF858));
        Check(!DecodeRemoteEntityUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),aimed.actor.data(),base,et,gameType,nullptr,true),"unknown actor implementation refuses held interaction despite matching definition");
        Put(aimed.actor.data(),0,base+FixtureRva(0xE6AF850));heldItem=0;
        Check(!DecodeRemoteEntityUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),aimed.actor.data(),base,et,gameType,nullptr,true),"occupied hand without live item refuses entity interaction");
        hand=EmptyHand();
        et.entityRuntime++;Check(!dec(),"runtime identity change refuses stale death crate");et.entityRuntime--;
        Put(aimed.actor.data(),528,std::uint8_t{3});Check(!dec(),"player flags never authorize entity use");Put(aimed.actor.data(),528,std::uint8_t{2});
        Put(hit.data(),44,std::numeric_limits<float>::infinity());Check(!dec(),"invalid native hit vector cannot reach interaction");Put(hit.data(),44,1.f);
        gameType=3;Check(!dec(),"spectator entity use is refused");gameType=0;
        Put(aimed.actor.data(),472,UINT64_C(101));Check(!dec(),"entity use requires aimed actor in current world");
        Put(aimed.actor.data(),472,UINT64_C(100));Put(aimed.actor.data(),0,base+FixtureRva(0xE7E4B90));Put(aimed.actor.data(),528,std::uint8_t{18});
        Check(MatchesHiveProxyOwner(aimed.actor.data(),base,et,200),"returned owner requires current exact native nonplayer proxy and resolved UID");
        Check(!MatchesHiveProxyOwner(aimed.actor.data(),base,et,201),"resolved proxy UID must equal native returned UID");
        Put(aimed.actor.data(),528,std::uint8_t{19});Put(aimed.actor.data(),16,std::uintptr_t{1});
        Check(!MatchesHiveProxyOwner(aimed.actor.data(),base,et,200),"proxy player exclusion precedes identity registry traversal");
        Put(hit.data(),24,int{0});
    }
    auto decodeUse=[&]{return DecodeRemoteUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),base,t,gameType,use);};
    Check(decodeUse() && use.face==3 && use.entry==0x25A0810,"survival retains its derived native wrapper and current hit face");
    Put(mode.data(),0,base+FixtureRva(0xE827560));use={255,0};gameType=1;
    Check(decodeUse() && use.face==3 && use.entry==0x259BC60,"creative uses the base entry without reading beyond its 200-byte allocation");
    gameType=2;use={255,0};
    Check(decodeUse() && use.face==3 && use.entry==0x259BC60,"adventure uses the base native interaction entry");
    for(auto rejected:{-1,3,4,5,6,7,255}){
        gameType=rejected;use={255,0};
        Check(!decodeUse() && use.face==255 && !use.entry,"spectator default unknown and missing game types never authorize native use");
    }
    gameType=0;Check(!decodeUse(),"survival type cannot borrow the base GameMode implementation");
    Put(mode.data(),0,base+FixtureRva(0xE827600));
    for(auto rejected:{1,2,6}){gameType=rejected;Check(!decodeUse(),"derived survival table cannot authorize a different or transitional game type");}
    gameType=0;
    for(auto admitted:{0,1,2}){
        gameType=admitted;Put(mode.data(),0,base+(admitted==0?0xE827600:0xE827560));
        auto beforeMode=mode;auto beforePlayer=creatorPlayer;auto beforeHit=hit;auto beforeEmpty=hand;
        Check(decodeUse() && mode==beforeMode && creatorPlayer==beforePlayer && hit==beforeHit && hand==beforeEmpty,"each admitted mode is decoded without changing native player mode hit or hand");
        hand[34]=std::byte{1};Check(!decodeUse(),"all admitted modes retain the empty-main-hand gate");hand=EmptyHand();
        Put(mode.data(),8,reinterpret_cast<void*>(1));Check(!decodeUse(),"all admitted modes retain exact reciprocal player ownership");Put(mode.data(),8,creatorPlayer.data());
        Put(creatorPlayer.data(),472,UINT64_C(0xABC));Check(!decodeUse(),"all admitted modes retain current world binding");Put(creatorPlayer.data(),472,t.world);
        Put(hit.data(),32,int{11});Check(!decodeUse(),"all admitted modes retain the exact current hit position");Put(hit.data(),32,int{10});
        RemoteCheck refusal{};gameType=6;use={255,0};
        Check(!DecodeRemoteUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),base,t,gameType,use,&refusal) &&
              refusal.reason==RemoteReason::GameType && refusal.observed==6 && !use.entry,"spectator is refused explicitly before either native mode entry is selected");
    }
    // Catch an accidental derived-field read on a real200-byte base object.
    auto pages=static_cast<std::byte*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(pages!=nullptr,"bounded base-mode fixture allocation succeeds");
    if(pages){
        DWORD previous{};bool protectedTail=VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&previous)!=0;
        Check(protectedTail,"base-mode allocation ends immediately at an inaccessible page");
        if(protectedTail){
            auto baseMode=pages+4096-200;Put(baseMode,0,base+FixtureRva(0xE827560));Put(baseMode,8,creatorPlayer.data());Put(creatorPlayer.data(),2720,baseMode);
            for(auto admitted:{1,2}){
                use={255,0};Check(DecodeRemoteUse(baseMode,creatorPlayer.data(),hit.data(),hand.data(),base,t,admitted,use) && use.entry==0x259BC60,"creative and adventure never read derived survival byte200");
            }
        }
        VirtualFree(pages,0,MEM_RELEASE);
    }
    gameType=0;Put(mode.data(),0,base+FixtureRva(0xE827600));Put(creatorPlayer.data(),2720,mode.data());
    hand[34]=std::byte{1};hand[35]=std::byte{1};
    Check(!decodeUse(),"held item cannot enter the native use-or-place fallback");hand=EmptyHand();
    Put(hit.data(),24,int{2});Check(!decodeUse(),"out-of-range native hit cannot cause a request");Put(hit.data(),24,int{0});
    Put(hit.data(),28,std::uint8_t{6});Check(!decodeUse(),"invalid block face is rejected");Put(hit.data(),28,std::uint8_t{3});
    Put(creatorPlayer.data(),2720,reinterpret_cast<void*>(1));Check(!decodeUse(),"game mode must be reciprocally owned by this player");Put(creatorPlayer.data(),2720,mode.data());
    RegistryOps ops{RegistryGet,RegistryEnter,RegistryLeave};
    Check(InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr) && registryStage==4 && registryCalls==1 && !registryBad,"native registry getter, scope, operation and restoration occur in order exactly once");
    registryStage=0;registryResult=false;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr) && registryStage==4 && registryCalls==2 && !registryBad,"operation refusal still restores the native registry scope");
    ops.leave=nullptr;registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr) && registryStage==0 && registryCalls==2,"incomplete registry cleanup contract admits no native operation");
    ops.leave=RegistryLeave;registryMissing=true;registryStage=0;registryResult=true;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr) && registryStage==4 && registryCalls==2,"missing native registry consumes scope cleanup but never executes an operation");
    registryMissing=false;registryControl[2]=0;registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr) && registryStage==4 && registryCalls==2,"expired registry cannot authorize an operation");
    registryControl[2]=1;registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryFault,nullptr) && registryStage==4 && registryCalls==3 && !registryBad,"native operation fault still restores registry scope exactly once");
    // Missing diagnostics previously conflated all these distinct failures
    // with an idle policy. Optional observation must preserve the decision.
    RouteFixture probe;RemoteCheck why{};
    auto probeRoute=[&]{return DecodeRemoteRoute(probe.client.data(),base,&why);};
    Check(probeRoute().has_value() && why.reason==RemoteReason::Pass,"successful route clears prior rejection diagnostics");
    Put(probe.network.data(),472,static_cast<void*>(nullptr));
    Check(!probeRoute() && why.reason==RemoteReason::Registration,"missing registration is distinguished before handler access");Put(probe.network.data(),472,&probe.registered);
    probe.registered=nullptr;
    Check(!probeRoute() && why.reason==RemoteReason::HandlerTable && why.observed==0,"empty registration differs from an absent registration object");probe.registered=probe.handler.data();
    Put(probe.handler.data(),0,base+FixtureRva(0x123456));
    Check(!probeRoute() && why.reason==RemoteReason::HandlerTable && why.observed==0x123456,"unexpected handler reports only its image-relative table label");Put(probe.handler.data(),0,base+FixtureRva(0xE7AF110));
    Put(probe.handler.data(),840,std::uint32_t{2});
    Check(!probeRoute() && why.reason==RemoteReason::IdentifierType && why.observed==2,"identifier rejection reports actual type without endpoint data");Put(probe.handler.data(),840,std::uint32_t{0});
    Put(probe.handler.data(),960,std::uint8_t{0});
    Check(!probeRoute() && why.reason==RemoteReason::Established,"unestablished handler reason cannot look like registry failure");Put(probe.handler.data(),960,std::uint8_t{1});
    Put(probe.network.data(),216,reinterpret_cast<void*>(0x1000));
    Check(!probeRoute() && why.reason==RemoteReason::LocalPeer,"local peer gate remains fail closed and independently visible");Put(probe.network.data(),216,static_cast<void*>(nullptr));
    Check(!DecodeRemoteRoute(reinterpret_cast<void*>(1),base,&why) && why.reason==RemoteReason::ReadFault,"inaccessible route reports fault without dereferencing later owners");
    auto probeUse=[&]{return DecodeRemoteUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),base,t,gameType,use,&why);};
    Put(mode.data(),0,base+FixtureRva(0xABCDEF));
    Check(!probeUse() && why.reason==RemoteReason::ModeTable && why.observed==0xABCDEF,"mode mismatch reports actual image table instead of removing survival gate");Put(mode.data(),0,base+FixtureRva(0xE827600));
    hand[34]=std::byte{1};
    Check(!probeUse() && why.reason==RemoteReason::HandOccupied,"nonempty main hand is a distinct use refusal");hand=EmptyHand();
    Put(hit.data(),32,int{11});
    Check(!probeUse() && why.reason==RemoteReason::HitPosition,"changed hit position is distinct from mode or hand refusal");Put(hit.data(),32,int{10});
    Check(probeUse() && use.face==3 && why.reason==RemoteReason::Pass && why.observed==0,"successful use clears stale failure scalar");
    // Catch conflating native validity with occupancy. These fixtures come
    // from the constructor/clear and the read-only tail of2486D00, not from
    // the implementation under test. Do not call the cache-mutating isNull.
    auto beforeHand=hand;
    Check(probeUse() && hand==beforeHand,"initialized native empty hand is admitted without modifying any stack byte");
    std::array<std::byte,16> expiredItem{};
    Put(hand.data(),8,expiredItem.data());
    Check(probeUse(),"expired weak item with zero payload is empty by the native predicate");
    std::array<std::byte,16> liveItem{};Put(liveItem.data(),0,reinterpret_cast<void*>(1234));
    Put(hand.data(),8,liveItem.data());use={255,0};
    Check(!probeUse() && use.face==255 && !use.entry,"live item identity with zero count is not trusted as empty");
    hand=EmptyHand();
    // Clearing may retain allocated empty vector storage. Sizes72/104,
    // rather than capacity pointers, determine whether its payload is empty.
    std::array<std::byte,32> storage{};
    Put(hand.data(),48,storage.data());Put(hand.data(),56,storage.data());Put(hand.data(),64,storage.data()+16);
    Put(hand.data(),80,storage.data());Put(hand.data(),88,storage.data());Put(hand.data(),96,storage.data()+16);
    Check(probeUse(),"native-cleared empty stack may retain allocated empty vector capacity");
    for(auto offset:{16u,24u,72u,104u}){
        hand=EmptyHand();Put(hand.data(),offset,UINT64_C(1));use={255,0};
        Check(!probeUse() && use.face==255 && !use.entry,"zero count cannot hide tag block or nonempty component payload");
    }
    hand=EmptyHand();Put(hand.data(),32,std::uint16_t{1});use={255,0};
    Check(!probeUse() && use.face==255 && !use.entry,"nonzero auxiliary data is not canonical empty");
    hand=EmptyHand();hand[35]=std::byte{0};
    Check(!probeUse() && std::strcmp(RemoteReasonName(why.reason),"hand-invalid")==0,"invalid stack is refused rather than treated as confirmed empty");
    hand[35]=std::byte{2};Check(!probeUse(),"unknown validity flag fails closed");
    hand=EmptyHand();Put(hand.data(),0,base+FixtureRva(0xE6D6CD8));
    Check(!probeUse() && std::strcmp(RemoteReasonName(why.reason),"hand-stack-table")==0,"foreign hand representation cannot authorize native use");
    hand={};Check(!probeUse(),"unconstructed all-zero storage is not a native empty hand");
    hand=EmptyHand();Put(hand.data(),8,reinterpret_cast<void*>(1));use={255,0};
    Check(!probeUse() && why.reason==RemoteReason::ReadFault && use.face==255 && !use.entry,"unreadable weak item fails closed without assigning a hit face");
    hand=EmptyHand();hand[34]=std::byte{64};
    Check(!probeUse() && why.reason==RemoteReason::HandOccupied && why.observed==0x140,"hand refusal records only count and validity scalars");
    hand=EmptyHand();
    Check(!DecodeRemoteUse(mode.data(),creatorPlayer.data(),hit.data(),nullptr,base,t,gameType,use,&why) && why.reason==RemoteReason::HandMissing,"missing main hand never authorizes interaction");
    // The new admission must fail if protection is omitted or a held item has
    // an unverified placement dispatcher / fertilizer-special block path.
    std::array<std::byte,1040> heldTable{};std::array<std::byte,176> heldItem{};std::array<std::byte,16> heldWeak{};
    Put(heldTable.data(),144,base+FixtureRva(0x76C20));Put(heldTable.data(),1000,base+FixtureRva(0x24C92E0));
    Put(heldItem.data(),0,heldTable.data());Put(heldWeak.data(),0,heldItem.data());
    hand=EmptyHand();hand[34]=std::byte{64};Put(hand.data(),8,heldWeak.data());
    auto heldBefore=hand;auto tableBefore=heldTable;auto itemBefore=heldItem;auto weakBefore=heldWeak;
    auto protectedUse=[&]{return DecodeRemoteUse(mode.data(),creatorPlayer.data(),hit.data(),hand.data(),base,t,gameType,use,&why,true);};
    for(auto allowedMode:{0,1,2}){
        gameType=allowedMode;Put(mode.data(),0,base+(allowedMode==0?0xE827600:0xE827560));use={255,0};
        Check(!probeUse() && why.reason==RemoteReason::HandOccupied,"occupied hand is still refused without an explicit protection contract");
        Check(protectedUse() && use.item==reinterpret_cast<std::uintptr_t>(heldItem.data()) && use.face==3 && use.entry==(allowedMode==0?0x25A0810u:0x259BC60u),"protected ordinary held item retains native mode wrapper and exact target");
    }
    Check(hand==heldBefore && heldTable==tableBefore && heldItem==itemBefore && heldWeak==weakBefore,"held admission never clears replaces or changes inventory or item bytes");
    gameType=3;use={255,0};Check(!protectedUse() && !use.entry && !use.item,"held protection cannot admit spectator");
    gameType=0;Put(mode.data(),0,base+FixtureRva(0xE827600));
    Put(heldTable.data(),1000,base+FixtureRva(0x24C92E8));use={255,0};Check(!protectedUse() && !use.entry && !use.item,"unknown placement entry is refused rather than escaping the scoped hook");Put(heldTable.data(),1000,base+FixtureRva(0x24C92E0));
    Put(heldTable.data(),144,base+FixtureRva(0x848A0));Check(!protectedUse(),"fertilizer-style block interaction is conservatively excluded");Put(heldTable.data(),144,base+FixtureRva(0x76C20));
    Put(heldWeak.data(),0,static_cast<void*>(nullptr));Check(!protectedUse(),"nonzero count with missing native item is not accepted");Put(heldWeak.data(),0,heldItem.data());
    hand[35]=std::byte{0};Check(!protectedUse(),"held protection does not bypass native stack validity");hand[35]=std::byte{1};
    Put(hit.data(),32,int{11});Check(!protectedUse(),"held protection does not bypass current aim");Put(hit.data(),32,int{10});
    Put(heldItem.data(),0,reinterpret_cast<void*>(1));Check(!protectedUse() && why.reason==RemoteReason::ReadFault,"unreadable held table fails closed");Put(heldItem.data(),0,heldTable.data());
    hand=EmptyHand();use={255,0};Check(protectedUse() && !use.item,"canonical empty hand retains the accepted existing path");
    registryMissing=true;registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr,&why) && why.reason==RemoteReason::RegistryMissing && registryStage==4 && registryCalls==3,"missing registry is diagnosed without changing scope cleanup or operation count");
    registryMissing=false;registryControl[2]=0;registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr,&why) && why.reason==RemoteReason::RegistryExpired && registryStage==4 && registryCalls==3,"expired registry reason remains distinct from missing registry");
    registryControl[2]=1;registryStage=0;registryResult=false;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr,&why) && why.reason==RemoteReason::RegistryRefused && registryStage==4 && registryCalls==4,"operation refusal remains distinct and restores scope");
    registryStage=0;
    Check(!InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryFault,nullptr,&why) && why.reason==RemoteReason::RegistryFault && registryStage==4 && registryCalls==5,"native fault reason is retained after finally restores scope");
    registryStage=0;registryResult=true;
    Check(InNativeRegistry(ops,reinterpret_cast<void*>(400),RegistryOperation,nullptr,&why) && why.reason==RemoteReason::Pass && registryStage==4 && registryCalls==6,"successful registry operation clears a prior fault diagnosis");
    Chest(chest.data(),10);Put(chest.data(),768,static_cast<void*>(nullptr));
    Check(DecodeRemoteChest(chest.data(),base,{10,64,-4},&why).has_value() && why.reason==RemoteReason::Pass,"recognized chest clears stale admission diagnostics");
    Put(chest.data(),0,base+FixtureRva(0xE85A041));
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4},&why) && why.reason==RemoteReason::ChestTable && why.observed==0xE85A041,"unknown chest table is visible without reading its contents");Put(chest.data(),0,base+FixtureRva(0xE85A040));
    Put(chest.data(),48,base+FixtureRva(0xE85A161));
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4},&why) && why.reason==RemoteReason::ChestContainerTable,"chest interface mismatch is distinct from absence of a block actor");Put(chest.data(),48,base+FixtureRva(0xE85A160));
    Put(chest.data(),20,std::uint8_t{25});
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4},&why) && why.reason==RemoteReason::ChestType && why.observed==25,"wrong chest type remains rejected and reports the numeric type");Put(chest.data(),20,std::uint8_t{2});
    Check(!DecodeRemoteChest(chest.data(),base,{11,64,-4},&why) && why.reason==RemoteReason::ChestPosition,"stale block actor position is a separate shape failure");
    Put(chest.data(),768,partner.data());
    Check(!DecodeRemoteChest(chest.data(),base,{10,64,-4},&why) && why.reason==RemoteReason::ChestPartner,"nonreciprocal double chest reports relationship failure");
    // Errcode1 assumes a current native model also has a visible/pending
    // container screen. Only our accepted headless owner earns an exception.
    // Literal offsets/caller below come from475EF30/5DC6A60, not the decoder.
    {
        std::array<std::byte,1664> clientState{};
        std::array<std::byte,3456> playerState{};
        std::array<std::byte,656> stackState{};
        std::array<std::byte,448> modelState{};
        std::uint8_t alive=1;int nativePublisher{};
        auto ownedModel=modelState.data()+16;
        Put(clientState.data(),0,base+FixtureRva(0xE9731B0));Put(clientState.data(),1488,stackState.data());
        Put(playerState.data(),3440,clientState.data());Put(playerState.data(),472,reinterpret_cast<void*>(100));
        Put(playerState.data(),1440,ownedModel);Put(playerState.data(),1448,modelState.data());Put(playerState.data(),1456,&nativePublisher);
        Put(stackState.data(),0,base+FixtureRva(0xE8F7A40));Put(stackState.data(),8,&alive);
        Put(modelState.data(),0,base+FixtureRva(0xE6D6060));Put(modelState.data(),8,int{1});Put(modelState.data(),12,int{2});
        Put(ownedModel,0,base+FixtureRva(0xE9AA770));Put(ownedModel,48,playerState.data());Put(ownedModel,80,std::uint8_t{2});
        Put(ownedModel,392,int{10});Put(ownedModel,396,int{64});Put(ownedModel,400,int{-4});Put(ownedModel,408,UINT64_MAX);
        Put(ownedModel,416,std::uint8_t{2});Put(ownedModel,424,std::uint8_t{2});
        RemoteRequest ownedRequest{{t,17,1000},7,reinterpret_cast<std::uintptr_t>(clientState.data()),reinterpret_cast<std::uintptr_t>(playerState.data()),27,2};
        RemoteSession ownedSession;
        auto admits=[&](std::uintptr_t caller=base+FixtureRva(0x475FC80),std::uint32_t owner=7){
            return MatchesRemoteConsistency(ownedSession,stackState.data(),base,caller,owner);
        };
        auto start=[&]{
            ownedSession=RemoteSession{};ownedSession.Step(ownedRequest,1000,true);
            ownedSession.Created(reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()));
            ownedSession.Accepted(2,reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()));
        };
        Check(!admits(),"idle session cannot exempt a manually opened model");
        ownedSession.Step(ownedRequest,1000,true);
        Check(!admits(),"pending interaction without a created model cannot exempt consistency");
        ownedSession.Created(reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()));
        Check(!admits(),"created but unaccepted model cannot exempt consistency");
        ownedSession.Accepted(2,reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()));
        auto clientBefore=clientState;auto playerBefore=playerState;auto stackBefore=stackState;auto modelBefore=modelState;
        Check(admits() && !ownedSession.Visible(),"accepted owned headless model is exempt before the first full contents arrive");
        ownedRequest.blockType=23;Put(ownedModel,416,std::uint8_t{23});Put(ownedModel,424,std::uint8_t{23});start();
        Check(admits(),"exact owned Ender headless model uses the same narrowly attributed consistency exception");
        Put(ownedModel,424,std::uint8_t{2});Check(!admits(),"Ender exception cannot hide a mismatched native content source");
        ownedRequest.blockType=2;Put(ownedModel,416,std::uint8_t{2});Put(ownedModel,424,std::uint8_t{2});start();
        Check(!admits(base+FixtureRva(0x475FC81)),"another return address cannot exempt the scene predicate");
        Check(!admits(base+FixtureRva(0x475FC80),8) && !admits(base+FixtureRva(0x475FC80),0),"foreign and missing callback owner cannot exempt consistency");
        Check(!MatchesRemoteConsistency(ownedSession,stackState.data(),0,base+FixtureRva(0x475FC80),7),"absent exact image identity cannot exempt consistency");
        Check(!MatchesRemoteConsistency(ownedSession,nullptr,base,base+FixtureRva(0x475FC80),7),"missing scene stack is not a headless owner");
        Check(!MatchesRemoteConsistency(ownedSession,reinterpret_cast<void*>(1),base,base+FixtureRva(0x475FC80),7),"inaccessible scene stack fails closed");
        Put(clientState.data(),1488,reinterpret_cast<void*>(1));Check(!admits(),"foreign per-client scene stack cannot borrow the owned model");Put(clientState.data(),1488,stackState.data());
        Put(clientState.data(),0,base+FixtureRva(0xE9731B8));Check(!admits(),"unverified client table cannot exempt consistency");Put(clientState.data(),0,base+FixtureRva(0xE9731B0));
        Put(playerState.data(),3440,reinterpret_cast<void*>(1));Check(!admits(),"nonreciprocal player client cannot exempt consistency");Put(playerState.data(),3440,clientState.data());
        Put(stackState.data(),0,base+FixtureRva(0xE8F7A48));Check(!admits(),"unknown scene stack cannot exempt consistency");Put(stackState.data(),0,base+FixtureRva(0xE8F7A40));
        alive=0;Check(!admits(),"expired scene stack cannot exempt consistency");alive=1;
        Put(stackState.data(),8,static_cast<void*>(nullptr));Check(!admits(),"absent scene validity cannot exempt consistency");Put(stackState.data(),8,&alive);
        Put(playerState.data(),472,reinterpret_cast<void*>(101));Check(!admits(),"changed world cannot exempt consistency");Put(playerState.data(),472,reinterpret_cast<void*>(100));
        Put(ownedModel,80,std::uint8_t{3});Check(!admits(),"different accepted server ID cannot borrow a headless exception");
        Put(ownedModel,80,std::uint8_t{255});Check(!admits(),"unaccepted native model ID is not exempt");Put(ownedModel,80,std::uint8_t{2});
        Put(modelState.data(),8,int{0});Check(!admits(),"expired current model strong owner is not exempt");Put(modelState.data(),8,int{1});
        Put(modelState.data(),12,int{0});Check(!admits(),"invalid current model weak owner is not exempt");Put(modelState.data(),12,int{2});
        Put(modelState.data(),0,base+FixtureRva(0xE6D8360));Check(!admits(),"ordinary UI adapter control is never a headless model control");Put(modelState.data(),0,base+FixtureRva(0xE6D6060));
        Put(ownedModel,81,std::uint8_t{1});Check(!admits(),"nonchest model type is not exempt");Put(ownedModel,81,std::uint8_t{0});
        Put(ownedModel,392,int{11});Check(!admits(),"mismatched chest position is not exempt");Put(ownedModel,392,int{10});
        Put(ownedModel,408,UINT64_C(42));Check(!admits(),"entity container does not inherit a block headless exception");Put(ownedModel,408,UINT64_MAX);
        Put(playerState.data(),1440,reinterpret_cast<void*>(1));Check(!admits(),"rebound current model cannot inherit attribution");Put(playerState.data(),1440,ownedModel);
        Put(playerState.data(),1456,static_cast<void*>(nullptr));Check(!admits(),"missing native publisher cannot exempt consistency");Put(playerState.data(),1456,&nativePublisher);
        std::array<std::byte,448> replacement=modelState;auto replacementModel=replacement.data()+16;
        Put(playerState.data(),1440,replacementModel);Put(playerState.data(),1448,replacement.data());
        Check(!admits(),"identical contents with a different native model and control cannot borrow ownership");
        Put(playerState.data(),1440,ownedModel);Put(playerState.data(),1448,modelState.data());
        ownedRequest.hiveProxy=true;ownedRequest.blockType=0;ownedRequest.preview.target=entityTarget;
        Put(ownedModel,392,int{0});Put(ownedModel,396,int{0});Put(ownedModel,400,int{0});Put(ownedModel,408,UINT64_C(999));
        Put(ownedModel,416,std::uint8_t{0});Put(ownedModel,424,std::uint8_t{0});
        ownedSession=RemoteSession{};ownedSession.Step(ownedRequest,1000,true);
        ownedSession.Created(reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()),999);
        ownedSession.Accepted(2,reinterpret_cast<std::uintptr_t>(ownedModel),reinterpret_cast<std::uintptr_t>(modelState.data()));
        Check(admits(),"proxy consistency exemption requires exact accepted model and stored server owner");
        Put(ownedModel,408,UINT64_C(998));Check(!admits(),"reused model with a different proxy owner immediately loses exemption");Put(ownedModel,408,UINT64_C(999));
        // Remaining ordinary model regression checks keep their original setup.
        ownedRequest.hiveProxy=false;ownedRequest.blockType=2;ownedRequest.preview.target=t;
        Put(ownedModel,392,int{10});Put(ownedModel,396,int{64});Put(ownedModel,400,int{-4});Put(ownedModel,408,UINT64_MAX);
        Put(ownedModel,416,std::uint8_t{2});Put(ownedModel,424,std::uint8_t{2});start();
        ownedSession.Applied(2,reinterpret_cast<std::uintptr_t>(ownedModel),27,0x7FFFFFF);
        Check(ownedSession.Visible() && admits(),"ready owned preview keeps its precise consistency exception");
        ownedSession.Cancel();Check(admits(),"closing session retains exception only while its exact native model remains current");
        Put(playerState.data(),1440,static_cast<void*>(nullptr));Put(playerState.data(),1448,static_cast<void*>(nullptr));
        Check(!admits(),"closed native model immediately loses the exception even before close reply");
        Put(playerState.data(),1440,ownedModel);Put(playerState.data(),1448,modelState.data());
        ownedSession.CloseReply(ownedRequest.client,7,2);Check(!admits(),"completed close does not exempt later manual containers");
        start();ownedSession.Relinquish();Check(!admits(),"manual takeover removes consistency exception");
        start();Check(admits(),"new precisely accepted owner can be exempt after a completed retirement");
        Check(clientState==clientBefore && playerState==playerBefore && stackState==stackBefore && modelState==modelBefore && alive==1,
            "consistency admission writes no native scene inventory or reference-count byte");
    }
    {
        PreviewWork w{{t,7,1000},{1001,200,300,true,false}};
        Check(BackgroundEligible(false,w,{27,2},nullptr),"remote ordinary chest needs normal server acceptance rather than an integrated snapshot");
        Check(!BackgroundEligible(true,w,{27,2},nullptr),"local transport alone never authorizes background fallback");
        w.paired=true;w.backgroundType=2;
        Check(BackgroundEligible(true,w,{27,2},nullptr),"paired ungenerated ordinary loot authorizes one integrated fallback");
        w.backgroundType=0;Check(!BackgroundEligible(true,w,{27,2},nullptr),"already readable local chest stays on read-only path");
        w.backgroundType=23;Check(BackgroundEligible(true,w,{27,23},nullptr),"paired own Ender target authorizes background interaction");
        Check(!BackgroundEligible(true,w,{54,23},nullptr) && !BackgroundEligible(false,w,{27,25},nullptr),"wrong Ender shape and unsupported remote types are refused");
        RemoteRequest inFlight{w.request,7,400,500,27,2,true};w.backgroundType=0;
        Check(BackgroundEligible(true,w,{27,2},&inFlight),"loot generation becoming ready does not cancel an owned matching pending session");
        inFlight.preview.generation++;Check(!BackgroundEligible(true,w,{27,2},&inFlight),"in-flight fallback does not carry across request generations");
    }
    native_detail::active.store(GameVersion::V45);
    {
        // Independently literal45 layout. Newer fields are deliberately empty
        // or poisoned; deriving these from Rva() would conceal a wrong binding.
        std::array<std::byte,3456> client45{},player45{};
        std::array<std::byte,7544> game45{};
        std::array<std::byte,480> network45{};
        std::array<std::byte,1088> handler45{};
        void* registration45=handler45.data();
        Put(client45.data(),0,base+0xE774DB0);Put(client45.data(),416,game45.data());
        Put(game45.data(),0,base+0xE759890);Put(game45.data(),48,base+0xE75A090);Put(game45.data(),2032,network45.data());
        Put(network45.data(),0,base+0xE749B70);Put(network45.data(),32,base+0xE749B90);Put(network45.data(),472,&registration45);
        Put(handler45.data(),0,base+0xE79FE50);Put(handler45.data(),88,client45.data());
        Put(handler45.data(),936,std::uint8_t{1});Put(handler45.data(),816,std::uint32_t{0});Put(handler45.data(),672,UINT64_C(12345));
        auto route45=[&]{return DecodeRemoteRoute(client45.data(),base);};
        auto decodedRoute45=route45();auto routeBeforeClient=client45;auto routeBeforeGame=game45;auto routeBeforeNetwork=network45;auto routeBeforeHandler=handler45;
        Check(decodedRoute45 && !decodedRoute45->integrated && decodedRoute45->network==reinterpret_cast<std::uintptr_t>(network45.data()) && decodedRoute45->handler==reinterpret_cast<std::uintptr_t>(handler45.data()),
              "45 route normalizes game48 getter1984 to root2032 and uses established936 identifier648");
        Put(handler45.data(),936,std::uint8_t{0});Put(handler45.data(),960,std::uint8_t{1});Check(!route45(),"45 newer established flag cannot authorize an unestablished connection");
        handler45=routeBeforeHandler;Put(handler45.data(),816,std::uint32_t{2});Check(!route45(),"45 remote identifier kind is checked at816");
        handler45=routeBeforeHandler;Put(handler45.data(),672,UINT64_MAX);Check(!route45(),"45 missing connected GUID is refused at its exact field");
        handler45=routeBeforeHandler;Put(game45.data(),2032,static_cast<void*>(nullptr));Put(game45.data(),1984,network45.data());Put(game45.data(),2080,network45.data());
        Check(!route45(),"45 unadjusted getter field and newer root field cannot substitute for root2032");game45=routeBeforeGame;
        Check(route45().has_value() && client45==routeBeforeClient && game45==routeBeforeGame && network45==routeBeforeNetwork && handler45==routeBeforeHandler,
              "45 route reads do not write native client game network or handler");
        alignas(8) std::array<std::byte,392> peerOwner45{};
        alignas(8) std::array<std::byte,432> session45{};
        alignas(8) std::array<std::byte,72> controllerOwner45{};
        alignas(8) std::array<std::byte,224> config45{};
        int transport45{};void* sessions45[]={session45.data()};auto peer45=peerOwner45.data()+16;auto controller45=controllerOwner45.data()+16;
        Put(peerOwner45.data(),0,base+0xE74AD20);Put(peerOwner45.data(),8,int{1});Put(peerOwner45.data(),12,int{1});
        Put(peer45,0,base+0xE76AC60);Put(peer45,8,network45.data()+32);Put(peer45,112,UINT64_C(0x1111));Put(peer45,128,std::uint8_t{1});Put(peer45,280,std::uint32_t{3});
        Put(network45.data(),216,peer45);Put(network45.data(),224,peerOwner45.data());
        Put(network45.data(),192,sessions45);Put(network45.data(),200,sessions45+1);Put(network45.data(),208,sessions45+1);
        Put(handler45.data(),648,UINT64_C(0x2222));Put(handler45.data(),664,std::uint8_t{1});Put(handler45.data(),816,std::uint32_t{3});
        Put(session45.data(),0,UINT64_C(0x2222));Put(session45.data(),16,std::uint8_t{1});Put(session45.data(),168,std::uint32_t{3});Put(session45.data(),280,&transport45);
        Put(session45.data(),416,controller45);Put(session45.data(),424,controllerOwner45.data());
        Put(controllerOwner45.data(),0,base+0xE694AB0);Put(controllerOwner45.data(),8,int{1});Put(controllerOwner45.data(),12,int{1});
        Put(controller45,0,base+0xE7AA500);Put(controller45,8,config45.data());Put(config45.data(),0,base+0xE7AA580);
        Put(config45.data(),48,UINT64_C(0x2222));Put(config45.data(),64,std::uint8_t{1});Put(config45.data(),216,std::uint32_t{3});
        decodedRoute45=route45();Check(decodedRoute45 && decodedRoute45->integrated,"45 integrated route matches connected endpoint and independently owned live controller");
        Put(config45.data(),48,UINT64_C(0x1111));Check(!route45(),"45 own peer endpoint never replaces connected session identity");Put(config45.data(),48,UINT64_C(0x2222));
        session45[409]=std::byte{1};Check(!route45(),"45 inactive local session cannot authorize preview");session45[409]=std::byte{};
        Put(controllerOwner45.data(),8,int{0});Check(!route45(),"45 retired session controller fails closed");Put(controllerOwner45.data(),8,int{1});
        Put(network45.data(),216,static_cast<void*>(nullptr));Put(network45.data(),224,static_cast<void*>(nullptr));
        auto friendRoute45=route45();Check(friendRoute45 && !friendRoute45->integrated,"45 friend connection uses identifier648 and active registered session without a local peer");
        Put(config45.data(),48,UINT64_C(0x9999));Check(!route45(),"45 friend controller identity mismatch fails closed");Put(config45.data(),48,UINT64_C(0x2222));
        session45[409]=std::byte{1};Check(!route45(),"45 closed friend session cannot authorize background opening");session45[409]=std::byte{};
        Put(handler45.data(),664,std::uint8_t{3});Check(!route45(),"45 unknown friend address variant fails closed");Put(handler45.data(),664,std::uint8_t{1});
        Put(handler45.data(),816,std::uint32_t{4});Check(!route45(),"45 local-only kind4 remains refused without a local peer");Put(handler45.data(),816,std::uint32_t{3});
        Check(route45().has_value(),"45 friend route recovers when the established owned connection is restored");
        std::array<std::byte,824> chest45{},partner45{},ender45{};
        BlockPosition pos45{10,64,-4};
        auto chestFields45=[&](void* p){Put(p,0,base+0xE92E240);Put(p,40,base+0xE8BCF20);Put(p,272,base+0xE92E360);Put(p,20,std::uint8_t{2});Put(p,8,int{10});Put(p,12,int{64});Put(p,16,int{-4});};
        chestFields45(chest45.data());chestFields45(partner45.data());
        auto decodedShape45=DecodeRemoteChest(chest45.data(),base,pos45);Check(decodedShape45 && decodedShape45->slots==27 && decodedShape45->blockType==2,"45 remote single chest uses interface40 inventory272");
        Put(chest45.data(),760,partner45.data());Put(partner45.data(),760,chest45.data());Put(partner45.data(),8,int{11});chest45[724]=std::byte{1};
        decodedShape45=DecodeRemoteChest(chest45.data(),base,pos45);Check(decodedShape45 && decodedShape45->slots==54,"45 double chest requires reciprocal partner760 and complementary lead724");
        partner45[724]=std::byte{1};Check(!DecodeRemoteChest(chest45.data(),base,pos45),"45 double chest with two leaders is refused");partner45[724]=std::byte{};
        Put(ender45.data(),0,base+0xE8BCE00);Put(ender45.data(),40,base+0xE8BCF20);Put(ender45.data(),272,base+0xE8BD0B0);Put(ender45.data(),20,std::uint8_t{23});
        Put(ender45.data(),8,int{10});Put(ender45.data(),12,int{64});Put(ender45.data(),16,int{-4});
        decodedShape45=DecodeRemoteChest(ender45.data(),base,pos45);Check(decodedShape45 && decodedShape45->slots==27 && decodedShape45->blockType==23,"45 own Ender background shape does not decode player contents from the block");
        Put(ender45.data(),760,chest45.data());Check(!DecodeRemoteChest(ender45.data(),base,pos45),"45 Ender cannot use a paired chest relationship");
        // Breaks caught: accepting the newer creator ABI/fields, skipping any
        // borrowed owner, selecting the wrong scene branch, or overwriting a
        // manual model. Values below come from exact45 native call chains.
        std::array<std::byte,88> factory45{};std::array<std::byte,24> scene45{};
        std::array<std::byte,16> functor45{};std::uint8_t live45=1;
        Put(client45.data(),512,factory45.data());Put(client45.data(),1480,scene45.data());Put(game45.data(),7240,scene45.data());
        Put(factory45.data(),8,&live45);Put(factory45.data(),24,client45.data());Put(factory45.data(),32,game45.data());
        Put(scene45.data(),0,base+0xE74B340);Put(scene45.data(),8,&live45);
        Put(functor45.data(),0,base+0xE970910);Put(functor45.data(),8,std::uint8_t{2});
        Put(player45.data(),3440,client45.data());Put(player45.data(),472,std::uintptr_t{100});
        Target creatorTarget45{};creatorTarget45.world=100;creatorTarget45.unique=200;creatorTarget45.runtime=300;
        creatorTarget45.kind=TargetKind::Block;creatorTarget45.x=10;creatorTarget45.y=64;creatorTarget45.z=-4;
        RemoteRequest creatorRequest45{{creatorTarget45,7,1000},1,reinterpret_cast<std::uintptr_t>(client45.data()),reinterpret_cast<std::uintptr_t>(player45.data()),27,2};
        int opaque45{};std::uint64_t entity45=UINT64_MAX;NativePair out45{};
        auto creatorMatches45=[&]{return MatchesRemoteCreator45(functor45.data(),factory45.data(),game45.data(),client45.data(),factory45.data()+40,
            &opaque45,player45.data(),&pos45,&entity45,&out45,base,creatorRequest45);};
        auto creatorSaved45=client45;auto factorySaved45=factory45;auto gameSaved45=game45;auto sceneSaved45=scene45;auto playerSaved45=player45;
        Check(creatorMatches45(),"45 ten-argument creator admits exact live global scene and all borrowed owners");
        Check(!MatchesRemoteCreator(functor45.data(),factory45.data(),&opaque45,player45.data(),&pos45,&entity45,&out45,base,creatorRequest45),"45 cannot enter through the newer seven-argument creator contract");
        Check(!MatchesRemoteCreator45(functor45.data(),factory45.data(),client45.data(),client45.data(),factory45.data()+40,&opaque45,player45.data(),&pos45,&entity45,&out45,base,creatorRequest45),"45 creator rejects substituted borrowed game");
        Check(!MatchesRemoteCreator45(functor45.data(),factory45.data(),game45.data(),game45.data(),factory45.data()+40,&opaque45,player45.data(),&pos45,&entity45,&out45,base,creatorRequest45),"45 creator rejects substituted borrowed client");
        Check(!MatchesRemoteCreator45(functor45.data(),factory45.data(),game45.data(),client45.data(),factory45.data()+48,&opaque45,player45.data(),&pos45,&entity45,&out45,base,creatorRequest45),"45 creator rejects a different factory subobject");
        Put(client45.data(),512,static_cast<void*>(nullptr));Put(client45.data(),520,factory45.data());Check(!creatorMatches45(),"45 newer client factory field cannot authorize headless creation");client45=creatorSaved45;
        Put(game45.data(),7240,static_cast<void*>(nullptr));Put(game45.data(),7536,scene45.data());Check(!creatorMatches45(),"45 newer global scene field cannot substitute for7240");
        Put(client45.data(),1648,std::uint8_t{1});Check(creatorMatches45(),"45 client1648 selects live per-client1480 without using global scene");
        client45=creatorSaved45;Put(factory45.data(),80,std::uint8_t{1});Check(creatorMatches45(),"45 factory80 independently selects live per-client scene");
        factory45=factorySaved45;Put(factory45.data(),64,std::uint8_t{1});Put(client45.data(),1656,std::uint8_t{1});Check(!creatorMatches45(),"45 newer branch flags cannot select a scene");
        client45=creatorSaved45;factory45=factorySaved45;game45=gameSaved45;
        live45=0;Check(!creatorMatches45(),"45 retired factory and scene cannot authorize suppression");live45=1;
        Put(player45.data(),1440,reinterpret_cast<void*>(1));Check(!creatorMatches45(),"45 existing native model preserves manual ownership");player45=playerSaved45;
        creatorRequest45.blockType=23;Put(functor45.data(),8,std::uint8_t{23});Check(creatorMatches45(),"45 own Ender uses same verified ten-argument scene creator");
        creatorRequest45.blockType=2;Put(functor45.data(),8,std::uint8_t{2});
        Check(creatorMatches45() && client45==creatorSaved45 && game45==gameSaved45 && factory45==factorySaved45 && scene45==sceneSaved45 && player45==playerSaved45 && !out45.object && !out45.control,
            "45 creator admission never constructs output or mutates borrowed native ownership");
        std::array<std::byte,208> mode45{};std::array<std::byte,136> hit45{};std::array<std::byte,152> hand45{};
        std::array<std::byte,1040> itemTable45{};std::array<std::byte,8> item45{},weak45{};RemoteUse use45{};
        Put(player45.data(),2720,mode45.data());Put(mode45.data(),8,player45.data());
        Put(hit45.data(),28,std::uint8_t{3});Put(hit45.data(),32,int{10});Put(hit45.data(),36,int{64});Put(hit45.data(),40,int{-4});
        Put(hand45.data(),0,base+0xE698240);hand45[35]=std::byte{1};hand45[36]=std::byte{1};
        for(int kind45:{0,1,2}){
            Put(mode45.data(),0,base+(kind45==0?0xE81A130:0xE81A090));
            Check(DecodeRemoteUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),base,creatorTarget45,kind45,use45),"45 exact survival creative adventure mode and canonical empty hand admit native use");
            Check(NativeRva(GameVersion::V45,use45.entry)==(kind45==0?0x2D337B0u:0x2D2EF90u),"45 decoded mode selects a callable independently verified native entry");
        }
        Put(mode45.data(),0,base+0xE81A130);Put(item45.data(),0,itemTable45.data());Put(weak45.data(),0,item45.data());Put(hand45.data(),8,weak45.data());hand45[34]=std::byte{64};
        Put(itemTable45.data(),144,base+0x76FB0);Put(itemTable45.data(),1000,base+0x20DAE10);
        auto held45Saved=hand45;auto table45Saved=itemTable45;
        Check(DecodeRemoteUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),base,creatorTarget45,0,use45,nullptr,true) && use45.item==reinterpret_cast<std::uintptr_t>(item45.data()),"45 held block requires actual native v1000 dispatcher20DAE10 not false masked nominee903CB0");
        Put(itemTable45.data(),1000,base+0x903CB0);Check(!DecodeRemoteUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),base,creatorTarget45,0,use45,nullptr,true),"45 similar but non-Item dispatcher903CB0 is refused");itemTable45=table45Saved;
        Put(itemTable45.data(),144,base+0x84190);Check(!DecodeRemoteUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),base,creatorTarget45,0,use45,nullptr,true),"45 fertilizer predicate remains conservatively refused");itemTable45=table45Saved;
        Check(!DecodeRemoteUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),base,creatorTarget45,0,use45) && hand45==held45Saved && itemTable45==table45Saved,"45 held mode never changes the hand and still requires scoped protection");
        {
            EntityFixture actor45;actor45.Definition("hivesky","death_crate");
            Put(actor45.actor.data(),0,base+0xE69D4E0);Put(actor45.actor.data(),472,std::uintptr_t{100});Put(actor45.actor.data(),528,std::uint8_t{2});
            auto et45=creatorTarget45;et45.kind=TargetKind::Entity;et45.entity=200;et45.entityRuntime=300;et45.unique=800;
            Put(hit45.data(),24,int{1});Put(hit45.data(),44,1.f);Put(hit45.data(),48,2.f);Put(hit45.data(),52,3.f);
            Check(DecodeRemoteEntityUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),actor45.actor.data(),base,et45,0,nullptr,true),"45 held deathcrate admits only independently verified generic Actor planner implementation");
            Put(actor45.actor.data(),0,base+0xE6AF850);
            Check(!DecodeRemoteEntityUse(mode45.data(),player45.data(),hit45.data(),hand45.data(),actor45.actor.data(),base,et45,0,nullptr,true),"45 cannot borrow newer generic Actor table");
            Put(actor45.actor.data(),0,base+0xEA442D0);Put(actor45.actor.data(),528,std::uint8_t{18});
            Check(MatchesHiveProxyOwner(actor45.actor.data(),base,et45,200),"45 returned Sheep implementation is recognized by exact native constructor and identifier registration");
            Check(!MatchesHiveProxyOwner(actor45.actor.data(),base,et45,201),"45 returned proxy still requires resolved unique identity");
            Put(actor45.actor.data(),528,std::uint8_t{19});Put(actor45.actor.data(),16,std::uintptr_t{1});
            Check(!MatchesHiveProxyOwner(actor45.actor.data(),base,et45,200),"45 returned proxy excludes players before reading identity registry");
            Check(hand45==held45Saved,"45 held entity admission and proxy observation never mutate hand");
            Put(hit45.data(),24,int{0});
        }
        std::array<std::byte,448> owner45{};auto model45=owner45.data()+16;int publisher45{};
        Target target45{};target45.world=100;target45.unique=200;target45.runtime=300;target45.kind=TargetKind::Block;target45.x=10;target45.y=64;target45.z=-4;
        Put(player45.data(),472,std::uintptr_t{100});Put(player45.data(),1440,model45);Put(player45.data(),1448,owner45.data());Put(player45.data(),1456,&publisher45);
        Put(owner45.data(),0,base+0xE749DE0);Put(owner45.data(),8,int{1});Put(owner45.data(),12,int{2});Put(model45,0,base+0xEB1D820);Put(model45,48,player45.data());
        Put(model45,80,std::uint8_t{2});Put(model45,392,int{10});Put(model45,396,int{64});Put(model45,400,int{-4});Put(model45,408,UINT64_MAX);Put(model45,416,std::uint8_t{2});Put(model45,424,std::uint8_t{2});
        auto decodedModel45=DecodeRemoteModel(player45.data(),base,target45);Check(decodedModel45 && decodedModel45->model==reinterpret_cast<std::uintptr_t>(model45) && decodedModel45->id==2,"45 factory448 allocation exposes the exact standard model and control");
        Put(model45,424,std::uint8_t{23});Check(!DecodeRemoteModel(player45.data(),base,target45),"45 mismatched native model source is refused");Put(model45,424,std::uint8_t{2});
        RemoteRequest consistency45{{target45,7,1000},7,reinterpret_cast<std::uintptr_t>(client45.data()),reinterpret_cast<std::uintptr_t>(player45.data()),27,2};
        RemoteSession owned45;owned45.Step(consistency45,1000,true);owned45.Created(reinterpret_cast<std::uintptr_t>(model45),reinterpret_cast<std::uintptr_t>(owner45.data()));
        owned45.Accepted(2,reinterpret_cast<std::uintptr_t>(model45),reinterpret_cast<std::uintptr_t>(owner45.data()));
        Check(MatchesRemoteConsistency(owned45,scene45.data(),base,base+0x2F2D130,7),"45 accepted headless model exempts only exact caller and per-client1480 scene");
        Put(client45.data(),1480,static_cast<void*>(nullptr));Put(client45.data(),1488,scene45.data());
        Check(!MatchesRemoteConsistency(owned45,scene45.data(),base,base+0x2F2D130,7),"45 newer1488 scene owner cannot substitute for missing1480");
        Put(client45.data(),1480,scene45.data());Put(client45.data(),1488,static_cast<void*>(nullptr));
        Check(!MatchesRemoteConsistency(owned45,scene45.data(),base,base+0x2F2D131,7) && !MatchesRemoteConsistency(owned45,scene45.data(),base,base+0x2F2D130,8),"45 foreign caller and callback owner never exempt consistency");
    }
    {
        // Native2FB8530's complete104-byte action, with an external canary.
        std::array<std::byte,112> action45{};action45[104]=std::byte{0xA5};
        action45[88]=std::byte{0xC3};action45[103]=std::byte{0xD4};
        int actor45{},player45{};
        Put(action45.data(),24,UINT64_C(15));
        Put(action45.data(),32,base+0xE7B2C50);
        Put(action45.data(),40,&player45);Put(action45.data(),48,&actor45);
        auto actionBefore=action45;
        Check(MatchesEntityOpenAction(action45.data(),&actor45,&player45,base),"45 open callback requires native action32 player40 actor48 flag96 zero");
        Check(action45==actionBefore,"45 callback validation is read-only");
        action45[96]=std::byte{1};
        Check(!MatchesEntityOpenAction(action45.data(),&actor45,&player45,base),"45 flagged alternate action is not a container-open callback");
        action45[96]=std::byte{0};Put(action45.data(),40,&actor45);
        Check(!MatchesEntityOpenAction(action45.data(),&actor45,&player45,base),"45 callback cannot swap actor and player ownership");
        action45=actionBefore;
        actionAddress=action45.data();actionOffset=32;actionDestroyed=actionFreed=0;actionBad=actionFault=false;
        Check(RetireEntityAction(action45.data(),ActionDestroy,ActionFree) && actionDestroyed==1 && actionFreed==1 && !actionBad,
              "45 retirement destroys only its native callback then frees its title");
        Check(std::all_of(action45.begin(),action45.begin()+104,[](auto v){return v==std::byte{};}) && action45[104]==std::byte{0xA5},
              "45 retirement clears complete action without overwriting adjacent native storage");
        action45=actionBefore;actionDestroyed=actionFreed=0;actionFault=true;
        Check(!RetireEntityAction(action45.data(),ActionDestroy,ActionFree) && !actionFreed && action45[32]==std::byte{} && action45[96]==std::byte{} && action45[104]==std::byte{0xA5},
              "45 callback fault prevents title cleanup and still retires only bounded action storage");
        actionFault=false;actionOffset=8;
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
