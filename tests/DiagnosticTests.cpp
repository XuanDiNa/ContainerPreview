#include <string_view>
#include "ContainerPreview/NativeAddress.h"
#include "ContainerPreview/DiagnosticTrace.h"
#include "EntityFixture.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace preview;
namespace {
std::uint32_t FixtureRva(std::uint32_t key){auto value=Rva(key);return value?value:key;}
int checks{},failures{};
void Check(bool b,char const* name){++checks;if(!b){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
template<std::size_t N,class T>void Put(std::array<std::byte,N>& a,unsigned offset,T value){std::memcpy(a.data()+offset,&value,sizeof value);}
constexpr std::uintptr_t base=0x140000000;
}
int main(int argc,char** argv){
    if(argc>1 && std::string_view(argv[1])=="51")native_detail::active.store(GameVersion::V51);
    // A repeated per-frame refusal must neither flood the queue nor hide a
    // different gate/target. Alternation must not bypass the process cap.
    RemoteGateBudget gateBudget;RemoteGateSample gate{};gate.gate=RemoteGate::Route;
    gate.check={RemoteReason::IdentifierType,2};gate.client=0x1234;gate.world=0x5678;gate.x=3;gate.y=42;gate.z=-73;gate.generation=7;
    Check(gateBudget.Admit(gate),"first remote gate failure is observable");
    Check(!gateBudget.Admit(gate),"identical per-frame gate does not consume trace capacity");
    gate.check={RemoteReason::Established,0};Check(gateBudget.Admit(gate),"changed native rejection is observable");
    gate.x++;Check(gateBudget.Admit(gate),"same rejection at a changed target is observable");
    gate.phase=1;Check(gateBudget.Admit(gate),"policy phase change is not hidden by repeated reason");
    unsigned admitted=4;for(unsigned i=0;i<200;++i){gate.action=i%2;admitted+=gateBudget.Admit(gate)?1:0;}
    Check(admitted==128 && !gateBudget.Admit(gate),"alternating states cannot exceed 128 remote gate samples per process");
    TraceRecord gateRecord{};gateRecord.stage=TraceStage::RemoteGate;gateRecord.remoteGate=gate;
    auto gateText=FormatTrace(gateRecord);
    Check(gateText.find("stage=remote-gate")!=std::string::npos && gateText.find("gate=route")!=std::string::npos &&
        gateText.find("reason=handler-not-established")!=std::string::npos && gateText.find("remotePhase=1")!=std::string::npos,
        "formatted remote diagnostic identifies boundary, native reason and session phase");
    // Breaks caught: accidental skipped/duplicated original, changed args/return,
    // or probe errors swallowed around the original rather than the observers.
    std::vector<int> order;unsigned calls{};
    auto result=DiagnosticForward([&](void* p,int n){++calls;order.push_back(2);Check(p==reinterpret_cast<void*>(7)&&n==19,"diagnostic preserves original arguments");return UINT64_C(0xFEDCBA9876543210);},
        [&](TracePhase p){order.push_back(p==TracePhase::Before?1:3);},reinterpret_cast<void*>(7),19);
    Check(calls==1 && result==UINT64_C(0xFEDCBA9876543210) && order==std::vector<int>{1,2,3},"before/after bracket exactly one full-width original result");
    calls=0;result=DiagnosticForward([&]{++calls;return UINT64_C(0xCDEF1234567890AB);},[](TracePhase){throw std::runtime_error("probe");});
    Check(calls==1 && result==UINT64_C(0xCDEF1234567890AB),"observer failure never blocks or changes original");
    bool propagated=false;try{DiagnosticForward([]()->int{throw std::runtime_error("original");},[](TracePhase){});}catch(std::runtime_error const&){propagated=true;}
    Check(propagated,"original exception is not hidden as a successful call");
    order.clear();calls=0;
    DiagnosticForward([&](int n){++calls;order.push_back(n);},[&](TracePhase p){order.push_back(p==TracePhase::Before?1:3);},2);
    Check(calls==1 && order==std::vector<int>{1,2,3},"void GUI original forwards arguments once between both observers");
    calls=0;DiagnosticForward([&]{++calls;},[](TracePhase){throw std::runtime_error("probe");});
    Check(calls==1,"void original still runs exactly once when diagnostic observers fail");
    propagated=false;try{DiagnosticForward([]()->void{throw std::runtime_error("original");},[](TracePhase){});}catch(std::runtime_error const&){propagated=true;}
    Check(propagated,"void native exception is never swallowed by a diagnostic");
    // Breaks caught: cross-callback/thread scope invention or a recursive close
    // losing its parent and therefore masquerading as an independent close.
    TraceRecord outer{},inner{},after{},outside{};
    {DiagnosticScope client(TraceStage::Client,0x1234);client.Stamp(outer);
        {DiagnosticScope nested(TraceStage::ClosePlayer);nested.Stamp(inner);}
        client.Stamp(after);
    }
    {DiagnosticScope unscoped(TraceStage::Content);unscoped.Stamp(outside);}
    Check(outer.call && outer.depth==1 && outer.clientDepth==1 && outer.scopedClient==0x1234,"client original has an explicit enclosing owner scope");
    Check(inner.parent==outer.call && inner.depth==2 && inner.clientDepth==1 && inner.scopedClient==0x1234,"nested callback preserves parent and client scope");
    Check(after.call==outer.call && outside.clientDepth==0 && outside.scopedClient==0,"leaving scope restores prior context without stale client inheritance");
    TraceRecord otherThread{},nestedClient{},restoredClient{};
    {DiagnosticScope c(TraceStage::Client,0x4321);
        std::thread worker([&]{DiagnosticScope remote(TraceStage::Content);remote.Stamp(otherThread);});worker.join();
        {DiagnosticScope next(TraceStage::Client,0x5678);next.Stamp(nestedClient);}
        c.Stamp(restoredClient);
    }
    Check(otherThread.clientDepth==0 && otherThread.scopedClient==0 && otherThread.parent==0,"a different native thread never inherits the enclosing client scope");
    Check(nestedClient.clientDepth==2 && nestedClient.scopedClient==0x5678 && restoredClient.scopedClient==0x4321 && restoredClient.clientDepth==1,"nested client scope restores the actual caller instead of keeping the latest client");
    // Exact-image layouts hand-transcribed from IDA; no external native calls.
    std::array<std::byte,128> handler{};std::array<std::byte,2048> client{};
    Put(client,0,base+FixtureRva(0xE9731B0));Put(handler,88,reinterpret_cast<std::uintptr_t>(client.data()));
    std::array<std::byte,192> packet{};Put(packet,0,base+FixtureRva(0xE7B0D90));
    Put(packet,48,std::uint8_t{9});Put(packet,49,std::uint8_t{0});
    Put(packet,52,-8);Put(packet,56,65);Put(packet,60,29);Put(packet,64,UINT64_MAX);
    auto unchanged=packet;auto open=CapturePacket(TraceStage::Open,handler.data(),packet.data(),base);
    Check((open.fields&(PacketFields|PositionFields|EntityField))==(PacketFields|PositionFields|EntityField) && open.container==9 && open.type==0 && open.x==-8 && open.y==65 && open.z==29 && open.entity==UINT64_MAX,"open fields are decoded from actual packet offsets, not callable offsets");
    Check(open.client==reinterpret_cast<std::uintptr_t>(client.data()) && packet==unchanged,"capture only reads packet and identifies the native client");
    Put(packet,0,base+FixtureRva(0xE837C90));Put(packet,160,UINT64_C(0x10000));Put(packet,168,UINT64_C(0x10A20));Put(packet,176,UINT64_C(0x10A20));
    auto full=CapturePacket(TraceStage::Content,handler.data(),packet.data(),base);
    Check((full.fields&CountField) && full.count==27,"full-content descriptor count uses native 96-byte stride without dereferencing elements");
    Put(packet,168,UINT64_C(0x10A21));Check(!(CapturePacket(TraceStage::Content,handler.data(),packet.data(),base).fields&CountField),"non-stride vector is unknown, not an empty container");
    Put(packet,168,UINT64_C(0xFFFFFF));Check(!(CapturePacket(TraceStage::Content,handler.data(),packet.data(),base).fields&CountField),"unbounded vector is not traversed or represented as known");
    Put(packet,160,UINT64_C(0));Put(packet,168,UINT64_C(0));Put(packet,176,UINT64_C(0));
    Check(CapturePacket(TraceStage::Content,handler.data(),packet.data(),base).count==0,"valid empty native descriptor vector is distinct from unknown");
    Put(packet,0,base+FixtureRva(0xE6D66B0));Put(packet,52,18u);
    auto slot=CapturePacket(TraceStage::Slot,handler.data(),packet.data(),base);
    Check((slot.fields&SlotField) && slot.slot==18 && slot.container==9,"slot event is a receipt label, not an applied-content acknowledgment");
    Put(packet,0,base+FixtureRva(0xE897540));Put(packet,50,std::uint8_t{1});
    Check((CapturePacket(TraceStage::ClosePacket,handler.data(),packet.data(),base).fields&PacketFields)!=0,"typed close is observed without creating a close reply");
    Put(packet,0,base+1);Check(!(CapturePacket(TraceStage::Open,handler.data(),packet.data(),base).fields&PacketFields),"wrong packet table is never decoded as an open");
    Check((CapturePacket(TraceStage::Open,nullptr,reinterpret_cast<void*>(1),base).fields&ReadFault)!=0,"bad native read is bounded and reported, not retried or written");
    std::array<std::byte,128> capture{};Put(capture,0,base+FixtureRva(0xE7B0290));Put(capture,56,std::uint8_t{9});Put(capture,57,std::uint8_t{0});Put(capture,60,-8);Put(capture,64,65);Put(capture,68,29);Put(capture,72,UINT64_MAX);Put(capture,120,std::uint8_t{1});
    auto delayed=CaptureDeferred(capture.data(),base);
    Check((delayed.fields&MayOpenField) && delayed.mayOpen==1 && delayed.container==9 && delayed.x==-8 && delayed.entity==UINT64_MAX,"deferred open decodes its separate captured-packet layout");
    std::array<std::byte,64> listener{};Put(listener,56,reinterpret_cast<std::uintptr_t>(client.data()));
    std::array<std::byte,384> variant{};Put(variant,376,std::uint8_t{16});Put(variant,24,std::uint8_t{0});Put(variant,28,-8);Put(variant,32,65);Put(variant,36,29);Put(variant,40,UINT64_MAX);
    auto inlineEvent=CaptureScene(listener.data(),variant.data(),base);
    Check((inlineEvent.fields&PositionFields) && inlineEvent.event==16 && inlineEvent.x==-8 && inlineEvent.entity==UINT64_MAX,"scene inline open is decoded with reference flag at byte48");
    std::array<std::byte,48> payload{};std::memcpy(payload.data(),variant.data(),48);Put(variant,0,reinterpret_cast<std::uintptr_t>(payload.data()));Put(variant,48,std::uint8_t{1});
    auto referred=CaptureScene(listener.data(),variant.data(),base);
    Check(referred.x==-8 && referred.y==65 && referred.z==29 && referred.entity==UINT64_MAX,"scene referenced payload uses the borrowed pointer synchronously");
    Put(variant,376,std::uint8_t{18});auto close=CaptureScene(listener.data(),variant.data(),base);
    Check((close.fields&SceneField) && close.event==18 && !(close.fields&PositionFields),"close event does not invent position fields from unrelated payload bytes");
    std::array<std::byte,1504> player{};std::array<std::byte,432> model{};std::array<std::byte,16> control{};
    Put(player,1440,reinterpret_cast<std::uintptr_t>(model.data()));Put(player,1448,reinterpret_cast<std::uintptr_t>(control.data()));Put(model,0,base+FixtureRva(0xE9AA770));Put(model,80,std::uint8_t{9});Put(model,81,std::uint8_t{0});Put(control,8,1);
    auto live=CapturePlayer(player.data(),base);Check((live.fields&ModelFields) && live.modelId==9 && live.modelType==0,"live native model identity is copied without taking ownership");
    Put(control,8,0);Check(!(CapturePlayer(player.data(),base).fields&ModelFields),"expired model control is never dereferenced as a live model");
    Put(control,8,1);Put(model,0,base+1);Check(!(CapturePlayer(player.data(),base).fields&ModelFields),"unverified model layout is not interpreted");
    std::array<int,3> factoryPosition{-8,65,29};std::uint8_t factoryType=2;auto playerBefore=player;
    auto factory=CaptureFactory(player.data(),factoryPosition.data(),&factoryType,base);
    Check((factory.fields&PositionFields) && factory.x==-8 && factory.y==65 && factory.z==29 && factory.type==2 && player==playerBefore,"factory probe reads original arguments without constructing a model or taking ownership");
    auto badFactory=CaptureFactory(player.data(),reinterpret_cast<void*>(1),&factoryType,base);
    Check((badFactory.fields&ReadFault)!=0 && player==playerBefore,"unreadable factory argument cannot write the player or block the original");
    // Breaks caught: wrong current/input model, ownership writes, treating an
    // explicit null binding as unknown, or re-reading a consumed input pair.
    Put(model,0,base+FixtureRva(0xE9AA770));Put(model,80,std::uint8_t{2});
    std::array<std::byte,432> incoming{};std::array<std::byte,16> inputControl{},inputPair{};
    Put(incoming,0,base+FixtureRva(0xE9AA770));Put(incoming,80,std::uint8_t{255});Put(incoming,81,std::uint8_t{0});Put(inputControl,8,1);
    Put(inputPair,0,reinterpret_cast<std::uintptr_t>(incoming.data()));Put(inputPair,8,reinterpret_cast<std::uintptr_t>(inputControl.data()));
    auto playerSaved=player;auto modelSaved=model;auto incomingSaved=incoming;auto controlSaved=inputControl;auto pairSaved=inputPair;
    auto binding=ObserveModelBinding(TracePhase::Before,{},player.data(),inputPair.data(),base);
    Check((binding.fields&(ModelFields|BindingField|InputModelFields))==(ModelFields|BindingField|InputModelFields) && binding.model==reinterpret_cast<std::uintptr_t>(model.data()) && binding.modelId==2 && binding.inputModel==reinterpret_cast<std::uintptr_t>(incoming.data()) && binding.inputId==255 && binding.inputType==0,"binding records old and incoming standard models separately before native ownership consumption");
    Check(player==playerSaved && model==modelSaved && incoming==incomingSaved && inputControl==controlSaved && inputPair==pairSaved,"binding observation leaves player models pair and strong controls byte-for-byte unchanged");
    // A real setter moves a new binding into the player. The observer neither
    // performs this state change nor takes any strong/weak reference itself.
    Put(player,1440,reinterpret_cast<std::uintptr_t>(incoming.data()));Put(player,1448,reinterpret_cast<std::uintptr_t>(inputControl.data()));Put(incoming,80,std::uint8_t{3});
    binding.caller=base+FixtureRva(0x6772D90);
    auto bound=ObserveModelBinding(TracePhase::After,binding,player.data(),reinterpret_cast<void*>(1),base);
    Check((bound.fields&ModelFields) && !(bound.fields&ReadFault) && bound.modelId==3 && bound.inputId==255 && bound.caller==base+FixtureRva(0x6772D90),"after binding reads only borrowed player and preserves pre-call input and caller even when input is inaccessible");
    std::array<std::byte,16> emptyPair{};
    auto clearing=ObserveModelBinding(TracePhase::Before,{},player.data(),emptyPair.data(),base);
    Check((clearing.fields&BindingField) && !(clearing.fields&InputModelFields) && clearing.inputModel==0 && clearing.inputId==-1,"explicit empty incoming binding is recorded as clear not an unknown live model");
    Put(player,1440,std::uintptr_t{});Put(player,1448,std::uintptr_t{});
    auto cleared=ObserveModelBinding(TracePhase::After,clearing,player.data(),reinterpret_cast<void*>(1),base);
    Check(cleared.model==0 && !(cleared.fields&(ModelFields|ReadFault)) && (cleared.fields&BindingField),"post-clear observation removes stale current model fields without re-reading consumed pair");
    Put(incoming,0,base+1);
    auto custom=ObserveModelBinding(TracePhase::Before,{},player.data(),inputPair.data(),base);
    Check((custom.fields&BindingField) && !(custom.fields&InputModelFields) && custom.inputId==-1,"unknown incoming model table stays opaque without interpreting its type");
    Put(incoming,0,base+FixtureRva(0xE9AA770));Put(inputControl,8,0);
    auto expired=ObserveModelBinding(TracePhase::Before,{},player.data(),inputPair.data(),base);
    Check((expired.fields&BindingField) && !(expired.fields&InputModelFields),"expired incoming control is not interpreted as an installed live model");
    auto absent=ObserveModelBinding(TracePhase::Before,{},player.data(),nullptr,base);
    Check(!(absent.fields&(BindingField|InputModelFields)) && absent.inputId==-1,"missing pair argument never invents a model or explicit clear");
    auto unreadable=ObserveModelBinding(TracePhase::Before,{},player.data(),reinterpret_cast<void*>(1),base);
    Check((unreadable.fields&ReadFault)!=0,"unreadable input pair produces bounded diagnostic fault");
    auto bindingLine=FormatTrace(bound);
    Check(bindingLine.find("inputId=255")!=std::string::npos && bindingLine.find("modelId=3")!=std::string::npos && bindingLine.find("caller=0x146772d90")!=std::string::npos,"formatted binding distinguishes pre-input from post-current model and exposes actual caller label");
    TraceRecord bindingScope{};{DiagnosticScope scope(TraceStage::ModelBinding);scope.Stamp(bindingScope);}
    Check(FormatTrace(bindingScope).find("stage=model-binding")!=std::string::npos,"binding trace is identifiable rather than conflated with receipt or factory");
    // Breaks caught: interpreting the dimension pair as GameMode, inventing a
    // player scope, decoding stack payloads, or conflating setter and receipt.
    std::array<std::byte,2736> usePlayer{};std::array<std::byte,16> gameMode{};
    Put(gameMode,8,reinterpret_cast<std::uintptr_t>(usePlayer.data()));
    Put(usePlayer,2720,reinterpret_cast<std::uintptr_t>(gameMode.data()));
    Put(usePlayer,472,UINT64_C(0xABCDEF));
    std::array<int,3> usePosition{-13,72,101};
    auto usePlayerSaved=usePlayer;auto gameModeSaved=gameMode;auto positionSaved=usePosition;
    auto use=CaptureBlockUse(gameMode.data(),usePosition.data(),5,2,1,base);
    Check((use.fields&(PositionFields|UseFields))==(PositionFields|UseFields) && use.player==reinterpret_cast<std::uintptr_t>(usePlayer.data()) && use.world==0xABCDEF && use.x==-13 && use.y==72 && use.z==101 && use.useArgs==std::array<int,3>{5,2,1},"native block-use metadata records the reciprocal GameMode player and exact byte arguments");
    Check(usePlayer==usePlayerSaved && gameMode==gameModeSaved && usePosition==positionSaved,"block-use diagnostic never changes player GameMode or target arguments");
    Put(usePlayer,2720,std::uintptr_t{0});
    auto foreignUse=CaptureBlockUse(gameMode.data(),reinterpret_cast<void*>(1),5,2,1,base);
    Check(foreignUse.player==0 && !(foreignUse.fields&(PositionFields|ReadFault)),"unpaired GameMode cannot authorize reading a target or attribute another player");
    Put(usePlayer,2720,reinterpret_cast<std::uintptr_t>(gameMode.data()));
    auto badUse=CaptureBlockUse(gameMode.data(),reinterpret_cast<void*>(1),5,2,1,base);
    Check((badUse.fields&ReadFault)!=0,"bad target metadata is bounded instead of preventing the native use");
    auto nullUse=CaptureBlockUse(nullptr,nullptr,0,0,0,base);
    Check(nullUse.player==0 && !(nullUse.fields&PositionFields),"missing use owner never invents a live player or target");
    // Breaks caught: wrong actor attribution, reading hit vectors/inventory,
    // accepting a foreign mode, losing full-width registers, post-call reads.
    EntityFixture entity;Put(entity.actor,0,base+FixtureRva(0xE91A000));Put(entity.actor,528,std::uint8_t{2});
    entity.Definition("hivesky","death_crate");
    auto definitionSaved=entity.definitions;
    Put(gameMode,0,base+FixtureRva(0xE827600));
    auto entitySaved=entity.actor;auto nodesSaved=entity.nodes;auto poolsSaved=entity.pools;
    auto identityDataSaved=entity.data;auto sparseSaved=entity.sparse;auto registrySaved=entity.registry;
    auto beforeEntity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),UINT64_C(0xFEDCBA9876543200),0,base);
    Check((beforeEntity.fields&(EntityUseFields|EntityField))==(EntityUseFields|EntityField) && beforeEntity.actor==reinterpret_cast<std::uintptr_t>(entity.actor.data()) && beforeEntity.actorTable==base+FixtureRva(0xE91A000) && beforeEntity.entity==200 && beforeEntity.entityRuntime==300 && beforeEntity.targetPlayer==0 && beforeEntity.interactionArg==UINT64_C(0xFEDCBA9876543200) && beforeEntity.player==reinterpret_cast<std::uintptr_t>(usePlayer.data()) && beforeEntity.world==0xABCDEF,"manual entity use records actual borrowed nonplayer identity and full-width native argument");
    Check(entity.actor==entitySaved && entity.nodes==nodesSaved && entity.pools==poolsSaved && entity.data==identityDataSaved && entity.sparse==sparseSaved && entity.registry==registrySaved,"entity identity observation leaves actor sparse pools identity data and registry unchanged");
    Check(entity.definitions==definitionSaved,"identifier classification never modifies or initializes native definition strings");
    Check(beforeEntity.actorFlags==2 && beforeEntity.targetPlayer==0 && beforeEntity.entity==200 && beforeEntity.entityRuntime==300,"Mob flag0x02 is nonplayer and retains both IDs for matching server container acceptance");
    Check(FormatTrace(beforeEntity).find("actorKind=death-crate")!=std::string::npos,"manual death-crate identity comes from native namespace and identifier component, not actor appearance or numeric table");
    entity.Definition("minecraft","pig");
    auto otherKind=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check(FormatTrace(otherKind).find("actorKind=other")!=std::string::npos,"other native identifiers are classified without logging arbitrary identifier strings");
    entity.Definition("minecraft","chest_minecart");
    Check(FormatTrace(ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base)).find("actorKind=chest-minecart")!=std::string::npos,"native chest-minecart proxy is distinguishable from an approved death crate without trusting its appearance");
    entity.Definition("other","death_crate");
    Check(FormatTrace(ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base)).find("actorKind=death-crate")==std::string::npos,"same entity name in another namespace is not the approved death crate");
    entity.Definition("hivesky","death_crate_extra");
    Check(FormatTrace(ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base)).find("actorKind=death-crate")==std::string::npos,"identifier prefix is not an exact death-crate match");
    entity.Definition("hivesky","death_crate");
    auto component=entity.definitions.data()+5*176;
    EntityFixture::Put(component,24,std::uint64_t{0});
    Check(FormatTrace(ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base)).find("actorKind=unknown")!=std::string::npos,"malformed native string capacity cannot identify the target");
    entity.Definition("hivesky","death_crate");
    EntityFixture::Put(component,32,std::uintptr_t{1});EntityFixture::Put(component,48,std::uint64_t{16});EntityFixture::Put(component,56,std::uint64_t{31});
    auto unreadableKind=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check(FormatTrace(unreadableKind).find("actorKind=unknown")!=std::string::npos && (unreadableKind.fields&EntityField),"unreadable identifier cannot erase independently captured numeric actor identity");
    entity.Definition("hivesky","death_crate");
    Put(entity.actor,528,std::uint8_t{0});
    auto plainEntity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check(plainEntity.actorFlags==0 && plainEntity.targetPlayer==0 && plainEntity.entity==200 && plainEntity.entityRuntime==300,"base actor without Mob or Player flags remains a numeric nonplayer diagnostic");
    Put(entity.actor,528,std::uint8_t{2});
    beforeEntity.caller=base+FixtureRva(0x5D95570);
    auto afterEntity=ObserveEntityUse(TracePhase::After,beforeEntity,reinterpret_cast<void*>(1),reinterpret_cast<void*>(1),0,UINT64_C(0xABCDEF9876543210),base);
    Check((afterEntity.fields&NativeResultField) && !(afterEntity.fields&ReadFault) && afterEntity.entity==200 && afterEntity.entityRuntime==300 && afterEntity.interactionResult==UINT64_C(0xABCDEF9876543210) && afterEntity.caller==base+FixtureRva(0x5D95570) && afterEntity.interactionArg==UINT64_C(0xFEDCBA9876543200),"entity return marker uses cached scalars only even when both native arguments are now inaccessible");
    Check(afterEntity.actorKind==DiagnosticActorKind::DeathCrate,"post-use provenance retains its pre-call classification without reading a destroyed actor");
    auto entityText=FormatTrace(afterEntity);
    Check(entityText.find("stage=entity-use")!=std::string::npos && entityText.find("entity=200")!=std::string::npos && entityText.find("runtime=300")!=std::string::npos && entityText.find("actorFlags=2")!=std::string::npos && entityText.find("nativeResult=0xabcdef9876543210")!=std::string::npos,"entity trace correlates native open IDs and raw classification without claiming a resource identifier or successful container open");
    Put(gameMode,0,base+FixtureRva(0xE827560));
    Check((ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),2,0,base).fields&EntityField)!=0,"base creative and adventure mode uses the same verified entity-interaction decoder");
    Put(gameMode,0,base+1);
    auto wrongMode=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),reinterpret_cast<void*>(1),0,0,base);
    Check(!(wrongMode.fields&(EntityUseFields|EntityField|ReadFault)) && !wrongMode.actor,"unknown mode layout does not authorize reading an actor");
    Put(gameMode,0,base+FixtureRva(0xE827600));Put(usePlayer,2720,std::uintptr_t{});
    auto unpairedEntity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),reinterpret_cast<void*>(1),0,0,base);
    Check(!(unpairedEntity.fields&(EntityUseFields|EntityField|ReadFault)),"nonreciprocal mode cannot attribute or inspect a target");
    Put(usePlayer,2720,reinterpret_cast<std::uintptr_t>(gameMode.data()));Put(entity.actor,528,std::uint8_t{1});
    Put(entity.actor,16,std::uintptr_t{1});
    auto playerEntity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check((playerEntity.fields&EntityUseFields) && !(playerEntity.fields&EntityField) && playerEntity.actorFlags==1 && playerEntity.targetPlayer==1 && !playerEntity.entity && !playerEntity.entityRuntime,"Player flag0x01 excludes target identity even without Mob flag0x02");
    Check(FormatTrace(playerEntity).find("actorKind=player")!=std::string::npos && !(playerEntity.fields&ReadFault),"player exclusion happens before even inaccessible identifier and identity components");
    Put(entity.actor,528,std::uint8_t{3});
    auto derivedPlayer=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check(derivedPlayer.actorFlags==3 && derivedPlayer.targetPlayer==1 && !(derivedPlayer.fields&EntityField) && !derivedPlayer.entity && !derivedPlayer.entityRuntime,"Player plus Mob flags still exclude the target identity without traversing inaccessible pools");
    Put(entity.actor,528,std::uint8_t{});Put(entity.actor,16,reinterpret_cast<std::uintptr_t>(entity.registry.data()));
    entity.data[1][5]=UINT64_MAX;
    auto noIdentity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),entity.actor.data(),0,0,base);
    Check((noIdentity.fields&EntityUseFields) && !(noIdentity.fields&EntityField) && !noIdentity.entity && !noIdentity.entityRuntime,"unassigned actor identity stays unknown rather than generating a new ID");
    auto faultEntity=ObserveEntityUse(TracePhase::Before,{},gameMode.data(),reinterpret_cast<void*>(1),0,0,base);
    Check((faultEntity.fields&ReadFault)!=0,"unreadable entity argument is bounded and reported");
    Check(!(ObserveEntityUse(TracePhase::Before,{},nullptr,nullptr,0,0,base).fields&EntityUseFields),"null entity interaction cannot invent a native owner");
    calls=0;auto wideArg=UINT64_C(0xAABBCCDDEEFF0000);
    auto entityResult=DiagnosticForward([&](void* m,void* a,void const* hit,std::uintptr_t arg){++calls;Check(m==gameMode.data() && a==entity.actor.data() && hit==reinterpret_cast<void*>(1) && arg==wideArg,"entity forwarding preserves all four arguments without inspecting hit vector");return UINT64_C(0x9988776655443301);},[](TracePhase){},gameMode.data(),entity.actor.data(),reinterpret_cast<void const*>(1),wideArg);
    Check(calls==1 && entityResult==UINT64_C(0x9988776655443301),"entity diagnostic forwards the native result exactly once without interpreting it as an open ACK");
    // Breaks caught: a foreign lookup being attached to this factory, accidental
    // player/target inventory reads, UID substitution and cache mutation.
    std::array<std::byte,3456> localPlayer{};std::array<std::byte,1200> localLevel{};
    Put(localLevel,0,base+FixtureRva(0xE7ADAB0));Put(localPlayer,3440,reinterpret_cast<std::uintptr_t>(client.data()));
    Put(localPlayer,472,reinterpret_cast<std::uintptr_t>(localLevel.data()));std::uint64_t requested=1273;
    auto source=CaptureEntityFactory(localPlayer.data(),&requested,base);
    Check((source.fields&EntityFactoryFields) && source.entity==1273 && source.player==reinterpret_cast<std::uintptr_t>(localPlayer.data()) && source.client==reinterpret_cast<std::uintptr_t>(client.data()) && source.world==reinterpret_cast<std::uintptr_t>(localLevel.data()),"entity factory captures its independently supplied owner UID and exact local client/world before construction");
    entity.data[1][5]=1273;Put(entity.actor,528,std::uint8_t{2});
    auto resolution=CaptureEntityResolution(source,localLevel.data(),1273,entity.actor.data(),base);
    Check((resolution.fields&(EntityFactoryFields|EntityUseFields|EntityField))==(EntityFactoryFields|EntityUseFields|EntityField) && resolution.requestedEntity==1273 && resolution.entity==1273 && resolution.entityRuntime==300 && resolution.actorKind==DiagnosticActorKind::DeathCrate,"native resolver result preserves the factory request and independently decoded actor identity and kind");
    auto sourceSaved=source;auto resolvedActorSaved=entity.actor;auto resolvedComponentSaved=entity.definitions;
    entity.data[1][5]=1261;
    auto mismatched=CaptureEntityResolution(source,localLevel.data(),1273,entity.actor.data(),base);
    Check(mismatched.requestedEntity==1273 && mismatched.entity==1261 && mismatched.entityRuntime==300,"a mismatched native return remains a mismatch instead of rewriting either UID to make it fit");
    Check(source.entity==sourceSaved.entity && entity.actor==resolvedActorSaved && entity.definitions==resolvedComponentSaved,"provenance observation never changes the request actor or native definition storage");
    auto foreignLevel=CaptureEntityResolution(source,reinterpret_cast<void*>(1),1273,reinterpret_cast<void*>(1),base);
    Check(!(foreignLevel.fields&(EntityFactoryFields|EntityUseFields|ReadFault)),"foreign-level lookup is rejected before reading its actor");
    auto foreignId=CaptureEntityResolution(source,localLevel.data(),999,reinterpret_cast<void*>(1),base);
    Check(!(foreignId.fields&(EntityFactoryFields|EntityUseFields|ReadFault)),"unrelated native lookup is not attributed to the scoped factory");
    auto nullSource=CaptureEntityResolution({},reinterpret_cast<void*>(1),1273,reinterpret_cast<void*>(1),base);
    Check(!(nullSource.fields&(EntityUseFields|ReadFault)),"resolver without factory ownership cannot inspect a target");
    auto missingActor=CaptureEntityResolution(source,localLevel.data(),1273,nullptr,base);
    Check((missingActor.fields&EntityFactoryFields) && missingActor.requestedEntity==1273 && !missingActor.actor && !(missingActor.fields&(EntityField|ReadFault)),"native lookup miss records the requested owner without inventing an actor or probing another one");
    Put(entity.actor,528,std::uint8_t{1});Put(entity.actor,16,std::uintptr_t{1});
    auto ownerPlayer=CaptureEntityResolution(source,localLevel.data(),1273,entity.actor.data(),base);
    Check(ownerPlayer.actorKind==DiagnosticActorKind::Player && ownerPlayer.targetPlayer==1 && !(ownerPlayer.fields&(EntityField|ReadFault)),"resolved player owner is excluded before identifier and identity pool reads");
    Put(entity.actor,528,std::uint8_t{2});Put(entity.actor,16,reinterpret_cast<std::uintptr_t>(entity.registry.data()));
    auto resolutionFault=CaptureEntityResolution(source,localLevel.data(),1273,reinterpret_cast<void*>(1),base);
    Check((resolutionFault.fields&ReadFault) && resolutionFault.requestedEntity==1273,"unreadable native resolver result is diagnostic-only and bounded");
    requested=UINT64_MAX;
    Check(!(CaptureEntityFactory(localPlayer.data(),&requested,base).fields&EntityFactoryFields),"sentinel block owner cannot create an entity-factory diagnostic scope");
    requested=0;Check(!(CaptureEntityFactory(localPlayer.data(),&requested,base).fields&EntityFactoryFields),"zero entity UID is not an owned entity request");
    requested=1273;Put(localPlayer,3440,std::uintptr_t{});
    Check(!(CaptureEntityFactory(localPlayer.data(),reinterpret_cast<void*>(1),base).fields&ReadFault),"foreign player is refused before reading a borrowed owner ID");
    Put(localPlayer,3440,reinterpret_cast<std::uintptr_t>(client.data()));Put(localLevel,0,base+1);
    Check(!(CaptureEntityFactory(localPlayer.data(),&requested,base).fields&EntityFactoryFields),"unknown world table cannot authorize entity lookup attribution");
    auto sourceText=FormatTrace(resolution);
    Check(sourceText.find("stage=entity-resolve")!=std::string::npos && sourceText.find("requestedEntity=1273")!=std::string::npos && sourceText.find("actorKind=death-crate")!=std::string::npos,"new diagnostic output distinguishes returned owner identity from the earlier interacted actor");
    Check(!EntityFactoryScope::Current(),"unscoped native entity lookup has no diagnostic owner");
    bool anotherThreadOwned=true;
    {EntityFactoryScope factoryScope(source);
        Check(EntityFactoryScope::Current()==&source,"factory provenance is bound to the current synchronous owner frame");
        {EntityFactoryScope invalidScope({});Check(!EntityFactoryScope::Current(),"nested invalid factory never inherits an outer factory's provenance");}
        Check(EntityFactoryScope::Current()==&source,"nested factory scope restores its actual enclosing owner");
        std::thread other([&]{anotherThreadOwned=EntityFactoryScope::Current()!=nullptr;});other.join();
        Check(!anotherThreadOwned,"another thread cannot inherit a borrowed native factory frame");
        try{EntityFactoryScope nested(resolution);throw 3;}catch(int){}
        Check(EntityFactoryScope::Current()==&source,"exception unwinding restores the outer attribution without retaining inner native data");
    }
    Check(!EntityFactoryScope::Current(),"factory return retires borrowed attribution before a later lookup can use it");
    Put(model,0,base+FixtureRva(0xE9AA770));Put(model,80,std::uint8_t{4});Put(model,81,std::uint8_t{0});
    auto applyModelSaved=model;auto applied=CaptureModelApply(model.data(),26,base);
    Check((applied.fields&(ModelFields|SlotField))==(ModelFields|SlotField) && applied.modelId==4 && applied.modelType==0 && applied.slot==26 && model==applyModelSaved,"setter observation identifies exact standard model and slot without taking ownership or inspecting a stack");
    auto badSlot=CaptureModelApply(model.data(),UINT_MAX,base);
    Check((badSlot.fields&ModelFields) && !(badSlot.fields&SlotField) && badSlot.slot==-1,"out-of-bound setter index remains unknown without truncating into a valid slot");
    Put(model,0,base+1);auto foreignApply=CaptureModelApply(model.data(),2,base);
    Check(!(foreignApply.fields&(ModelFields|SlotField)) && foreignApply.modelId==-1,"unknown setter model stays opaque even with plausible numeric slot");
    auto badApply=CaptureModelApply(reinterpret_cast<void*>(1),1,base);
    Check((badApply.fields&ReadFault)!=0,"unreadable setter model is a bounded diagnostic failure");
    Check(!(CaptureModelApply(nullptr,1,base).fields&ModelFields),"null setter model is not a live inventory");
    {DiagnosticScope contentScope(TraceStage::Content);contentScope.Stamp(outer);
        DiagnosticScope setterScope(TraceStage::ModelApply);setterScope.Stamp(applied);}
    Check(applied.parent==outer.call && applied.clientDepth==0 && FormatTrace(applied).find("stage=model-apply")!=std::string::npos,"setter rows retain the actual enclosing content call without inventing a client scope or ACK");
    use.stage=TraceStage::BlockUse;auto useLine=FormatTrace(use);
    Check(useLine.find("stage=block-use")!=std::string::npos && useLine.find("useArgs=5,2,1")!=std::string::npos,"formatted use rows expose original byte arguments and distinct native stage");
    // Breaks caught: reading a consumed input or destroyed controller, admitting
    // unrelated screens, taking ownership, or calling an opaque scene pointer.
    std::array<std::byte,4832> controller{};std::array<std::byte,96> controllerContext{};
    std::array<std::byte,16> controllerPair{},controllerControl{},outputPair{};
    std::array<std::byte,1> clientValidity{std::byte{1}};
    Put(controller,0,base+FixtureRva(0xE9B1870));Put(controllerControl,0,base+FixtureRva(0xE99FEB0));Put(controllerControl,8,1);
    Put(controllerPair,0,reinterpret_cast<std::uintptr_t>(controller.data()));Put(controllerPair,8,reinterpret_cast<std::uintptr_t>(controllerControl.data()));
    Put(controller,3400,reinterpret_cast<std::uintptr_t>(controllerContext.data()));
    Put(controllerContext,72,reinterpret_cast<std::uintptr_t>(clientValidity.data()));Put(controllerContext,88,reinterpret_cast<std::uintptr_t>(client.data()));
    // Test11's live trace disproved the old direct-model fixture at4800.
    // Exact6C8B3A0/C542130/33E3B90: shared adapter -> two weak model pairs.
    // Breaks caught: decoding adapter bytes as model ID/type, accepting a direct
    // model instead, or dereferencing expired/mismatched/unknown inner owners.
    std::array<std::byte,200> adapter{};std::array<std::byte,16> adapterControl{};
    Put(adapter,0,base+FixtureRva(0xEB3CA70));Put(adapterControl,0,base+FixtureRva(0xE6D8360));Put(adapterControl,8,1);
    Put(adapter,24,reinterpret_cast<std::uintptr_t>(model.data()));Put(adapter,32,reinterpret_cast<std::uintptr_t>(control.data()));
    Put(adapter,184,reinterpret_cast<std::uintptr_t>(model.data()));Put(adapter,192,reinterpret_cast<std::uintptr_t>(control.data()));
    Put(controller,4800,reinterpret_cast<std::uintptr_t>(adapter.data()));Put(controller,4808,reinterpret_cast<std::uintptr_t>(adapterControl.data()));
    Put(model,0,base+FixtureRva(0xE9AA770));Put(model,80,std::uint8_t{255});Put(control,0,base+FixtureRva(0xE6D6060));Put(control,8,1);
    auto controllerSaved=controller;auto contextSaved=controllerContext;auto controllerPairSaved=controllerPair;auto controllerControlSaved=controllerControl;
    auto adapterSaved=adapter;auto adapterControlSaved=adapterControl;auto innerModelSaved=model;auto innerControlSaved=control;
    auto screenBefore=ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),reinterpret_cast<void*>(1),base);
    Check((screenBefore.fields&(ControllerFields|AdapterFields|ModelFields))==(ControllerFields|AdapterFields|ModelFields) && screenBefore.controller==reinterpret_cast<std::uintptr_t>(controller.data()) && screenBefore.model==reinterpret_cast<std::uintptr_t>(model.data()) && screenBefore.modelId==255,"screen-build resolves actual model through exact native adapter before ownership consumption");
    Check(screenBefore.adapter==reinterpret_cast<std::uintptr_t>(adapter.data()) && screenBefore.adapter!=screenBefore.model,"adapter and server-update model are separate correlation labels");
    Check(screenBefore.client==reinterpret_cast<std::uintptr_t>(client.data()) && !(screenBefore.fields&(ScreenResultField|ReadFault)),"screen-build correlates a valid borrowed client without reading uninitialized output");
    Check(controller==controllerSaved && controllerContext==contextSaved && controllerPair==controllerPairSaved && controllerControl==controllerControlSaved,"screen-build observation leaves controller context pair and reference counts unchanged");
    Check(adapter==adapterSaved && adapterControl==adapterControlSaved && model==innerModelSaved && control==innerControlSaved,"adapter observation does not acquire release or modify native weak or strong ownership");
    // Output labels are deliberately unreadable: observing the output pair must
    // not dereference the scene or its control block, or reuse consumed input.
    Put(outputPair,0,std::uintptr_t{1});Put(outputPair,8,std::uintptr_t{3});screenBefore.caller=base+FixtureRva(0x6772382);
    auto screenAfter=ObserveScreenBuild(TracePhase::After,screenBefore,reinterpret_cast<void*>(1),outputPair.data(),base);
    Check((screenAfter.fields&ScreenResultField) && !(screenAfter.fields&ReadFault) && screenAfter.screen==1 && screenAfter.controller==screenBefore.controller && screenAfter.modelId==255 && screenAfter.caller==base+FixtureRva(0x6772382),"post-build copies opaque output label without dereferencing scene or consumed input");
    Put(outputPair,0,std::uintptr_t{});Put(outputPair,8,std::uintptr_t{});
    auto noScreen=ObserveScreenBuild(TracePhase::After,screenBefore,reinterpret_cast<void*>(1),outputPair.data(),base);
    Check((noScreen.fields&ScreenResultField) && noScreen.screen==0,"explicit null screen result remains distinguishable from unobserved output");
    auto badScreen=ObserveScreenBuild(TracePhase::After,screenBefore,nullptr,reinterpret_cast<void*>(1),base);
    Check((badScreen.fields&ReadFault) && !(badScreen.fields&ScreenResultField),"unreadable screen output is bounded and never declared a valid result");
    Put(controller,0,base+1);
    auto foreignScreen=ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),nullptr,base);
    Check(!(foreignScreen.fields&(ControllerFields|ModelFields)),"nonstandard controller remains opaque instead of being classified as chest data");
    auto ignoredOutput=ObserveScreenBuild(TracePhase::After,foreignScreen,reinterpret_cast<void*>(1),reinterpret_cast<void*>(1),base);
    Check(!(ignoredOutput.fields&(ScreenResultField|ReadFault)),"unrelated screen output is not inspected after a rejected controller");
    Put(controller,0,base+FixtureRva(0xE9B1870));Put(controllerControl,8,0);
    auto expiredController=ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),nullptr,base);
    Check(!(expiredController.fields&ControllerFields),"expired incoming controller control never authorizes a screen observation");
    Put(controllerControl,8,1);Put(controllerControl,0,base+1);
    Check(!(ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),nullptr,base).fields&ControllerFields),"unknown controller control table is not treated as the native inline allocation");
    Put(controllerControl,0,base+FixtureRva(0xE99FEB0));clientValidity[0]=std::byte{0};
    auto invalidClient=ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),nullptr,base);
    Check((invalidClient.fields&ControllerFields) && invalidClient.client==0,"invalid NonOwnerPointer never attributes a deleted client to a live controller");
    clientValidity[0]=std::byte{1};Put(model,0,base+1);
    auto unknownModel=ObserveScreenBuild(TracePhase::Before,{},controllerPair.data(),nullptr,base);
    Check((unknownModel.fields&ControllerFields) && !(unknownModel.fields&ModelFields),"verified controller cannot turn an unknown model layout into known inventory data");
    Put(model,0,base+FixtureRva(0xE9AA770));
    Put(controller,4800,reinterpret_cast<std::uintptr_t>(model.data()));Put(controller,4808,reinterpret_cast<std::uintptr_t>(control.data()));
    auto directModel=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check(!(directModel.fields&(AdapterFields|ModelFields)) && directModel.model==0,"standard model at4800 is not an admitted replacement for the actual adapter contract");
    Put(controller,4800,reinterpret_cast<std::uintptr_t>(adapter.data()));Put(controller,4808,reinterpret_cast<std::uintptr_t>(adapterControl.data()));
    Put(adapter,0,base+1);Put(adapter,184,std::uintptr_t{1});Put(adapter,192,std::uintptr_t{1});
    auto unknownAdapter=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check(!(unknownAdapter.fields&(AdapterFields|ModelFields|ReadFault)),"unknown adapter stays opaque even when its fake inner pair is inaccessible");
    adapter=adapterSaved;Put(adapterControl,0,base+1);
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&(AdapterFields|ModelFields)),"unknown adapter control cannot authorize inner model reads");
    Put(adapterControl,0,base+FixtureRva(0xE6D8360));Put(adapterControl,8,0);Put(controller,4800,std::uintptr_t{1});
    auto deadAdapter=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check(!(deadAdapter.fields&(AdapterFields|ModelFields|ReadFault)),"expired adapter is rejected before dereferencing its object");
    Put(adapterControl,8,-1);
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&(AdapterFields|ModelFields|ReadFault)),"negative adapter count is not a live native owner");
    Put(adapterControl,8,1);Put(controller,4800,reinterpret_cast<std::uintptr_t>(adapter.data()));
    Put(adapter,184,std::uintptr_t{1});
    auto mismatchedModel=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check((mismatchedModel.fields&AdapterFields) && !(mismatchedModel.fields&(ModelFields|ReadFault)),"different base and derived model objects are rejected before dereference");
    adapter=adapterSaved;Put(adapter,192,std::uintptr_t{1});
    auto mismatchedControl=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check((mismatchedControl.fields&AdapterFields) && !(mismatchedControl.fields&(ModelFields|ReadFault)),"different base and derived controls are not interchangeable model owners");
    adapter=adapterSaved;Put(control,0,base+1);
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&ModelFields),"unknown model control does not validate an otherwise standard model table");
    Put(control,0,base+FixtureRva(0xE6D6060));Put(control,8,0);Put(adapter,24,std::uintptr_t{1});Put(adapter,184,std::uintptr_t{1});
    auto deadInner=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check((deadInner.fields&AdapterFields) && !(deadInner.fields&(ModelFields|ReadFault)),"expired inner weak pair is rejected before its model pointer is read");
    Put(control,8,-1);
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&(ModelFields|ReadFault)),"negative model strong count cannot be interpreted as live inventory");
    adapter=adapterSaved;Put(control,8,1);Put(adapter,32,std::uintptr_t{});Put(adapter,192,std::uintptr_t{});
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&ModelFields),"matching null inner controls remain unavailable rather than a known empty model");
    adapter=adapterSaved;Put(adapter,24,std::uintptr_t{1});Put(adapter,184,std::uintptr_t{1});
    auto badInner=CaptureController(TraceStage::ScreenBuild,controller.data(),base);
    Check((badInner.fields&ReadFault) && !(badInner.fields&ModelFields),"inaccessible inner model cannot create accepted ID fields");
    adapter=adapterSaved;
    Put(model,0,base+FixtureRva(0xE9AA770));Put(model,80,std::uint8_t{3});Put(controller,0,base+FixtureRva(0xE9A6FD0));
    auto destroying=CaptureController(TraceStage::ControllerDestroy,controller.data(),base);
    Check((destroying.fields&(ControllerFields|AdapterFields|ModelFields))==(ControllerFields|AdapterFields|ModelFields) && destroying.modelId==3 && destroying.client==reinterpret_cast<std::uintptr_t>(client.data()),"base destruction resolves the accepted model through still-borrowed native adapter");
    auto destroyedMarker=destroying;
    DiagnosticForward([&]{adapter.fill(std::byte{});controller.fill(std::byte{});},[&](TracePhase phase){if(phase==TracePhase::Before)destroyedMarker=CaptureController(TraceStage::ControllerDestroy,controller.data(),base);});
    Check(destroyedMarker.modelId==3 && destroyedMarker.adapter==reinterpret_cast<std::uintptr_t>(adapter.data()),"destructor after marker retains scalars without re-reading destroyed adapter or controller");
    controller=controllerSaved;Put(controller,0,base+FixtureRva(0xE9A6FD0));adapter=adapterSaved;
    Check(!(CaptureController(TraceStage::ScreenBuild,controller.data(),base).fields&ControllerFields),"intermediate destructor table cannot masquerade as a constructed screen controller");
    Check(!(CaptureController(TraceStage::Content,controller.data(),base).fields&ControllerFields),"non-controller stages cannot authorize controller layout reads");
    Put(control,8,0);auto expiredOwnedModel=CaptureController(TraceStage::ControllerDestroy,controller.data(),base);
    Check((expiredOwnedModel.fields&ControllerFields) && !(expiredOwnedModel.fields&ModelFields),"expired owned-model control is not decoded during destruction");Put(control,8,1);
    auto badController=CaptureController(TraceStage::ControllerDestroy,reinterpret_cast<void*>(1),base);
    Check((badController.fields&ReadFault)!=0,"inaccessible native controller creates only a bounded diagnostic fault");
    Check(!(ObserveScreenBuild(TracePhase::Before,{},nullptr,nullptr,base).fields&ControllerFields),"missing screen input cannot invent a controller");
    TraceRecord buildScope{},destroyScope{};
    {DiagnosticScope s(TraceStage::ScreenBuild);s.Stamp(buildScope);DiagnosticScope d(TraceStage::ControllerDestroy);d.Stamp(destroyScope);}
    Check(FormatTrace(buildScope).find("stage=screen-build")!=std::string::npos && FormatTrace(destroyScope).find("stage=controller-destroy")!=std::string::npos && destroyScope.parent==buildScope.call,"new lifecycle rows distinguish presentation from destruction with actual nesting");
    screenAfter.stage=TraceStage::ScreenBuild;auto screenLine=FormatTrace(screenAfter);
    Check(screenLine.find("screen=0x1")!=std::string::npos && screenLine.find("controller=0x")!=std::string::npos && screenLine.find("adapter=0x")!=std::string::npos,"formatted lifecycle rows distinguish adapter from model and opaque screen labels");
    // Breaks caught: global/stale preview attribution, cross-thread borrowing,
    // missing UV evidence, wrong atlas layout or writes into renderer resources.
    int previewStack{},otherStack{};
    Check(!PreviewIconScope::Matches(&previewStack) && !PreviewIconScope::Matches(nullptr),"normal icons and null stacks are not attributed to preview");
    {PreviewIconScope previewScope(&previewStack);
        Check(PreviewIconScope::Matches(&previewStack) && !PreviewIconScope::Matches(&otherStack),"only the exact synchronously borrowed preview clone is marked");
        {PreviewIconScope nestedScope(&otherStack);Check(PreviewIconScope::Matches(&otherStack) && !PreviewIconScope::Matches(&previewStack),"nested preview scope replaces its own attribution");}
        bool inherited=true;std::thread worker([&]{inherited=PreviewIconScope::Matches(&previewStack);});worker.join();
        Check(!inherited && PreviewIconScope::Matches(&previewStack),"preview attribution restores after nesting and never crosses native threads");
    }
    Check(!PreviewIconScope::Matches(&previewStack),"preview attribution is removed on scope exit");
    std::array<std::byte,688> renderer{};std::array<std::byte,16> textureControl{};
    std::array<std::byte,192> actorContext{},screen{};std::array<std::byte,704> tessellator{};
    Put(renderer,240,std::uintptr_t{0x12345678});Put(renderer,248,reinterpret_cast<std::uintptr_t>(textureControl.data()));Put(textureControl,8,3);Put(renderer,664,std::uint8_t{1});
    Put(actorContext,40,reinterpret_cast<std::uintptr_t>(screen.data()));Put(screen,184,reinterpret_cast<std::uintptr_t>(tessellator.data()));
    Put(tessellator,672,std::uint8_t{1});Put(tessellator,597,std::uint8_t{0});Put(tessellator,404,std::uint8_t{1});
    auto rendererSaved=renderer;auto textureSaved=textureControl;auto tessSaved=tessellator;
    auto gui=CaptureGuiIcon(renderer.data(),actorContext.data(),12);
    Check((gui.fields&GuiRenderFields) && gui.renderer==reinterpret_cast<std::uintptr_t>(renderer.data()) && gui.renderMode==12 && gui.texturePresent==1 && gui.textureStrong==3 && gui.rendererPath==1,"GUI observation captures actual texture liveness and renderer branch before original drawing");
    Check(gui.tessStarted==1 && gui.tessBusy==0 && gui.tessUv==1 && renderer==rendererSaved && textureControl==textureSaved && tessellator==tessSaved,"tessellator observations are read-only and do not reset native render state");
    std::array<std::byte,32> icon{};Put(icon,4,.125f);Put(icon,8,.1875f);Put(icon,12,.25f);Put(icon,16,.3125f);
    Put(icon,20,std::uint16_t{16});Put(icon,22,std::uint16_t{32});Put(icon,24,std::uint16_t{256});Put(icon,26,std::uint16_t{512});auto iconSaved=icon;
    TraceRecord resolved{},nestedIcon{};
    {IconObservationScope observe(resolved);
        IconObservationScope::Resolved(icon.data(),7,1,base);
        {IconObservationScope nestedObserve(nestedIcon);IconObservationScope::Resolved(nullptr,2,0,base);}
        bool touched=false;std::thread worker([&]{IconObservationScope::Resolved(reinterpret_cast<void*>(1),99,0,base);touched=true;});worker.join();
        Check(touched && !(resolved.fields&ReadFault) && nestedIcon.icon==0 && nestedIcon.iconFrame==2,"nested and foreign-thread icon callbacks cannot overwrite the active GUI sample");
    }
    Check((resolved.fields&IconResultFields) && resolved.uv==std::array<float,4>{.125f,.1875f,.25f,.3125f} && resolved.atlasDimensions==std::array<unsigned,4>{16,32,256,512} && resolved.iconFrame==7 && resolved.iconAnimated==1 && resolved.iconFallback==0,"resolved icon captures independently transcribed native UV and dimension fields");
    Check(icon==iconSaved,"resolved atlas descriptor remains byte-for-byte unchanged");
    TraceRecord fallbackIcon{};
    {IconObservationScope observe(fallbackIcon);IconObservationScope::Resolved(icon.data(),0,0,reinterpret_cast<std::uintptr_t>(icon.data())-0x11D52E58);}
    Check(fallbackIcon.iconFallback==1 && (fallbackIcon.fields&IconResultFields),"the exact native empty fallback descriptor is distinguishable from a resolved item atlas");
    TraceRecord stackSpecific{};
    {IconObservationScope observe(stackSpecific,&previewStack);
        IconObservationScope::Resolved(icon.data(),3,0,base,&otherStack);
        Check(stackSpecific.iconResolves==0,"an unrelated nested resolver cannot masquerade as the sampled GUI stack");
        IconObservationScope::Resolved(icon.data(),4,1,base,&previewStack);
    }
    Check(stackSpecific.iconResolves==1 && stackSpecific.iconFrame==4,"only the matching native stack resolver supplies the GUI atlas evidence");
    auto resolvedSaved=resolved;IconObservationScope::Resolved(reinterpret_cast<void*>(1),1,0,base);
    Check(resolved.fields==resolvedSaved.fields && resolved.icon==resolvedSaved.icon,"out-of-scope resolver results are neither read nor retained");
    TraceRecord badIcon{};{IconObservationScope observe(badIcon);IconObservationScope::Resolved(reinterpret_cast<void*>(1),0,0,base);}
    Check((badIcon.fields&ReadFault) && !(badIcon.fields&IconResultFields),"unreadable returned icon creates bounded diagnostic fault without inventing atlas data");
    auto badGui=CaptureGuiIcon(reinterpret_cast<void*>(1),actorContext.data(),12);
    Check((badGui.fields&ReadFault)!=0,"unreadable render metadata cannot escape the passive observer");
    auto nullGui=CaptureGuiIcon(nullptr,nullptr,12);
    Check(!(nullGui.fields&GuiRenderFields) && nullGui.texturePresent==-1,"missing native render objects remain unknown rather than ready");
    IconSampleBudget budget;bool budgetAccepted=true;for(unsigned i=0;i<32;++i)budgetAccepted=budget.Admit(false)&&budgetAccepted;
    Check(budgetAccepted && !budget.Admit(false) && budget.Admit(true),"normal GUI samples are bounded without exhausting the independent preview budget");
    for(unsigned i=1;i<32;++i)budgetAccepted=budget.Admit(true)&&budgetAccepted;
    Check(budgetAccepted && !budget.Admit(true) && !budget.Admit(false),"both icon sample categories remain permanently bounded per process");
    gui.stage=TraceStage::GuiIcon;gui.previewIcon=true;gui.fields|=resolved.fields;gui.uv=resolved.uv;gui.atlasDimensions=resolved.atlasDimensions;
    auto iconLine=FormatTrace(gui);
    Check(iconLine.find("stage=gui-icon")!=std::string::npos && iconLine.find("preview=1")!=std::string::npos && iconLine.find("uv=0.125,0.1875,0.25,0.3125")!=std::string::npos && iconLine.find("dimensions=16,32,256,512")!=std::string::npos,"formatted GUI rows expose comparable coordinates without item names or NBT");
    // Queue tests catch silent overwrite, reordered entries and unbounded logs.
    DiagnosticBuffer queue;TraceRecord input{};input.container=9;
    bool accepted=true;for(unsigned i=0;i<512;++i)accepted=queue.Push(input)&&accepted;
    Check(accepted,"fixed queue accepts a bounded batch");
    Check(!queue.Push(input) && queue.Dropped()==1,"full queue drops diagnostics, not game operations");
    std::array<TraceRecord,64> batch{};auto n=queue.Drain(batch);
    Check(n==64 && batch.front().sequence==1 && batch.back().sequence==64 && batch[0].container==9,"drain preserves ordered value snapshots without game pointers");
    unsigned total=static_cast<unsigned>(n);while((n=queue.Drain(batch)))total+=static_cast<unsigned>(n);
    Check(total==512,"drain consumes exactly the retained rows");
    accepted=true;for(unsigned i=512;i<8192;++i){accepted=queue.Push(input)&&accepted;queue.Drain(batch);}
    Check(accepted,"capture budget permits bounded subsequent batches");
    Check(!queue.Push(input),"process capture budget prevents unlimited diagnostic log growth");
    TraceRecord formatted{};formatted.stage=TraceStage::Content;formatted.phase=TracePhase::After;formatted.container=9;formatted.count=27;formatted.clientDepth=0;
    auto line=FormatTrace(formatted);Check(line.find("content")!=std::string::npos && line.find("count=27")!=std::string::npos && line.find("clientDepth=0")!=std::string::npos,"log exposes receipt and owner-scope evidence without claiming readiness");
    auto selectedVersion=CurrentVersion();native_detail::active.store(GameVersion::V45);
    {
        // Exact45 creator6883760, controller6E1CEC0/6E16810 and destructor
        //69F72E0 retain E975ED0 at both observed boundaries. A newer state
        //72/88 must not hide the real64/80 validity edge.
        std::array<std::byte,4832> c45{};std::array<std::byte,96> state45{};
        std::array<std::byte,200> a45{};std::array<std::byte,16> ac45{},mc45{},cp45{},cc45{};
        std::array<std::byte,432> m45{};std::array<std::byte,16> client45{};std::uint8_t valid45=1;
        Put(c45,0,base+0xE975ED0);Put(c45,3400,reinterpret_cast<std::uintptr_t>(state45.data()));
        Put(state45,64,reinterpret_cast<std::uintptr_t>(&valid45));Put(state45,80,reinterpret_cast<std::uintptr_t>(client45.data()));Put(client45,0,base+0xE774DB0);
        Put(c45,4800,reinterpret_cast<std::uintptr_t>(a45.data()));Put(c45,4808,reinterpret_cast<std::uintptr_t>(ac45.data()));
        Put(ac45,0,base+0xE6C41B0);Put(ac45,8,1);Put(a45,0,base+0xEB1C8E0);
        Put(a45,24,reinterpret_cast<std::uintptr_t>(m45.data()));Put(a45,32,reinterpret_cast<std::uintptr_t>(mc45.data()));
        Put(a45,184,reinterpret_cast<std::uintptr_t>(m45.data()));Put(a45,192,reinterpret_cast<std::uintptr_t>(mc45.data()));
        Put(mc45,0,base+0xE749DE0);Put(mc45,8,1);Put(m45,0,base+0xEB1D820);Put(m45,80,std::uint8_t{255});
        Put(cc45,0,base+0xE9704E0);Put(cc45,8,1);Put(cp45,0,reinterpret_cast<std::uintptr_t>(c45.data()));Put(cp45,8,reinterpret_cast<std::uintptr_t>(cc45.data()));
        auto before45=ObserveScreenBuild(TracePhase::Before,{},cp45.data(),reinterpret_cast<void*>(1),base);
        Check((before45.fields&(ControllerFields|AdapterFields|ModelFields))==(ControllerFields|AdapterFields|ModelFields) && before45.client==reinterpret_cast<std::uintptr_t>(client45.data()) && before45.modelId==255,"45 six-argument screen-build probe resolves exact state64/80 and both model weak pairs");
        auto destroy45=CaptureController(TraceStage::ControllerDestroy,c45.data(),base);
        Check((destroy45.fields&(ControllerFields|AdapterFields|ModelFields))==(ControllerFields|AdapterFields|ModelFields) && destroy45.client==before45.client,"45 scalar destructor presents the same independently verified intermediate controller table");
        valid45=0;Put(state45,72,reinterpret_cast<std::uintptr_t>(&live));Put(state45,88,reinterpret_cast<std::uintptr_t>(client45.data()));
        Check(CaptureController(TraceStage::ScreenBuild,c45.data(),base).client==0,"45 newer validity and client offsets cannot attribute a retired context");valid45=1;
        Put(a45,24,std::uintptr_t{1});Check(!(CaptureController(TraceStage::ScreenBuild,c45.data(),base).fields&ModelFields),"45 mismatched adapter weak model pairs remain opaque");
        // Native1E7DCE0/1E7EF60 uses656/581/388, not the newer
        // tessellator flags. Poison newer fields to catch accidental reuse.
        std::array<std::byte,672> renderer45{};std::array<std::byte,192> context45{};
        std::array<std::byte,192> screen45{};std::array<std::byte,688> tess45{};
        Put(context45,40,reinterpret_cast<std::uintptr_t>(screen45.data()));Put(screen45,184,reinterpret_cast<std::uintptr_t>(tess45.data()));
        Put(tess45,656,std::uint8_t{1});Put(tess45,581,std::uint8_t{2});Put(tess45,388,std::uint8_t{3});
        Put(tess45,672,std::uint8_t{7});Put(tess45,597,std::uint8_t{8});Put(tess45,404,std::uint8_t{9});
        auto icon45=CaptureGuiIcon(renderer45.data(),context45.data(),12);
        Check((icon45.fields&GuiRenderFields) && icon45.tessStarted==1 && icon45.tessBusy==2 && icon45.tessUv==3,"45 icon diagnostics read exact tessellator flags despite newer field poison");
        Put(tess45,656,std::uint8_t{0});Put(tess45,581,std::uint8_t{0});Put(tess45,388,std::uint8_t{0});
        icon45=CaptureGuiIcon(renderer45.data(),context45.data(),12);
        Check(icon45.tessStarted==0 && icon45.tessBusy==0 && icon45.tessUv==0,"45 inactive tessellator does not borrow newer active flags");
        std::array<std::byte,192> packet45{};std::array<std::byte,96> handler45{};
        Put(handler45,88,reinterpret_cast<std::uintptr_t>(client45.data()));Put(packet45,0,base+0xE7A1A90);
        Put(packet45,48,std::uint8_t{5});Put(packet45,49,std::uint8_t{2});Put(packet45,52,int{-3});Put(packet45,56,int{64});Put(packet45,60,int{9});Put(packet45,64,UINT64_MAX);
        auto open45=CapturePacket(TraceStage::Open,handler45.data(),packet45.data(),base);
        Check((open45.fields&(PacketFields|PositionFields|EntityField))==(PacketFields|PositionFields|EntityField) && open45.client==reinterpret_cast<std::uintptr_t>(client45.data()) && open45.container==5 && open45.x==-3 && open45.entity==UINT64_MAX,"45 exact open packet layout preserves client and target envelope");
        Put(packet45,0,base+0xE834D50);Put(packet45,160,std::uintptr_t{0x100000});Put(packet45,168,std::uintptr_t{0x100A20});Put(packet45,176,std::uintptr_t{0x100A20});
        auto contents45=CapturePacket(TraceStage::Content,handler45.data(),packet45.data(),base);
        Check((contents45.fields&CountField) && contents45.count==27,"45 full-content observation counts native96-byte network descriptors without dereferencing them");
        Put(packet45,0,base+0xE6C2BF0);Put(packet45,52,unsigned{26});auto slot45=CapturePacket(TraceStage::Slot,handler45.data(),packet45.data(),base);
        Check((slot45.fields&SlotField) && slot45.slot==26,"45 exact single-slot packet uses its independent table");
        Put(packet45,0,base+0xE9DBAE0);Check((CapturePacket(TraceStage::ClosePacket,handler45.data(),packet45.data(),base).fields&PacketFields)!=0,"45 native close packet is distinguishable from unrecognized traffic");
        Put(packet45,0,base+0xE7B0D90);Check(!(CapturePacket(TraceStage::Open,handler45.data(),packet45.data(),base).fields&PacketFields),"45 cannot borrow newer packet table identity");
        std::array<std::byte,128> deferred45{};std::array<std::byte,3456> p45{};
        Put(deferred45,0,base+0xE7A0FE0);Put(deferred45,56,std::uint8_t{5});Put(deferred45,57,std::uint8_t{2});Put(deferred45,60,int{-3});Put(deferred45,64,int{64});Put(deferred45,68,int{9});Put(deferred45,72,UINT64_MAX);Put(deferred45,88,reinterpret_cast<std::uintptr_t>(p45.data()));Put(deferred45,120,std::uint8_t{1});
        auto deferredRecord45=CaptureDeferred(deferred45.data(),base);
        Check((deferredRecord45.fields&(PacketFields|PositionFields|EntityField|MayOpenField))==(PacketFields|PositionFields|EntityField|MayOpenField) && deferredRecord45.mayOpen==1 && deferredRecord45.player==reinterpret_cast<std::uintptr_t>(p45.data()) && deferredRecord45.x==-3,"45 deferred native capture retains its independent table and may-open120 guard");
        std::array<std::byte,208> mode45{};Put(mode45,0,base+0xE81A130);Put(mode45,8,reinterpret_cast<std::uintptr_t>(p45.data()));Put(p45,2720,reinterpret_cast<std::uintptr_t>(mode45.data()));
        auto useRecord45=CaptureBlockUse(mode45.data(),nullptr,1,2,3,base);
        Check((useRecord45.fields&UseFields) && useRecord45.player==reinterpret_cast<std::uintptr_t>(p45.data()),"45 block-use frame recognizes native mode before authorizing scoped creator ownership");
        EntityFixture entity45;entity45.Definition("hivesky","death_crate");
        auto entityUse45=ObserveEntityUse(TracePhase::Before,{},mode45.data(),entity45.actor.data(),0,0,base);
        Check((entityUse45.fields&EntityUseFields) && entityUse45.actorKind==DiagnosticActorKind::DeathCrate,"45 attributed entity use recognizes exact game mode before observing the nonplayer definition");
        std::array<std::byte,16> level45{};Put(level45,0,base+0xE79E800);Put(p45,472,reinterpret_cast<std::uintptr_t>(level45.data()));Put(p45,3440,reinterpret_cast<std::uintptr_t>(client45.data()));std::uint64_t uid45=200;
        Check((CaptureEntityFactory(p45.data(),&uid45,base).fields&EntityFactoryFields)!=0,"45 returned-owner factory must bind exact client and client-world tables");
    }
    native_detail::active.store(selectedVersion);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
