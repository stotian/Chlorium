#pragma once
#include "../R4-20/IdentityLinearPhase.hpp"

namespace chlorium::research::transform_shadow {
struct IdentityStep {int stamp;bool clearTracking,resetVisual;};

// Synchronous exclusive borrow of a fixture, not a GD cache across frames.
// Known callbacks may write positions/FP flags, but never membership, linears,
// tables, helper topology or lifetimes. No external callback or yield is allowed.
inline bool ownedIdentityProgram(World& w,const OwnedTransformOracle& oracle,std::span<const IdentityStep> input) {
    if(input.empty()||input.size()>64||World::active)return false;
    std::array<IdentityStep,64> steps{};
    std::copy(input.begin(),input.end(),steps.begin());
    for(std::size_t n=1;n<input.size();++n)
        if(steps[n].stamp<steps[n-1].stamp||
           (steps[n].stamp!=steps[n-1].stamp&&!steps[n].clearTracking))return false;
#ifndef CHLORIUM_PROGRAM_SKIP_VALIDATION_MUTANT
    if(!detail::identityEligible(w,oracle,1,1))return false;
#endif
    const auto flags=detail::identityFlags(oracle);
    for(std::size_t n=0;n<input.size();++n) {
#ifdef CHLORIUM_PROGRAM_BORROW_INPUT_MUTANT
        const auto step=input[n];
#else
        const auto step=steps[n];
#endif
        w.state.layer.put(0x3e0,step.stamp);
        if(step.clearTracking)w.state.layer.put(0xee4,0);
        detail::executeIdentity(w,oracle,flags,1,1,step.resetVisual);
    }
    return true;
}
}
