#pragma once
#include "PreviewModel.h"
#include <array>
#include <cstddef>
#include <span>
namespace preview {
struct ActorIdentity {
    std::uintptr_t registry{};
    std::uint64_t unique{}, runtime{};
    bool local{}, server{};
};
bool SameIntegratedPlayer(ActorIdentity const& local, ActorIdentity const& server);
std::uintptr_t ReadComponent(void const* actor,std::uint32_t hash,std::size_t stride) noexcept;
ActorIdentity ReadIdentity(void const* actor) noexcept;
bool IsHiveDeathCrate(void const* actor) noexcept;
struct StackOps {
    bool (*copy)(void* destination,void const* source){};
    bool (*destroy)(void* destination){};
};
class OwnedStacks {
public:
    explicit OwnedStacks(StackOps ops):ops_(ops){}
    OwnedStacks(OwnedStacks const&)=delete;
    OwnedStacks& operator=(OwnedStacks const&)=delete;
    // No native calls during C++ teardown. Clear only from the native owner
    // callback; the plugin intentionally pins its runtime for process lifetime.
    bool Replace(std::span<void const* const>,std::uint32_t thread);
    bool Clear(std::uint32_t thread);
    void const* At(unsigned slot) const;
    unsigned Size() const {return size_;}
    bool Faulted() const {return faulted_;}
private:
    struct alignas(8) Stack {std::array<std::byte,152> bytes;};
    std::array<Stack,54> stacks_{};
    StackOps ops_;
    std::uint32_t owner_{};
    unsigned size_{};
    bool faulted_{};
};
}
