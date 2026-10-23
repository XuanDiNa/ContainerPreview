#pragma once
#include "NativeContainers.h"
#include "NativeState.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
namespace preview {
struct PreviewWork {Request request;ActorIdentity local;bool paired{};unsigned backgroundType{};};
class PreviewBridge {
public:
    explicit PreviewBridge(StackOps ops):ops_(ops){}
    void Update(Config const&,Eligibility,Target const&,ActorIdentity,std::uint64_t now);
    std::optional<PreviewWork> Work(std::uint64_t now);
    bool Publish(PreviewWork const&,ActorIdentity,std::uintptr_t serverWorld,BorrowedContents const&,std::uint32_t thread,std::uint64_t now);
    bool AuthorizeRemote(PreviewWork const&,RemoteLease,std::uint64_t now);
    bool BeginBackground(PreviewWork const&,std::uint32_t owner,std::uint64_t now);
    bool PublishRemote(PreviewWork const&,RemoteLease,BorrowedContents const&,std::uint32_t thread,std::uint64_t now);
    void RevokeRemote();
    void Cleanup(std::uint32_t thread,std::uint64_t now);
    void View(std::uint64_t now,std::function<void(Display,Snapshot const&,OwnedStacks const&)> const&);
private:
    bool CopyContents(BorrowedContents const&,std::uintptr_t world,std::uint32_t thread);
    struct Pool {std::uintptr_t world{};std::uint32_t thread{};std::unique_ptr<OwnedStacks> stacks;};
    std::mutex mutex_;
    PreviewModel model_;
    ActorIdentity local_{};
    Snapshot snapshot_{};
    RemoteLease remote_;
    Request remoteRequest_;
    Request pairedRequest_,reservation_;
    std::uint64_t pairedTime_{};
    unsigned backgroundType_{};
    std::uint32_t reservationOwner_{};
    StackOps ops_;
    Pool active_;
    // If a server owner disappears before cleanup, keep native values pinned.
    // Never destroy them from the new world/render/worker thread.
    std::array<Pool,8> retired_{};
};
}
