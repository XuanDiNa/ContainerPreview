#include "ContainerPreview/NativeWorld.h"
#include <cstring>
#include <iostream>
using namespace preview;
namespace {int checks{},failures{};void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}template<class T>void Put(void* p,unsigned offset,T v){std::memcpy(static_cast<std::byte*>(p)+offset,&v,sizeof v);}}
int main(){
    std::array<std::byte,1496> client{};std::array<std::byte,24> sceneStack{};std::uint8_t valid=1;
    Put(client.data(),1488,sceneStack.data());Put(sceneStack.data(),8,&valid);
    Check(ReadClientSceneStack(client.data())==sceneStack.data(),"client-owned scene stack is borrowed synchronously when its nonowner validity is live");
    Check(!ReadClientSceneStack(nullptr),"null client cannot expose a scene stack");
    valid=0;Check(!ReadClientSceneStack(client.data()),"deleted scene stack is not returned");
    valid=1;Put(sceneStack.data(),8,static_cast<void*>(nullptr));
    Check(!ReadClientSceneStack(client.data()),"missing nonowner validity is refused");
    Put(sceneStack.data(),8,&valid);Put(client.data(),1488,static_cast<void*>(nullptr));
    Check(!ReadClientSceneStack(client.data()),"missing per-client stack fails closed without global-stack guessing");
    Put(client.data(),1488,sceneStack.data());Put(sceneStack.data(),8,reinterpret_cast<void*>(1));
    Check(!ReadClientSceneStack(client.data()),"unreadable validity memory is refused without a native call");
    native_detail::active.store(GameVersion::V45);
    Put(sceneStack.data(),8,&valid);Put(client.data(),1480,sceneStack.data());
    Put(client.data(),1488,reinterpret_cast<void*>(1));
    Check(ReadClientSceneStack(client.data())==sceneStack.data(),"45 scene owner1480 is used without reading newer owner1488");
    auto originalClient=client;auto originalStack=sceneStack;
    Check(ReadClientSceneStack(client.data())==sceneStack.data() && client==originalClient && sceneStack==originalStack,
          "45 scene observation never acquires or changes native ownership");
    valid=0;Check(!ReadClientSceneStack(client.data()),"45 retired scene owner remains refused");valid=1;
    Put(client.data(),1480,static_cast<void*>(nullptr));Put(client.data(),1488,sceneStack.data());
    Check(!ReadClientSceneStack(client.data()),"45 missing scene does not fall back to incompatible newer owner");
    native_detail::active.store(GameVersion::Unknown);
    Check(!ReadClientSceneStack(client.data()),"unknown ABI cannot interpret client scene fields");
    native_detail::active.store(GameVersion::V52);
    std::array<std::byte,136> hit{};Target context{100,200,300,1};Put(hit.data(),24,int{0});Put(hit.data(),32,int{-1});Put(hit.data(),36,int{64});Put(hit.data(),40,int{15});
    auto t=DecodeHit(context,hit.data(),{},false);
    Check(t.kind==TargetKind::Block && t.world==100 && t.dimension==1 && t.x==-1 && t.y==64 && t.z==15,"block hit preserves signed coordinates and world context");
    Put(hit.data(),24,int{1});ActorIdentity entity{700,400,500,false,false};t=DecodeHit(context,hit.data(),entity,false);
    Check(t.kind==TargetKind::Entity && t.entity==400 && t.entityRuntime==500,"entity hit requires both independent IDs");
    Check(DecodeHit(context,hit.data(),entity,true).kind==TargetKind::None,"player entity is never eligible");
    entity.runtime=0;Check(DecodeHit(context,hit.data(),entity,false).kind==TargetKind::None,"incomplete entity identity is not carried across threads");
    Put(hit.data(),24,int{2});Check(DecodeHit(context,hit.data(),{},false).kind==TargetKind::None,"out of range target is excluded");
    Put(hit.data(),24,int{3});Check(DecodeHit(context,hit.data(),{},false).kind==TargetKind::None,"miss target is excluded");
    Check(DecodeHit(context,nullptr,{},false).kind==TargetKind::None,"missing hit result fails closed");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
