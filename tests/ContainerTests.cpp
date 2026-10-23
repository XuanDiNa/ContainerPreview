#include <string_view>
#include "ContainerPreview/NativeAddress.h"
#include "ContainerPreview/NativeContainers.h"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>
using namespace preview;
namespace {
std::uintptr_t FixtureRva(std::uintptr_t key){auto value=key<=UINT32_MAX?Rva(static_cast<std::uint32_t>(key)):0;return value?value:key;}
int checks{},failures{};
void Check(bool result,char const* name){++checks;if(!result){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
template<class T>void Put(void* p,std::size_t offset,T value){std::memcpy(static_cast<std::byte*>(p)+offset,&value,sizeof value);}
constexpr std::uintptr_t base=0x140000000;
struct Block {
    std::vector<std::byte> bytes=std::vector<std::byte>(5000),items=std::vector<std::byte>(27*152);
    Block(std::uintptr_t root=0xE85A040,std::uintptr_t face=0xE85A160,std::uintptr_t container=0xE85A240,unsigned offset=280){
        Put(bytes.data(),0,base+FixtureRva(root));Put(bytes.data(),48,base+FixtureRva(face));Put(bytes.data(),offset,base+FixtureRva(container));
        Put(bytes.data(),8,int{1});Put(bytes.data(),12,int{64});Put(bytes.data(),16,int{2});Put(bytes.data(),20,std::uint8_t{2});
        Put(bytes.data(),688,reinterpret_cast<std::uintptr_t>(items.data()));Put(bytes.data(),696,reinterpret_cast<std::uintptr_t>(items.data()+items.size()));
        Put(bytes.data(),732,std::uint8_t{1});
    }
    BorrowedContents Get(){return InspectBlock(bytes.data(),base,{1,64,2});}
};
}
int main(int argc,char** argv){
    if(argc>1 && std::string_view(argv[1])=="51")native_detail::active.store(GameVersion::V51);
    Block chest;
    auto result=chest.Get();
    Check(result.ready && result.slots==27 && result.items[0]==chest.items.data() && result.items[26]==chest.items.data()+3952,"known chest exposes 27 bounded borrowed slots");
    Check(!InspectBlock(chest.bytes.data(),base,{1,65,2}).ready,"replaced block position is rejected");
    Put(chest.bytes.data(),256,std::uint64_t{9});Check(!chest.Get().ready && chest.Get().recognized,"ungenerated chest loot is unknown not empty");Put(chest.bytes.data(),256,std::uint64_t{});
    Put(chest.bytes.data(),48,base+FixtureRva(0xE85A161));Check(!chest.Get().ready,"unrecognized interface fails closed");Put(chest.bytes.data(),48,base+FixtureRva(0xE85A160));
    Put(chest.bytes.data(),696,reinterpret_cast<std::uintptr_t>(chest.items.data()+152));Check(!chest.Get().ready,"short chest vector cannot masquerade as 27 empty slots");Put(chest.bytes.data(),696,reinterpret_cast<std::uintptr_t>(chest.items.data()+27*152));
    Block partner;
    Put(partner.bytes.data(),8,int{2});Put(partner.bytes.data(),732,std::uint8_t{});
    Put(chest.bytes.data(),768,reinterpret_cast<std::uintptr_t>(partner.bytes.data()));Put(partner.bytes.data(),768,reinterpret_cast<std::uintptr_t>(chest.bytes.data()));
    result=chest.Get();Check(result.ready && result.slots==54 && result.items[27]==partner.items.data(),"paired chest preserves native leading half order");
    result=InspectBlock(partner.bytes.data(),base,{2,64,2});Check(result.ready && result.items[0]==chest.items.data(),"trailing chest produces same 54 slot order");
    Put(partner.bytes.data(),256,std::uint64_t{4});Check(!chest.Get().ready,"unknown loot in either half withholds whole double chest");Put(partner.bytes.data(),256,std::uint64_t{});
    Put(partner.bytes.data(),768,std::uintptr_t{});Check(!chest.Get().ready,"one-sided chest partnership is not trusted");
    struct Fixed{std::uintptr_t root,face,container;unsigned offset,first,count;};
    for(auto p:{Fixed{0xE858940,0xE85AB40,0xE858B30,240,664,3},Fixed{0xE858940,0xE858A20,0xE858B30,240,664,3},Fixed{0xE858940,0xE8F6FC0,0xE858B30,240,664,3},Fixed{0xE858D70,0xE858E50,0xE858F60,240,672,5},Fixed{0xE8F41A0,0xE8F4280,0xE8F4390,240,792,5},Fixed{0xE8F3250,0xE8F3330,0xE8F3410,280,688,9},Fixed{0xE8F3570,0xE8F3650,0xE8F3410,280,688,9},Fixed{0xE8F28F0,0xE8F29D0,0xE8F2AE0,280,688,9}}){
        Block b(p.root,p.face,p.container,p.offset);result=b.Get();
        Check(result.ready && result.slots==p.count && result.items[p.count-1]==b.bytes.data()+p.first+(p.count-1)*152,"fixed container uses its verified distinct layout and bounds");
        if(p.offset==280){Put(b.bytes.data(),256,std::uint64_t{5});Check(!b.Get().ready,"randomizable fixed container does not generate loot");}
    }
    Block shelf(0xE8F2160,0xE8F2240,0xE8F2320,240);
    std::array<std::byte,6*152> books{};Put(shelf.bytes.data(),648,reinterpret_cast<std::uintptr_t>(books.data()));Put(shelf.bytes.data(),656,reinterpret_cast<std::uintptr_t>(books.data()+books.size()));
    result=shelf.Get();Check(result.ready && result.slots==6 && result.items[5]==books.data()+760,"bookshelf validates full six element vector");
    std::array<std::byte,512> component{};std::array<std::byte,27*152> entityItems{};
    Put(component.data(),424,reinterpret_cast<std::uintptr_t>(entityItems.data()));Put(component.data(),432,reinterpret_cast<std::uintptr_t>(entityItems.data()+entityItems.size()));
    result=InspectEntityComponent(component.data());Check(result.ready && result.slots==27 && result.items[26]==entityItems.data()+3952,"entity component returns bounded native inventory without opening it");
    Put(component.data(),488,std::uint64_t{8});Check(!InspectEntityComponent(component.data()).ready,"entity loot table stays unknown");Put(component.data(),488,std::uint64_t{});
    Put(component.data(),432,reinterpret_cast<std::uintptr_t>(entityItems.data()+1));Check(!InspectEntityComponent(component.data()).ready,"malformed entity stride is rejected");
    Put(component.data(),432,reinterpret_cast<std::uintptr_t>(entityItems.data())+55*152);Check(!InspectEntityComponent(component.data()).ready,"oversized entity inventory is rejected");
    Check(!InspectBlock(nullptr,base,{}).ready && !InspectEntityComponent(nullptr).ready,"null containers fail closed");
    Block loot;Put(loot.bytes.data(),256,UINT64_C(9));
    Check(!loot.Get().ready && loot.Get().backgroundType==2,"unopened ordinary loot is explicitly eligible for native background generation");
    Put(loot.bytes.data(),256,UINT64_C(0));Put(loot.bytes.data(),696,std::uintptr_t{0});
    Check(!loot.Get().ready && loot.Get().backgroundType==0,"malformed vector is not misclassified as ungenerated loot");
    Block ender(0xE8F3920,0xE85A160,0xE8F3A40);Put(ender.bytes.data(),20,std::uint8_t{23});
    Check(ender.Get().recognized && !ender.Get().ready && ender.Get().backgroundType==23,"Ender block contents are never read as its player's inventory and require background opening");
    Put(ender.bytes.data(),768,loot.bytes.data());Check(!ender.Get().backgroundType,"malformed paired Ender does not authorize background opening");
    Block barrelLoot(0xE857C50,0xE857D70,0xE857F00);Put(barrelLoot.bytes.data(),256,UINT64_C(9));
    Check(!barrelLoot.Get().backgroundType,"unopened barrel does not acquire ordinary chest background eligibility");
    // Independent 45 constructor evidence, not offsets computed from the
    // production selector. Reusing 52's interface/loot/vector/partner fields
    // must fail these read-only decoder cases. This does not admit the image.
    native_detail::active.store(GameVersion::V45);
    auto make45=[](std::uintptr_t root,std::uintptr_t face,std::uintptr_t inventory,unsigned offset){
        std::vector<std::byte> bytes(5000);
        Put(bytes.data(),0,base+root);Put(bytes.data(),40,base+face);Put(bytes.data(),offset,base+inventory);
        Put(bytes.data(),8,int{1});Put(bytes.data(),12,int{64});Put(bytes.data(),16,int{2});
        return bytes;
    };
    auto native45=make45(0xE92E240,0xE8BCF20,0xE92E360,272);
    Put(native45.data(),20,std::uint8_t{2});Put(native45.data(),724,std::uint8_t{1});
    Put(native45.data(),680,reinterpret_cast<std::uintptr_t>(chest.items.data()));
    Put(native45.data(),688,reinterpret_cast<std::uintptr_t>(chest.items.data()+27*152));
    auto get45=[&]{return InspectBlock(native45.data(),base,{1,64,2});};
    auto original45=native45;
    result=get45();Check(result.ready && result.slots==27 && result.items[26]==chest.items.data()+3952,"45 chest uses exact interface40 container272 vector680");
    Check(native45==original45,"45 inspection leaves every native object byte unchanged");
    Put(native45.data(),248,UINT64_C(9));
    Check(!get45().ready && get45().backgroundType==2,"45 loot size248 authorizes only ordinary chest fallback");
    Put(native45.data(),248,UINT64_C(0));
    Put(native45.data(),256,UINT64_C(15));
    Check(get45().ready,"45 string capacity256 must not be confused with unresolved loot size");
    Put(native45.data(),40,base+0xE85A160);
    Check(!get45().recognized,"52 interface cannot be accepted inside a 45 chest");
    Put(native45.data(),40,base+0xE8BCF20);
    auto second45=native45;Put(second45.data(),8,int{2});Put(second45.data(),724,std::uint8_t{});
    Put(native45.data(),760,reinterpret_cast<std::uintptr_t>(second45.data()));
    Put(second45.data(),760,reinterpret_cast<std::uintptr_t>(native45.data()));
    result=get45();Check(result.ready && result.slots==54,"45 reciprocal adjacent partner760 combines both halves");
    Put(second45.data(),760,std::uintptr_t{});
    Check(!get45().ready,"45 one-sided partner fails closed");
    for(auto f:{Fixed{0xE8BF060,0xE8BF140,0xE8BF250,232,656,3},
                Fixed{0xE8BF060,0xE92D0A0,0xE8BF250,232,656,3},
                Fixed{0xE8BF060,0xE8C0BB0,0xE8BF250,232,656,3},
                Fixed{0xE92D180,0xE92D260,0xE92D370,232,664,5},
                Fixed{0xE8BD900,0xE8BD9E0,0xE8BDAF0,232,784,5},
                Fixed{0xE8BEEA0,0xE8BEF80,0xE8BCAB0,272,680,9},
                Fixed{0xE8BC8F0,0xE8BC9D0,0xE8BCAB0,272,680,9},
                Fixed{0xE92ECC0,0xE92EDA0,0xE92EEB0,272,680,9}}){
        auto bytes=make45(f.root,f.face,f.container,f.offset);
        result=InspectBlock(bytes.data(),base,{1,64,2});
        Check(result.ready && result.slots==f.count && result.items[f.count-1]==bytes.data()+f.first+(f.count-1)*152,
              "45 fixed inventories use independently constructed native offsets and tables");
        if(f.offset==272){
            Put(bytes.data(),248,UINT64_C(3));
            result=InspectBlock(bytes.data(),base,{1,64,2});
            Check(result.recognized && !result.ready && !result.backgroundType,"45 fixed randomizable inventories do not generate loot");
        }
    }
    for(auto f:{Fixed{0xE92C440,0xE92C560,0xE92C640,272,680,27},
                Fixed{0xE8C0620,0xE8C0740,0xE8C0850,272,680,27}}){
        auto bytes=make45(f.root,f.face,f.container,f.offset);
        Put(bytes.data(),680,reinterpret_cast<std::uintptr_t>(chest.items.data()));
        Put(bytes.data(),688,reinterpret_cast<std::uintptr_t>(chest.items.data()+chest.items.size()));
        result=InspectBlock(bytes.data(),base,{1,64,2});
        Check(result.ready && result.slots==27 && result.items[26]==chest.items.data()+3952,"45 barrel and shulker use their own verified interfaces");
        Put(bytes.data(),248,UINT64_C(3));
        result=InspectBlock(bytes.data(),base,{1,64,2});
        Check(result.recognized && !result.ready && !result.backgroundType,"45 unresolved barrel and shulker cannot authorize chest automation");
    }
    auto ender45=make45(0xE8BCE00,0xE8BCF20,0xE8BD0B0,272);
    Put(ender45.data(),20,std::uint8_t{23});
    result=InspectBlock(ender45.data(),base,{1,64,2});
    Check(result.recognized && !result.ready && result.backgroundType==23,"45 Ender requests own inventory without reading block slots");
    Put(ender45.data(),760,reinterpret_cast<std::uintptr_t>(native45.data()));
    Check(!InspectBlock(ender45.data(),base,{1,64,2}).backgroundType,"45 malformed Ender partner cannot authorize opening");
    auto shelf45=make45(0xE92E530,0xE92E610,0xE92E6F0,232);
    Put(shelf45.data(),640,reinterpret_cast<std::uintptr_t>(books.data()));
    Put(shelf45.data(),648,reinterpret_cast<std::uintptr_t>(books.data()+books.size()));
    result=InspectBlock(shelf45.data(),base,{1,64,2});
    Check(result.ready && result.slots==6 && result.items[5]==books.data()+760,"45 bookshelf vector640 uses complete six stack bounds");
    native_detail::active.store(GameVersion::Unknown);
    Check(!get45().recognized,"unknown profile cannot interpret a valid 45 object");
    native_detail::active.store(GameVersion::V52);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
