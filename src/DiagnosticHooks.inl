// Original lifecycle forwarding plus a narrowly attributed headless model
// creator. The automatic interaction path lives in RemoteControl.inl.
using PacketHandler=std::uint64_t(*)(void*,void*,void*);
using DeferredOpen=std::uint64_t(*)(void*,void*);
using SceneEvent=std::uint64_t(*)(void*,void*);
using ModelFactory=void*(*)(void*,void*,void const*,void const*);
using PlayerClose=std::uint64_t(*)(void*);
using ModelBinding=void*(*)(void*,void*);
using GuiItem=void(*)(void*,void*,unsigned,void*,float,float,float,float,float,unsigned,bool,int,void const*);
using IconResolve=void*(*)(void*,unsigned,unsigned);
using BlockUse=std::uint64_t(*)(void*,void const*,unsigned char,unsigned char,unsigned char);
// Exact2598F80: GameMode, borrowed Actor, borrowed hit Vec3, fourth register.
// Native tests its low byte; forward the complete incoming register unchanged.
using EntityUse=std::uint64_t(*)(void*,void*,void const*,std::uintptr_t);
using ItemAdjust=std::uint64_t(*)(void*,void*,void*,void*,void*);
using ItemUseOn=void*(*)(void*,void*,void*,void*,unsigned char,unsigned char,void*,std::uint64_t);
using ItemUseOn45=void*(*)(void*,void*,void*,int,int,int,unsigned char,void*,std::uint64_t);
using StackEmpty=unsigned char(*)(void*);
ItemAdjust itemAdjustOriginal{};ItemUseOn itemUseOnOriginal{};StackEmpty stackEmptyOriginal{};
ItemUseOn45 itemUseOn45Original{};
// Native standard-model v64 is called with a fourth argument by the handler;
// the implementation ignores it. Preserve the full incoming register value.
using ModelApply=std::uint64_t(*)(void*,unsigned,void const*,std::uintptr_t);
using ScreenBuild=void*(*)(void*,void*,void*,void*);
using ScreenBuild45=void*(*)(void*,void*,void*,void*,void*,void*);
using ControllerDestroy=void(*)(void*);
PacketHandler openOriginal{},contentOriginal{},slotOriginal{},closePacketOriginal{};
DeferredOpen deferredOriginal{};SceneEvent sceneOriginal{};
ModelFactory factoryOriginal{};PlayerClose closePlayerOriginal{};
ModelBinding modelBindingOriginal{};
GuiItem guiItemOriginal{};IconResolve iconResolveOriginal{};IconSampleBudget iconSampleBudget;
BlockUse blockUseOriginal{};ModelApply modelApplyOriginal{};
EntityUse entityUseOriginal{};
using EntityPlan=void*(*)(void*,void*,void*,void const*);
EntityPlan entityPlanOriginal{};
using EntityPlan45=void*(*)(void*,void*,void*,void*,void const*);
EntityPlan45 entityPlan45Original{};
using EntityModelFactory=void*(*)(void*,void*,void const*);
using ActorLookup=void*(*)(void*,std::uint64_t,unsigned char);
EntityModelFactory entityFactoryOriginal{};ActorLookup actorLookupOriginal{};
std::uint64_t EntityUseHook(void*,void*,void const*,std::uintptr_t);
ScreenBuild screenBuildOriginal{};ControllerDestroy controllerDestroyOriginal{};
ScreenBuild45 screenBuild45Original{};
DiagnosticBuffer* diagnosticBuffer{};
void TraceRecordValue(DiagnosticScope const&,TraceRecord,TracePhase);
std::uint64_t ItemAdjustHook(void* item,void* stack,void* player,void* face,void* pos){
    return FilterHeldAdjust(itemAdjustOriginal,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),item,stack,player,face,pos);
}
void* ItemUseOnHook(void* stack,void* out,void* player,void* pos,unsigned char face,unsigned char hand,void* hit,std::uint64_t flags){
    return FilterHeldItemUse(itemUseOnOriginal,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),stack,out,player,pos,face,hand,hit,flags);
}
void* ItemUseOn45Hook(void* stack,void* out,void* player,int x,int y,int z,unsigned char face,void* hit,std::uint64_t flags){
    return FilterHeldItemUse45(itemUseOn45Original,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),stack,out,player,x,y,z,face,hit,flags);
}
unsigned char StackEmptyHook(void* stack){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return FilterEntityHandEmpty([caller](void* value){return FilterHeldEmpty(stackEmptyOriginal,caller,value);},caller,stack);
}
bool EntityContainerAction(void* output,void* actor,void* player){
    auto old=CurrentVersion()==GameVersion::V45;
    return MatchesEntityOpenAction(output,actor,player,profile.Base()) &&
        profile.Virtual(actor,old?872:864)==profile.Base()+Rva(0x1A28840) &&
        profile.Virtual(static_cast<std::byte*>(output)+(old?32:8),16)==profile.Base()+Rva(0x1AAFEB0);
}
bool EntityDiscardAction(void* output){
    // Retire the native action without invoking it. The callback destructor
    // uses its native v8 contract; title cleanup is exact6ABE20. Retire only
    // the version's action storage (45:104 bytes;51/52:80), never an item stack.
    auto destroy=profile.Virtual(static_cast<std::byte*>(output)+(CurrentVersion()==GameVersion::V45?32:8),8);
    auto ok=RetireEntityAction(output,reinterpret_cast<void(*)(void*)>(destroy),profile.Function<void(*)(void*)>(0x6ABE20));
    if(!ok)nativeFault.store(true);return ok;
}
void* EntityPlanHook(void* actor,void* output,void* player,void const* hit){
    return FilterEntityPlan(entityPlanOriginal,EntityContainerAction,EntityDiscardAction,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()),actor,output,player,hit);
}
void* EntityPlan45Hook(void* actor,void* result,void* player,void* action,void const* hit){
    return FilterEntityPlan45(entityPlan45Original,EntityContainerAction,EntityDiscardAction,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()),actor,result,player,action,hit);
}
#include "RemoteControl.inl"
bool TraceEnabled(){return status.load(std::memory_order_acquire)==1 && diagnosticBuffer && Settings().enabled;}
void TraceRecordValue(DiagnosticScope const& scope,TraceRecord value,TracePhase phase){
    if(!TraceEnabled())return;
    scope.Stamp(value);value.phase=phase;value.owner=Owner();value.time=GetTickCount64();
    auto config=Settings();value.held=Held(config);value.foreground=Foreground();
    diagnosticBuffer->Push(value);
}
std::uint64_t TracePacketCall(TraceStage stage,PacketHandler original,void* handler,void* network,void* packet,std::uintptr_t caller){
    DiagnosticScope scope(stage);TraceRecord value{};
    return DiagnosticForward(original,[&](TracePhase phase){
        if(!TraceEnabled())return;
        // The packet stays entirely with the original. Post means original
        // returned, NOT that an inventory update has been applied/acknowledged.
        if(phase==TracePhase::Before){value=CapturePacket(stage,handler,packet,profile.Base());value.caller=caller;}
        TraceRecordValue(scope,value,phase);
    },handler,network,packet);
}
std::uint64_t OpenHook(void* h,void* n,void* p){return TracePacketCall(TraceStage::Open,openOriginal,h,n,p,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
std::uint64_t ContentHook(void* h,void* n,void* p){
    auto frame=RemoteBeginContent(h,p);RemoteFrameScope scope(remoteContent,&frame);
    auto result=TracePacketCall(TraceStage::Content,contentOriginal,h,n,p,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    RemoteFinishContent(frame);return result;
}
std::uint64_t SlotHook(void* h,void* n,void* p){return TracePacketCall(TraceStage::Slot,slotOriginal,h,n,p,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
std::uint64_t ClosePacketHook(void* h,void* n,void* p){
    auto value=CapturePacket(TraceStage::ClosePacket,h,p,profile.Base());
    auto result=TracePacketCall(TraceStage::ClosePacket,closePacketOriginal,h,n,p,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    RemoteClosed(value);return result;
}
std::uint64_t DeferredHook(void* capture,void* region){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto frame=RemoteBeginDeferred(capture,region);RemoteFrameScope attribution(remoteDeferred,&frame);
    DiagnosticScope scope(TraceStage::DeferredOpen);TraceRecord value{};
    auto result=DiagnosticForward(deferredOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        if(phase==TracePhase::Before){value=CaptureDeferred(capture,profile.Base());value.caller=caller;}
        TraceRecordValue(scope,value,phase);
    },capture,region);
    RemoteFinishDeferred(frame);return result;
}
std::uint64_t SceneHook(void* listener,void* variant){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::Scene);TraceRecord value{};
    return DiagnosticForward(sceneOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        if(phase==TracePhase::Before){value=CaptureScene(listener,variant,profile.Base());value.caller=caller;}
        if(value.event==16 || value.event==18)TraceRecordValue(scope,value,phase);
    },listener,variant);
}
void* FactoryHook(void* out,void* player,void const* position,void const* blockType){
    DiagnosticScope scope(TraceStage::Factory);TraceRecord value{};
    return DiagnosticForward(factoryOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        if(phase==TracePhase::Before)value=CaptureFactory(player,position,blockType,profile.Base());
        else{
            // Re-read only the still-borrowed function argument, not the weak
            // result/control or the factory's internal temporary ownership.
            auto model=CapturePlayer(player,profile.Base());
            value.model=model.model;value.modelId=model.modelId;value.modelType=model.modelType;
            value.fields=(value.fields&~ModelFields)|(model.fields&(ModelFields|ReadFault));
        }
        TraceRecordValue(scope,value,phase);
    },out,player,position,blockType);
}
std::uint64_t ClosePlayerHook(void* player){
    RemoteNativeClose(player);
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::ClosePlayer);
    return DiagnosticForward(closePlayerOriginal,[&](TracePhase phase){
        if(TraceEnabled()){auto value=CapturePlayer(player,profile.Base());value.caller=caller;TraceRecordValue(scope,value,phase);}
    },player);
}
void* ModelBindingHook(void* player,void* inputPair){
    RemoteNativeBinding(player);
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::ModelBinding);TraceRecord value{};value.caller=caller;
    return DiagnosticForward(modelBindingOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        value=ObserveModelBinding(phase,value,player,inputPair,profile.Base());
        TraceRecordValue(scope,value,phase);
    },player,inputPair);
}
void GuiItemHook(void* renderer,void* actorContext,unsigned mode,void* stack,float x,float y,float light,float alpha,float scale,unsigned frame,bool animate,int z,void const* customIcon){
    bool preview=PreviewIconScope::Matches(stack);
    auto config=Settings();
    if(!TraceEnabled() || !Foreground() || !Held(config) || !iconSampleBudget.Admit(preview)){
        guiItemOriginal(renderer,actorContext,mode,stack,x,y,light,alpha,scale,frame,animate,z,customIcon);return;
    }
    DiagnosticScope scope(TraceStage::GuiIcon);TraceRecord value{};
    IconObservationScope observation(value,stack);
    DiagnosticForward(guiItemOriginal,[&](TracePhase phase){
        if(phase==TracePhase::Before){value=CaptureGuiIcon(renderer,actorContext,mode);value.previewIcon=preview;}
        else TraceRecordValue(scope,value,phase);
    },renderer,actorContext,mode,stack,x,y,light,alpha,scale,frame,animate,z,customIcon);
}
void* IconResolveHook(void* stack,unsigned frame,unsigned animated){
    // Observe the actual borrowed return inside its original call chain only.
    // No extra icon resolution, texture initialization or ownership change.
    auto result=iconResolveOriginal(stack,frame,animated);
    IconObservationScope::Resolved(result,frame,animated,profile.Base(),stack);
    return result;
}
std::uint64_t BlockUseHook(void* gameMode,void const* position,unsigned char arg3,unsigned char arg4,unsigned char arg5){
    RemoteManualUse();
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::BlockUse);TraceRecord value{};
    return DiagnosticForward(blockUseOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        if(phase==TracePhase::Before){value=CaptureBlockUse(gameMode,position,arg3,arg4,arg5,profile.Base());value.caller=caller;}
        TraceRecordValue(scope,value,phase);
    },gameMode,position,arg3,arg4,arg5);
}
std::uint64_t EntityUseHook(void* gameMode,void* actor,void const* hit,std::uintptr_t arg4){
    RemoteManualUse();
    if(!TraceEnabled())return entityUseOriginal(gameMode,actor,hit,arg4);
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::EntityUse);
    auto value=ObserveEntityUse(TracePhase::Before,{},gameMode,actor,arg4,0,profile.Base());
    value.caller=caller;TraceRecordValue(scope,value,TracePhase::Before);
    auto result=entityUseOriginal(gameMode,actor,hit,arg4);
    value=ObserveEntityUse(TracePhase::After,value,nullptr,nullptr,0,result,profile.Base());
    TraceRecordValue(scope,value,TracePhase::After);
    return result;
}
void* EntityFactoryHook(void* out,void* player,void const* entity){
    if(!TraceEnabled())return entityFactoryOriginal(out,player,entity);
    DiagnosticScope scope(TraceStage::EntityFactory);
    auto value=CaptureEntityFactory(player,entity,profile.Base());value.caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    EntityFactoryScope frame(value);
    if(value.fields&(EntityFactoryFields|ReadFault))TraceRecordValue(scope,value,TracePhase::Before);
    auto result=entityFactoryOriginal(out,player,entity);
    // Native construction can invalidate borrowed input. This marker only
    // reuses pre-call scalars; no output weak ownership is acquired or changed.
    if(value.fields&(EntityFactoryFields|ReadFault))TraceRecordValue(scope,value,TracePhase::After);
    return result;
}
void* ActorLookupHook(void* level,std::uint64_t id,unsigned char includeRemoved){
    // The returned Actor is borrowed inside the original lookup call chain.
    // Never initiate an extra lookup or probe an unrelated level/player.
    auto result=actorLookupOriginal(level,id,includeRemoved);
    auto frame=EntityFactoryScope::Current();
    if(!frame || !TraceEnabled() || includeRemoved || frame->world!=reinterpret_cast<std::uintptr_t>(level) || frame->entity!=id)return result;
    DiagnosticScope scope(TraceStage::EntityResolve);
    auto value=CaptureEntityResolution(*frame,level,id,result,profile.Base());
    value.caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());TraceRecordValue(scope,value,TracePhase::After);
    return result;
}
std::uint64_t ModelApplyHook(void* model,unsigned slot,void const* stack,std::uintptr_t arg4){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::ModelApply);
    auto result=DiagnosticForward(modelApplyOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        auto value=CaptureModelApply(model,slot,profile.Base());value.caller=caller;
        TraceRecordValue(scope,value,phase);
    },model,slot,stack,arg4);
    RemoteSetterApplied(model,slot);return result;
}
void* ScreenBuildHook(void* factory,void* outPair,void* name,void* controllerPair){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    // This is the exact standard-controller allocation call. Other screens and
    // indirect callers are forwarded without interpreting their input layout.
    if(caller!=profile.Base()+Rva(0x6772382))return screenBuildOriginal(factory,outPair,name,controllerPair);
    DiagnosticScope scope(TraceStage::ScreenBuild);TraceRecord value{};value.caller=caller;
    return DiagnosticForward(screenBuildOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        value=ObserveScreenBuild(phase,value,controllerPair,outPair,profile.Base());
        TraceRecordValue(scope,value,phase);
    },factory,outPair,name,controllerPair);
}
void* ScreenBuild45Hook(void* factory,void* outPair,void* game,void* client,void* metadata,void* controllerPair){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if(caller!=profile.Base()+Rva(0x6772382))return screenBuild45Original(factory,outPair,game,client,metadata,controllerPair);
    DiagnosticScope scope(TraceStage::ScreenBuild);TraceRecord value{};value.caller=caller;
    return DiagnosticForward(screenBuild45Original,[&](TracePhase phase){
        if(!TraceEnabled())return;
        value=ObserveScreenBuild(phase,value,controllerPair,outPair,profile.Base());
        TraceRecordValue(scope,value,phase);
    },factory,outPair,game,client,metadata,controllerPair);
}
void ControllerDestroyHook(void* controller){
    auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DiagnosticScope scope(TraceStage::ControllerDestroy);TraceRecord value{};
    DiagnosticForward(controllerDestroyOriginal,[&](TracePhase phase){
        if(!TraceEnabled())return;
        if(phase==TracePhase::Before){value=CaptureController(TraceStage::ControllerDestroy,controller,profile.Base());value.caller=caller;}
        // After is a return marker using copied scalars only. Native destruction
        // may have invalidated every object visible in the before row.
        if(value.fields&(ControllerFields|ReadFault))TraceRecordValue(scope,value,phase);
    },controller);
}
void TraceClientSample(DiagnosticScope const& scope,void* client,ClientObservation const& observation,bool held,bool foreground){
    struct Last {void* client{};Target target{};bool held{},foreground{},hud{},seen{};};
    thread_local Last last;
    auto const& target=observation.target;
    if(!TraceEnabled())return;
    if(last.seen && last.client==client && last.target==target && last.held==held && last.foreground==foreground && last.hud==observation.hud)return;
    last={client,target,held,foreground,observation.hud,true};
    TraceRecord value{};value.client=reinterpret_cast<std::uintptr_t>(client);value.world=target.world;
    if(target.kind==TargetKind::Block){value.x=target.x;value.y=target.y;value.z=target.z;value.fields|=PositionFields;}
    TraceRecordValue(scope,value,TracePhase::Sample);
}
void DrainDiagnostic(){
    if(!diagnosticBuffer)return;
    std::array<TraceRecord,64> values{};auto count=diagnosticBuffer->Drain(values);
    if(!count)return;
    std::ofstream out(directory/"ContainerPreview.log",std::ios::app);
    for(std::size_t i=0;i<count;++i)out<<values[i].time<<' '<<FormatTrace(values[i])<<'\n';
}
