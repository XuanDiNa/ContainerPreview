#include <string_view>
#include "ContainerPreview/NativeAddress.h"
#include "ContainerPreview/HeldUse.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>
using namespace preview;
namespace {
std::uint32_t FixtureRva(std::uint32_t key){auto value=Rva(key);return value?value:key;}
int checks{},failures{};
void Check(bool ok,char const* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
}
int main(int argc,char** argv){
    if(argc>1 && std::string_view(argv[1])=="51")native_detail::active.store(GameVersion::V51);
    constexpr std::uintptr_t b=0x140000000;void* p=reinterpret_cast<void*>(100);void* other=reinterpret_cast<void*>(200);
    unsigned calls{};auto adjust=[&](void* item,void* stack,void* player,void* face,void* pos)->std::uint64_t{
        ++calls;return item==other && stack==other && player==p && face==other && pos==other?UINT64_C(0xFEDCBA9876543210):0;
    };
    auto use=[&](void* stack,void* out,void* player,void* pos,unsigned char face,unsigned char hand,void* hit,std::uint64_t flags)->void*{
        ++calls;Check(stack==other && player==p && pos==other && face==5 && hand==1 && hit==other && flags==UINT64_C(0xFEDCBA9876543210),"ordinary use preserves every argument");
        *static_cast<unsigned char*>(out)=7;return other;
    };
    auto empty=[&](void* stack)->unsigned char{++calls;return stack==other?0xA4:0;};
    Check(FilterHeldAdjust(adjust,b+FixtureRva(0x25B5161),other,other,p,other,other)==UINT64_C(0xFEDCBA9876543210) && calls==1,"manual adjustment outside preview forwards once with full result");
    unsigned char result=9;
    Check(FilterHeldItemUse(use,b+FixtureRva(0x259E356),other,&result,p,other,5,1,other,UINT64_C(0xFEDCBA9876543210))==other && result==7 && calls==2,"manual item use outside preview is unchanged");
    Check(FilterHeldEmpty(empty,b+FixtureRva(0x25B55D6),other)==0xA4 && calls==3,"manual empty query preserves nonboolean register value");
    {
        HeldUseScope guard(b,p,true);
        Check(guard.Active(),"validated owned synchronous operation activates protection");
        Check(FilterHeldAdjust(adjust,b+FixtureRva(0x25B5161),other,other,p,other,other)==0 && calls==3,"owned preview skips item-specific placement planning");
        Check(FilterHeldAdjust(adjust,b+FixtureRva(0x25B5162),other,other,p,other,other)==UINT64_C(0xFEDCBA9876543210) && calls==4,"other adjustment callsite is not intercepted");
        FilterHeldAdjust(adjust,b+FixtureRva(0x25B5161),other,other,other,other,other);
        Check(calls==5,"other player adjustment is not intercepted");
        auto reentrantCalls=calls;
        FilterHeldAdjust(adjust,b+FixtureRva(0x25B5161),other,p,p,other,other);
        Check(calls==reentrantCalls+1,"reentrant adjustment for another stack forwards without replacing the owned clone binding");
        for(auto caller:{b+FixtureRva(0x259E356),b+FixtureRva(0x259E3ED)}){
            result=9;auto savedCalls=calls;
            Check(FilterHeldItemUse(use,caller,other,&result,p,other,5,1,other,UINT64_C(0xFEDCBA9876543210))==&result && result==0 && calls==savedCalls,"both item-use fallback branches return native Pass without consuming items");
        }
        auto savedCalls=calls;result=9;
        Check(FilterHeldItemUse(use,b+FixtureRva(0x259E357),other,&result,p,other,5,1,other,UINT64_C(0xFEDCBA9876543210))==other && result==7 && calls==savedCalls+1,"foreign item-use callsite forwards all arguments and result");
        savedCalls=calls;result=9;
        auto foreignStackUse=[&](void* s,void* out,void* player,void* pos,unsigned char face,unsigned char hand,void* hit,std::uint64_t flags)->void*{
            ++calls;if(s==p && player==p && pos==other && face==5 && hand==1 && hit==other && flags==17)*static_cast<unsigned char*>(out)=6;return p;
        };
        Check(FilterHeldItemUse(foreignStackUse,b+FixtureRva(0x259E356),p,&result,p,other,5,1,other,17)==p && result==6 && calls==savedCalls+1,"same-player use-on for another stack is not mistaken for the owned cloned stack");
        savedCalls=calls;
        Check(FilterHeldEmpty(empty,b+FixtureRva(0x25B55D6),other)==1 && calls==savedCalls,"secondary hit fallback is cancelled without modifying a stack or faking container acceptance");
        Check(FilterHeldEmpty(empty,b+FixtureRva(0x25B55D6),p)==0 && calls==savedCalls+1,"same-thread secondary query for a different stack must retain native behavior");
        savedCalls=calls;
        Check(FilterHeldEmpty(empty,b+FixtureRva(0x25B55D7),other)==0xA4 && calls==savedCalls+1,"other emptiness queries retain native behavior");
        bool threadUnprotected{};
        std::thread worker([&]{threadUnprotected=!HeldUseScope::ProtectsSecondary(b+FixtureRva(0x25B55D6));});worker.join();
        Check(threadUnprotected,"preview protection never leaks to another native thread");
        {HeldUseScope nested(b,other,true);Check(HeldUseScope::ProtectsAdjustment(b+FixtureRva(0x25B5161),other) && !HeldUseScope::ProtectsAdjustment(b+FixtureRva(0x25B5161),p),"nested scope is attributed only to its own player");}
        Check(HeldUseScope::ProtectsAdjustment(b+FixtureRva(0x25B5161),p),"nested scope restores the outer attribution");
        try{HeldUseScope nested(b,other,true);throw 7;}catch(int){}
        Check(HeldUseScope::ProtectsAdjustment(b+FixtureRva(0x25B5161),p),"exception unwinding restores protection attribution");
    }
    Check(!HeldUseScope::ProtectsSecondary(b+FixtureRva(0x25B55D6)),"protection retires before ordinary operations resume");
    {HeldUseScope disabled(b,p,false);Check(!disabled.Active() && !HeldUseScope::ProtectsSecondary(b+FixtureRva(0x25B55D6)),"unapproved operation cannot enable filtering");}
    {HeldUseScope invalid(0,p,true);Check(!invalid.Active(),"missing exact image refuses activation");}
    {HeldUseScope invalid(b,nullptr,true);Check(!invalid.Active(),"missing player refuses activation");}
    native_detail::active.store(GameVersion::V45);
    unsigned use45Calls{};unsigned char result45=0xBA;std::array<unsigned char,3> byteCanary{0xC1,0xBA,0xD2};
    auto use45=[&](void* s,void* out,void* player,int x,int y,int z,unsigned char face,void* hit,std::uint64_t flags)->void*{
        ++use45Calls;
        Check(s==other && out==&byteCanary[1] && player==p && x==-312 && y==73 && z==90001 && face==5 && hit==other && flags==UINT64_C(0xFEDCBA9876543210),
              "45 native use-on forwards all nine arguments without narrowing signed coordinates or flags");
        *static_cast<unsigned char*>(out)=6;return &result45;
    };
    auto invoke45=[&](std::uintptr_t caller){return FilterHeldItemUse45(use45,caller,other,&byteCanary[1],p,-312,73,90001,5,other,UINT64_C(0xFEDCBA9876543210));};
    Check(invoke45(b+0x2D31521)==&result45 && use45Calls==1 && byteCanary[1]==6,"45 manual use-on preserves complete native return");
    {
        HeldUseScope exact45(b,p,true);
        auto saved=calls;
        Check(FilterHeldAdjust(adjust,b+0x2D3CD5E,other,other,p,other,other)==0 && calls==saved,
              "45 exact native adjustment binds only the owned cloned stack");
        Check(HeldUseScope::ProtectsItemUse(b+0x2D31521,p) && HeldUseScope::ProtectsItemUse(b+0x2D315C1,p),
              "45 both native use-on continuations are protected");
        Check(FilterHeldEmpty(empty,b+0x2D3D1CB,other)==1 && calls==saved,
              "45 secondary-hit fallback protects only the bound stack");
        Check(!HeldUseScope::ProtectsItemUse(b+0x259E356,p) && !HeldUseScope::ProtectsSecondary(b+0x25B55D6),
              "45 never inherits newer callsite identities");
        for(auto caller:{b+0x2D31521,b+0x2D315C1}){
            byteCanary[1]=0xBA;
            Check(invoke45(caller)==&byteCanary[1] && byteCanary==std::array<unsigned char,3>{0xC1,0,0xD2} && use45Calls==1,
                  "45 owned use-on suppresses both item-use branches and changes only result byte");
        }
        Check(invoke45(b+0x2D31522)==&result45 && use45Calls==2 && byteCanary[1]==6,"45 neighboring use-on caller remains native");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
