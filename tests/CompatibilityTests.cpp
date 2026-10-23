#include "ContainerPreview/HeldUse.h"
#include "ContainerPreview/EntityUse.h"
#include <iostream>
using namespace preview;
namespace {
int checks{},failures{};
void Check(bool ok,char const* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
}
int main(){
    constexpr std::uintptr_t base=0x140000000;
    int player{},actor{},hand{},clone{};
    Check(NativeRva(GameVersion::V51,0x5DA7C00)==0x5DA7B30,"51 client callback exact independently reviewed entry");
    Check(NativeRva(GameVersion::V51,0x4839060)==0x4838F90,"51 render callback exact independently reviewed entry");
    Check(NativeRva(GameVersion::V51,0x67721D0)==0x6772100,"51 standard creator exact entry");
    Check(NativeRva(GameVersion::V51,0x6772382)==0x67722B2,"51 standard screen build exact return");
    Check(NativeRva(GameVersion::V51,0x11D52E58)==0x11D52E58 && NativeRva(GameVersion::V51,0x11E450E0)==0x11E450E0,
          "51 diagnostic sentinels independently verified by exact IDA operands");
    for(auto version:{GameVersion::V51,GameVersion::V52}){
        native_detail::active.store(version);
        auto adjust=version==GameVersion::V51?0x25B53C1u:0x25B5161u;
        auto use=version==GameVersion::V51?0x259E5B6u:0x259E356u;
        auto secondary=version==GameVersion::V51?0x25B5836u:0x25B55D6u;
        auto handEmpty=version==GameVersion::V51?0x1A2B5CFu:0x1A2B2DFu;
        HeldUseScope held(base,&player,true);
        Check(HeldUseScope::BindAdjustment(base+adjust,&player,&clone),"version-specific owned clone binding");
        Check(HeldUseScope::ProtectsItemUse(base+use,&player),"version-specific fallback is protected");
        Check(HeldUseScope::ProtectsSecondary(base+secondary),"version-specific secondary query is protected");
        Check(!HeldUseScope::ProtectsSecondary(base+secondary+1),"adjacent secondary caller never matches");
        unsigned forwarded{};unsigned char result=9;
        auto original=[&](void*,void*,void*,void*,unsigned char,unsigned char,void*,std::uint64_t)->void*{++forwarded;return nullptr;};
        Check(FilterHeldItemUse(original,base+use,&clone,&result,&player,nullptr,0,0,nullptr,0)==&result && !result && !forwarded,
              "selected native version cancels only attributed held-item fallback");
        EntityUseScope entity(base,&player,&actor,&hand,true);
        Check(EntityUseScope::MatchesPlan(base+0x1F9F13,&actor,&player),"independently equal planner return matches on both images");
        {EntityUseScope::PlanScope planning;
            Check(EntityUseScope::ProtectsHand(base+handEmpty,&hand),"version-specific entity planner hand predicate");
            Check(!EntityUseScope::ProtectsHand(base+handEmpty+1,&hand),"adjacent entity predicate never matches");}
        if(version==GameVersion::V51){
            Check(!HeldUseScope::ProtectsAdjustment(base+0x25B5161,&player),"52 held caller cannot filter 51 operation");
            Check(!HeldUseScope::ProtectsItemUse(base+0x259E356,&player),"52 fallback cannot filter 51 operation");
        }
    }
    for(auto version:{GameVersion::Unknown}){
        native_detail::active.store(version);
        Check(!Rva(0x25B5161) && !Rva(0x1F9F13),"unadmitted profile has no fallback RVA");
        HeldUseScope held(base,&player,true);
        Check(!HeldUseScope::ProtectsAdjustment(base,&player),"unmapped held caller cannot alias module base");
        Check(!HeldUseScope::ProtectsSecondary(base),"unmapped secondary cannot alias module base");
        EntityUseScope entity(base,&player,&actor,&hand,true);
        Check(!EntityUseScope::MatchesPlan(base,&actor,&player),"unmapped entity caller cannot alias module base");
        {EntityUseScope::PlanScope planning;Check(!EntityUseScope::ProtectsHand(base,&hand),"unmapped hand caller cannot alias module base");}
    }
    // Physical45 adapter bindings do not admit its executable: identity and
    // complete-entry verification remain a separate gate in NativeProfile.
    Check(NativeRva(GameVersion::V45,0x24C92E0)==0x20DAE10,"45 actual Item-v1000 dispatcher is not superficial nominee903CB0");
    Check(NativeRva(GameVersion::V45,0x4839060)==0x47A0EF0,"45 HUD render uses independently verified two-argument entry");
    Check(NativeRva(GameVersion::V45,0x117D580)==0xD72560,"45 Level hit getter is proved by native table and exact body");
    Check(NativeRva(GameVersion::V45,0xE8554E0)==0xE7EF3A0,"45 BlockSource table comes from constructor2484090 and destructor2484330");
    Check(NativeRva(GameVersion::V45,0x475FC80)==0x2F2D130,"45 headless consistency exemption uses exact native continuation");
    Check(NativeRva(GameVersion::V45,0x1AAFEB0)==0x1990D30,"45 container-open callback is the actual E7B2C50/v16 target, not unrelated lookalike31E45C0");
    Check(NativeRva(GameVersion::V45,0x11D52E58)==0x11A31668 && NativeRva(GameVersion::V45,0x11E450E0)==0x11A73930,"45 diagnostic atlas sentinel identities follow exact resolver branches");
    native_detail::active.store(GameVersion::V52);
    Check(!NativeRva(GameVersion::V51,0xDEADBEEF),"unknown native key fails closed");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
