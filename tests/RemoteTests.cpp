#include "ContainerPreview/RemoteSession.h"
#include <iostream>
using namespace preview;
namespace {
int checks{},failures{};
void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}
RemoteRequest MakeRequest(){return {{{100,200,300,0,-64,38,31,TargetKind::Block},7,1000},1,400,500,27,2};}
RemoteOpen MakeOpen(RemoteRequest const& r){return {r.preview.target,r.owner,r.client,r.player,2,0,UINT64_MAX,true};}
void Activate(RemoteSession& s,RemoteRequest const& r){s.Step(r,1000);s.Created(600,700);s.Accepted(2,600,700);}
}
int main(){
    // These cases catch duplicate native requests, target-only matching,
    // initial blank storage shown as known, and closing a user's manual UI.
    RemoteSession s;auto r=MakeRequest();auto o=MakeOpen(r);
    Check(s.Step(r,1000)==RemoteAction::Open && s.Phase()==RemotePhase::Pending,"eligible intent issues exactly one open");
    Check(s.Step(r,1001)==RemoteAction::None && !s.Visible(),"held key never repeats the pending request or shows unreceived slots");
    Check(s.Matches(o),"matching accepted deferred response belongs to the pending request");
    auto wrong=o;wrong.target.x++;
    Check(!s.Matches(wrong),"different position is not suppressed");
    wrong=o;wrong.target.world++;
    Check(!s.Matches(wrong),"different world is not suppressed");
    wrong=o;wrong.owner++;
    Check(!s.Matches(wrong),"different callback owner cannot claim the session");
    wrong=o;wrong.player++;
    Check(!s.Matches(wrong),"different native player cannot claim the session");
    wrong=o;wrong.client++;
    Check(!s.Matches(wrong),"different client cannot claim the session");
    wrong=o;wrong.entity=123;
    Check(!s.Matches(wrong),"entity response cannot impersonate a block open");
    wrong=o;wrong.type=37;
    Check(!s.Matches(wrong),"custom container type is forwarded untouched");
    wrong=o;wrong.mayOpen=false;
    Check(!s.Matches(wrong),"native denial never admits model creation");
    wrong=o;wrong.id=255;
    Check(!s.Matches(wrong),"initial unaccepted model ID is not a server session");
    Check(s.Created(600,700) && !s.Created(601,701),"one factory result is claimed once before accepted ID assignment");
    Check(!s.Matches(o),"a second deferred response cannot create a second model for the pending operation");
    Check(!s.Accepted(255,600,700) && !s.Accepted(2,601,700),"acceptance needs valid ID and the exact created model");
    Check(s.Accepted(2,600,700) && s.Phase()==RemotePhase::Active && !s.Visible(),"server ID assignment alone is not full content");
    Check(!s.Applied(3,600,27,0x7FFFFFF) && !s.Applied(2,601,27,0x7FFFFFF),"other session or model updates cannot make content ready");
    Check(!s.Applied(2,600,26,0x3FFFFFF) && !s.Applied(2,600,27,0x3FFFFFF),"wrong count or missing final setter cannot make content ready");
    Check(s.Applied(2,600,27,0x7FFFFFF) && s.Visible(),"all matching full-update setters make the owned model visible");
    Check(s.Step(std::nullopt,1010)==RemoteAction::Close && !s.Visible(),"release invalidates visibility before requesting native close");
    Check(s.Step(std::nullopt,1011)==RemoteAction::None,"release cannot send native close twice");
    Check(!s.CloseReply(401,1,2) && !s.CloseReply(400,2,2) && !s.CloseReply(400,1,3),"unrelated close replies do not retire the session");
    Check(s.CloseReply(400,1,2) && s.Phase()==RemotePhase::Idle,"matching native close reply completes retirement");
    Check(s.Step(r,1012)==RemoteAction::Open,"a new hold after release can start a new session");
    RemoteSession late;late.Step(r,1000);
    Check(late.Step(std::nullopt,1001)==RemoteAction::None && late.Matches(o),"release before reply keeps a non-visible late-response tombstone");
    Check(late.Created(600,700) && late.Accepted(2,600,700) && late.Step(std::nullopt,1002)==RemoteAction::Close,"late accepted owned session is closed only after actual ID assignment");
    RemoteSession timeout;timeout.Step(r,1000);
    Check(timeout.Step(r,4001)==RemoteAction::None && timeout.Matches(o),"timed out request is not retried and can still retire a late reply");
    timeout.Created(600,700);timeout.Accepted(2,600,700);
    Check(timeout.Step(r,4002)==RemoteAction::Close && !timeout.Visible(),"late timeout reply never becomes visible despite the key still being held");
    RemoteSession moved;Activate(moved,r);auto next=r;next.preview.target.x++;next.preview.generation++;
    Check(moved.Step(next,1005)==RemoteAction::Close && !moved.Visible(),"target change closes the prior session before any new open");
    Check(moved.Step(next,1006)==RemoteAction::None,"new target waits for previous native close reply");
    moved.CloseReply(400,1,2);
    Check(moved.Step(next,1007)==RemoteAction::Open,"new target can open after confirmed retirement");
    RemoteSession manual;Activate(manual,r);manual.Relinquish();
    Check(!manual.Visible() && !manual.Matches(o) && manual.Step(r,1002)==RemoteAction::None,"manual takeover relinquishes all suppression and automatic retries for this hold");
    manual.Step(std::nullopt,1003);
    Check(manual.Step(r,1004)==RemoteAction::Open,"manual takeover is rearmed only by a new hold");
    RemoteSession ui;Activate(ui,r);ui.Relinquish();ui.Step(std::nullopt,1003,true);
    Check(ui.Step(r,1004)==RemoteAction::None,"UI or eligibility loss while key is held cannot rearm automatic opening");
    auto afterUi=r;afterUi.preview.generation++;
    Check(ui.Step(afterUi,1005,true)==RemoteAction::None,"a new bridge generation after manual UI cannot bypass the held-key fence");
    afterUi.preview.target.x++;
    Check(ui.Step(afterUi,1006,true)==RemoteAction::None,"changing aim after manual takeover cannot bypass the held-key fence");
    ui.Step(std::nullopt,1007,false);
    Check(ui.Step(afterUi,1008,true)==RemoteAction::Open,"physical release rearms after manual UI and a target change");
    RemoteSession bad;auto invalid=r;invalid.owner=0;
    Check(bad.Step(invalid,1000)==RemoteAction::None,"missing owner is ineligible");
    invalid=r;invalid.slots=55;
    Check(bad.Step(invalid,1000)==RemoteAction::None,"unbounded remote contents cannot start an operation");
    invalid=r;invalid.preview.target.kind=TargetKind::Player;
    Check(bad.Step(invalid,1000)==RemoteAction::None,"player previews are excluded");
    invalid=r;invalid.blockType=25;
    Check(bad.Step(invalid,1000)==RemoteAction::None,"first background candidate never admits a non-chest block");
    RemoteSession failedCopy;Activate(failedCopy,r);failedCopy.Applied(2,600,27,0x7FFFFFF);
    Check(failedCopy.Cancel()==RemoteAction::Close && failedCopy.Phase()==RemotePhase::Closing && !failedCopy.Visible(),"publication failure closes accepted ownership instead of abandoning an invisible model");
    Check(failedCopy.Cancel()==RemoteAction::None && failedCopy.Model()==600 && failedCopy.Control()==700,"repeated cancellation retains exact close attribution without another close");
    Check(failedCopy.CloseReply(400,1,2),"canceled copy retires only on its exact native close reply");
    RemoteSession canceledPending;canceledPending.Step(r,1000);
    Check(canceledPending.Cancel()==RemoteAction::None && canceledPending.Matches(o),"cancel before acceptance preserves the late-reply tombstone without sending an invalid close");
    canceledPending.Created(600,700);canceledPending.Accepted(2,600,700);
    Check(canceledPending.Step(r,1001,true)==RemoteAction::Close && !canceledPending.Visible(),"canceled pending response is closed even if a valid hold remains");
    RemoteSession reversed;reversed.Step(r,1000);reversed.Step(r,999,true);reversed.Created(600,700);reversed.Accepted(2,600,700);
    Check(reversed.Step(r,1001,true)==RemoteAction::Close,"clock reversal never makes a late accepted request visible");
    RemoteSession ender;auto er=r;er.blockType=23;
    Check(ender.Step(er,1000)==RemoteAction::Open,"own Ender Chest admits one native background request");
    Check(ender.Matches(MakeOpen(er)) && ender.Created(800,900) && ender.Accepted(3,800,900),"Ender uses the standard type-zero accepted container envelope");
    Check(!ender.Applied(3,800,54,0x3FFFFFFFFFFFFF) && ender.Applied(3,800,27,0x7FFFFFF) && ender.Visible(),"Ender content requires exactly 27 fully applied slots");
    RemoteSession invalidEnder;er.slots=54;
    Check(invalidEnder.Step(er,1000)==RemoteAction::None,"an Ender Chest cannot borrow a double-chest shape");
    // Rejecting all entity targets is the missing feature; accepting all entity
    // replies would instead break block attribution and the experiment fence.
    auto proxy=r;proxy.preview.target.kind=TargetKind::Entity;
    proxy.preview.target.entity=900;proxy.preview.target.entityRuntime=901;
    proxy.blockType=0;proxy.hiveProxy=true;
    RemoteSession hive;auto reply=MakeOpen(proxy);reply.entity=999;
    Check(hive.Step(proxy,1000)==RemoteAction::Open && hive.Matches(reply),"explicit remote death-crate request accepts a dynamic server owner distinct from target");
    auto rejected=reply;rejected.entity=0;Check(!hive.Matches(rejected),"zero returned entity cannot claim proxy session");
    rejected=reply;rejected.entity=UINT64_MAX;Check(!hive.Matches(rejected),"block sentinel cannot claim proxy session");
    rejected=reply;rejected.entity=proxy.preview.target.unique;Check(!hive.Matches(rejected),"local player UID cannot be a proxy owner");
    rejected=reply;rejected.target.world++;Check(!hive.Matches(rejected),"proxy acceptance retains exact world attribution");
    rejected=reply;rejected.mayOpen=false;Check(!hive.Matches(rejected),"proxy native denial is never overridden");
    Check(hive.Created(600,700,999) && hive.Entity()==999 && hive.RequestValue().preview.target.entity==900,"model creation records proxy without rewriting aimed UID");
    Check(hive.Accepted(2,600,700) && !hive.Visible() && hive.Applied(2,600,27,0x7FFFFFF) && hive.Visible(),"proxy requires full native slot application before display");
    hive.Relinquish();Check(!hive.Matches(reply) && hive.Step(proxy,1001,true)==RemoteAction::None,"manual takeover cannot absorb a delayed proxy reply or restart held request");
    RemoteSession proxyLate;proxyLate.Step(proxy,1000);proxyLate.Step(std::nullopt,1001);
    Check(proxyLate.Matches(reply) && proxyLate.Created(600,700,999) && proxyLate.Accepted(2,600,700) && proxyLate.Step(proxy,1002,true)==RemoteAction::Close && !proxyLate.Visible(),"canceled proxy reply is retired without showing contents");
    RemoteSession proxyBad;auto malformed=proxy;malformed.hiveProxy=false;
    Check(proxyBad.Step(malformed,1000)==RemoteAction::None,"generic entities cannot authorize automatic use");
    malformed=proxy;malformed.integrated=true;Check(proxyBad.Step(malformed,1000)==RemoteAction::None,"Hive experiment cannot open integrated entities");
    malformed=proxy;malformed.slots=54;Check(proxyBad.Step(malformed,1000)==RemoteAction::None,"proxy experiment admits only 27 slots");
    malformed=proxy;malformed.preview.target.entity=0;Check(proxyBad.Step(malformed,1000)==RemoteAction::None,"missing aimed actor identity fails closed");
    RemoteSession reuse;reuse.Step(proxy,1000);reuse.Created(600,700,999);reuse.Accepted(2,600,700);reuse.Applied(2,600,27,0x7FFFFFF);
    auto another=proxy;another.preview.target.entity=902;another.preview.target.entityRuntime=903;another.preview.generation++;
    Check(reuse.Step(another,1001,true)==RemoteAction::Close && !reuse.Visible(),"switching crates hides prior proxy contents before closing");
    Check(reuse.Step(another,1002,true)==RemoteAction::None,"shared owner cannot start overlapping crate operations");
    reuse.CloseReply(400,1,2);auto nextProxy=MakeOpen(another);nextProxy.entity=999;
    Check(reuse.Step(another,1003,true)==RemoteAction::Open && reuse.Matches(nextProxy) && reuse.Created(601,701,999),"native proxy UID may be reused only after exact previous session retirement");
    // Catches an input recheck taking the manual takeover fence even though
    // the native entry was never called. The adapter must prove no submission.
    RemoteSession paused;
    Check(!paused.DeferUnsubmitted(),"idle session cannot retire a nonexistent reservation");
    paused.Step(r,1000,true);
    Check(paused.DeferUnsubmitted() && paused.Phase()==RemotePhase::Idle && !paused.Matches(o),"pre-dispatch input pause drops only the unsubmitted reservation");
    Check(paused.Step(r,1001,true)==RemoteAction::Open,"standing after unsubmitted pause rearms the same hold without a manual UI");
    paused.Created(600,700);
    Check(!paused.DeferUnsubmitted() && paused.Model()==600 && paused.Matches(o)==false,"a created pending model cannot use pre-dispatch recovery");
    paused.Accepted(2,600,700);
    Check(!paused.DeferUnsubmitted() && paused.Phase()==RemotePhase::Active,"accepted session cannot use pre-dispatch recovery");
    paused.Step(std::nullopt,1002,true);
    Check(!paused.DeferUnsubmitted() && paused.Phase()==RemotePhase::Closing,"closing ownership cannot use pre-dispatch recovery");
    RemoteSession issued;
    Check(!issued.BeginNativeUse(),"idle operation cannot be marked submitted");
    issued.Step(r,1000,true);
    Check(issued.BeginNativeUse() && !issued.BeginNativeUse(),"the exact pending operation enters native use only once");
    Check(!issued.DeferUnsubmitted() && issued.Matches(o),"native entry without immediate reply cannot use unsubmitted recovery");
    issued.Step(std::nullopt,5000,false);
    Check(!issued.DeferUnsubmitted() && issued.Matches(o) && issued.Step(r,5001,true)==RemoteAction::None,"release and timeout do not erase submitted request attribution");
    issued.Relinquish();issued.Step(std::nullopt,5002,false);issued.Step(r,5003,true);
    Check(issued.DeferUnsubmitted(),"new reservation resets native-start tracking after explicit relinquish and release");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
