#pragma once
#include <array>
#include <cstdint>
namespace preview {
struct BlockPosition {int x{},y{},z{};};
struct BorrowedContents {
    bool ready{};
    unsigned slots{};
    std::array<void const*,54> items{};
    bool recognized{};
    // Value-only native interaction eligibility, not a pointer or contents.
    unsigned backgroundType{};
};
// Borrowed values are only consumed synchronously by the verified world owner
// callback. Never publish this result to the rendering thread.
BorrowedContents InspectBlock(void const*,std::uintptr_t image,BlockPosition) noexcept;
BorrowedContents InspectEntityComponent(void const*) noexcept;
}
