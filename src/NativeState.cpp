#include "ContainerPreview/NativeState.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstring>
namespace preview {
namespace {
template<class T> T Read(std::uintptr_t p,std::size_t offset) {
    T value;std::memcpy(&value,reinterpret_cast<void const*>(p+offset),sizeof value);return value;
}
std::uintptr_t Pool(std::uintptr_t registry,std::uint32_t hash) {
    auto first=Read<std::uintptr_t>(registry,72),end=Read<std::uintptr_t>(registry,80);
    auto nodes=Read<std::uintptr_t>(registry,104),last=Read<std::uintptr_t>(registry,112);
    if(!first || !nodes || end<=first || last<nodes || (end-first)%8 || (last-nodes)%32)return 0;
    auto buckets=(end-first)/8,count=(last-nodes)/32;
    if(buckets>(1u<<20) || (buckets&(buckets-1)) || count>(1u<<20))return 0;
    auto index=Read<std::uint64_t>(first,8*(hash&(buckets-1)));
    for(unsigned chain=0;index!=UINT64_MAX && chain<128;++chain) {
        if(index>=count)return 0;
        auto node=nodes+index*32;
        if(Read<std::uint32_t>(node,8)==hash)return Read<std::uintptr_t>(node,16);
        index=Read<std::uint64_t>(node,0);
    }
    return 0;
}
bool Member(std::uintptr_t pool,std::uint32_t entity,std::uint32_t& packed) {
    if(!pool)return false;
    auto first=Read<std::uintptr_t>(pool,8),last=Read<std::uintptr_t>(pool,16);
    auto pageIndex=(entity&0x3FFFFu)>>11;
    if(!first || last<first || (last-first)%8 || last-first>128*8 || pageIndex>=(last-first)/8)return false;
    auto page=Read<std::uintptr_t>(first,pageIndex*8);
    if(!page)return false;
    packed=Read<std::uint32_t>(page,(entity&0x7FFu)*4);
    return (packed^(entity&0xFFFC0000u))<=0x3FFFEu;
}
}
bool SameIntegratedPlayer(ActorIdentity const& local,ActorIdentity const& server) {
    return local.local && !local.server && server.server && !server.local &&
        local.registry && server.registry && local.registry!=server.registry &&
        local.unique && local.runtime && local.unique!=UINT64_MAX && local.runtime!=UINT64_MAX &&
        local.unique==server.unique && local.runtime==server.runtime;
}
bool IsHiveDeathCrate(void const* actor) noexcept {
    __try {
        if(!actor)return false;
        auto a=reinterpret_cast<std::uintptr_t>(actor);
        if(Read<std::uint8_t>(a,528)&1)return false; // Player before registry.
        auto c=ReadComponent(actor,0xDEB6534F,176);if(!c)return false;
        // Exact IDA namespace/name MSVC strings; compare bounded ASCII only.
        auto equal=[](std::uintptr_t p,char const* expected,std::size_t n){
            auto size=Read<std::uint64_t>(p,16),capacity=Read<std::uint64_t>(p,24);
            if(size!=n || capacity<size || capacity>4096 || (capacity<16 && capacity!=15))return false;
            auto text=capacity<16?reinterpret_cast<char const*>(p):Read<char const*>(p,0);
            return text && !std::memcmp(text,expected,n+1);
        };
        return equal(c,"hivesky",7) && equal(c+32,"death_crate",11);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::uintptr_t ReadComponent(void const* actor,std::uint32_t hash,std::size_t stride) noexcept {
    __try {
        if(!actor || !stride || stride>4096)return 0;
        auto a=reinterpret_cast<std::uintptr_t>(actor);
        auto registry=Read<std::uintptr_t>(a,16);if(!registry)return 0;
        auto entity=Read<std::uint32_t>(a,24);auto pool=Pool(registry,hash);std::uint32_t packed{};
        if(!Member(pool,entity,packed))return 0;
        auto pages=Read<std::uintptr_t>(pool,80);if(!pages)return 0;
        auto page=Read<std::uintptr_t>(pages,((packed&0x3FFFFu)>>7)*8);if(!page)return 0;
        return page+(packed&0x7Fu)*stride;
    }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
ActorIdentity ReadIdentity(void const* actor) noexcept {
    __try {
        if(!actor)return {};
        auto a=reinterpret_cast<std::uintptr_t>(actor),registry=Read<std::uintptr_t>(a,16);
        auto entity=Read<std::uint32_t>(a,24);std::uint32_t packed{};
        if(!registry)return {};
        auto local=Member(Pool(registry,0xACB47B38),entity,packed);
        auto server=Member(Pool(registry,0x122C9226),entity,packed);
        auto unique=ReadComponent(actor,0x18F957AF,8),runtime=ReadComponent(actor,0xFC0DBBB5,8);
        if(!unique || !runtime)return {};
        auto uid=Read<std::uint64_t>(unique,0),rid=Read<std::uint64_t>(runtime,0);
        if(!uid || !rid || uid==UINT64_MAX || rid==UINT64_MAX)return {};
        return {registry,uid,rid,local,server};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
bool OwnedStacks::Replace(std::span<void const* const> items,std::uint32_t thread) {
    if(faulted_ || !thread || !ops_.copy || !ops_.destroy || items.size()>54 || (owner_ && owner_!=thread))return false;
    for(auto item:items)if(!item)return false;
    if(!Clear(thread))return false;
    owner_=thread;
    for(auto item:items) {
        if(!ops_.copy(stacks_[size_].bytes.data(),item)){faulted_=true;return false;}
        // Exact 1.26.52.03: copy 2482180/2481C90 refreshes clock+40 but
        // retains flag+36. Renderer 55AD170 uses 2491B80's ~200ms pickup
        // interpolation; refreshing snapshots must not restart that animation.
        // Normalize only our constructed display clone, never the source stack.
        stacks_[size_].bytes[36]=std::byte{0};
        ++size_;
    }
    return true;
}
bool OwnedStacks::Clear(std::uint32_t thread) {
    if(faulted_ || !thread || !ops_.destroy || (owner_ && owner_!=thread))return false;
    while(size_) {
        if(!ops_.destroy(stacks_[size_-1].bytes.data())){faulted_=true;return false;}
        --size_;
    }
    owner_=0;
    return true;
}
void const* OwnedStacks::At(unsigned slot) const {return !faulted_ && slot<size_?stacks_[slot].bytes.data():nullptr;}
}
