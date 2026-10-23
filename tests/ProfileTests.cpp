#include "ContainerPreview/NativeProfile.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string_view>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
using namespace preview;
namespace {int checks{},failures{};void Check(bool b,char const* name){++checks;if(!b){++failures;std::cerr<<"FAIL "<<name<<'\n';}}}
// Independent literal vtable edges, read from the real disk image. These
// catch a plausible but unrelated code nomination before a hook is enabled.
void Exact45TableRegression(std::span<std::byte const> image){
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(image.data()+reinterpret_cast<IMAGE_DOS_HEADER const*>(image.data())->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=0x6A8378BA)return;
    struct Edge {unsigned table,slot,target;};
    constexpr Edge edges[]={
        {0xE69D4E0,936,0x18FB230},{0xE69D4E0,872,0x18F8610},
        {0xE7B2C50,16,0x1990D30},{0xE774DB0,632,0xD5AAE0},
        {0xE774DB0,3168,0xD72780},{0xE74B340,376,0x57E6D0},
        {0xE74B340,480,0x57ECE0},{0xE79E800,2680,0xD72560},
        {0xE832700,2680,0xD72560},{0xE81A130,48,0x2D337B0},
        {0xE81A090,48,0x2D2EF90},
        {0xE7D4900,144,0x76FB0},{0xE7D4900,1000,0x20DAE10},
        {0xE7D4D30,1000,0x20DAE10},{0xE7D5140,1000,0x20DAE10}
    };
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(auto const& edge:edges){
        auto rva=edge.table+edge.slot;std::uintptr_t value{};bool found=false;
        for(unsigned n=0;n<nt->FileHeader.NumberOfSections;++n){auto const& s=sections[n];
            if(rva<s.VirtualAddress || rva-s.VirtualAddress+8>s.SizeOfRawData)continue;
            auto offset=s.PointerToRawData+rva-s.VirtualAddress;
            if(offset+8<=image.size()){std::memcpy(&value,image.data()+offset,8);found=true;}break;
        }
        Check(found && value==UINT64_C(0x140000000)+edge.target,"exact45 disk vtable edge matches independently reviewed native role");
    }
}
// A data-only simulated loaded image: no game code is executed or patched.
// A detoured layout-proof constructor must not prevent admission; changing a
// runtime dependency must still refuse it with the exact offending RVA.
void LoadedGuardRegression(GameVersion version){
    auto guards=NativeGuards(version);
    std::size_t size{};for(auto const& g:guards)size=std::max(size,std::size_t(g.rva)+64);
    auto image=static_cast<std::byte*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(image!=nullptr,"synthetic loaded image allocated without executing native code");if(!image)return;
    for(auto const& g:guards)std::memcpy(image+g.rva,g.bytes.data(),g.bytes.size());
    ProfileDiagnostic diagnostic;
    auto base=reinterpret_cast<std::uintptr_t>(image);
    Check(VerifyLoadedGuards(version,base,diagnostic),"unaltered runtime entries pass the actual loaded-image verifier");
    auto ctor=version==GameVersion::V45?0xD33240u:version==GameVersion::V51?0x5D974B0u:0x5D97580u;
    std::array<std::byte,5> saved{};std::memcpy(saved.data(),image+ctor,saved.size());
    constexpr std::array<std::byte,5> detour{std::byte{0xE9},std::byte{0x03},std::byte{0x92},std::byte{0x8F},std::byte{0xBB}};
    std::memcpy(image+ctor,detour.data(),detour.size());diagnostic={};
    Check(VerifyLoadedGuards(version,base,diagnostic) && diagnostic.failure==ProfileFailure::None,
        "detoured proof-only client constructor does not reject otherwise intact runtime seams");
    for(auto const& g:guards){
        if(g.rva==ctor)continue;
        image[g.rva]^=std::byte{1};diagnostic={};
        Check(!VerifyLoadedGuards(version,base,diagnostic) && diagnostic.failure==ProfileFailure::EntryMismatch && diagnostic.rva!=ctor,
            "each remaining runtime dependency refuses modified bytes even with a detoured proof constructor");
        image[g.rva]^=std::byte{1};
    }
    DWORD previousProtection{},discard{};
    auto protectedProof=VirtualProtect(image+ctor,1,PAGE_NOACCESS,&previousProtection)!=0;
    Check(protectedProof,"proof-only synthetic page made unreadable");
    if(protectedProof){
        diagnostic={};
        Check(VerifyLoadedGuards(version,base,diagnostic),"loaded verification never reads disk-only layout proof code");
        VirtualProtect(image+ctor,1,previousProtection,&discard);
    }
    std::memcpy(image+ctor,saved.data(),saved.size());
    auto clientUpdate=version==GameVersion::V45?0xD44840u:version==GameVersion::V51?0x5DA7B30u:0x5DA7C00u;
    image[clientUpdate]^=std::byte{1};diagnostic={};
    Check(!VerifyLoadedGuards(version,base,diagnostic) && diagnostic.failure==ProfileFailure::EntryMismatch && diagnostic.rva==clientUpdate,
        "changed real client hook still rejects with its exact loaded RVA");
    auto protectedRuntime=VirtualProtect(image+clientUpdate,1,PAGE_NOACCESS,&previousProtection)!=0;
    Check(protectedRuntime,"runtime synthetic page made unreadable");
    if(protectedRuntime){
        diagnostic={};
        Check(!VerifyLoadedGuards(version,base,diagnostic) && diagnostic.failure==ProfileFailure::EntryRead && diagnostic.rva==clientUpdate,
            "unreadable runtime entry still refuses without an access violation escaping");
        VirtualProtect(image+clientUpdate,1,previousProtection,&discard);
    }
    VirtualFree(image,0,MEM_RELEASE);
}
int wmain(int argc,wchar_t** argv){
    Check(AcceptedIdentity({0x8664,0x6A8378BA,0x12888000,0x12568F46}) &&
          AcceptedIdentity({0x8664,0x6A8378BA,0x12889000,0x1256BF65}),"independently complete45 original and launcher identities admitted");
    Check(!AcceptedIdentity({0x8664,0x6A8378BA,0x12888000,0x1256BF65}),"mixed45 original and launcher headers remain refused");
    LoadedGuardRegression(GameVersion::V45);
    auto physical45=NativeGuards(GameVersion::V45);bool complete45=!physical45.empty();std::uint32_t previous45{};
    for(auto const& binding:native_detail::addresses45){
        complete45&=binding.reference>previous45 && NativeRva(GameVersion::V45,binding.reference)!=0;previous45=binding.reference;
        if(binding.target<0xE000000)complete45&=std::any_of(physical45.begin(),physical45.end(),[&](auto const& g){return g.rva==binding.target;});
    }
    Check(complete45,"45 separate physical catalog is sorted and every callable or continuation has an exact byte guard");
    for(auto version:{GameVersion::V51,GameVersion::V52}){
        LoadedGuardRegression(version);
        bool complete=true;std::uint32_t previous{};
        for(auto const& value:native_detail::addresses){
            complete&=value.reference>previous && NativeRva(version,value.reference)!=0;
            previous=value.reference;
        }
        Check(complete,"admitted profile has a sorted complete native key catalog");
        auto guards=NativeGuards(version);bool guarded=guards.size()==152;
        for(auto const& reference:NativeGuards(GameVersion::V52)){
            auto target=NativeRva(version,reference.rva);
            guarded&=target && std::any_of(guards.begin(),guards.end(),[&](auto const& g){return g.rva==target;});
        }
        Check(guarded,"every disk evidence entry has its selected-version exact-byte guard");
        Check(std::all_of(guards.begin(),guards.end(),[&](auto const& g){
            return (g.scope==EntryGuardScope::DiskEvidence)==(g.rva==(version==GameVersion::V51?0x5D974B0u:0x5D97580u));
        }),"only the audited non-invoked client constructor is disk-only evidence");
    }
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C0C000,0x128E3A0F}),"26.51 original profile admitted independently");
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C0D000,0x128DA158}),"26.51 launcher profile admitted independently");
    Check(!AcceptedIdentity({0x8664,0x6AA482FD,0x12C0C000,0x128DA158}),"mixed 26.51 identity fails closed");
    Check(!AcceptedIdentity({0x8664,0x6AB54E37,0x12C0C000,0x128E3A0F}),"cross-version header identity fails closed");
    Check(AcceptedIdentity({0x8664,0x6AB54E37,0x12C01000,0x128CD801}),"exact original PE identity admitted");
    Check(AcceptedIdentity({0x8664,0x6AB54E37,0x12C02000,0x128DA293}),"exact launcher PE identity admitted");
    Check(!AcceptedIdentity({0x8664,0x6AB54E37,0x12C01000,0x128DA293}),"mixed original and launcher identity rejected");
    Check(!AcceptedIdentity({0x14C,0x6AB54E37,0x12C01000,0x128CD801}),"wrong architecture rejected");
    std::array<std::byte,32> bytes{};bytes[0]=std::byte{0x55};auto altered=bytes;altered[31]=std::byte{1};
    Check(MatchEntry(bytes,bytes),"matching complete native entry accepted");
    Check(!MatchEntry(altered,bytes) && !MatchEntry(std::span(bytes).first(31),bytes),"altered and truncated entries rejected");
    Check(!MatchEntry({},{}),"empty guard never admits a binding");
    ProfileDiagnostic diagnostic;
    Check(!VerifyLoadedGuards(GameVersion::Unknown,0x140000000,diagnostic),"unknown guard profile cannot pass as an empty loaded catalog");
    Check(!VerifyLoadedGuards(GameVersion::V51,0,diagnostic),"absent module cannot pass loaded guard admission");
    Check(!VerifyLoadedEntry(0x5DC6430,altered,bytes,diagnostic) && diagnostic.failure==ProfileFailure::EntryMismatch &&
        diagnostic.rva==0x5DC6430 && diagnostic.expected==bytes && diagnostic.observed==altered,
        "loaded entry refusal records exact binding and observed bytes");
    auto description=DescribeProfileDiagnostic(diagnostic);
    Check(description.find("loaded-entry-mismatch")!=std::string::npos && description.find("5DC6430")!=std::string::npos &&
        description.find("expected=")!=std::string::npos && description.find("observed=")!=std::string::npos,
        "startup refusal is actionable instead of conflating all identity and entry checks");
    diagnostic={};
    Check(VerifyLoadedEntry(0x1171390,bytes,bytes,diagnostic) && diagnostic.failure==ProfileFailure::None,
        "matching native entry does not fabricate a startup failure");
    Check(!VerifyFile({}),"empty image refused without unsafe header access");
    NativeProfile uninitialized;
    Check(!uninitialized.GuardedMethod<void*(*)(void*)>(0x5DC2630,reinterpret_cast<void*>(1),0xE9731B0),"unvalidated client cannot yield a callable native world getter");
    if(argc>1){
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);auto n=input.tellg();
        if(n<=0){std::cerr<<"Cannot open exact image\n";return 2;}
        std::vector<std::byte> image(static_cast<std::size_t>(n));input.seekg(0);input.read(reinterpret_cast<char*>(image.data()),n);
        if(argc>2 && std::wstring_view(argv[2])==L"reject"){
            Check(!VerifyFile(image),"unadmitted real executable fails closed without executing game code");
            std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
        }
        Check(VerifyFile(image),"real exact executable passes complete read-only verification");
        Exact45TableRegression(image);
        std::vector<std::byte> loaded(image.begin(),image.begin()+8192);
        auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(loaded.data()+reinterpret_cast<IMAGE_DOS_HEADER*>(loaded.data())->e_lfanew);
        Check(VerifyLoadedHeaders(image,loaded,0x7FF600000000),"preferred base in loaded header remains accepted");
        nt->OptionalHeader.ImageBase=0x7FF600000000;
        Check(VerifyLoadedHeaders(image,loaded,0x7FF600000000),"ASLR loaded header accepts exactly the actual module base");
        nt->OptionalHeader.ImageBase=0x7FF500000000;
        Check(!VerifyLoadedHeaders(image,loaded,0x7FF600000000),"unrelated loaded base fails closed");
        nt->OptionalHeader.ImageBase=0x140000000;
        nt->OptionalHeader.CheckSum^=1;
        Check(!VerifyLoadedHeaders(image,loaded,0x7FF600000000),"loaded checksum drift fails closed");
        nt->OptionalHeader.CheckSum^=1;
        IMAGE_FIRST_SECTION(nt)->VirtualAddress^=0x1000;
        Check(!VerifyLoadedHeaders(image,loaded,0x7FF600000000),"loaded section drift fails closed");
        IMAGE_FIRST_SECTION(nt)->VirtualAddress^=0x1000;
        nt->FileHeader.NumberOfSections-=1;
        Check(!VerifyLoadedHeaders(image,loaded,0x7FF600000000),"loaded section count drift fails closed");
        Check(!VerifyLoadedHeaders(image,{},0x7FF600000000),"missing loaded header fails closed");
        auto diskNt=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(image.data()+reinterpret_cast<IMAGE_DOS_HEADER const*>(image.data())->e_lfanew);
        auto ctor=diskNt->FileHeader.TimeDateStamp==0x6A8378BA?0xD33240u:diskNt->FileHeader.TimeDateStamp==0x6AA482FD?0x5D974B0u:0x5D97580u;
        auto sections=IMAGE_FIRST_SECTION(diskNt);
        for(unsigned i=0;i<diskNt->FileHeader.NumberOfSections;++i){
            auto const& s=sections[i];if(ctor<s.VirtualAddress || ctor-s.VirtualAddress>=s.SizeOfRawData)continue;
            auto offset=s.PointerToRawData+ctor-s.VirtualAddress;image[offset]^=std::byte{1};
            Check(!VerifyFile(image),"disk-only proof constructor is still protected by full file and text verification");
            image[offset]^=std::byte{1};break;
        }
        image[4096]^=std::byte{1};Check(!VerifyFile(image),"real altered executable is refused");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
