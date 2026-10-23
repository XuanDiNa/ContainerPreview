#include "ContainerPreview/PreviewBridge.h"
namespace preview {
namespace {bool SameRequest(Request const& a,Request const& b){return a.target==b.target && a.generation==b.generation;}}
bool PreviewBridge::BeginBackground(PreviewWork const& work,std::uint32_t owner,std::uint64_t now){
    std::lock_guard lock(mutex_);auto r=model_.Current(now);
    if(!r || !owner || reservationOwner_ || remote_.Valid() || !SameRequest(*r,work.request) ||
       local_.registry!=work.local.registry || local_.unique!=work.local.unique || local_.runtime!=work.local.runtime || !local_.local || local_.server)return false;
    reservation_=*r;reservationOwner_=owner;snapshot_={};return true;
}
bool PreviewBridge::AuthorizeRemote(PreviewWork const& work,RemoteLease lease,std::uint64_t now){
    std::lock_guard lock(mutex_);auto r=model_.Current(now);
    if(!r || !lease.Valid() || r->target!=work.request.target || r->generation!=work.request.generation ||
       local_.registry!=work.local.registry || local_.unique!=work.local.unique || local_.runtime!=work.local.runtime ||
       !local_.local || local_.server || (remote_.Valid() && remote_!=lease) ||
       (reservationOwner_ && (reservationOwner_!=lease.owner || !SameRequest(reservation_,*r))))return false;
    remote_=lease;remoteRequest_=*r;reservation_=*r;reservationOwner_=lease.owner;return true;
}
bool PreviewBridge::PublishRemote(PreviewWork const& work,RemoteLease lease,BorrowedContents const& contents,std::uint32_t thread,std::uint64_t now){
    std::lock_guard lock(mutex_);auto r=model_.Current(now);
    if(!r || !lease.Valid() || remote_!=lease || lease.owner!=thread || r->target!=remoteRequest_.target || r->generation!=remoteRequest_.generation ||
       r->target!=work.request.target || r->generation!=work.request.generation || local_.registry!=work.local.registry ||
       !contents.ready || !contents.slots || contents.slots>54)return false;
    if(!CopyContents(contents,r->target.world,thread))return false;
    snapshot_={r->target,r->generation,now,false,Contents::Ready,contents.slots,lease};return true;
}
void PreviewBridge::RevokeRemote(){std::lock_guard lock(mutex_);remote_={};remoteRequest_={};reservation_={};reservationOwner_=0;if(snapshot_.remote.Valid())snapshot_={};}
void PreviewBridge::Update(Config const& config,Eligibility e,Target const& target,ActorIdentity local,std::uint64_t now){
    std::lock_guard lock(mutex_);
    if(local_.registry!=local.registry || local_.unique!=local.unique || local_.runtime!=local.runtime){
        pairedRequest_={};pairedTime_=0;backgroundType_=0;remote_={};remoteRequest_={};reservation_={};reservationOwner_=0;snapshot_={};
    }
    e.local=e.local && local.registry && local.local && !local.server && local.unique==target.unique && local.runtime==target.runtime;
    model_.Update(config,e,target,now);local_=local;
    auto r=model_.Current(now);
    if(!r || r->target!=remoteRequest_.target || r->generation!=remoteRequest_.generation){remote_={};remoteRequest_={};}
    if(!r || !SameRequest(*r,reservation_)){reservation_={};reservationOwner_=0;}
}
std::optional<PreviewWork> PreviewBridge::Work(std::uint64_t now){
    std::lock_guard lock(mutex_);auto request=model_.Current(now);
    bool paired=request && SameRequest(*request,pairedRequest_) && now>=pairedTime_ && now-pairedTime_<=500;
    return request?std::optional<PreviewWork>{{*request,local_,paired,paired?backgroundType_:0}}:std::nullopt;
}
bool PreviewBridge::Publish(PreviewWork const& work,ActorIdentity server,std::uintptr_t world,BorrowedContents const& contents,std::uint32_t thread,std::uint64_t now){
    std::lock_guard lock(mutex_);auto request=model_.Current(now);
    if(!request || !thread || !world || request->generation!=work.request.generation || request->target!=work.request.target ||
       local_.registry!=work.local.registry || !SameIntegratedPlayer(local_,server) ||
       (contents.ready && (!contents.slots || contents.slots>54)))return false;
    pairedRequest_=*request;pairedTime_=now;
    backgroundType_=!contents.ready && contents.recognized && (contents.backgroundType==2 || contents.backgroundType==23)?contents.backgroundType:0;
    if(remote_.Valid() || (reservationOwner_ && SameRequest(reservation_,*request)))return false;
    if(!CopyContents(contents,world,thread))return false;
    snapshot_={request->target,request->generation,now,true,contents.ready?Contents::Ready:(contents.recognized?Contents::Unknown:Contents::Absent),contents.ready?contents.slots:0};
    return true;
}
bool PreviewBridge::CopyContents(BorrowedContents const& contents,std::uintptr_t world,std::uint32_t thread){
    if(active_.stacks && (active_.world!=world || active_.thread!=thread)){
        if(active_.thread==thread && active_.stacks->Clear(thread))active_={};
        else {
            Pool* spare=nullptr;for(auto& pool:retired_)if(!pool.stacks){spare=&pool;break;}
            if(!spare)return false;
            *spare=std::move(active_);active_={};
        }
    }
    if(!active_.stacks)active_={world,thread,std::make_unique<OwnedStacks>(ops_)};
    snapshot_={};
    bool copied=contents.ready?active_.stacks->Replace({contents.items.data(),contents.slots},thread):active_.stacks->Clear(thread);
    if(!copied)return false;
    return true;
}
void PreviewBridge::Cleanup(std::uint32_t thread,std::uint64_t now){
    std::lock_guard lock(mutex_);
    for(auto& pool:retired_)if(pool.stacks && pool.thread==thread && pool.stacks->Clear(thread))pool={};
    if(!model_.Current(now) && active_.stacks && active_.thread==thread && active_.stacks->Clear(thread)){active_={};snapshot_={};}
}
void PreviewBridge::View(std::uint64_t now,std::function<void(Display,Snapshot const&,OwnedStacks const&)> const& draw){
    std::lock_guard lock(mutex_);
    if(active_.stacks && !active_.stacks->Faulted()){
        auto request=model_.Current(now);
        bool source=snapshot_.pairedServer || (remote_.Valid() && snapshot_.remote==remote_);
        bool current=request && source && HasPreviewSource(snapshot_) && request->target==snapshot_.target && request->generation==snapshot_.generation && now>=snapshot_.time && now-snapshot_.time<=500;
        draw(current?model_.View(snapshot_,now):Display::Hidden,snapshot_,*active_.stacks);
    }
}
}
