#include "ContainerPreview/HookSet.h"
#include <sstream>
namespace preview {
bool InstallHookSet(std::span<HookSpec const> specs,HookBackend const& api,HookDiagnostic* output){
    HookDiagnostic local;auto& diagnostic=output?*output:local;diagnostic={};diagnostic.requested=specs.size();
    auto fail=[&](HookFailure stage,std::size_t index=NoHookIndex,int status=0){
        diagnostic.failure=stage;diagnostic.index=index;diagnostic.backendStatus=status;
        if(index<specs.size())diagnostic.target=reinterpret_cast<std::uintptr_t>(specs[index].target);
        return false;
    };
    if(specs.empty() || specs.size()>MaxHookCount || !api.create || !api.enable || !api.disable || !api.remove)return fail(HookFailure::Validation);
    for(std::size_t i=0;i<specs.size();++i){auto const& s=specs[i];if(!s.target || !s.detour || !s.original)return fail(HookFailure::Validation,i);}
    std::size_t created=0,enabled=0;
    for(auto const& s:specs){
        auto status=api.create(s.target,s.detour,s.original);
        if(status){fail(HookFailure::Create,created,status);break;}
        ++created;
    }
    if(created==specs.size())for(auto const& s:specs){
        auto status=api.enable(s.target);
        if(status){fail(HookFailure::Enable,enabled,status);break;}
        ++enabled;
    }
    if(created==specs.size() && enabled==specs.size())return true;
    while(enabled)api.disable(specs[--enabled].target);
    while(created)api.remove(specs[--created].target);
    return false;
}
std::string DescribeHookDiagnostic(HookDiagnostic const& diagnostic,std::uintptr_t imageBase){
    char const* stage="unknown";
    switch(diagnostic.failure){
        case HookFailure::None:stage="ready";break;
        case HookFailure::Validation:stage="validation";break;
        case HookFailure::Create:stage="create";break;
        case HookFailure::Enable:stage="enable";break;
    }
    std::ostringstream out;
    out<<"stage="<<stage<<"; requested="<<diagnostic.requested<<"; capacity="<<MaxHookCount;
    if(diagnostic.index!=NoHookIndex)out<<"; index0="<<diagnostic.index;
    out<<"; backendStatus="<<diagnostic.backendStatus;
    if(diagnostic.target){
        out<<std::uppercase<<std::hex<<"; target=0x"<<diagnostic.target;
        if(imageBase && diagnostic.target>=imageBase)out<<"; rva=0x"<<(diagnostic.target-imageBase);
    }
    return out.str();
}
}
