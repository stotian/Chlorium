#pragma once
#include "../R4-39/PackedCorners.hpp"
#include "OrderedKernel.hpp"

namespace chlorium::research::maskedcorners {
// Preserve each native operand order, including NaN payload choice and signed zero.
inline bool materialize(const pass::SpriteRecord& source,snapshot::NativeQuadState& output,const OrderedKernel& kernel){
    if((source.flags&(64u|128u))||(_mm_getcsr()&0x1f80u)!=0x1f80u)return false;
    if((source.flags&1u)||!(source.flags&512u)||!(source.flags&1024u)){
        constexpr std::array<std::uint32_t,3> zero{};
        for(auto& vertex:output.corners)std::memcpy(&vertex.x,zero.data(),sizeof(zero));return true;
    }
    kernel.method()(source.matrix,source.localRect,source.z,&output);
    return true;
}
}
