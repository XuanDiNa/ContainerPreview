#include "ContainerPreview/HookSet.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace preview;
namespace {
int checks{},failures{},createCount{},enableCount{},failCreate{},failEnable{},failureStatus{1};std::vector<int> disabled,removed;
void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}
int Id(void* p){return static_cast<int>(reinterpret_cast<std::uintptr_t>(p));}
int Create(void*,void*,void** original){++createCount;if(createCount==failCreate)return failureStatus;*original=reinterpret_cast<void*>(123);return 0;}
int Enable(void*){return ++enableCount==failEnable?failureStatus:0;}int Disable(void* p){disabled.push_back(Id(p));return 0;}int Remove(void* p){removed.push_back(Id(p));return 0;}
void Reset(){createCount=enableCount=failCreate=failEnable=0;failureStatus=1;disabled.clear();removed.clear();}
}
int main(){
    std::array<void*,3> originals{};std::array<HookSpec,3> specs{};
    for(unsigned i=0;i<3;++i)specs[i]={reinterpret_cast<void*>(static_cast<std::uintptr_t>(i+1)),reinterpret_cast<void*>(4),&originals[i]};HookBackend backend{Create,Enable,Disable,Remove};
    Check(InstallHookSet(specs,backend) && createCount==3 && enableCount==3 && removed.empty(),"complete hook set installs without touching unrelated hooks");
    Reset();failCreate=2;Check(!InstallHookSet(specs,backend) && removed==std::vector<int>{1} && enableCount==0,"partial creation rolls back before any hook is enabled");
    Reset();failEnable=2;Check(!InstallHookSet(specs,backend) && disabled==std::vector<int>{1} && removed==std::vector<int>{3,2,1},"partial enable disables and removes only this set in reverse order");
    Reset();Check(!InstallHookSet({},backend) && createCount==0,"empty set cannot claim initialization");
    // Regression: test4 supplied 11 hooks to an installer capped at 8. These
    // exercise the real installer policy, not just three backend operations.
    std::array<void*,28> manyOriginals{};std::array<HookSpec,28> many{};
    for(unsigned i=0;i<many.size();++i)many[i]={reinterpret_cast<void*>(static_cast<std::uintptr_t>(i+1)),reinterpret_cast<void*>(40),&manyOriginals[i]};
    auto eleven=std::span<HookSpec const>(many).first(11);
    Reset();Check(InstallHookSet(eleven,backend) && createCount==11 && enableCount==11 && removed.empty(),"actual eleven-hook diagnostic set is admitted and enabled completely");
    Reset();Check(InstallHookSet(std::span<HookSpec const>(many).first(12),backend) && createCount==12 && enableCount==12 && removed.empty(),"expanded twelve-hook binding diagnostic set installs completely");
    Reset();Check(InstallHookSet(std::span<HookSpec const>(many).first(14),backend) && createCount==14 && enableCount==14 && removed.empty(),"actual fourteen-hook GUI diagnostic set installs completely");
    Reset();failCreate=9;Check(!InstallHookSet(eleven,backend) && createCount==9 && enableCount==0 && removed==std::vector<int>{8,7,6,5,4,3,2,1},"late creation failure rolls back eight created hooks without any enabling");
    Reset();failEnable=10;Check(!InstallHookSet(eleven,backend) && disabled==std::vector<int>{9,8,7,6,5,4,3,2,1} && removed==std::vector<int>{11,10,9,8,7,6,5,4,3,2,1},"late enable failure rolls back only the eleven-hook set in reverse order");
    Reset();Check(InstallHookSet(std::span<HookSpec const>(many).first(16),backend) && createCount==16 && enableCount==16,"bounded capacity admits a complete sixteen-hook set");
    auto eighteen=std::span<HookSpec const>(many).first(18);
    Reset();Check(InstallHookSet(eighteen,backend) && createCount==18 && enableCount==18 && removed.empty(),"actual eighteen-hook presentation diagnostic set installs completely");
    Reset();failCreate=18;Check(!InstallHookSet(eighteen,backend) && createCount==18 && enableCount==0 && removed.size()==17 && removed.front()==17 && removed.back()==1,"final presentation hook creation failure rolls back all seventeen created entries before enabling");
    Reset();failEnable=18;Check(!InstallHookSet(eighteen,backend) && createCount==18 && enableCount==18 && disabled.size()==17 && disabled.front()==17 && disabled.back()==1 && removed.size()==18 && removed.front()==18 && removed.back()==1,"final presentation hook enable failure rolls back exactly the full owned set");
    Reset();Check(InstallHookSet(std::span<HookSpec const>(many).first(19),backend) && createCount==19 && enableCount==19,"actual nineteen-hook background set installs completely");
    auto twenty=std::span<HookSpec const>(many).first(20);
    Reset();Check(InstallHookSet(twenty,backend) && createCount==20 && enableCount==20 && removed.empty(),"complete twenty-hook headless consistency set installs together");
    Reset();failCreate=20;Check(!InstallHookSet(twenty,backend) && createCount==20 && enableCount==0 && removed.size()==19 && removed.front()==19 && removed.back()==1,"last consistency hook creation failure rolls back nineteen owned entries before enabling");
    Reset();failEnable=20;Check(!InstallHookSet(twenty,backend) && createCount==20 && enableCount==20 && disabled.size()==19 && disabled.front()==19 && disabled.back()==1 && removed.size()==20 && removed.front()==20 && removed.back()==1,"last consistency hook enable failure rolls back only the full owned twenty-hook set");
    auto twentyOne=std::span<HookSpec const>(many).first(21);
    Reset();Check(InstallHookSet(twentyOne,backend) && createCount==21 && enableCount==21 && removed.empty(),"complete entity-interaction diagnostic set installs all twenty-one hooks");
    Reset();failCreate=21;Check(!InstallHookSet(twentyOne,backend) && createCount==21 && enableCount==0 && removed.size()==20 && removed.front()==20 && removed.back()==1,"entity hook creation failure rolls back twenty entries before enabling");
    Reset();failEnable=21;Check(!InstallHookSet(twentyOne,backend) && createCount==21 && enableCount==21 && disabled.size()==20 && disabled.front()==20 && disabled.back()==1 && removed.size()==21 && removed.front()==21 && removed.back()==1,"entity hook enable failure rolls back only the owned twenty-one-hook set");
    auto twentyFour=std::span<HookSpec const>(many).first(24);
    Reset();Check(InstallHookSet(twentyFour,backend) && createCount==24 && enableCount==24 && removed.empty(),"all three held-use protections install with the complete existing hook set");
    Reset();failCreate=24;Check(!InstallHookSet(twentyFour,backend) && createCount==24 && enableCount==0 && removed.size()==23 && removed.front()==23 && removed.back()==1,"last held-use protection creation failure rolls back all twenty-three entries before enabling");
    Reset();failEnable=24;Check(!InstallHookSet(twentyFour,backend) && createCount==24 && enableCount==24 && disabled.size()==23 && disabled.front()==23 && disabled.back()==1 && removed.size()==24 && removed.front()==24 && removed.back()==1,"held-use enable failure rolls back the entire owned set without a partially protected preview");
    auto twentySix=std::span<HookSpec const>(many).first(26);
    Reset();Check(InstallHookSet(twentySix,backend) && createCount==26 && enableCount==26 && removed.empty(),"entity factory and observed native resolver install together with all existing protections");
    Reset();failCreate=26;Check(!InstallHookSet(twentySix,backend) && createCount==26 && enableCount==0 && removed.size()==25 && removed.front()==25 && removed.back()==1,"last entity-provenance creation failure rolls back the whole owned set before enabling");
    Reset();failEnable=26;Check(!InstallHookSet(twentySix,backend) && createCount==26 && enableCount==26 && disabled.size()==25 && removed.size()==26 && removed.front()==26 && removed.back()==1,"entity-provenance enable failure rolls back all owned hooks without affecting others");
    auto twentySeven=std::span<HookSpec const>(many).first(27);
    Reset();Check(InstallHookSet(twentySeven,backend) && createCount==27 && enableCount==27,"entity planner protection installs together with all existing hooks");
    Reset();failCreate=27;Check(!InstallHookSet(twentySeven,backend) && createCount==27 && enableCount==0 && removed.size()==26,"planner creation failure never enables a partially protected set");
    Reset();failEnable=27;Check(!InstallHookSet(twentySeven,backend) && createCount==27 && enableCount==27 && disabled.size()==26 && removed.size()==27,"planner enable failure rolls back all owned hooks");
    Reset();Check(!InstallHookSet(many,backend) && createCount==0 && enableCount==0 && removed.empty(),"oversized set is refused before any native backend call");
    // Missing stage/index/raw status would hide the cause of a native refusal.
    HookDiagnostic diagnostic;
    Reset();Check(!InstallHookSet({},backend,&diagnostic) && diagnostic.failure==HookFailure::Validation && diagnostic.requested==0 && diagnostic.index==NoHookIndex && diagnostic.target==0 && diagnostic.backendStatus==0,"empty validation refusal reports no native attempt");
    Reset();Check(!InstallHookSet(many,backend,&diagnostic) && diagnostic.failure==HookFailure::Validation && diagnostic.requested==28 && diagnostic.index==NoHookIndex && createCount==0,"capacity refusal reports the actual requested count before any backend call");
    auto invalid=specs;invalid[1].detour=nullptr;
    Reset();Check(!InstallHookSet(invalid,backend,&diagnostic) && diagnostic.failure==HookFailure::Validation && diagnostic.requested==3 && diagnostic.index==1 && diagnostic.target==2 && diagnostic.backendStatus==0 && createCount==0,"invalid entry reports its zero-based index and target before creating any hook");
    for(unsigned missing=0;missing<4;++missing){
        auto incomplete=backend;
        if(missing==0)incomplete.create=nullptr;
        if(missing==1)incomplete.enable=nullptr;
        if(missing==2)incomplete.disable=nullptr;
        if(missing==3)incomplete.remove=nullptr;
        Reset();Check(!InstallHookSet(specs,incomplete,&diagnostic) && diagnostic.failure==HookFailure::Validation && diagnostic.requested==3 && diagnostic.index==NoHookIndex && diagnostic.target==0 && diagnostic.backendStatus==0 && createCount==0,"missing backend operation is a validation refusal, not a native failure");
    }
    Reset();failCreate=9;failureStatus=13;
    Check(!InstallHookSet(eleven,backend,&diagnostic) && diagnostic.failure==HookFailure::Create && diagnostic.requested==11 && diagnostic.index==8 && diagnostic.target==9 && diagnostic.backendStatus==13 && removed==std::vector<int>{8,7,6,5,4,3,2,1},"late creation diagnostic preserves the failing target and raw status through rollback");
    Reset();failEnable=10;failureStatus=-4;
    Check(!InstallHookSet(eleven,backend,&diagnostic) && diagnostic.failure==HookFailure::Enable && diagnostic.requested==11 && diagnostic.index==9 && diagnostic.target==10 && diagnostic.backendStatus==-4 && removed==std::vector<int>{11,10,9,8,7,6,5,4,3,2,1},"late enable diagnostic preserves the failing target and negative raw status through rollback");
    auto description=DescribeHookDiagnostic(diagnostic,1);
    Check(description.find("stage=enable")!=std::string::npos && description.find("requested=11")!=std::string::npos && description.find("index0=9")!=std::string::npos && description.find("rva=0x9")!=std::string::npos && description.find("backendStatus=-4")!=std::string::npos,"worker diagnostic text exposes native failure stage count index rebased RVA and raw error");
    Check(DescribeHookDiagnostic(diagnostic,100).find("rva=")==std::string::npos,"diagnostic text never underflows an out-of-image target into a fabricated RVA");
    Reset();Check(InstallHookSet(eleven,backend,&diagnostic) && diagnostic.failure==HookFailure::None && diagnostic.requested==11 && diagnostic.index==NoHookIndex && diagnostic.target==0 && diagnostic.backendStatus==0,"successful installation resets a stale failure diagnostic");
    int calls=0,observed=0;auto original=[&](void* p,int flag)->std::uint64_t{++calls;return p==reinterpret_cast<void*>(77) && flag==19?0xFEDCBA9876543210ull:0;};
    auto result=ForwardOnce(original,[&]{++observed;throw 7;},reinterpret_cast<void*>(77),19);
    Check(result==0xFEDCBA9876543210ull && calls==1 && observed==1,"observer failure preserves full original result and exact single forwarding");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
