#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
namespace preview {
inline constexpr std::size_t MaxHookCount=27;
inline constexpr std::size_t NoHookIndex=static_cast<std::size_t>(-1);
struct HookSpec {void* target;void* detour;void** original;};
struct HookBackend {
    int (*create)(void*,void*,void**);
    int (*enable)(void*);
    int (*disable)(void*);
    int (*remove)(void*);
};
enum class HookFailure {None,Validation,Create,Enable};
struct HookDiagnostic {
    HookFailure failure{HookFailure::None};
    std::size_t requested{},index{NoHookIndex};
    std::uintptr_t target{};
    int backendStatus{};
};
bool InstallHookSet(std::span<HookSpec const>,HookBackend const&,HookDiagnostic* diagnostic=nullptr);
std::string DescribeHookDiagnostic(HookDiagnostic const&,std::uintptr_t imageBase);
template<class Original,class Observer,class... Args>
auto ForwardOnce(Original original,Observer observer,Args... args)->decltype(original(args...)){
    auto result=original(args...);
    try{observer();}catch(...){}
    return result;
}
}
