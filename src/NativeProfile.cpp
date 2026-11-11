#include "ContainerPreview/NativeProfile.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
namespace preview {
namespace {
#include "NativeGuards.inl"
#include "NativeGuards51.inl"
#include "NativeGuards45.inl"
struct ImageSpec {
    GameVersion version;ImageIdentity identity;std::size_t size,textSize;
    char const* hash;char const* textHash;char const* alternateHash{};
};
constexpr ImageSpec images[]={
    {GameVersion::V45,{0x8664,0x6A8378BA,0x12888000,0x12568F46},307657080,240686080,
        "008408E6F544D7987F9CDE4E34C8E853690A65AFD97F8498DA2CD6ED24C4793B","194006ACBAE5371133A09DC727E1176233D304A9E7A0F7AFC28D73E41D37F685"},
    {GameVersion::V45,{0x8664,0x6A8378BA,0x12889000,0x1256BF65},307659128,240686080,
        "4143182F113D774062809039154248E7AF7578A81C92E54323F7D53361F8EFA1","194006ACBAE5371133A09DC727E1176233D304A9E7A0F7AFC28D73E41D37F685"},
    {GameVersion::V52,{0x8664,0x6AB54E37,0x12C01000,0x128CD801},311215984,240802816,
        "4A92BFA3CE2428B40EE517B7C1125D9F6F79274382DE03846351992663B2E2E4","6FA218D247874940235AACB8FD7E3BBF0E3D1E259630D7F83FCB536F06733AEB"},
    {GameVersion::V52,{0x8664,0x6AB54E37,0x12C02000,0x128DA293},311218032,240802816,
        "0B66101DC873434E853A898329CB85CEE0DE52D0AE91AC796EFA0FE05B61522C","6FA218D247874940235AACB8FD7E3BBF0E3D1E259630D7F83FCB536F06733AEB"},
    {GameVersion::V51,{0x8664,0x6AA482FD,0x12C0C000,0x128E3A0F},311249032,240802816,
        "258EC563FD37B3EFCF67C2C5207AA0DF6AB8E20DC09ACF2394380D697CC24DAC","0CE1026651411CA94636B2B713F4B7D6838ADC3D652A85DDCCDF54E53DE0B4F5"},
    {GameVersion::V51,{0x8664,0x6AA482FD,0x12C0D000,0x128DA158},311251456,240802816,
        "1ECE1D37E7762F6B32C20DB355A7F0B199A85FC6A60C59CF693922C999A16AE7","0CE1026651411CA94636B2B713F4B7D6838ADC3D652A85DDCCDF54E53DE0B4F5",
        // Same exact PE identity and all original section bytes; the reviewed
        // launcher-only import section differs. Both full-file hashes stay pinned.
        "E3BD6B85F5F37C7814B131BA4E0C412957397E2804B71871E6180C773F69605B"},
};
ImageSpec const* ImageFor(ImageIdentity const& id){
    for(auto const& s:images){auto const& k=s.identity;
        if(id.machine==k.machine && id.timestamp==k.timestamp && id.imageSize==k.imageSize && id.checksum==k.checksum)return &s;}
    return nullptr;
}
bool Hash(std::span<std::byte const> data,char const* expected){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE handle{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    DWORD objectSize{},used{};
    bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&used,0)>=0;
    std::vector<std::uint8_t> object(ok?objectSize:0);std::array<std::uint8_t,32> digest{};
    if(ok)ok=BCryptCreateHash(algorithm,&handle,object.data(),objectSize,nullptr,0,0)>=0;
    if(ok)ok=BCryptHashData(handle,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(data.data())),static_cast<ULONG>(data.size()),0)>=0;
    if(ok)ok=BCryptFinishHash(handle,digest.data(),32,0)>=0;
    if(handle)BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr char hex[]="0123456789ABCDEF";
    for(unsigned i=0;ok && i<32;++i)ok=hex[digest[i]>>4]==expected[i*2] && hex[digest[i]&15]==expected[i*2+1];
    return ok;
}
IMAGE_NT_HEADERS64 const* Headers(std::span<std::byte const> data,std::uintptr_t loadedBase=0){
    if(data.size()<sizeof(IMAGE_DOS_HEADER))return nullptr;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER const*>(data.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<64 || dos->e_lfanew>4096 || data.size()<static_cast<std::size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64))return nullptr;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(data.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
       nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) ||
       (nt->OptionalHeader.ImageBase!=0x140000000 && (!loadedBase || nt->OptionalHeader.ImageBase!=loadedBase)) ||
       !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96)return nullptr;
    auto end=static_cast<std::size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)+nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER);
    return end<=data.size()?nt:nullptr;
}
bool ReadMemory(void const* from,void* to,std::size_t size) noexcept{
    __try {if(!from)return false;std::memcpy(to,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
bool AcceptedIdentity(ImageIdentity const& id) noexcept{
    return ImageFor(id)!=nullptr;
}
bool MatchEntry(std::span<std::byte const> actual,std::span<std::byte const> expected) noexcept{
    return expected.size()>=16 && expected.size()<=32 && actual.size()>=expected.size() && std::equal(expected.begin(),expected.end(),actual.begin());
}
bool VerifyFile(std::span<std::byte const> data) noexcept{
    try{
        auto nt=Headers(data);if(!nt)return false;
        auto spec=ImageFor({nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,nt->OptionalHeader.CheckSum});
        if(!spec || data.size()!=spec->size)return false;
        auto section=IMAGE_FIRST_SECTION(nt);bool found=false;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
            auto const& s=section[i];
            if(s.PointerToRawData>data.size() || s.SizeOfRawData>data.size()-s.PointerToRawData)return false;
            if(std::memcmp(s.Name,".text\0\0\0",8))continue;
            if(found || s.VirtualAddress!=0x1000 || s.SizeOfRawData!=spec->textSize || !(s.Characteristics&IMAGE_SCN_MEM_EXECUTE))return false;
            found=Hash(data.subspan(s.PointerToRawData,s.SizeOfRawData),spec->textHash);
            if(!found)return false;
            for(auto const& guard:NativeGuards(spec->version)){
                if(guard.rva<0x1000 || guard.rva-0x1000>s.SizeOfRawData-32 || !MatchEntry(data.subspan(s.PointerToRawData+guard.rva-0x1000,32),std::as_bytes(std::span(guard.bytes))))return false;
            }
        }
        return found && (Hash(data,spec->hash) || (spec->alternateHash && Hash(data,spec->alternateHash)));
    }catch(...){return false;}
}
std::span<EntryGuard const> NativeGuards(GameVersion version){
    if(version==GameVersion::V52)return guards;
    if(version==GameVersion::V51)return guards51;
    if(version==GameVersion::V45)return guards45;
    return {};
}
bool VerifyLoadedGuards(GameVersion version,std::uintptr_t base,ProfileDiagnostic& diagnostic) noexcept{
    if(!base || NativeGuards(version).empty()){diagnostic.failure=ProfileFailure::Module;return false;}
    for(auto const& g:NativeGuards(version)){
        // DiskEvidence is not a runtime call/hook exemption. Only the audited
        // ClientInstance constructor has that scope; VerifyFile still checks
        // its bytes and the complete image/text hashes. Entry() stays exact.
        if(g.scope==EntryGuardScope::DiskEvidence)continue;
        std::array<std::byte,32> actual{};
        if(!ReadMemory(reinterpret_cast<void const*>(base+g.rva),actual.data(),actual.size())){
            diagnostic.failure=ProfileFailure::EntryRead;diagnostic.rva=g.rva;return false;
        }
        if(!VerifyLoadedEntry(g.rva,actual,std::as_bytes(std::span(g.bytes)),diagnostic))return false;
    }
    return true;
}
bool VerifyLoadedHeaders(std::span<std::byte const> disk,std::span<std::byte const> loaded,std::uintptr_t moduleBase) noexcept{
    auto nt=Headers(disk),live=Headers(loaded,moduleBase);
    if(!moduleBase || !nt || !live)return false;
    // Windows may rewrite only the loaded ImageBase to the ASLR address.
    // Normalize that value, then compare the complete NT headers and sections.
    auto normalized=*live;normalized.OptionalHeader.ImageBase=nt->OptionalHeader.ImageBase;
    auto diskDos=reinterpret_cast<IMAGE_DOS_HEADER const*>(disk.data());
    auto liveDos=reinterpret_cast<IMAGE_DOS_HEADER const*>(loaded.data());
    return diskDos->e_lfanew==liveDos->e_lfanew &&
        !std::memcmp(nt,&normalized,sizeof normalized) &&
        !std::memcmp(IMAGE_FIRST_SECTION(nt),IMAGE_FIRST_SECTION(live),nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER));
}
bool VerifyLoadedEntry(std::uint32_t rva,std::span<std::byte const> actual,std::span<std::byte const> expected,ProfileDiagnostic& diagnostic) noexcept{
    if(MatchEntry(actual,expected))return true;
    diagnostic.failure=ProfileFailure::EntryMismatch;diagnostic.rva=rva;
    diagnostic.expected={};diagnostic.observed={};
    std::copy_n(expected.begin(),std::min(expected.size(),diagnostic.expected.size()),diagnostic.expected.begin());
    std::copy_n(actual.begin(),std::min(actual.size(),diagnostic.observed.size()),diagnostic.observed.begin());
    return false;
}
std::string DescribeProfileDiagnostic(ProfileDiagnostic const& d){
    char const* stage="unknown";
    switch(d.failure){
        case ProfileFailure::None:stage="validated";break;
        case ProfileFailure::Module:stage="invalid-module";break;
        case ProfileFailure::Path:stage="module-path";break;
        case ProfileFailure::Open:stage="image-open";break;
        case ProfileFailure::Map:stage="image-mapping";break;
        case ProfileFailure::Disk:stage="disk-identity";break;
        case ProfileFailure::LoadedRead:stage="loaded-header-read";break;
        case ProfileFailure::LoadedHeaders:stage="loaded-header-mismatch";break;
        case ProfileFailure::EntryRead:stage="loaded-entry-read";break;
        case ProfileFailure::EntryMismatch:stage="loaded-entry-mismatch";break;
    }
    std::ostringstream out;out<<stage<<"; win32="<<d.systemError<<std::uppercase<<std::hex
        <<"; moduleBase=0x"<<d.moduleBase<<"; headerBase=0x"<<d.headerBase;
    if(d.rva)out<<"; rva=0x"<<d.rva;
    if(d.failure==ProfileFailure::EntryMismatch){
        out<<std::setfill('0')<<"; expected=";for(auto b:d.expected)out<<std::setw(2)<<std::to_integer<unsigned>(b);
        out<<"; observed=";for(auto b:d.observed)out<<std::setw(2)<<std::to_integer<unsigned>(b);
    }
    return out.str();
}
bool NativeProfile::Initialize(void* module,ProfileDiagnostic* output) noexcept{
    ProfileDiagnostic local;auto& diagnostic=output?*output:local;diagnostic={};
    diagnostic.moduleBase=reinterpret_cast<std::uintptr_t>(module);
    auto fail=[&](ProfileFailure failure,DWORD error=0){diagnostic.failure=failure;diagnostic.systemError=error;return false;};
    if(base_ || !module)return fail(ProfileFailure::Module);
    wchar_t path[32768]{};auto pathLength=GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);
    if(!pathLength || pathLength>=32768)return fail(ProfileFailure::Path,GetLastError());
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return fail(ProfileFailure::Open,GetLastError());
    LARGE_INTEGER length{};if(!GetFileSizeEx(file,&length)){auto error=GetLastError();CloseHandle(file);return fail(ProfileFailure::Map,error);}
    HANDLE mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);auto error=GetLastError();CloseHandle(file);
    if(!mapping)return fail(ProfileFailure::Map,error);
    auto bytes=static_cast<std::byte const*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));error=GetLastError();CloseHandle(mapping);
    if(!bytes)return fail(ProfileFailure::Map,error);
    if(length.QuadPart<0 || !VerifyFile({bytes,static_cast<std::size_t>(length.QuadPart)})){UnmapViewOfFile(bytes);return fail(ProfileFailure::Disk);}
    disk_=bytes;diskSize_=static_cast<std::size_t>(length.QuadPart);base_=reinterpret_cast<std::uintptr_t>(module);
    auto nt=Headers({disk_,diskSize_});auto s=IMAGE_FIRST_SECTION(nt);sectionCount_=nt->FileHeader.NumberOfSections;
    auto spec=ImageFor({nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,nt->OptionalHeader.CheckSum});
    for(unsigned i=0;i<sectionCount_;++i){
        sections_[i]={s[i].VirtualAddress,s[i].PointerToRawData,s[i].SizeOfRawData,s[i].Characteristics};
        if(s[i].VirtualAddress==0x1000){textOffset_=s[i].PointerToRawData;textSize_=s[i].SizeOfRawData;}
    }
    // Independently verify the loaded headers, not merely the file name.
    std::array<std::byte,8192> loaded{};
    bool valid=ReadMemory(module,loaded.data(),loaded.size());
    if(!valid)diagnostic.failure=ProfileFailure::LoadedRead;
    else {
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER const*>(loaded.data());
        if(dos->e_lfanew>=64 && dos->e_lfanew<=4096)
            diagnostic.headerBase=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(loaded.data()+dos->e_lfanew)->OptionalHeader.ImageBase;
        valid=VerifyLoadedHeaders({disk_,diskSize_},loaded,base_);
        if(!valid)diagnostic.failure=ProfileFailure::LoadedHeaders;
    }
    if(valid)valid=VerifyLoadedGuards(spec->version,base_,diagnostic);
    if(!valid){UnmapViewOfFile(disk_);disk_=nullptr;base_=0;return false;}
    native_detail::active.store(spec->version,std::memory_order_release);
    return true;
}
bool NativeProfile::Entry(std::uintptr_t fn) const noexcept{
    if(!base_ || !disk_ || fn<base_+0x1000 || fn-base_-0x1000>textSize_-32)return false;
    std::array<std::byte,32> live{};
    return ReadMemory(reinterpret_cast<void const*>(fn),live.data(),32) && MatchEntry(live,{disk_+textOffset_+(fn-base_-0x1000),32});
}
bool NativeProfile::Table(void const* object,std::uint32_t rva) const noexcept{
    auto target=Rva(rva);std::uintptr_t actual{};return base_ && target && ReadMemory(object,&actual,sizeof actual) && actual==base_+target;
}
std::uintptr_t NativeProfile::Virtual(void const* object,unsigned offset) const noexcept{
    if(!base_ || offset>4096 || offset%8)return 0;
    std::uintptr_t table{},fn{};
    if(!ReadMemory(object,&table,sizeof table) || table<base_ || !ReadMemory(reinterpret_cast<void const*>(table+offset),&fn,sizeof fn))return 0;
    auto rva=table-base_+offset;
    for(unsigned i=0;i<sectionCount_;++i){auto const& s=sections_[i];
        if((s.flags&IMAGE_SCN_MEM_WRITE) || (s.flags&IMAGE_SCN_MEM_EXECUTE) || rva<s.rva || rva-s.rva+8>s.size)continue;
        std::uintptr_t expected{};std::memcpy(&expected,disk_+s.raw+(rva-s.rva),8);
        return expected>=0x140000000 && fn==base_+(expected-0x140000000) && Entry(fn)?fn:0;
    }
    return 0;
}
}
