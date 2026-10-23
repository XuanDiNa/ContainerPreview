#include <string_view>
#include "ContainerPreview/NativeAddress.h"
#include "ContainerPreview/EntityUse.h"
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
    constexpr std::uintptr_t b=0x140000000;
    int player{},actor{},hand{},foreign{};std::array<unsigned char,80> output{};
    unsigned planned{},discarded{},emptyCalls{};bool allowed=true,bad{};
    auto empty=[&](void* stack)->unsigned char{++emptyCalls;return stack==&hand?0xA4:0xB2;};
    auto validate=[&](void* out,void* a,void* p){bad|=out!=output.data() || a!=&actor || p!=&player;return allowed;};
    auto discard=[&](void* out){++discarded;std::memset(out,0,80);return true;};
    auto plan=[&](void* a,void* out,void* p,void const* hit)->void*{
        ++planned;bad|=a!=&actor || out!=output.data() || p!=&player || hit!=&foreign;
        output[8]=7;
        return out;
    };
    Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&hand)==0xA4 && emptyCalls==1,"manual hand predicate retains complete AL result");
    Check(FilterEntityPlan(plan,validate,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&player,&foreign)==output.data() && planned==1 && !discarded && !bad,"manual plan forwards once with exact four arguments and result");
    {
        EntityUseScope use(b,&player,&actor,&hand,true);
        Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&hand)==0xA4,"outer dispatch alone never changes an emptiness query");
        auto ownedPlan=[&](void* a,void* out,void* p,void const* hit)->void*{
            Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&hand)==1,"owned planner skips held dye planning without clearing the hand");
            Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2E0),&hand)==0xA4,"adjacent caller retains original empty behavior");
            Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&foreign)==0xB2,"foreign stack cannot inherit held protection");
            bool workerProtected=true;std::thread worker([&]{workerProtected=EntityUseScope::ProtectsHand(b+FixtureRva(0x1A2B2DF),&hand);});worker.join();
            Check(!workerProtected,"other threads never inherit entity protection");
            return plan(a,out,p,hit);
        };
        auto invoke=[&]{return FilterEntityPlan(ownedPlan,validate,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&player,&foreign);};
        Check(invoke()==output.data() && !discarded && output[8]==7 && !bad,"exact owned container action is preserved for native execution");
        allowed=false;
        Check(invoke()==output.data() && discarded==1 && output==std::array<unsigned char,80>{},"noncontainer action is retired before native caller can execute it");
        Check(!EntityUseScope::ProtectsHand(b+FixtureRva(0x1A2B2DF),&hand),"planner scope retires before returning to native dispatch");
        unsigned validations{};auto countValidation=[&](void*,void*,void*){++validations;return false;};
        for(auto entry:{b+FixtureRva(0x1F9F14),b+FixtureRva(0x1F9F12)})FilterEntityPlan(plan,countValidation,discard,entry,&actor,output.data(),&player,&foreign);
        auto foreignPlan=[&](void*,void* out,void*,void const*)->void*{++planned;return out;};
        FilterEntityPlan(foreignPlan,countValidation,discard,b+FixtureRva(0x1F9F13),&foreign,output.data(),&player,&foreign);
        FilterEntityPlan(foreignPlan,countValidation,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&foreign,&foreign);
        Check(!validations && discarded==1,"foreign callsite actor or player forwards without filtering actions");
        {EntityUseScope nested(b,&foreign,&foreign,&foreign,true);Check(!EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"nested entity operation cannot borrow outer attribution");}
        Check(EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"nested operation restores outer identity");
        {EntityUseScope nested(0,nullptr,nullptr,nullptr,false);Check(!EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"disabled nested call suspends outer attribution");}
        auto reentrant=[&](void* a,void* out,void* p,void const* hit)->void*{
            auto nestedPlan=[&](void*,void* nestedOut,void*,void const*)->void*{
                Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&hand)==0xA4,"reentrant plan suspends the outer empty-hand filter");return nestedOut;
            };
            FilterEntityPlan(nestedPlan,countValidation,discard,b+FixtureRva(0x1F9F13),a,out,p,hit);
            Check(FilterEntityHandEmpty(empty,b+FixtureRva(0x1A2B2DF),&hand)==1,"outer planner attribution resumes after reentrant forwarding");
            return plan(a,out,p,hit);
        };
        allowed=true;
        FilterEntityPlan(reentrant,validate,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&player,&foreign);
        Check(!validations,"reentrant native plan is not cancelled or validated as an owned top-level action");
        volatile bool throwPlan=true;
        auto fault=[&](void*,void* out,void*,void const*)->void*{if(throwPlan)throw 7;return out;};
        try{FilterEntityPlan(fault,validate,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&player,&foreign);}catch(int){}
        Check(!EntityUseScope::ProtectsHand(b+FixtureRva(0x1A2B2DF),&hand),"C++ unwinding retires planner attribution");
    }
    Check(!EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"manual operations resume after owned dispatch");
    {EntityUseScope disabled(b,&player,&actor,&hand,false);Check(!EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"disabled scope never enables entity filters");}
    {EntityUseScope missing(b,&player,&actor,nullptr,true);Check(!EntityUseScope::MatchesPlan(b+FixtureRva(0x1F9F13),&actor,&player),"missing hand refuses entity protection");}
    auto sentinelPlan=[&](void* a,void* out,void* p,void const* h)->void*{
        Check(a==&actor && out==output.data() && p==&player && h==&foreign,"manual forwarding retains complete argument identities");return &foreign;
    };
    Check(FilterEntityPlan(sentinelPlan,validate,discard,b+FixtureRva(0x1F9F13),&actor,output.data(),&player,&foreign)==&foreign,"manual planner keeps full original return rather than substituting output");
    native_detail::active.store(GameVersion::V45);
    std::array<unsigned char,112> action45{};std::array<unsigned char,3> result45{0xC1,0xA0,0xD2};
    unsigned plans45{},discards45{},validations45{};bool allow45=true,planning45=false;void* return45=&result45[1];
    auto plan45=[&](void* a,void* result,void* p,void* action,void const* hit)->void*{
        ++plans45;
        Check(a==&actor && result==&result45[1] && p==&player && action==action45.data() && hit==&foreign,"45 planner preserves separate result/action and all five borrowed arguments");
        Check(FilterEntityHandEmpty(empty,b+0x18FB2B6,&hand)==(planning45?1:0xA4),"45 planner hand predicate is scoped");
        result45[1]=0xA3;action45[32]=7;return return45;
    };
    auto validate45=[&](void* action,void* a,void* p){++validations45;return allow45 && action==action45.data() && a==&actor && p==&player;};
    auto discard45=[&](void* action){++discards45;std::memset(action,0,104);return true;};
    auto invoke45=[&](std::uintptr_t caller){return FilterEntityPlan45(plan45,validate45,discard45,caller,&actor,&result45[1],&player,action45.data(),&foreign);};
    Check(invoke45(b+0x2FB87C6)==&result45[1] && plans45==1 && !validations45 && !discards45,"45 manual planner is forwarded without filtering");
    {
        EntityUseScope exact45(b,&player,&actor,&hand,true);
        Check(EntityUseScope::MatchesPlan(b+0x2FB87C6,&actor,&player),"45 planner admits independently verified native caller");
        Check(!EntityUseScope::MatchesPlan(b+0x1F9F13,&actor,&player),"45 planner refuses newer native caller");
        {
            EntityUseScope::PlanScope planning;
            Check(EntityUseScope::ProtectsHand(b+0x18FB2B6,&hand),"45 owned planning protects its exact held predicate");
            Check(!EntityUseScope::ProtectsHand(b+0x18FB2B7,&hand) && !EntityUseScope::ProtectsHand(b+0x18FB2B6,&foreign),
                  "45 adjacent caller and foreign hand retain original behavior");
        }
        planning45=true;
        Check(invoke45(b+0x2FB87C6)==&result45[1] && validations45==1 && !discards45 && result45[1]==0xA3 && action45[32]==7,"45 validated open action retains native result flags and callback");
        allow45=false;action45[104]=0xE4;action45[111]=0xD6;
        Check(invoke45(b+0x2FB87C6)==&result45[1] && discards45==1 && result45==std::array<unsigned char,3>{0xC1,0,0xD2} && !action45[32] && action45[104]==0xE4 && action45[111]==0xD6,
              "45 rejected action is retired and result cleared before native execution without touching canaries");
        allow45=true;return45=&foreign;
        Check(invoke45(b+0x2FB87C6)==&result45[1] && discards45==2 && result45[1]==0,"45 malformed native result cannot authorize action execution");
        planning45=false;
        Check(invoke45(b+0x2FB87C7)==&foreign && discards45==2 && result45[1]==0xA3,"45 foreign continuation preserves complete native return and action");
        Check(!EntityUseScope::ProtectsHand(b+0x18FB2B6,&hand),"45 planning scope retires before returning to native caller");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
