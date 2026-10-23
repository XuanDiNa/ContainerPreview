#include "ContainerPreview/PreviewModel.h"
#include <algorithm>
#include <cmath>
namespace preview {
bool HasPreviewSource(Snapshot const& s){
    if(s.pairedServer)return s.remote==RemoteLease{};
    return s.remote.Valid();
}
namespace {
bool Fresh(std::uint64_t now,std::uint64_t then) { return now>=then && now-then<=500; }
}
std::optional<Grid> LayoutGrid(unsigned slots, float width, float height) {
    if(slots<1 || slots>54 || !std::isfinite(width) || !std::isfinite(height) || width<32 || height<32)return {};
    unsigned columns=std::min(slots,9u), rows=(slots+columns-1)/columns;
    float cell=std::min({20.f,(width-16.f)/static_cast<float>(columns),(height-32.f)/static_cast<float>(rows)});
    if(cell<8)return {};
    float panelWidth=std::max(cell*static_cast<float>(columns)+8,std::min(64.f,width-8)), panelHeight=cell*static_cast<float>(rows)+20;
    float x=(width-panelWidth)*.5f,y=std::min(height-panelHeight-4,height*.5f+12);
    return Grid{x,std::max(4.f,y),panelWidth,panelHeight,cell,columns,rows};
}
void PreviewModel::Update(Config const& config, Eligibility e, Target const& target, std::uint64_t now) {
    bool valid=config.enabled && e.foreground && e.hud && e.held && e.local &&
        target.world && target.unique && target.runtime &&
        (target.kind==TargetKind::Block || (target.kind==TargetKind::Entity && target.entity && target.entityRuntime));
    if(!valid) { if(request_){request_.reset();++generation_;}return; }
    if(!request_ || request_->target!=target || !Fresh(now,request_->time))++generation_;
    request_=Request{target,generation_,now};
}
std::optional<Request> PreviewModel::Current(std::uint64_t now) const {
    return request_ && Fresh(now,request_->time) ? request_ : std::nullopt;
}
Display PreviewModel::View(Snapshot const& snapshot, std::uint64_t now) const {
    auto request=Current(now);
    if(!request)return Display::Hidden;
    if(HasPreviewSource(snapshot) && snapshot.target==request->target && snapshot.generation==request->generation &&
       Fresh(now,snapshot.time) && snapshot.contents==Contents::Absent)return Display::Hidden;
    if(!HasPreviewSource(snapshot) || snapshot.target!=request->target || snapshot.generation!=request->generation ||
       !Fresh(now,snapshot.time) || snapshot.contents!=Contents::Ready || snapshot.slots<1 || snapshot.slots>54)return Display::Waiting;
    return Display::Grid;
}
}
