#pragma once
#include "NativeProfile.h"
#include "PreviewBridge.h"
namespace preview {
struct ClientObservation {Target target;ActorIdentity identity;bool hud{};};
Target DecodeHit(Target context,void const* hit,ActorIdentity entity,bool player) noexcept;
// Borrowed only for the duration of the current client/render callback.
void* ReadClientSceneStack(void* client) noexcept;
bool NativeHud(NativeProfile const&,void* client) noexcept;
ClientObservation ObserveClient(NativeProfile const&,void* client) noexcept;
struct ServerObservation {ActorIdentity identity;BorrowedContents contents;bool paired{};};
ServerObservation ObserveServer(NativeProfile const&,void* level,PreviewWork const&) noexcept;
}
