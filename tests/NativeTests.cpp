#include "ContainerPreview/NativeState.h"
#include <cstring>
#include <iostream>
#include <vector>
#include "EntityFixture.h"
using namespace preview;
namespace {
int failures{},checks{};
void Check(bool b,char const* name) {++checks;if(!b){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
struct StackFixture {
    std::uint64_t* owned;
    std::array<std::byte,144> other{};
};
static_assert(sizeof(StackFixture)==152);
bool Copy(void* dst,void const* src) {
    auto& value=*static_cast<StackFixture const*>(src);
    auto& result=*static_cast<StackFixture*>(dst);
    result=value;result.owned=new std::uint64_t(*value.owned);return true;
}
bool Destroy(void* dst) {delete static_cast<StackFixture*>(dst)->owned;return true;}
bool RefuseCopy(void*,void const*) {return false;}
std::uint64_t cloneClock{};
bool CopyWithFreshClock(void* dst,void const* src) {
    if(!Copy(dst,src))return false;
    // Model the external native constructor: copy flag+36, refresh clock+40.
    auto stamp=++cloneClock;
    std::memcpy(static_cast<std::byte*>(dst)+40,&stamp,sizeof stamp);
    return true;
}
struct RegistryFixture {
    std::array<std::byte,160> registry{},pool{},actor{};
    std::array<std::uint64_t,1> buckets{0};
    std::array<std::byte,32> nodes{};
    std::array<std::uint32_t,2048> sparse{};
    std::array<std::uintptr_t,1> sparsePages{},dataPages{};
    std::vector<std::byte> data;
    template<class T>static void Put(void* p,std::size_t o,T value){std::memcpy(static_cast<std::byte*>(p)+o,&value,sizeof value);}
    explicit RegistryFixture(std::uint32_t hash,std::size_t stride):data(128*stride) {
        sparse.fill(UINT32_MAX);sparse[17]=5;
        sparsePages[0]=reinterpret_cast<std::uintptr_t>(sparse.data());
        dataPages[0]=reinterpret_cast<std::uintptr_t>(data.data());
        Put(actor.data(),16,reinterpret_cast<std::uintptr_t>(registry.data()));Put(actor.data(),24,std::uint32_t{17});
        Put(registry.data(),72,reinterpret_cast<std::uintptr_t>(buckets.data()));Put(registry.data(),80,reinterpret_cast<std::uintptr_t>(buckets.data()+1));
        Put(registry.data(),104,reinterpret_cast<std::uintptr_t>(nodes.data()));Put(registry.data(),112,reinterpret_cast<std::uintptr_t>(nodes.data()+32));
        Put(nodes.data(),0,UINT64_MAX);Put(nodes.data(),8,hash);Put(nodes.data(),16,reinterpret_cast<std::uintptr_t>(pool.data()));
        Put(pool.data(),8,reinterpret_cast<std::uintptr_t>(sparsePages.data()));Put(pool.data(),16,reinterpret_cast<std::uintptr_t>(sparsePages.data()+1));
        Put(pool.data(),80,reinterpret_cast<std::uintptr_t>(dataPages.data()));
    }
};
}
int main(){
    // Changing the player bit to Mob bit2 or matching a partial identifier
    // must break exact death-crate admission, without touching native strings.
    EntityFixture crate;crate.Definition("hivesky","death_crate");
    EntityFixture::Put(crate.actor.data(),528,std::uint8_t{2});
    auto saved=crate.definitions;
    Check(IsHiveDeathCrate(crate.actor.data()),"exact Mob death-crate definition is admitted");
    Check(crate.definitions==saved,"production death-crate classification is read-only");
    EntityFixture::Put(crate.actor.data(),528,std::uint8_t{3});
    EntityFixture::Put(crate.actor.data(),16,std::uintptr_t{1});
    Check(!IsHiveDeathCrate(crate.actor.data()),"player flag is rejected before traversing an unreadable component registry");
    EntityFixture::Put(crate.actor.data(),16,reinterpret_cast<std::uintptr_t>(crate.registry.data()));
    EntityFixture::Put(crate.actor.data(),528,std::uint8_t{2});
    crate.Definition("minecraft","death_crate");Check(!IsHiveDeathCrate(crate.actor.data()),"other namespace cannot authorize entity automation");
    crate.Definition("hivesky","death_crate_extra");Check(!IsHiveDeathCrate(crate.actor.data()),"identifier prefix is not an exact death crate");
    Check(!IsHiveDeathCrate(nullptr),"null actor fails closed");
    ActorIdentity local{100,200,300,true,false},server{101,200,300,false,true};
    Check(SameIntegratedPlayer(local,server),"matching distinct client/server identities admit integrated snapshot");
    auto bad=server;bad.registry=100;Check(!SameIntegratedPlayer(local,bad),"same registry is not integrated pairing");
    bad=server;bad.runtime=301;Check(!SameIntegratedPlayer(local,bad),"unique ID alone cannot pair stale actor");
    bad=server;bad.unique=201;Check(!SameIntegratedPlayer(local,bad),"runtime ID alone cannot pair another player");
    bad=server;bad.server=false;Check(!SameIntegratedPlayer(local,bad),"unmarked actor is not authoritative player");
    auto incomplete=local;incomplete.unique=0;Check(!SameIntegratedPlayer(incomplete,server),"incomplete local identity fails closed");
    RegistryFixture f(0x1779E2C7,512);
    Check(ReadComponent(f.actor.data(),0x1779E2C7,512)==reinterpret_cast<std::uintptr_t>(f.data.data()+5*512),"native sparse component lookup returns exact packed element");
    Check(!ReadComponent(f.actor.data(),0x18F957AF,8),"absent component does not create pool");
    f.sparse[17]=UINT32_MAX;Check(!ReadComponent(f.actor.data(),0x1779E2C7,512),"removed component generation is rejected");
    f.sparse[17]=5;f.buckets[0]=2;Check(!ReadComponent(f.actor.data(),0x1779E2C7,512),"out-of-bounds pool node is rejected");
    Check(!ReadComponent(nullptr,0,8),"null actor fails closed");
    RegistryFixture role(0xACB47B38,1),unique(0x18F957AF,8),runtime(0xFC0DBBB5,8);
    std::array<std::byte,96> idNodes{};
    RegistryFixture::Put(role.registry.data(),104,reinterpret_cast<std::uintptr_t>(idNodes.data()));
    RegistryFixture::Put(role.registry.data(),112,reinterpret_cast<std::uintptr_t>(idNodes.data()+96));
    std::array<std::uint32_t,3> hashes{0xACB47B38,0x18F957AF,0xFC0DBBB5};
    std::array<void*,3> pools{role.pool.data(),unique.pool.data(),runtime.pool.data()};
    for(unsigned i=0;i<3;++i) {
        RegistryFixture::Put(idNodes.data(),i*32,i==2?UINT64_MAX:std::uint64_t(i+1));
        RegistryFixture::Put(idNodes.data(),i*32+8,hashes[i]);RegistryFixture::Put(idNodes.data(),i*32+16,reinterpret_cast<std::uintptr_t>(pools[i]));
    }
    RegistryFixture::Put(unique.data.data(),40,std::uint64_t{200});RegistryFixture::Put(runtime.data.data(),40,std::uint64_t{300});
    auto identity=ReadIdentity(role.actor.data());
    Check(identity.local && !identity.server && identity.unique==200 && identity.runtime==300,"identity reads independent role and both native IDs without lazy creation");
    RegistryFixture::Put(unique.data.data(),40,UINT64_MAX);
    Check(!ReadIdentity(role.actor.data()).unique,"unassigned ID sentinel is never lazily allocated");
    std::uint64_t payload=123;
    StackFixture source{&payload};
    std::array<void const*,1> items{&source};
    OwnedStacks stacks({Copy,Destroy});
    Check(stacks.Replace(items,1),"owner-thread snapshot uses native copy boundary");
    auto copy=static_cast<StackFixture const*>(stacks.At(0));
    Check(copy && copy!=&source && copy->owned!=&payload && *copy->owned==123,"snapshot owns distinct native value");
    payload=456;Check(copy && *copy->owned==123,"subsequent world edits do not change owned snapshot");
    Check(!stacks.Replace(items,2) && stacks.Size()==1,"wrong thread cannot replace or destroy native snapshot");
    Check(!stacks.Clear(2) && stacks.Size()==1,"render thread cannot destroy native stacks");
    Check(!stacks.At(1),"slot outside constructed native range is rejected");
    std::array<void const*,55> oversized{};oversized.fill(&source);
    Check(!stacks.Replace(oversized,1) && stacks.Size()==1,"oversized replacement retains owned snapshot");
    Check(stacks.Clear(1) && stacks.Size()==0,"owner thread retires constructed native copies");
    // A missed normalization would replay native pickup scaling every snapshot.
    // Check the real OwnedStacks output, not a rendering or normalization mock.
    for(unsigned i=0;i<source.other.size();++i)source.other[i]=std::byte((i*7+3)&255);
    source.other[36-8]=std::byte{1};
    auto originalBytes=source.other;
    Check(stacks.Replace(items,1),"animated source can publish a preview copy");
    copy=static_cast<StackFixture const*>(stacks.At(0));
    Check(copy && copy->other[36-8]==std::byte{0},"preview clone disables native pickup animation");
    Check(source.other==originalBytes,"preview animation normalization never modifies source bytes");
    auto expectedBytes=originalBytes;expectedBytes[36-8]=std::byte{0};
    Check(copy && copy->other==expectedBytes && *copy->owned==payload,
          "normalization preserves all other copied bytes and independently owned payload");
    Check(stacks.Clear(1),"normalized preview copy retains valid native destruction");
    OwnedStacks refreshed({CopyWithFreshClock,Destroy});
    bool repeatedStable=true,clockPreserved=true;
    for(unsigned i=0;i<20;++i) {
        if(!refreshed.Replace(items,1)){repeatedStable=false;break;}
        auto bytes=static_cast<std::byte const*>(refreshed.At(0));
        if(!bytes){repeatedStable=false;break;}
        repeatedStable &= bytes[36]==std::byte{0};
        std::uint64_t stamp{};std::memcpy(&stamp,bytes+40,sizeof stamp);
        clockPreserved &= stamp==i+1;
    }
    Check(repeatedStable,"repeated native clock resets cannot re-enable preview pickup animation");
    Check(clockPreserved,"normalization does not overwrite the native clone timestamp");
    Check(source.other==originalBytes,"repeated snapshots leave source animation and item data unchanged");
    Check(refreshed.Clear(1),"repeated normalized snapshots are owner-thread destructible");
    OwnedStacks failed({RefuseCopy,Destroy});
    Check(!failed.Replace(items,1) && failed.Faulted() && failed.Size()==0,"failed native construction quarantines buffer");
    Check(!failed.Replace(items,1),"quarantined native buffer never retries partial construction");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
