#include "ContainerPreview/NativeContainers.h"
#include "ContainerPreview/NativeAddress.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstring>
#include <span>
namespace preview {
namespace {
template<class T>T Read(std::uintptr_t p,std::size_t offset){T value;std::memcpy(&value,reinterpret_cast<void const*>(p+offset),sizeof value);return value;}
struct Profile {std::uintptr_t root,face,container;unsigned offset,first,count;bool vector,loot,chest;};
constexpr Profile profiles[]={
    {0xE85A040,0xE85A160,0xE85A240,280,688,27,true,true,true},
    {0xE857C50,0xE857D70,0xE857F00,280,688,27,true,true,true},
    {0xE8F6A40,0xE8F6B60,0xE8F6C70,280,688,27,true,true,true},
    {0xE858940,0xE85AB40,0xE858B30,240,664,3,false,false,false},
    {0xE858940,0xE858A20,0xE858B30,240,664,3,false,false,false},
    {0xE858940,0xE8F6FC0,0xE858B30,240,664,3,false,false,false},
    {0xE858D70,0xE858E50,0xE858F60,240,672,5,false,false,false},
    {0xE8F41A0,0xE8F4280,0xE8F4390,240,792,5,false,false,false},
    {0xE8F3250,0xE8F3330,0xE8F3410,280,688,9,false,true,false},
    {0xE8F3570,0xE8F3650,0xE8F3410,280,688,9,false,true,false},
    {0xE8F28F0,0xE8F29D0,0xE8F2AE0,280,688,9,false,true,false},
    {0xE8F2160,0xE8F2240,0xE8F2320,240,648,6,true,false,false}
};
// Exact 45 IDA:5AE6240/4A10410,49EE580,5AF8940,49F3310,
//5B1C890,6A495D0,4A17F70,49EA050,6A478B0,6A479F0,
//6A49300 and6A49710. Physical bindings, not newer-ABI deltas.
// Admission still requires the separate exact-image native profile and guards.
constexpr Profile profiles45[]={
    {0xE92E240,0xE8BCF20,0xE92E360,272,680,27,true,true,true},
    {0xE92C440,0xE92C560,0xE92C640,272,680,27,true,true,true},
    {0xE8C0620,0xE8C0740,0xE8C0850,272,680,27,true,true,true},
    {0xE8BF060,0xE8BF140,0xE8BF250,232,656,3,false,false,false},
    {0xE8BF060,0xE92D0A0,0xE8BF250,232,656,3,false,false,false},
    {0xE8BF060,0xE8C0BB0,0xE8BF250,232,656,3,false,false,false},
    {0xE92D180,0xE92D260,0xE92D370,232,664,5,false,false,false},
    {0xE8BD900,0xE8BD9E0,0xE8BDAF0,232,784,5,false,false,false},
    {0xE8BEEA0,0xE8BEF80,0xE8BCAB0,272,680,9,false,true,false},
    {0xE8BC8F0,0xE8BC9D0,0xE8BCAB0,272,680,9,false,true,false},
    {0xE92ECC0,0xE92EDA0,0xE92EEB0,272,680,9,false,true,false},
    {0xE92E530,0xE92E610,0xE92E6F0,232,640,6,true,false,false}
};
struct Bindings {
    std::span<Profile const> profiles;
    unsigned face,loot,lead,partner;
    std::uintptr_t ordinaryChest,ender,enderFace,enderContainer;
    bool physical;
    std::uintptr_t Resolve(std::uintptr_t key) const {return physical?key:Rva(static_cast<std::uint32_t>(key));}
};
Bindings SelectBindings(){
    //45 Ender49EDA50 reuses the chest interface but never its player's data.
    if(CurrentVersion()==GameVersion::V45)return {profiles45,40,248,724,760,0xE92E240,0xE8BCE00,0xE8BCF20,0xE8BD0B0,true};
    if(CurrentVersion()==GameVersion::V51 || CurrentVersion()==GameVersion::V52)
        return {profiles,48,256,732,768,0xE85A040,0xE8F3920,0xE85A160,0xE8F3A40,false};
    return {};
}
bool Matches(std::uintptr_t p,std::uintptr_t base,Profile const& profile,Bindings const& abi){
    auto root=abi.Resolve(profile.root),face=abi.Resolve(profile.face),inventory=abi.Resolve(profile.container);
    return root && face && inventory && Read<std::uintptr_t>(p,0)==base+root && Read<std::uintptr_t>(p,abi.face)==base+face &&
        Read<std::uintptr_t>(p,profile.offset)==base+inventory;
}
std::uintptr_t First(std::uintptr_t p,Profile const& profile,Bindings const& abi){
    if(profile.loot && Read<std::uint64_t>(p,abi.loot))return 0;
    if(!profile.vector)return p+profile.first;
    auto first=Read<std::uintptr_t>(p,profile.first),last=Read<std::uintptr_t>(p,profile.first+8);
    return first && first%8==0 && last>=first && last-first==profile.count*152 ? first : 0;
}
void Append(BorrowedContents& result,std::uintptr_t first,unsigned count){
    for(unsigned i=0;i<count;++i)result.items[result.slots++]=reinterpret_cast<void const*>(first+i*152);
}
BorrowedContents Unknown(){BorrowedContents result{};result.recognized=true;return result;}
}
BorrowedContents InspectBlock(void const* object,std::uintptr_t base,BlockPosition pos) noexcept {
    __try {
        if(!object || !base)return {};
        auto abi=SelectBindings();if(abi.profiles.empty())return {};
        auto p=reinterpret_cast<std::uintptr_t>(object);
        if(Read<int>(p,8)!=pos.x || Read<int>(p,12)!=pos.y || Read<int>(p,16)!=pos.z)return {};
        if(abi.ender && Read<std::uintptr_t>(p,0)==base+abi.Resolve(abi.ender) && Read<std::uintptr_t>(p,abi.face)==base+abi.Resolve(abi.enderFace) &&
           Read<std::uintptr_t>(p,abi.profiles.front().offset)==base+abi.Resolve(abi.enderContainer) && Read<std::uint8_t>(p,20)==23 && !Read<std::uintptr_t>(p,abi.partner)){
            auto result=Unknown();result.backgroundType=23;return result;
        }
        for(auto const& profile:abi.profiles){
            if(!Matches(p,base,profile,abi))continue;
            BorrowedContents result{};result.recognized=true;
            auto partner=profile.chest?Read<std::uintptr_t>(p,abi.partner):0;
            if(partner){
                // No pairing/mutation helper is called. Require the already
                // established, reciprocal, adjacent native relationship.
                if(partner==p || !Matches(partner,base,profile,abi) || Read<std::uintptr_t>(partner,abi.partner)!=p)return Unknown();
                auto dx=static_cast<std::int64_t>(Read<int>(partner,8))-pos.x;
                auto dz=static_cast<std::int64_t>(Read<int>(partner,16))-pos.z;
                if(Read<int>(partner,12)!=pos.y || !((dx==0 && (dz==1 || dz==-1)) || (dz==0 && (dx==1 || dx==-1))))return Unknown();
                bool lead=(Read<std::uint8_t>(p,abi.lead)&1)!=0;
                if(lead==((Read<std::uint8_t>(partner,abi.lead)&1)!=0))return Unknown();
                if(profile.root==abi.ordinaryChest && Read<std::uint8_t>(p,20)==2 && Read<std::uint8_t>(partner,20)==2 &&
                   (Read<std::uint64_t>(p,abi.loot) || Read<std::uint64_t>(partner,abi.loot))){result.backgroundType=2;return result;}
                auto first=First(p,profile,abi),other=First(partner,profile,abi);if(!first || !other)return Unknown();
                Append(result,lead?first:other,27);Append(result,lead?other:first,27);
            }else{
                if(profile.root==abi.ordinaryChest && Read<std::uint8_t>(p,20)==2 && Read<std::uint64_t>(p,abi.loot)){result.backgroundType=2;return result;}
                auto first=First(p,profile,abi);if(!first)return Unknown();Append(result,first,profile.count);
            }
            result.ready=true;return result;
        }
        return {};
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
BorrowedContents InspectEntityComponent(void const* object) noexcept {
    __try {
        if(!object)return {};
        auto p=reinterpret_cast<std::uintptr_t>(object);
        if(Read<std::uint64_t>(p,488))return Unknown();
        auto first=Read<std::uintptr_t>(p,424),last=Read<std::uintptr_t>(p,432);
        if(!first || first%8 || last<first || (last-first)%152 || last-first<152 || last-first>54*152)return Unknown();
        BorrowedContents result{};Append(result,first,static_cast<unsigned>((last-first)/152));result.ready=true;result.recognized=true;return result;
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
}
