#pragma once
#include "../R4-42/GroupTape.hpp"

namespace chlorium::research::groupprogram {
using grouptape::State;using grouptape::Command;using grouptape::Range;
inline constexpr std::size_t maxObjects=65536,maxCommands=8192,maxRanges=65536,maxSteps=64;
// The R42 arithmetic bound applies to each object's ordered list, not the global command table.
inline bool programEligible(std::span<const Command> commands,std::span<const Range> ranges,
                            std::span<const unsigned> indices,unsigned mxcsr)noexcept{
    if(commands.size()>maxCommands || ranges.empty() || ranges.size()>maxRanges || indices.size()>maxRanges*maxSteps ||
       (mxcsr&0x7f80u)!=0x1f80u || (mxcsr&~0xffffu))return false;
    for(const auto& command:commands)if(command.control[2] || command.control[3] ||
        !grouptape::bounded(command.delta[0]) || !grouptape::bounded(command.delta[1]))return false;
    for(const auto index:indices)if(index>=commands.size())return false;
    for(const auto& range:ranges){
        if(range[1]>maxSteps || range[0]>indices.size() || range[1]>indices.size()-range[0])return false;
        for(unsigned i=1;i<range[1];++i)if(indices[range[0]+i]<indices[range[0]+i-1])return false;
    }
    return true;
}
inline bool seedEligible(std::span<const State> states,std::span<const Command> commands,std::span<const Range> ranges,
                         std::span<const unsigned> indices,unsigned mxcsr)noexcept{
    if(states.empty() || states.size()>maxObjects || !programEligible(commands,ranges,indices,mxcsr))return false;
    for(const auto& state:states)if((state.flags&0x0000ffc0u) || (state.flags>>16)>=ranges.size() ||
        !grouptape::bounded(state.position[0]) || !grouptape::bounded(state.position[1]))return false;
    return true;
}
}
