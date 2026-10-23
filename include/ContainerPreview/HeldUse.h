#pragma once
#include "NativeAddress.h"
#include <cstdint>
namespace preview {
class HeldUseScope {
public:
    // Borrowed only for one validated synchronous GameMode transaction.
    HeldUseScope(std::uintptr_t base,void* player,bool enabled) noexcept:
        previous_(current_),base_(base),player_(player),active_(enabled && base && player){current_=active_?this:nullptr;}
    ~HeldUseScope(){current_=previous_;}
    HeldUseScope(HeldUseScope const&)=delete;
    HeldUseScope& operator=(HeldUseScope const&)=delete;
    bool Active() const {return active_;}
    static bool ProtectsAdjustment(std::uintptr_t caller,void* player){return Match(caller,0x25B5161,player);}
    static bool BindAdjustment(std::uintptr_t caller,void* player,void* stack){
        // Bind the first borrowed clone only; reentrant foreign operations
        // cannot replace its attribution and escape the later fallback checks.
        if(!stack || !ProtectsAdjustment(caller,player) || (current_->stack_ && current_->stack_!=stack))return false;
        current_->stack_=stack;return true;
    }
    static bool ProtectsStack(void const* stack){return current_ && stack && stack==current_->stack_;}
    static bool ProtectsItemUse(std::uintptr_t caller,void* player){return Match(caller,0x259E356,player) || Match(caller,0x259E3ED,player);}
    static bool ProtectsSecondary(std::uintptr_t caller){auto rva=Callsite(0x25B55D6);return current_ && rva && caller==current_->base_+rva;}
private:
    static std::uint32_t Callsite(unsigned key){
        // Exact45 native2D3CA90/2D304E0, not translated instruction deltas.
        if(CurrentVersion()==GameVersion::V45)switch(key){
            case 0x25B5161:return 0x2D3CD5E;
            case 0x259E356:return 0x2D31521;
            case 0x259E3ED:return 0x2D315C1;
            case 0x25B55D6:return 0x2D3D1CB;
            default:return 0;
        }
        return Rva(key);
    }
    static bool Match(std::uintptr_t caller,unsigned key,void* player){auto rva=Callsite(key);return current_ && rva && player==current_->player_ && caller==current_->base_+rva;}
    inline static thread_local HeldUseScope* current_{};
    HeldUseScope* previous_{};std::uintptr_t base_{};void* player_{};void* stack_{};bool active_{};
};
template<class F>std::uint64_t FilterHeldAdjust(F original,std::uintptr_t caller,void* item,void* stack,void* player,void* face,void* pos){
    if(HeldUseScope::BindAdjustment(caller,player,stack))return 0;
    return original(item,stack,player,face,pos);
}
template<class F>void* FilterHeldItemUse(F original,std::uintptr_t caller,void* stack,void* out,void* player,void* pos,unsigned char face,unsigned char hand,void* hit,std::uint64_t flags){
    if(out && HeldUseScope::ProtectsStack(stack) && HeldUseScope::ProtectsItemUse(caller,player)){*static_cast<unsigned char*>(out)=0;return out;}
    return original(stack,out,player,pos,face,hand,hit,flags);
}
template<class F>unsigned char FilterHeldEmpty(F original,std::uintptr_t caller,void* stack){
    if(HeldUseScope::ProtectsStack(stack) && HeldUseScope::ProtectsSecondary(caller))return 1;
    return original(stack);
}
//45 passes three signed block coordinates, not a borrowed BlockPos/hand pair.
template<class F>void* FilterHeldItemUse45(F original,std::uintptr_t caller,void* stack,void* out,void* player,int x,int y,int z,unsigned char face,void* hit,std::uint64_t flags){
    if(out && HeldUseScope::ProtectsStack(stack) && HeldUseScope::ProtectsItemUse(caller,player)){*static_cast<unsigned char*>(out)=0;return out;}
    return original(stack,out,player,x,y,z,face,hit,flags);
}
}
