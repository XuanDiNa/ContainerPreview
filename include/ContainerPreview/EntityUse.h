#pragma once
#include "NativeAddress.h"
#include <cstdint>
namespace preview {
class EntityUseScope {
public:
    // Actor/hand are borrowed only until the synchronous native use returns.
    EntityUseScope(std::uintptr_t base,void* player,void* actor,void* hand,bool enabled) noexcept:
        previous_(current_),base_(base),player_(player),actor_(actor),hand_(hand){
        current_=enabled && base && player && actor && hand?this:nullptr;
    }
    ~EntityUseScope(){current_=previous_;}
    EntityUseScope(EntityUseScope const&)=delete;
    EntityUseScope& operator=(EntityUseScope const&)=delete;
    static bool MatchesPlan(std::uintptr_t caller,void* actor,void* player){
        //45 caller2FB8530 supplies distinct result/action/hit arguments.
        auto rva=CurrentVersion()==GameVersion::V45?0x2FB87C6:Rva(0x1F9F13);
        return current_ && rva && !current_->planning_ && caller==current_->base_+rva && actor==current_->actor_ && player==current_->player_;
    }
    static bool ProtectsHand(std::uintptr_t caller,void const* hand){
        auto rva=CurrentVersion()==GameVersion::V45?0x18FB2B6:Rva(0x1A2B2DF);
        return current_ && rva && current_->planning_ && caller==current_->base_+rva && hand==current_->hand_;
    }
    class PlanScope {
    public:
        PlanScope():scope_(current_){scope_->planning_=true;}
        ~PlanScope(){scope_->planning_=false;}
        PlanScope(PlanScope const&)=delete;
        PlanScope& operator=(PlanScope const&)=delete;
    private:EntityUseScope* scope_;
    };
private:
    inline static thread_local EntityUseScope* current_{};
    EntityUseScope* previous_{};std::uintptr_t base_{};void *player_{},*actor_{},*hand_{};bool planning_{};
};
template<class F>unsigned char FilterEntityHandEmpty(F original,std::uintptr_t caller,void* hand){
    return EntityUseScope::ProtectsHand(caller,hand)?1:original(hand);
}
template<class F,class V,class D>void* FilterEntityPlan(F original,V validate,D discard,std::uintptr_t caller,void* actor,void* output,void* player,void const* hit){
    if(!output || !EntityUseScope::MatchesPlan(caller,actor,player)){
        EntityUseScope suspend(0,nullptr,nullptr,nullptr,false);
        return original(actor,output,player,hit);
    }
    EntityUseScope::PlanScope planning;
    auto result=original(actor,output,player,hit);
    if(result!=output || !validate(output,actor,player))discard(output);
    return output;
}
//45 returns a result byte separately from its104-byte action object.
template<class F,class V,class D>void* FilterEntityPlan45(F original,V validate,D discard,std::uintptr_t caller,void* actor,void* result,void* player,void* action,void const* hit){
    if(!result || !action || !EntityUseScope::MatchesPlan(caller,actor,player)){
        EntityUseScope suspend(0,nullptr,nullptr,nullptr,false);
        return original(actor,result,player,action,hit);
    }
    EntityUseScope::PlanScope planning;
    auto returned=original(actor,result,player,action,hit);
    auto flags=static_cast<unsigned char*>(result);
    if(returned!=result || !(*flags&1) || !validate(action,actor,player)){
        //2FB8530 tests bit1 and may also tag the interacted entity. Clear all
        // result flags before retiring callbacks; never report native success.
        *flags=0;discard(action);
    }
    return result;
}
}
