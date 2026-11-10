#pragma once
#include "../R4-64/GroupProgram.hpp"
#include "../../src/renderer/NativeAffinePoint.hpp"

namespace chlorium::research::mixedprogram {
using groupprogram::Range;
struct alignas(16) State {
    double position[2]{};unsigned last[2]{},stamp=0,flags=0;float offset[2]{};unsigned reserved[2]{};
};
struct alignas(16) Command {
    double delta[2]{};float linear[4]{},affineCenter[4]{},move[4]{};unsigned control[4]{};
};
static_assert(sizeof(State)==48 && offsetof(State,offset)==32 && sizeof(Command)==80 && offsetof(Command,control)==64);
// Structural/finite fixture admission only; this is not a production arithmetic or reader-ownership gate.
inline bool fixtureInput(std::span<const State> states,std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices,unsigned fp){
    if(states.empty() || states.size()>65536 || commands.size()>8192 || ranges.empty() || ranges.size()>65536 || indices.size()>65536*64 || (fp&0x7f80)!=0x1f80 || (fp&~0xffffu))return false;
    for(const auto& s:states){if((s.flags&0x0000ff00) || (s.flags>>16)>=ranges.size() || s.reserved[0] || s.reserved[1])return false;
        for(auto x:s.position)if(!grouptape::bounded(x))return false;for(auto x:s.offset)if(!std::isfinite(x) || std::abs(x)>65536)return false;}
    for(const auto& c:commands){if(c.control[1]>2 || c.control[2]>(c.control[1]==2?3u:1u) || c.control[3])return false;
        for(auto x:c.delta)if(!grouptape::bounded(x))return false;
        for(auto values:{std::span(c.linear),std::span(c.affineCenter),std::span(c.move)})for(auto x:values)if(!std::isfinite(x) || std::abs(x)>65536 || (x && std::abs(x)<0x1p-126f))return false;
        if(c.move[2] || c.move[3] || (c.control[1]==1 && (c.delta[0] || c.delta[1])))return false;}
    for(auto i:indices)if(i>=commands.size())return false;
    for(auto r:ranges){if(r[1]>64 || r[0]>indices.size() || r[1]>indices.size()-r[0])return false;for(unsigned k=1;k<r[1];++k)if(indices[r[0]+k]<indices[r[0]+k-1])return false;}
    return true;
}
}
