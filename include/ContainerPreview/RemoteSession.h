#pragma once
#include "PreviewModel.h"
#include <optional>
namespace preview {
// Opaque identity labels only. This policy never dereferences or retains a
// native reference. The adapter must revalidate live owners before every call.
struct RemoteRequest {
    Request preview;
    std::uint32_t owner{};
    std::uintptr_t client{},player{};
    unsigned slots{},blockType{};
    bool integrated{};
    bool hiveProxy{}; // Exact definition admission is the native adapter's job.
};
struct RemoteOpen {
    Target target;
    std::uint32_t owner{};
    std::uintptr_t client{},player{};
    unsigned id{},type{};
    std::uint64_t entity{UINT64_MAX};
    bool mayOpen{};
};
enum class RemoteAction {None,Open,Close};
enum class RemotePhase {Idle,Pending,Active,Closing};
class RemoteSession {
public:
    RemoteAction Step(std::optional<RemoteRequest> const&,std::uint64_t now,bool held=false);
    bool Matches(RemoteOpen const&) const;
    bool Created(std::uintptr_t model,std::uintptr_t control,std::uint64_t entity=UINT64_MAX);
    bool Accepted(unsigned id,std::uintptr_t model,std::uintptr_t control);
    bool Applied(unsigned id,std::uintptr_t model,unsigned count,std::uint64_t slotMask);
    bool CloseReply(std::uintptr_t client,std::uint32_t owner,unsigned id);
    RemoteAction Cancel();
    void Relinquish();
    bool BeginNativeUse();
    // Caller must prove its native entry has not been invoked. Never use for
    // native return0, timeout, denial, cancellation or a possible late reply.
    bool DeferUnsubmitted();
    RemotePhase Phase() const {return phase_;}
    bool Visible() const {return phase_==RemotePhase::Active && wanted_ && ready_;}
    RemoteRequest const& RequestValue() const {return request_;}
    std::uintptr_t Model() const {return model_;}
    std::uintptr_t Control() const {return control_;}
    unsigned Id() const {return id_;}
    std::uint64_t Entity() const {return entity_;}
private:
    RemotePhase phase_{};
    RemoteRequest request_{};
    std::optional<Request> attempted_;
    std::uintptr_t model_{},control_{};
    unsigned id_{255};
    std::uint64_t entity_{UINT64_MAX};
    std::uint64_t since_{};
    bool wanted_{},ready_{},manualFence_{},nativeStarted_{};
};
}
