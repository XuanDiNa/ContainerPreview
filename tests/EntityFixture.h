#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
// Hand-transcribed sparse identity layout, not a native identity-call mock.
// Only test fixtures own these buffers; no helper is added to production code.
struct EntityFixture {
    std::array<std::byte,544> actor{};
    std::array<std::byte,160> registry{};
    std::array<std::byte,128> nodes{};
    std::array<std::array<std::byte,96>,3> pools{};
    std::array<std::array<std::uint32_t,2048>,3> sparse{};
    std::array<std::uintptr_t,3> sparsePages{},dataPages{};
    std::array<std::array<std::uint64_t,128>,3> data{};
    std::array<std::uint64_t,1> buckets{0};
    std::array<std::byte,96> definitionPool{};
    std::array<std::uint32_t,2048> definitionSparse{};
    std::uintptr_t definitionSparsePage{},definitionDataPage{};
    std::array<std::byte,176*128> definitions{};
    template<class T>static void Put(void* p,std::size_t offset,T value){std::memcpy(static_cast<std::byte*>(p)+offset,&value,sizeof value);}
    EntityFixture(){
        Put(actor.data(),16,reinterpret_cast<std::uintptr_t>(registry.data()));Put(actor.data(),24,std::uint32_t{17});
        Put(registry.data(),72,reinterpret_cast<std::uintptr_t>(buckets.data()));Put(registry.data(),80,reinterpret_cast<std::uintptr_t>(buckets.data()+1));
        Put(registry.data(),104,reinterpret_cast<std::uintptr_t>(nodes.data()));Put(registry.data(),112,reinterpret_cast<std::uintptr_t>(nodes.data()+96));
        constexpr std::array<std::uint32_t,3> hashes{0xACB47B38,0x18F957AF,0xFC0DBBB5};
        for(unsigned i=0;i<3;++i){
            sparse[i].fill(UINT32_MAX);sparse[i][17]=5;
            sparsePages[i]=reinterpret_cast<std::uintptr_t>(sparse[i].data());dataPages[i]=reinterpret_cast<std::uintptr_t>(data[i].data());
            Put(nodes.data(),i*32,i==2?UINT64_MAX:std::uint64_t(i+1));Put(nodes.data(),i*32+8,hashes[i]);Put(nodes.data(),i*32+16,reinterpret_cast<std::uintptr_t>(pools[i].data()));
            Put(pools[i].data(),8,reinterpret_cast<std::uintptr_t>(&sparsePages[i]));Put(pools[i].data(),16,reinterpret_cast<std::uintptr_t>(&sparsePages[i]+1));
            Put(pools[i].data(),80,reinterpret_cast<std::uintptr_t>(&dataPages[i]));
        }
        data[1][5]=200;data[2][5]=300;
    }
    EntityFixture(EntityFixture const&)=delete;
    EntityFixture& operator=(EntityFixture const&)=delete;
    void Definition(char const* nameSpace,char const* name){
        Put(registry.data(),112,reinterpret_cast<std::uintptr_t>(nodes.data()+128));
        Put(nodes.data(),64,std::uint64_t{3});Put(nodes.data(),96,UINT64_MAX);
        Put(nodes.data(),104,std::uint32_t{0xDEB6534F});Put(nodes.data(),112,reinterpret_cast<std::uintptr_t>(definitionPool.data()));
        definitionSparse.fill(UINT32_MAX);definitionSparse[17]=5;
        definitionSparsePage=reinterpret_cast<std::uintptr_t>(definitionSparse.data());
        definitionDataPage=reinterpret_cast<std::uintptr_t>(definitions.data());
        Put(definitionPool.data(),8,reinterpret_cast<std::uintptr_t>(&definitionSparsePage));
        Put(definitionPool.data(),16,reinterpret_cast<std::uintptr_t>(&definitionSparsePage+1));
        Put(definitionPool.data(),80,reinterpret_cast<std::uintptr_t>(&definitionDataPage));
        auto component=definitions.data()+5*176;
        auto string=[&](unsigned offset,char const* text){
            auto n=std::strlen(text);std::memset(component+offset,0,32);
            if(n<16)std::memcpy(component+offset,text,n+1);
            else Put(component,offset,reinterpret_cast<std::uintptr_t>(text));
            Put(component,offset+16,n);Put(component,offset+24,n<16?std::size_t{15}:n);
        };
        string(0,nameSpace);string(32,name);
    }
};
