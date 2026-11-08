#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace chlorium::research::grouptape {
struct alignas(16) State {double position[2]{};std::uint32_t last[2]{},stamp=0,flags=0;};
struct alignas(16) Command {double delta[2]{};std::uint32_t control[4]{};};
using Range=std::array<unsigned,2>;
static_assert(sizeof(State)==32&&sizeof(Command)==32&&sizeof(Range)==8);
inline bool bounded(double value) {
    // <=64 additions on this 2^-84 lattice cannot produce FP64/FP32 subnormals or overflow.
    const auto bits=std::bit_cast<std::uint64_t>(value)&0x7fffffffffffffffull;
    return !bits||(bits>=0x3df0000000000000ull&&bits<=0x41f0000000000000ull);
}
// Arithmetic domain only: native observers and CPU readers require a separate ownership proof.
inline bool arithmeticEligible(std::span<const State> states,std::span<const Command> commands,
                     std::span<const Range> ranges,std::span<const unsigned> indices,unsigned mxcsr) {
    if(states.empty()||states.size()>65536||commands.size()>64||ranges.empty()||ranges.size()>64||
       indices.size()>4096||(mxcsr&0x7f80u)!=0x1f80u||mxcsr&~0xffffu)return false;
    for(const auto& state:states)if(state.flags&0x0000ffc0u||(state.flags>>16)>=ranges.size()||!bounded(state.position[0])||!bounded(state.position[1]))return false;
    for(const auto& command:commands)if(command.control[2]||command.control[3]||!bounded(command.delta[0])||!bounded(command.delta[1]))return false;
    for(const auto& range:ranges){
        if(range[0]>indices.size()||range[1]>indices.size()-range[0])return false;
        unsigned previous=0;
        for(unsigned k=0;k<range[1];++k){const auto index=indices[range[0]+k];if(index>=commands.size()||(k&&index<=previous))return false;previous=index;}
    }
    return true;
}
}
