#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
namespace preview {
enum class GameVersion { Unknown, V45, V51, V52 };
struct NativeAddress {std::uint32_t reference,target;};
namespace native_detail {
#include "NativeAddresses.inl"
#include "NativeAddresses45.inl"
// A single validated process profile is pinned before hooks are installed.
// Baseline fixtures use V52; no native operation is enabled by this default.
inline std::atomic<GameVersion> active{GameVersion::V52};
}
inline std::uint32_t NativeRva(GameVersion version,std::uint32_t reference) noexcept {
    // Display-diagnostic sentinels in zero-filled native data, not callables.
    // IDA 1B86DA0/1B872A0 (52), 1B87090/1B87590 (51): independently equal.
    if((version==GameVersion::V52 || version==GameVersion::V51) && (reference==0x11D52E58 || reference==0x11E450E0))return reference;
    auto all=version==GameVersion::V45?std::span<NativeAddress const>(native_detail::addresses45):std::span<NativeAddress const>(native_detail::addresses);
    auto p=std::lower_bound(all.begin(),all.end(),reference,
        [](NativeAddress const& value,std::uint32_t key){return value.reference<key;});
    if(p==all.end() || p->reference!=reference)return 0;
    if(version==GameVersion::V52)return reference;
    if(version==GameVersion::V51 || version==GameVersion::V45)return p->target;
    return 0;
}
inline GameVersion CurrentVersion() noexcept {return native_detail::active.load(std::memory_order_acquire);}
inline std::uint32_t Rva(std::uint32_t reference) noexcept {return NativeRva(CurrentVersion(),reference);}
inline char const* VersionName(GameVersion version) noexcept {
    switch(version){case GameVersion::V45:return "1.26.45.01";case GameVersion::V51:return "1.26.51.01";
        case GameVersion::V52:return "1.26.52.03";default:return "unsupported";}
}
}
