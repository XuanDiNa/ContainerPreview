#pragma once
#include "NativeWorld.h"
#include "RemoteSession.h"
#include "RemoteDiagnostics.h"
namespace preview {
struct RemoteRoute {std::uintptr_t client{},network{},handler{};bool integrated{};};
// Read-only synchronous decoders. Returned pointers are labels, not ownership.
std::optional<RemoteRoute> DecodeRemoteRoute(void const* client,std::uintptr_t base,RemoteCheck* check=nullptr) noexcept;
struct RemoteShape {unsigned slots{},blockType{};};
std::optional<RemoteShape> DecodeRemoteChest(void const* block,std::uintptr_t base,BlockPosition,RemoteCheck* check=nullptr) noexcept;
struct RemoteModel {std::uintptr_t model{},control{};unsigned id{};};
std::optional<RemoteModel> DecodeRemoteModel(void const* player,std::uintptr_t base,Target const&,unsigned blockType=2,std::uint64_t entity=UINT64_MAX) noexcept;
bool BackgroundEligible(bool integrated,PreviewWork const&,RemoteShape,RemoteRequest const* owned) noexcept;
bool BackgroundInputAllowed(void const* player,bool shiftHeld,RemoteCheck* check=nullptr) noexcept;
bool MatchesRemoteConsistency(RemoteSession const&,void const* stack,std::uintptr_t base,std::uintptr_t caller,std::uint32_t owner) noexcept;
struct NativePair {void* object{};void* control{};};
bool ReleaseModelWeak(NativePair&,std::uintptr_t base,void(*destroy)(void*)) noexcept;
bool MatchesRemoteCreator(void const* functor,void const* factory,void const* name,void const* player,
    void const* position,void const* entity,void const* output,std::uintptr_t base,RemoteRequest const&,std::uint64_t returnedEntity=UINT64_MAX) noexcept;
//45's ten-argument creator carries three additional borrowed owners. Never
// route it through the seven-argument51/52 ABI or omit their validation.
bool MatchesRemoteCreator45(void const* functor,void const* factory,void const* game,void const* client,
    void const* factoryView,void const* metadata,void const* player,void const* position,void const* entity,
    void const* output,std::uintptr_t base,RemoteRequest const&,std::uint64_t returnedEntity=UINT64_MAX) noexcept;
// heldProtection requires the exact planner/empty predicate hooks plus an
// EntityUseScope spanning this synchronous operation. It is not a bypass flag.
bool DecodeRemoteEntityUse(void const* mode,void const* player,void const* hit,void const* hand,void const* actor,
    std::uintptr_t base,Target const&,int gameType,RemoteCheck* check=nullptr,bool heldProtection=false) noexcept;
using RemoteEntityUseEntry=std::uint64_t(*)(void*,void*,void const*,std::uintptr_t);
bool InvokeRemoteEntityUse(RemoteEntityUseEntry,void* mode,void* actor,void const* hitVector) noexcept;
bool MatchesEntityOpenAction(void const* output,void const* actor,void const* player,std::uintptr_t base) noexcept;
bool RetireEntityAction(void* output,void(*destroy)(void*),void(*freeTitle)(void*)) noexcept;
bool MatchesHiveProxyOwner(void const* actor,std::uintptr_t base,Target const&,std::uint64_t returnedEntity) noexcept;
struct RemoteUse {unsigned char face{};std::uint32_t entry{};std::uintptr_t item{};};
// heldProtection is a caller contract: all three exact held-use hooks must be
// installed, with HeldUseScope covering the synchronous native operation.
bool DecodeRemoteUse(void const* gameMode,void const* player,void const* hit,void const* hand,std::uintptr_t base,Target const&,int gameType,RemoteUse&,RemoteCheck* check=nullptr,bool heldProtection=false) noexcept;
using RemoteUseEntry=std::uint64_t(*)(void*,void const*,unsigned char,unsigned char,unsigned char);
// Completion only, never a container-open acknowledgment. Catch a native fault
// below caller-owned TLS/RAII scopes so those scopes can retire normally.
bool InvokeRemoteUse(RemoteUseEntry,void* gameMode,BlockPosition const*,unsigned char face) noexcept;
struct RegistryOps {
    void*(*get)(void*,void*){};
    void*(*enter)(void*,void*){};
    void(*leave)(void*){};
};
bool InNativeRegistry(RegistryOps,void* client,bool(*operation)(void*),void* state,RemoteCheck* check=nullptr) noexcept;
}
