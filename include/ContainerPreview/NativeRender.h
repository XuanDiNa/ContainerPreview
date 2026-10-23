#pragma once
#include "NativeProfile.h"
#include "NativeState.h"
#include <string_view>
namespace preview {
struct UiRect {float left,right,top,bottom;};
struct UiColor {float r,g,b,a;};
struct NativeText {char data[16]{};std::uint64_t length{},capacity{15};};
static_assert(sizeof(NativeText)==32);
bool MakeText(NativeText&,std::string_view);
using InventoryItemDraw=void(*)(void*,void*,void*,unsigned,float,float,unsigned char,float,float,float,int);
// Native complete base-icon pass; kept separate to test the external ABI boundary.
void DrawInventoryIcon(InventoryItemDraw,void* renderer,void* actorContext,void const* stack,float x,float y,float scale);
struct RenderSink {
    void* user{};
    void (*fill)(void*,UiRect const&,UiColor const&){};
    void (*text)(void*,UiRect const&,NativeText&){};
    void (*item)(void*,void const*,float,float,float){};
    // Reconstruct and release display-only stacks in the current client registry.
    StackOps itemCopies{};
};
bool DrawPreview(RenderSink const&,Display,Snapshot const&,OwnedStacks const&,float width,float height);
bool RenderNative(NativeProfile const&,void* context,Display,Snapshot const&,OwnedStacks const&,StackOps) noexcept;
}
