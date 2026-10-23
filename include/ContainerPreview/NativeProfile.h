#pragma once
#include "NativeAddress.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace preview {
struct ImageIdentity {std::uint16_t machine{};std::uint32_t timestamp{},imageSize{},checksum{};};
bool AcceptedIdentity(ImageIdentity const&) noexcept;
bool MatchEntry(std::span<std::byte const>,std::span<std::byte const>) noexcept;
bool VerifyFile(std::span<std::byte const>) noexcept;
bool VerifyLoadedHeaders(std::span<std::byte const> disk,std::span<std::byte const> loaded,std::uintptr_t moduleBase) noexcept;
enum class ProfileFailure {None,Module,Path,Open,Map,Disk,LoadedRead,LoadedHeaders,EntryRead,EntryMismatch};
struct ProfileDiagnostic {
    ProfileFailure failure{ProfileFailure::None};
    std::uint32_t systemError{},rva{};
    std::uintptr_t moduleBase{},headerBase{};
    std::array<std::byte,32> expected{},observed{};
};
bool VerifyLoadedEntry(std::uint32_t rva,std::span<std::byte const> actual,std::span<std::byte const> expected,ProfileDiagnostic&) noexcept;
std::string DescribeProfileDiagnostic(ProfileDiagnostic const&);
enum class EntryGuardScope {LoadedRequired,DiskEvidence};
struct EntryGuard {
    std::uint32_t rva;std::array<std::uint8_t,32> bytes;
    EntryGuardScope scope{EntryGuardScope::LoadedRequired};
};
std::span<EntryGuard const> NativeGuards(GameVersion version=CurrentVersion());
bool VerifyLoadedGuards(GameVersion version,std::uintptr_t moduleBase,ProfileDiagnostic&) noexcept;
class NativeProfile {
public:
    // The module and read-only disk mapping remain pinned for process lifetime.
    bool Initialize(void* module,ProfileDiagnostic* diagnostic=nullptr) noexcept;
    bool Entry(std::uintptr_t function) const noexcept;
    bool Table(void const* object,std::uint32_t rva) const noexcept;
    std::uintptr_t Virtual(void const* object,unsigned byteOffset) const noexcept;
    std::uintptr_t Base() const {return base_;}
    template<class T>T Function(std::uint32_t rva) const {auto target=Rva(rva);return target?reinterpret_cast<T>(base_+target):nullptr;}
    template<class T>T GuardedMethod(std::uint32_t rva,void const* object,std::uint32_t table) const {return Table(object,table)?Function<T>(rva):nullptr;}
private:
    std::uintptr_t base_{};
    std::byte const* disk_{};
    std::size_t diskSize_{},textOffset_{},textSize_{};
    struct Section {std::uint32_t rva,raw,size,flags;};
    std::array<Section,96> sections_{};
    unsigned sectionCount_{};
};
}
