#pragma once
#include "RemoteDiagnostics.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <type_traits>
namespace preview {
enum class TraceStage {Client,Open,DeferredOpen,Scene,Factory,Content,Slot,ClosePacket,ClosePlayer,ModelBinding,GuiIcon,BlockUse,ModelApply,ScreenBuild,ControllerDestroy,RemoteGate,EntityUse,EntityFactory,EntityResolve};
enum class TracePhase {Before,After,Sample};
enum class DiagnosticActorKind {Unknown,Other,DeathCrate,Player,ChestMinecart};
enum TraceField : unsigned {PacketFields=1,PositionFields=2,EntityField=4,CountField=8,
    SlotField=16,SceneField=32,ModelFields=64,MayOpenField=128,ReadFault=256,
    BindingField=512,InputModelFields=1024,GuiRenderFields=2048,IconResultFields=4096,UseFields=8192,
    ControllerFields=16384,ScreenResultField=32768,AdapterFields=65536,EntityUseFields=131072,NativeResultField=262144,EntityFactoryFields=524288};
// Values only. Numeric addresses are correlation labels, never retained owners.
struct TraceRecord {
    std::uint64_t sequence{},time{},call{},parent{},scopedClient{},client{},world{},player{},model{},entity{},inputModel{},caller{};
    std::uint32_t owner{},depth{},clientDepth{},fields{};
    TraceStage stage{};TracePhase phase{};
    int container{-1},type{-1},x{},y{},z{},count{-1},slot{-1},event{-1},modelId{-1},modelType{-1},mayOpen{-1};
    int inputId{-1},inputType{-1};
    std::array<int,3> useArgs{-1,-1,-1};
    std::uint64_t actor{},actorTable{},entityRuntime{},interactionArg{},interactionResult{};
    int targetPlayer{-1},actorFlags{-1};
    DiagnosticActorKind actorKind{DiagnosticActorKind::Unknown};
    std::uint64_t requestedEntity{};
    std::uint64_t renderer{},icon{};
    std::uint64_t controller{},screen{},adapter{};
    std::array<float,4> uv{};
    std::array<unsigned,4> atlasDimensions{};
    int renderMode{-1},texturePresent{-1},textureStrong{-1},rendererPath{-1},tessStarted{-1},tessBusy{-1},tessUv{-1};
    int iconFallback{-1},iconFrame{-1},iconAnimated{-1},iconResolves{};
    bool previewIcon{};
    bool held{},foreground{};
    RemoteGateSample remoteGate{};
};
static_assert(std::is_trivially_copyable_v<TraceRecord>);
class DiagnosticScope {
public:
    explicit DiagnosticScope(TraceStage,std::uintptr_t client=0) noexcept;
    ~DiagnosticScope();
    DiagnosticScope(DiagnosticScope const&)=delete;
    DiagnosticScope& operator=(DiagnosticScope const&)=delete;
    void Stamp(TraceRecord&) const noexcept;
private:
    std::uint64_t previousCall_{},previousClient_{},call_{};
    unsigned previousDepth_{},previousClientDepth_{};
    TraceStage stage_;
};
template<class Original,class Observer,class... Args>
auto DiagnosticForward(Original original,Observer observe,Args... args)->decltype(original(args...)) {
    try{observe(TracePhase::Before);}catch(...){}
    if constexpr(std::is_void_v<decltype(original(args...))>){
        original(args...);
        try{observe(TracePhase::After);}catch(...){}
    }else{
        auto result=original(args...);
        try{observe(TracePhase::After);}catch(...){}
        return result;
    }
}
class DiagnosticBuffer {
public:
    bool Push(TraceRecord) noexcept;
    std::size_t Drain(std::span<TraceRecord>) noexcept;
    std::uint64_t Dropped() const {return dropped_.load();}
private:
    std::array<TraceRecord,512> records_{};
    std::mutex mutex_;
    std::size_t head_{},size_{};
    std::uint64_t accepted_{};
    std::atomic<std::uint64_t> dropped_{};
};
TraceRecord CapturePacket(TraceStage,void const* handler,void const* packet,std::uintptr_t base) noexcept;
TraceRecord CaptureDeferred(void const* capture,std::uintptr_t base) noexcept;
TraceRecord CaptureScene(void const* listener,void const* variant,std::uintptr_t base) noexcept;
TraceRecord CapturePlayer(void const* player,std::uintptr_t base) noexcept;
TraceRecord CaptureFactory(void const* player,void const* position,void const* blockType,std::uintptr_t base) noexcept;
// Before: borrow input pair. After: reuse those values and inspect only the
// still-borrowed player argument; the original consumes the input ownership.
TraceRecord ObserveModelBinding(TracePhase,TraceRecord,void const* player,void const* inputPair,std::uintptr_t base) noexcept;
TraceRecord CaptureBlockUse(void const* gameMode,void const* position,unsigned char arg3,unsigned char arg4,unsigned char arg5,std::uintptr_t base) noexcept;
// Before borrows mode/actor synchronously. After copies only cached numeric
// values and native result: the original may invalidate either native object.
TraceRecord ObserveEntityUse(TracePhase,TraceRecord,void const* gameMode,void const* actor,std::uintptr_t arg4,std::uint64_t result,std::uintptr_t base) noexcept;
// Entity factory borrows its player/id only before native construction. The
// resolver observation consumes the actual native return, never calls lookup
// again, and never treats equal/timely numeric IDs as ownership authorization.
TraceRecord CaptureEntityFactory(void const* player,void const* entity,std::uintptr_t base) noexcept;
TraceRecord CaptureEntityResolution(TraceRecord const& factory,void const* level,std::uint64_t requested,void const* actor,std::uintptr_t base) noexcept;
class EntityFactoryScope {
public:
    explicit EntityFactoryScope(TraceRecord const&) noexcept;
    ~EntityFactoryScope();
    EntityFactoryScope(EntityFactoryScope const&)=delete;
    EntityFactoryScope& operator=(EntityFactoryScope const&)=delete;
    static TraceRecord const* Current() noexcept;
private:TraceRecord const* previous_{};
};
// Borrow only the setter's model and numeric slot. Never inspect its stack.
TraceRecord CaptureModelApply(void const* model,unsigned slot,std::uintptr_t base) noexcept;
// Synchronous borrowed controller -> exact adapter -> matching weak model pairs.
// No reference acquisition. Destruction callers cache scalars before native
// return; neither controller nor adapter is readable after destruction.
TraceRecord CaptureController(TraceStage,void const* controller,std::uintptr_t base) noexcept;
// Native screen construction consumes input ownership. Before reads input only;
// after reads the borrowed output pair only and never dereferences its objects.
TraceRecord ObserveScreenBuild(TracePhase,TraceRecord,void const* inputPair,void const* outputPair,std::uintptr_t base) noexcept;
// Scopes are synchronous/thread-local. Only copied scalar metadata reaches logs.
class PreviewIconScope {
public:
    explicit PreviewIconScope(void const*) noexcept;
    ~PreviewIconScope();
    PreviewIconScope(PreviewIconScope const&)=delete;
    PreviewIconScope& operator=(PreviewIconScope const&)=delete;
    static bool Matches(void const*) noexcept;
private:void const* previous_{};
};
class IconObservationScope {
public:
    explicit IconObservationScope(TraceRecord&,void const* stack=nullptr) noexcept;
    ~IconObservationScope();
    IconObservationScope(IconObservationScope const&)=delete;
    IconObservationScope& operator=(IconObservationScope const&)=delete;
    static void Resolved(void const*,unsigned,unsigned,std::uintptr_t,void const* stack=nullptr) noexcept;
private:TraceRecord* previous_{};void const* previousStack_{};
};
class IconSampleBudget {
public:bool Admit(bool preview) noexcept;
private:std::atomic<unsigned> normal_{},preview_{};
};
TraceRecord CaptureGuiIcon(void const* renderer,void const* actorContext,unsigned mode) noexcept;
std::string FormatTrace(TraceRecord const&);
}
