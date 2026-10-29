#pragma once
#include "../R4-22/OwnedIdentityProgram.hpp"

namespace chlorium::research::transform_shadow {
struct ScaleStep {int stamp;bool clearTracking,resetVisual;float x,y;};
inline bool ownedScaleProgram(World& w,const OwnedTransformOracle& oracle,std::span<const ScaleStep> input) {
    if(input.empty()||input.size()>64||World::active)return false;
    std::array<ScaleStep,64> steps{};std::copy(input.begin(),input.end(),steps.begin());
    const auto power=[](float value){const auto bits=std::bit_cast<std::uint32_t>(value);return (bits&0x807fffffu)==0&&(bits>>23)>=123&&(bits>>23)<=131;};
    for(std::size_t n=0;n<input.size();++n) {
        if(!power(steps[n].x)||!power(steps[n].y))return false;
        if(n&&(steps[n].stamp<=steps[n-1].stamp||!steps[n].clearTracking))return false;
    }
    if(!detail::identityEligible(w,oracle,1,1))return false;
    std::array<bool,8192> seen{};
    for(const auto member:w.members) {
        if(!member)break;
        const auto index=w.identity(member);
        if(index<0||static_cast<std::size_t>(index)>=seen.size())return false;
        const auto& object=w.state.objects[index];if(object.get<unsigned char>(0x520))continue;
        // Repeated members or an old stamp can reuse a non-unit source matrix.
        if(seen[index]||steps[0].stamp<=object.get<int>(0x4e0))return false;
        seen[index]=true;
    }
    std::array<unsigned,64> flags{};
    const auto controls=_mm_getcsr();const auto error=errno;
    const auto minusZero=std::bit_cast<float>(0x80000000u);
    bool supported=true;
    for(std::size_t n=0;n<input.size();++n) {
        _mm_setcsr(controls&~63u);
        const auto matrix=pose::scalar({1,0,minusZero,1,0,0},{steps[n].x,0,minusZero,steps[n].y,0,0});
        const auto linear=pose::decompose(matrix,oracle.atan2());
        flags[n]=_mm_getcsr()&63;
        supported&=linear.scaleX==steps[n].x&&linear.scaleY==steps[n].y&&linear.rotationX==0.f&&linear.rotationY==0.f;
    }
    _mm_setcsr(controls);errno=error;
    if(!supported)return false;
    for(std::size_t n=0;n<input.size();++n) {
#ifdef CHLORIUM_SCALE_BORROW_INPUT_MUTANT
        const auto step=input[n];
#else
        const auto step=steps[n];
#endif
        w.state.layer.put(0x3e0,step.stamp);
        if(step.clearTracking)w.state.layer.put(0xee4,0);
        detail::executeIdentity<true>(w,oracle,flags[n],step.x,step.y,step.resetVisual,step.x,step.y);
    }
    return true;
}
}
