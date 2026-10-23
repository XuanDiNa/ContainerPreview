#include "ContainerPreview/RemoteSession.h"
namespace preview {
namespace {
bool Valid(RemoteRequest const& r){
    auto const& t=r.preview.target;
    return r.owner && r.client && r.player && r.preview.generation && t.world && t.unique && t.runtime &&
        ((t.kind==TargetKind::Block && !r.hiveProxy && ((r.blockType==2 && (r.slots==27 || r.slots==54)) || (r.blockType==23 && r.slots==27))) ||
         (t.kind==TargetKind::Entity && r.hiveProxy && !r.integrated && r.blockType==0 && r.slots==27 &&
          t.entity && t.entityRuntime && t.entity!=UINT64_MAX && t.entityRuntime!=UINT64_MAX && t.entity!=t.unique));
}
bool Same(RemoteRequest const& a,RemoteRequest const& b){
    return a.preview.target==b.preview.target && a.preview.generation==b.preview.generation &&
        a.owner==b.owner && a.client==b.client && a.player==b.player && a.slots==b.slots && a.blockType==b.blockType && a.integrated==b.integrated && a.hiveProxy==b.hiveProxy;
}
bool AcceptedId(unsigned id){return id>0 && id<=100;}
}
RemoteAction RemoteSession::Step(std::optional<RemoteRequest> const& desired,std::uint64_t now,bool held){
    bool valid=desired && Valid(*desired);
    if(!valid && !held){attempted_.reset();manualFence_=false;}
    if(phase_!=RemotePhase::Idle){
        wanted_=wanted_ && valid && Same(request_,*desired);
        if(phase_==RemotePhase::Pending && (now<since_ || now-since_>3000))wanted_=false;
        if(phase_==RemotePhase::Active && !wanted_){phase_=RemotePhase::Closing;ready_=false;return RemoteAction::Close;}
        return RemoteAction::None;
    }
    if(!valid || manualFence_ || (attempted_ && attempted_->target==desired->preview.target && attempted_->generation==desired->preview.generation))return RemoteAction::None;
    request_=*desired;attempted_=desired->preview;since_=now;wanted_=true;ready_=nativeStarted_=false;
    model_=control_=0;id_=255;entity_=UINT64_MAX;phase_=RemotePhase::Pending;return RemoteAction::Open;
}
bool RemoteSession::Matches(RemoteOpen const& o) const{
    bool entity=request_.hiveProxy?(o.entity && o.entity!=UINT64_MAX && o.entity!=request_.preview.target.unique):o.entity==UINT64_MAX;
    return phase_==RemotePhase::Pending && !model_ && o.mayOpen && AcceptedId(o.id) && o.type==0 && entity &&
        o.target==request_.preview.target && o.owner==request_.owner && o.client==request_.client && o.player==request_.player;
}
bool RemoteSession::Created(std::uintptr_t model,std::uintptr_t control,std::uint64_t entity){
    if(phase_!=RemotePhase::Pending || model_ || !model || !control ||
       (request_.hiveProxy?(!entity || entity==UINT64_MAX || entity==request_.preview.target.unique):entity!=UINT64_MAX))return false;
    model_=model;control_=control;entity_=entity;return true;
}
bool RemoteSession::Accepted(unsigned id,std::uintptr_t model,std::uintptr_t control){
    if(phase_!=RemotePhase::Pending || !AcceptedId(id) || !model_ || model!=model_ || control!=control_)return false;
    id_=id;phase_=RemotePhase::Active;return true;
}
bool RemoteSession::Applied(unsigned id,std::uintptr_t model,unsigned count,std::uint64_t mask){
    if(phase_!=RemotePhase::Active || id!=id_ || model!=model_ || count!=request_.slots ||
       mask!=((UINT64_C(1)<<request_.slots)-1))return false;
    ready_=true;return true;
}
bool RemoteSession::CloseReply(std::uintptr_t client,std::uint32_t owner,unsigned id){
    if((phase_!=RemotePhase::Closing && phase_!=RemotePhase::Active) || client!=request_.client || owner!=request_.owner || id!=id_)return false;
    phase_=RemotePhase::Idle;model_=control_=0;ready_=wanted_=false;return true;
}
void RemoteSession::Relinquish(){phase_=RemotePhase::Idle;model_=control_=0;ready_=wanted_=false;manualFence_=true;}
bool RemoteSession::BeginNativeUse(){
    if(phase_!=RemotePhase::Pending || nativeStarted_ || !wanted_ || model_ || control_ || id_!=255)return false;
    nativeStarted_=true;return true;
}
bool RemoteSession::DeferUnsubmitted(){
    if(phase_!=RemotePhase::Pending || nativeStarted_ || model_ || control_ || id_!=255 || entity_!=UINT64_MAX)return false;
    phase_=RemotePhase::Idle;request_={};attempted_.reset();since_=0;wanted_=ready_=false;
    return true;
}
RemoteAction RemoteSession::Cancel(){
    wanted_=ready_=false;
    if(phase_!=RemotePhase::Active)return RemoteAction::None;
    phase_=RemotePhase::Closing;return RemoteAction::Close;
}
}
