#include "ContainerPreview/PreviewBridge.h"
#include <cstring>
#include <iostream>
using namespace preview;
namespace {
int checks{},failures{},copies{},destroys{};std::uint32_t onThread{};bool wrongThread{};
struct Stack {std::uint64_t value;std::uint32_t thread;std::array<std::byte,140> rest;};static_assert(sizeof(Stack)==152);
bool Copy(void* out,void const* in){auto& s=*static_cast<Stack*>(out);s=*static_cast<Stack const*>(in);s.thread=onThread;++copies;return true;}
bool Destroy(void* value){wrongThread|=static_cast<Stack*>(value)->thread!=onThread;++destroys;return true;}
void Check(bool b,char const* name){++checks;if(!b){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
}
int main(){
    PreviewBridge bridge({Copy,Destroy});Target target{100,200,300,0,1,64,2,TargetKind::Block};
    ActorIdentity local{1001,200,300,true,false},server{1002,200,300,false,true};
    bridge.Update({}, {true,true,true,true},target,local,1000);auto work=bridge.Work(1000);
    Check(work && work->request.target==target,"client request crosses bridge by value with complete identity");
    if(!work)return 1;
    Stack original{42};BorrowedContents contents{};contents.ready=true;contents.slots=1;contents.items[0]=&original;onThread=1;
    Check(bridge.Publish(*work,server,700,contents,1,1001),"paired owner publishes deep copied contents");
    original.value=99;bool shown=false;
    bridge.View(1001,[&](Display display,Snapshot const& s,OwnedStacks const& stacks){shown=display==Display::Grid && s.slots==1 && static_cast<Stack const*>(stacks.At(0))->value==42;});
    Check(shown,"render reads independent frozen native value");
    auto old=*work;target.x=2;bridge.Update({}, {true,true,true,true},target,local,1002);
    Check(!bridge.Publish(old,server,700,contents,1,1003),"late result for former target is discarded before copying");
    Check(copies==1,"rejected late result never clones native source");
    work=bridge.Work(1003);auto mismatch=server;mismatch.runtime=301;
    Check(work && !bridge.Publish(*work,mismatch,700,contents,1,1003),"server must match both IDs and roles");
    if(!work)return 1;
    contents.ready=false;contents.recognized=true;contents.slots=0;Check(bridge.Publish(*work,server,700,contents,1,1003),"paired unknown data replaces stale grid without becoming empty");
    bool waiting=false;bridge.View(1003,[&](Display d,Snapshot const&,OwnedStacks const&){waiting=d==Display::Waiting;});Check(waiting,"unknown paired snapshot displays unavailable state");
    contents.ready=true;contents.slots=1;onThread=2;
    Check(bridge.Publish(*work,server,701,contents,2,1004) && !wrongThread,"new server thread never destroys old owner values");
    bridge.Update({}, {true,true,false,true},target,local,1005);bool visible=false;
    bridge.View(1005,[&](Display d,Snapshot const&,OwnedStacks const&){visible=d!=Display::Hidden;});Check(!visible,"release suppresses draw immediately");
    onThread=2;bridge.Cleanup(2,1005);Check(destroys==2 && !wrongThread,"correct server owner retires native copy on release");
    Check(!bridge.Work(2000),"expired or released request cannot be re-used by server");
    bridge.Update({}, {true,true,true,true},target,local,2100);work=bridge.Work(2100);if(!work)return 1;
    onThread=10;Check(bridge.Publish(*work,server,800,contents,10,2100),"new owner starts independent world pool");
    onThread=20;Check(bridge.Publish(*work,server,801,contents,20,2101) && !wrongThread,"live old copies are quarantined when the server thread changes");
    auto beforeCleanup=destroys;onThread=10;bridge.Cleanup(10,2102);
    Check(destroys==beforeCleanup+1 && !wrongThread,"returning old owner alone can retire quarantined native values");
    shown=false;bridge.View(2102,[&](Display d,Snapshot const&,OwnedStacks const& s){shown=d==Display::Grid && s.Size()==1;});
    Check(shown,"retiring old pool preserves current owner snapshot");
    bridge.Update({}, {true,true,false,true},target,local,2103);onThread=20;bridge.Cleanup(20,2103);
    PreviewBridge remote({Copy,Destroy});onThread=30;
    RemoteLease lease{400,500,600,700,30,2};
    remote.Update({}, {true,true,true,true},target,local,3000);auto rw=remote.Work(3000);if(!rw)return 1;
    auto before=copies;
    Check(!remote.PublishRemote(*rw,lease,contents,30,3000) && copies==before,"unleased remote contents cannot be copied or displayed");
    Check(remote.AuthorizeRemote(*rw,lease,3000),"accepted current native session gets a separate remote lease");
    auto wrongLease=lease;wrongLease.model++;
    Check(!remote.PublishRemote(*rw,wrongLease,contents,30,3001) && copies==before,"a different model cannot use the accepted remote lease");
    Check(!remote.PublishRemote(*rw,lease,contents,31,3001) && copies==before,"foreign callback owner cannot clone remote values");
    Check(remote.PublishRemote(*rw,lease,contents,30,3001),"matching applied remote contents are cloned on their native owner");
    shown=false;remote.View(3001,[&](Display d,Snapshot const& s,OwnedStacks const& v){shown=d==Display::Grid && !s.pairedServer && s.remote==lease && static_cast<Stack const*>(v.At(0))->value==99;});
    Check(shown,"remote display is truthful and never claims integrated-server pairing");
    remote.RevokeRemote();shown=false;
    remote.View(3002,[&](Display d,Snapshot const&,OwnedStacks const&){shown=d!=Display::Hidden;});
    Check(!shown && !remote.PublishRemote(*rw,lease,contents,30,3002),"revocation hides immediately and refuses later remote updates");
    Check(remote.AuthorizeRemote(*rw,lease,3003),"same still-current request may be explicitly reauthorized by its native owner");
    target.z++;remote.Update({}, {true,true,true,true},target,local,3004);
    Check(!remote.PublishRemote(*rw,lease,contents,30,3004),"target change rejects old lease before any native copy");
    remote.Update({}, {true,true,false,true},target,local,3005);remote.Cleanup(30,3005);
    Check(!wrongThread,"remote cleanup preserves native owner affinity");
    {
        PreviewBridge b({Copy,Destroy});onThread=41;
        b.Update({}, {true,true,true,true},target,local,4000);auto w=b.Work(4000);if(!w)return 1;
        Check(!w->paired && !w->backgroundType,"client target alone never authorizes integrated fallback");
        BorrowedContents loot{};loot.recognized=true;loot.backgroundType=2;
        Check(b.Publish(*w,server,900,loot,41,4001),"paired server may publish value-only unopened loot eligibility");
        w=b.Work(4001);if(!w)return 1;
        Check(w->paired && w->backgroundType==2,"current paired eligibility crosses only the matching target generation");
        Check(b.BeginBackground(*w,42,4002),"pending background request reserves its data source before native interaction");
        auto prior=copies;contents.ready=true;contents.slots=1;
        Check(!b.Publish(*w,server,900,contents,41,4003) && copies==prior,"read-only owner cannot race a pending background request or clone a replacement snapshot");
        bool grid=false;b.View(4003,[&](Display d,Snapshot const&,OwnedStacks const&){grid=d==Display::Grid;});
        Check(!grid,"pending reservation never presents old read-only contents as accepted background data");
        auto liveWork=b.Work(4003);
        Check(liveWork && liveWork->paired && !liveWork->backgroundType,"paired liveness updates while reserved without switching snapshot source");
        RemoteLease own{400,500,600,700,42,2},foreign=own;foreign.owner=43;
        Check(!b.AuthorizeRemote(*w,foreign,4004) && b.AuthorizeRemote(*w,own,4004),"only the reservation owner can authorize its accepted native model");
        onThread=42;Check(b.PublishRemote(*w,own,contents,42,4004),"fully applied background data publishes after acceptance");
        b.RevokeRemote();onThread=41;
        Check(b.Publish(*w,server,900,contents,41,4005),"normal paired read-only sampling resumes after background retirement");
        b.Update({}, {true,true,true,true},target,local,4400);auto expired=b.Work(4506);
        Check(expired && !expired->paired,"expired integrated observations cannot authorize a later native interaction");
        target.x++;b.Update({}, {true,true,true,true},target,local,4507);auto changed=b.Work(4507);
        Check(changed && !changed->paired && !changed->backgroundType,"new target cannot inherit the previous chest's background eligibility");
        b.Update({}, {true,true,false,true},target,local,4508);onThread=41;b.Cleanup(41,4508);onThread=42;b.Cleanup(42,4508);
        Check(!wrongThread,"source switching leaves all native stack destruction on the original owner");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
