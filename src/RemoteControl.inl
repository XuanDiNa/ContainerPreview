// Included after the exact native trampoline declarations. Session state is
// confined to the first admitted client callback owner, never the log worker.
RemoteSession remoteSession;
std::atomic<std::uint32_t> remoteOwner{};
std::atomic<std::uint64_t> remoteOpens{},remoteCreates{},remoteAccepts{},remoteFull{},remoteCopies{},remoteCloses{},remoteManual{},remoteConsistency{};
thread_local bool remoteCallingUse{},remoteCreating{},remoteClosing{};
struct RemoteDeferredFrame {RemoteOpen open;BlockPosition position;bool valid{},consumed{};};
thread_local RemoteDeferredFrame* remoteDeferred{};
struct RemoteContentFrame {RemoteLease lease;unsigned count{};std::uint64_t mask{};bool valid{};};
thread_local RemoteContentFrame* remoteContent{};
RemoteGateBudget remoteGateBudget;
void RemoteGateAt(RemoteGateSample& sample,RemoteGate gate){sample.gate=gate;sample.check={};}
void RecordRemoteGate(RemoteGateSample sample,bool physicalHeld){
    if(!physicalHeld || !Settings().enabled || !Foreground() || !diagnosticBuffer)return;
    sample.phase=static_cast<unsigned>(remoteSession.Phase());
    if(!remoteGateBudget.Admit(sample))return;
    DiagnosticScope scope(TraceStage::RemoteGate);TraceRecord value{};
    value.client=sample.client;value.world=sample.world;value.x=sample.x;value.y=sample.y;value.z=sample.z;
    value.remoteGate=sample;TraceRecordValue(scope,value,TracePhase::Sample);
}
template<class T>struct RemoteFrameScope {
    T*& current;T* previous;int exceptions=std::uncaught_exceptions();
    RemoteFrameScope(T*& slot,T* next):current(slot),previous(slot){current=next;}
    ~RemoteFrameScope(){current=previous;if(std::uncaught_exceptions()>exceptions)nativeFault.store(true);}
};
struct RemoteFlag {bool& flag;bool old;RemoteFlag(bool& f):flag(f),old(f){flag=true;}~RemoteFlag(){flag=old;}};
template<class T>bool RemoteRead(void const* p,unsigned offset,T& v){
    __try {if(!p)return false;std::memcpy(&v,static_cast<std::byte const*>(p)+offset,sizeof v);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool RemoteOnOwner(){return status.load(std::memory_order_acquire)==1 && remoteOwner.load(std::memory_order_acquire)==Owner();}
RegistryOps RemoteRegistry(){return {profile.Function<void*(*)(void*,void*)>(0x5DD4520),profile.Function<void*(*)(void*,void*)>(0x3489AD0),profile.Function<void(*)(void*)>(0x3489C20)};}
RemoteLease CurrentRemoteLease(){auto const& r=remoteSession.RequestValue();return {r.client,r.player,remoteSession.Model(),remoteSession.Control(),r.owner,remoteSession.Id()};}
void RemoteRelinquish(){if(!RemoteOnOwner())return;bridge->RevokeRemote();remoteSession.Relinquish();}
bool RemoteLive(RemoteRequest const& r,RemoteCheck* check=nullptr){
    __try {
        auto client=reinterpret_cast<void*>(r.client);
        if(!profile.Table(client,0xE9731B0) || r.owner!=Owner())return false;
        auto world=profile.Function<void*(*)(void*)>(0x5DC2630)(client);
        auto player=profile.Function<void*(*)(void*)>(0x5DAED30)(client);
        if(reinterpret_cast<std::uintptr_t>(world)!=r.preview.target.world || reinterpret_cast<std::uintptr_t>(player)!=r.player)return false;
        auto id=ReadIdentity(player);
        auto route=DecodeRemoteRoute(client,profile.Base(),check);
        return id.local && !id.server && id.unique==r.preview.target.unique && id.runtime==r.preview.target.runtime && route && route->integrated==r.integrated;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool RemoteCloseOperation(void*){
    auto const& r=remoteSession.RequestValue();if(!RemoteLive(r))return false;
    auto model=DecodeRemoteModel(reinterpret_cast<void*>(r.player),profile.Base(),r.preview.target,r.blockType,remoteSession.Entity());
    if(!model || model->model!=remoteSession.Model() || model->control!=remoteSession.Control() || model->id!=remoteSession.Id())return false;
    RemoteFlag closing(remoteClosing);++remoteCloses;
    closePlayerOriginal(reinterpret_cast<void*>(r.player));return true;
}
void RemoteCloseOwned(){
    bridge->RevokeRemote();auto const& r=remoteSession.RequestValue();
    if(!RemoteLive(r) || !InNativeRegistry(RemoteRegistry(),reinterpret_cast<void*>(r.client),RemoteCloseOperation,nullptr))RemoteRelinquish();
}
bool RemoteCopyOperation(void*){
    auto const& r=remoteSession.RequestValue();if(!remoteSession.Visible() || !RemoteLive(r))return false;
    auto model=DecodeRemoteModel(reinterpret_cast<void*>(r.player),profile.Base(),r.preview.target,r.blockType,remoteSession.Entity());
    if(!model || model->model!=remoteSession.Model() || model->control!=remoteSession.Control() || model->id!=remoteSession.Id())return false;
    auto now=GetTickCount64();auto work=bridge->Work(now);
    if(!work || work->request.generation!=r.preview.generation || work->request.target!=r.preview.target)return false;
    BorrowedContents contents{};contents.ready=contents.recognized=true;contents.slots=r.slots;
    for(unsigned i=0;i<r.slots;++i){
        contents.items[i]=profile.Function<void const*(*)(void*,unsigned)>(0x343EC10)(reinterpret_cast<void*>(model->model),i);
        if(!contents.items[i])return false;
    }
    auto copied=bridge->PublishRemote(*work,CurrentRemoteLease(),contents,r.owner,now);if(copied)++remoteCopies;return copied;
}
void RemoteCopy(){
    if(!remoteSession.Visible())return;
    auto client=reinterpret_cast<void*>(remoteSession.RequestValue().client);
    if(!InNativeRegistry(RemoteRegistry(),client,RemoteCopyOperation,nullptr)){
        bridge->RevokeRemote();
        if(remoteSession.Cancel()==RemoteAction::Close)RemoteCloseOwned();
    }
}
struct RemoteUseContext {void* client{};RemoteRequest request;bool submitted{};RemoteGateSample sample;};
bool RemoteUseOperation(void* state){
    auto& c=*static_cast<RemoteUseContext*>(state);auto const& r=c.request;auto& sample=c.sample;
    RemoteGateAt(sample,RemoteGate::Live);if(!RemoteLive(r,&sample.check))return false;
    RemoteGateAt(sample,RemoteGate::UseHud);if(!NativeHud(profile,c.client))return false;
    RemoteGateAt(sample,RemoteGate::UseInput);
    if(!BackgroundInputAllowed(reinterpret_cast<void*>(r.player),ShiftHeld(),&sample.check))return false;
    void *source=profile.Function<void*(*)(void*)>(0x5DAEBB0)(c.client),*mode{};
    RemoteGateAt(sample,RemoteGate::UseRegion);if(!profile.Table(source,0xE8554E0))return false;
    RemoteGateAt(sample,RemoteGate::ModeRead);if(!RemoteRead(reinterpret_cast<void*>(r.player),2720,mode))return false;
    auto hit=profile.Function<void*(*)(void*)>(0x117D580)(reinterpret_cast<void*>(r.preview.target.world));
    RemoteGateAt(sample,RemoteGate::HandGetter);
    auto handGetter=profile.Virtual(reinterpret_cast<void*>(r.player),616);if(!handGetter)return false;
    auto hand=reinterpret_cast<void*(*)(void*)>(handGetter)(reinterpret_cast<void*>(r.player));RemoteUse interaction{};
    // Exact read-only209D80 resolves the player's current component/default,
    // not the world's mode alone or a table shared with spectator.
    auto gameType=profile.Function<int(*)(void*)>(0x209D80)(reinterpret_cast<void*>(r.player));
    RemoteGateAt(sample,RemoteGate::Use);
    if(r.hiveProxy){
        if(r.integrated)return false;
        // This pointer exists only in the native registry callback. Re-resolve
        // from the current hit, never retain an Actor across a frame/tick.
        auto actor=profile.Function<void*(*)(void*)>(0x3529DB0)(hit);
        if(!DecodeRemoteEntityUse(mode,reinterpret_cast<void*>(r.player),hit,hand,actor,profile.Base(),r.preview.target,gameType,&sample.check,true))return false;
        // Exact table/slot was independently checked against the guarded disk
        // image. Do not call Virtual(): its entry-byte check would reject our
        // own installed planner detour. Hook-set readiness already guarantees
        // the initial entry guard and complete owned protection installation.
        std::uintptr_t planner{};
        if(!profile.Table(actor,0xE6AF850) || !RemoteRead(reinterpret_cast<void*>(profile.Base()+Rva(0xE6AF850)),CurrentVersion()==GameVersion::V45?936:928,planner) || planner!=profile.Base()+Rva(0x1A2B270)){sample.check={RemoteReason::HeldItemUnsupported,0};return false;}
        auto currentType=profile.Function<int(*)(void*)>(0x209D80)(reinterpret_cast<void*>(r.player));
        if(currentType!=gameType || !DecodeRemoteEntityUse(mode,reinterpret_cast<void*>(r.player),hit,hand,actor,profile.Base(),r.preview.target,currentType,&sample.check,true))return false;
        RemoteFlag use(remoteCallingUse);
        EntityUseScope heldGuard(profile.Base(),reinterpret_cast<void*>(r.player),actor,hand,true);
        if(!remoteSession.BeginNativeUse())return false;
        c.submitted=true;++remoteOpens;
        auto entry=gameType==0?profile.Function<RemoteEntityUseEntry>(0x25A0590):EntityUseHook;
        if(!InvokeRemoteEntityUse(entry,mode,actor,static_cast<std::byte const*>(hit)+44)){
            sample.check={RemoteReason::ReadFault,0};nativeFault.store(true);return false;
        }
        RemoteGateAt(sample,RemoteGate::Submitted);sample.check={RemoteReason::Pass,0};return true;
    }
    if(!DecodeRemoteUse(mode,reinterpret_cast<void*>(r.player),hit,hand,profile.Base(),r.preview.target,gameType,interaction,&sample.check,true))return false;
    BlockPosition pos{r.preview.target.x,r.preview.target.y,r.preview.target.z};
    auto block=profile.Function<void*(*)(void*,BlockPosition const*)>(0x2D80310)(source,&pos);
    RemoteGateAt(sample,RemoteGate::UseChest);
    auto shape=DecodeRemoteChest(block,profile.Base(),pos,&sample.check);
    if(!shape || shape->slots!=r.slots || shape->blockType!=r.blockType)return false;
    // Refuse a mode change observed while validating the native chest shape.
    RemoteGateAt(sample,RemoteGate::Use);
    auto currentType=profile.Function<int(*)(void*)>(0x209D80)(reinterpret_cast<void*>(r.player));
    if(currentType!=gameType){sample.check={RemoteReason::GameType,static_cast<unsigned>(currentType)};return false;}
    if(!DecodeRemoteUse(mode,reinterpret_cast<void*>(r.player),hit,hand,profile.Base(),r.preview.target,currentType,interaction,&sample.check,true))return false;
    if(interaction.item && profile.Virtual(reinterpret_cast<void*>(interaction.item),144)!=profile.Base()+Rva(0x76C20)){sample.check={RemoteReason::HeldItemUnsupported,0};return false;}
    // Preserve survival's wrapper. Creative/adventure use the base virtual48
    // entry, still through BlockUseHook and ordinary native transaction gates.
    //26AFFB0 invokes25B4E70 synchronously before returning. TLS filters are
    // confined to this owned native use; they never replace the held stack,
    // generate a transaction or turn a rejected use into a success result.
    RemoteFlag use(remoteCallingUse);HeldUseScope heldGuard(profile.Base(),reinterpret_cast<void*>(r.player),true);
    if(!remoteSession.BeginNativeUse())return false;
    c.submitted=true;++remoteOpens;
    if(!InvokeRemoteUse(profile.Function<RemoteUseEntry>(interaction.entry),mode,&pos,interaction.face)){
        sample.check={RemoteReason::ReadFault,0};nativeFault.store(true);return false;
    }
    RemoteGateAt(sample,RemoteGate::Submitted);sample.check={RemoteReason::Pass,0};return true;
}
std::optional<RemoteRequest> RemoteDesired(void* client,ClientObservation const& observation,std::uint64_t now,RemoteGateSample& sample){
    __try {
        RemoteGateAt(sample,RemoteGate::Hud);if(!observation.hud)return {};
        RemoteGateAt(sample,RemoteGate::BlockTarget);if(observation.target.kind!=TargetKind::Block && observation.target.kind!=TargetKind::Entity)return {};
        RemoteGateAt(sample,RemoteGate::Route);auto route=DecodeRemoteRoute(client,profile.Base(),&sample.check);if(!route)return {};
        RemoteGateAt(sample,RemoteGate::Work);auto work=bridge->Work(now);if(!work || work->request.target!=observation.target)return {};
        sample.generation=work->request.generation;
        auto player=profile.Function<void*(*)(void*)>(0x5DAED30)(client);
        auto source=profile.Function<void*(*)(void*)>(0x5DAEBB0)(client);
        RemoteGateAt(sample,RemoteGate::Region);
        if(!profile.Table(source,0xE8554E0))return {};
        RemoteGateAt(sample,RemoteGate::PlayerBinding);
        void* current{};void* control{};if(!RemoteRead(player,1440,current) || !RemoteRead(player,1448,control))return {};
        RemoteGateAt(sample,RemoteGate::CurrentModel);
        if(remoteSession.Phase()==RemotePhase::Idle && (current || control))return {};
        if(remoteSession.Phase()==RemotePhase::Idle){
            RemoteGateAt(sample,RemoteGate::UseInput);
            if(!BackgroundInputAllowed(player,ShiftHeld(),&sample.check))return {};
        }
        if(observation.target.kind==TargetKind::Entity){
            if(route->integrated)return {};
            auto hit=profile.Function<void*(*)(void*)>(0x117D580)(reinterpret_cast<void*>(observation.target.world));
            auto actor=profile.Function<void*(*)(void*)>(0x3529DB0)(hit);
            if(!IsHiveDeathCrate(actor))return {};
            auto id=ReadIdentity(actor);
            if(id.unique!=observation.target.entity || id.runtime!=observation.target.entityRuntime)return {};
            return RemoteRequest{work->request,Owner(),reinterpret_cast<std::uintptr_t>(client),reinterpret_cast<std::uintptr_t>(player),27,0,false,true};
        }
        BlockPosition pos{observation.target.x,observation.target.y,observation.target.z};
        auto block=profile.Function<void*(*)(void*,BlockPosition const*)>(0x2D80310)(source,&pos);
        RemoteGateAt(sample,RemoteGate::Chest);
        auto shape=DecodeRemoteChest(block,profile.Base(),pos,&sample.check);if(!shape)return {};
        RemoteGateAt(sample,RemoteGate::Policy);
        auto owned=remoteSession.Phase()!=RemotePhase::Idle?&remoteSession.RequestValue():nullptr;
        if(!BackgroundEligible(route->integrated,*work,*shape,owned))return {};
        return RemoteRequest{work->request,Owner(),reinterpret_cast<std::uintptr_t>(client),reinterpret_cast<std::uintptr_t>(player),shape->slots,shape->blockType,route->integrated};
    }__except(EXCEPTION_EXECUTE_HANDLER){sample.check={RemoteReason::ReadFault,0};return {};}
}
void RemoteTick(void* client,ClientObservation const& observation,bool physicalHeld,std::uint64_t now){
    auto owner=remoteOwner.load(std::memory_order_acquire);
    if(owner && owner!=Owner())return;
    if(!owner){if(!observation.hud)return;std::uint32_t empty{};remoteOwner.compare_exchange_strong(empty,Owner(),std::memory_order_acq_rel);if(!RemoteOnOwner())return;}
    bridge->Cleanup(Owner(),now);
    if(remoteSession.Phase()!=RemotePhase::Idle && !RemoteLive(remoteSession.RequestValue()))RemoteRelinquish();
    auto const& target=observation.target;
    RemoteGateSample sample{};sample.client=reinterpret_cast<std::uintptr_t>(client);sample.world=target.world;
    sample.x=target.x;sample.y=target.y;sample.z=target.z;
    auto desired=RemoteDesired(client,observation,now,sample);
    auto action=remoteSession.Step(desired,now,physicalHeld);
    sample.action=static_cast<unsigned>(action);
    if(action==RemoteAction::Close)RemoteCloseOwned();
    else if(action==RemoteAction::Open){
        RemoteGateAt(sample,RemoteGate::Registry);RemoteUseContext c{client,*desired,false,sample};RemoteCheck registryCheck{};
        auto work=bridge->Work(now);
        if(!work || !bridge->BeginBackground(*work,Owner(),now)){RemoteRelinquish();return;}
        if(!InNativeRegistry(RemoteRegistry(),client,RemoteUseOperation,&c,&registryCheck)){
            if(registryCheck.reason!=RemoteReason::RegistryRefused || c.sample.gate==RemoteGate::Registry)c.sample.check=registryCheck;
            if(!c.submitted){
                // Only this pre-entry pause can be rearmed within the same
                // hold. A completed native call (even return0), timeout or
                // possible late reply must retain ordinary attribution.
                bool paused=c.sample.gate==RemoteGate::UseInput &&
                    (c.sample.check.reason==RemoteReason::ShiftHeld || c.sample.check.reason==RemoteReason::Sneaking);
                if(!paused || !remoteSession.DeferUnsubmitted())RemoteRelinquish();
            }
        }
        sample=c.sample;
    }
    if(remoteSession.Phase()==RemotePhase::Idle)bridge->RevokeRemote();else if(remoteSession.Visible())RemoteCopy();
    RecordRemoteGate(sample,physicalHeld);
}
void RemoteManualUse(){
    if(!RemoteOnOwner() || remoteCallingUse || remoteSession.Phase()==RemotePhase::Idle)return;
    if(remoteSession.Phase()==RemotePhase::Active)RemoteCloseOwned();
    ++remoteManual;RemoteRelinquish();
}
RemoteDeferredFrame RemoteBeginDeferred(void* capture,void* region){
    RemoteDeferredFrame frame{};if(!RemoteOnOwner() || remoteDeferred)return frame;
    auto const& r=remoteSession.RequestValue();if(!RemoteLive(r))return frame;
    auto v=CaptureDeferred(capture,profile.Base());
    if((v.fields&(PacketFields|PositionFields|EntityField|MayOpenField|ReadFault))!=(PacketFields|PositionFields|EntityField|MayOpenField) ||
       v.player!=r.player || !profile.Table(region,0xE8554E0))return frame;
    std::uintptr_t level{};if(!RemoteRead(region,32,level) || level!=r.preview.target.world)return frame;
    int dimension=INT_MIN;profile.Function<void*(*)(void*,int*)>(0x2D7DDF0)(region,&dimension);
    auto t=r.preview.target;t.world=v.world;t.dimension=dimension;
    frame.position={v.x,v.y,v.z};
    if(!r.hiveProxy){t.x=v.x;t.y=v.y;t.z=v.z;}
    frame.open={t,Owner(),r.client,v.player,static_cast<unsigned>(v.container),static_cast<unsigned>(v.type),v.entity,v.mayOpen==1};
    frame.valid=remoteSession.Matches(frame.open);return frame;
}
using ChestCreator=void*(*)(void*,void*,void*,void*,void*,void const*,void const*);
ChestCreator creatorOriginal{};
using ChestCreator45=void*(*)(void*,void*,void*,void*,void*,void*,void*,void*,void const*,void const*);
ChestCreator45 creator45Original{};
struct RemoteCreateContext {void* player{};void const* position{};unsigned char type{};NativePair weak;std::uint64_t entity{UINT64_MAX};bool proxy{},attempted{};};
bool RemoteCreateOperation(void* state){
    auto& c=*static_cast<RemoteCreateContext*>(state);
    if(c.proxy){
        auto const& r=remoteSession.RequestValue();
        auto actor=actorLookupOriginal(reinterpret_cast<void*>(r.preview.target.world),c.entity,0);
        if(!MatchesHiveProxyOwner(actor,profile.Base(),r.preview.target,c.entity))return false;
    }
    RemoteFlag creating(remoteCreating);c.attempted=true;
    if(c.proxy)entityFactoryOriginal(&c.weak,c.player,&c.entity);
    else factoryOriginal(&c.weak,c.player,c.position,&c.type);
    return true;
}
// Called only after the ABI-specific hook validates its complete native frame.
// False means no factory was attempted, so ordinary forwarding remains safe.
bool TryRemoteCreate(void* out,void* player,void const* pos){
    auto frame=remoteDeferred;
    auto const& r=remoteSession.RequestValue();
    if(r.hiveProxy){
        BlockPosition actual{};
        if(!RemoteRead(pos,0,actual) || actual.x!=frame->position.x || actual.y!=frame->position.y || actual.z!=frame->position.z)
            return false;
    }
    RemoteCreateContext c{player,pos,static_cast<unsigned char>(remoteSession.RequestValue().blockType)};
    c.proxy=r.hiveProxy;c.entity=frame->open.entity;
    auto created=InNativeRegistry(RemoteRegistry(),reinterpret_cast<void*>(remoteSession.RequestValue().client),RemoteCreateOperation,&c);
    if(!created && !c.attempted)return false;
    frame->consumed=true;
    auto model=DecodeRemoteModel(player,profile.Base(),r.preview.target,r.blockType,c.entity);
    bool claimed=created && model && model->id==255 && model->model==reinterpret_cast<std::uintptr_t>(c.weak.object) &&
        model->control==reinterpret_cast<std::uintptr_t>(c.weak.control) && remoteSession.Created(model->model,model->control,c.entity);
    bool released=ReleaseModelWeak(c.weak,profile.Base(),profile.Function<void(*)(void*)>(0x76BD0));
    if(claimed && released)++remoteCreates;else nativeFault.store(true);
    // Exact callback output is construction storage. Empty scene is consumed
    // by the proven null branches in4F5BD0 and concrete SceneStack4B22D00.
    *static_cast<NativePair*>(out)={};return true;
}
bool CreatorFrame(std::uintptr_t caller){
    auto frame=remoteDeferred;
    return RemoteOnOwner() && frame && frame->valid && !frame->consumed && caller==profile.Base()+Rva(0x53820D) && remoteSession.Matches(frame->open);
}
void* CreatorHook(void* functor,void* out,void* factory,void* metadata,void* player,void const* pos,void const* entity){
    if(CreatorFrame(reinterpret_cast<std::uintptr_t>(_ReturnAddress())) &&
       MatchesRemoteCreator(functor,factory,metadata,player,pos,entity,out,profile.Base(),remoteSession.RequestValue(),remoteDeferred->open.entity) && TryRemoteCreate(out,player,pos))return out;
    return creatorOriginal(functor,out,factory,metadata,player,pos,entity);
}
void* Creator45Hook(void* functor,void* out,void* factory,void* game,void* client,void* factoryView,void* metadata,void* player,void const* pos,void const* entity){
    if(CreatorFrame(reinterpret_cast<std::uintptr_t>(_ReturnAddress())) &&
       MatchesRemoteCreator45(functor,factory,game,client,factoryView,metadata,player,pos,entity,out,profile.Base(),remoteSession.RequestValue(),remoteDeferred->open.entity) && TryRemoteCreate(out,player,pos))return out;
    return creator45Original(functor,out,factory,game,client,factoryView,metadata,player,pos,entity);
}
void RemoteFinishDeferred(RemoteDeferredFrame const& frame){
    if(!RemoteOnOwner() || !frame.valid || !frame.consumed)return;
    auto const& r=remoteSession.RequestValue();auto model=DecodeRemoteModel(reinterpret_cast<void*>(r.player),profile.Base(),r.preview.target,r.blockType,remoteSession.Entity());
    if(!model || model->id!=frame.open.id || !remoteSession.Accepted(model->id,model->model,model->control)){nativeFault.store(true);return;}
    ++remoteAccepts;auto now=GetTickCount64();auto config=Settings();bool held=Held(config);
    auto work=bridge->Work(now);bool wanted=config.enabled && held && Foreground() && NativeHud(profile,reinterpret_cast<void*>(r.client)) && work &&
        work->request.target==r.preview.target && work->request.generation==r.preview.generation;
    if(wanted && !bridge->AuthorizeRemote(*work,CurrentRemoteLease(),now))wanted=false;
    auto action=remoteSession.Step(wanted?std::optional<RemoteRequest>{r}:std::nullopt,now,held);
    if(action==RemoteAction::Close)RemoteCloseOwned();
}
RemoteContentFrame RemoteBeginContent(void* handler,void* packet){
    RemoteContentFrame f{};if(!RemoteOnOwner() || remoteContent || remoteSession.Phase()!=RemotePhase::Active)return f;
    auto const& r=remoteSession.RequestValue();if(!RemoteLive(r))return f;
    auto v=CapturePacket(TraceStage::Content,handler,packet,profile.Base());
    auto route=DecodeRemoteRoute(reinterpret_cast<void*>(r.client),profile.Base());
    f.lease=CurrentRemoteLease();f.count=r.slots;
    f.valid=route && route->integrated==r.integrated && route->handler==reinterpret_cast<std::uintptr_t>(handler) && !(v.fields&ReadFault) && (v.fields&(PacketFields|CountField))==(PacketFields|CountField) &&
        v.client==r.client && v.container==static_cast<int>(f.lease.id) && v.count==static_cast<int>(f.count);return f;
}
void RemoteSetterApplied(void* model,unsigned slot){
    auto f=remoteContent;if(RemoteOnOwner() && f && f->valid && reinterpret_cast<std::uintptr_t>(model)==f->lease.model && slot<f->count)f->mask|=UINT64_C(1)<<slot;
}
void RemoteFinishContent(RemoteContentFrame const& f){
    if(!RemoteOnOwner() || !f.valid || CurrentRemoteLease()!=f.lease)return;
    auto const& r=remoteSession.RequestValue();auto model=DecodeRemoteModel(reinterpret_cast<void*>(r.player),profile.Base(),r.preview.target,r.blockType,remoteSession.Entity());
    if(!model || model->model!=f.lease.model || model->control!=f.lease.control || model->id!=f.lease.id)return;
    if(remoteSession.Applied(f.lease.id,f.lease.model,f.count,f.mask)){++remoteFull;RemoteCopy();}
}
void RemoteNativeClose(void* player){
    if(RemoteOnOwner() && !remoteClosing && remoteSession.Phase()!=RemotePhase::Idle && reinterpret_cast<std::uintptr_t>(player)==remoteSession.RequestValue().player)RemoteRelinquish();
}
void RemoteNativeBinding(void* player){
    if(!remoteCreating && !remoteClosing)RemoteNativeClose(player);
}
void RemoteClosed(TraceRecord const& v){
    if(RemoteOnOwner() && !(v.fields&ReadFault) && (v.fields&PacketFields) && remoteSession.CloseReply(v.client,Owner(),static_cast<unsigned>(v.container)))bridge->RevokeRemote();
}
using ScenePending=bool(*)(void*);
ScenePending scenePendingOriginal{};
bool ScenePendingHook(void* stack){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    bool pending=scenePendingOriginal(stack);
    if(pending || !RemoteOnOwner() || !MatchesRemoteConsistency(remoteSession,stack,profile.Base(),caller,Owner()))return pending;
    auto const& r=remoteSession.RequestValue();
    if(!RemoteLive(r) || !NativeHud(profile,reinterpret_cast<void*>(r.client)))return pending;
    //475EF30 assumes every model has a pending/visible container screen.
    // Exempt only this proven owned headless model from that one query.
    // Never change stack140, actual UI behavior, chat or the global error flag.
    ++remoteConsistency;return true;
}
