#pragma once
#include "../R4-33/SpriteCorners.hpp"

namespace chlorium::research::packedcorners {
inline __m128 broadcast(std::uint32_t value){const auto lane=corners::bits(value);return _mm_shuffle_ps(lane,lane,0);}
// Four corners of ONE sprite; no batching across native callback barriers.
inline bool materialize(const pass::SpriteRecord& source,snapshot::NativeQuadState& output){
    if(!corners::domain(source)||(_mm_getcsr()&0x1f80u)!=0x1f80u)return false;
    if((source.flags&1u)||!(source.flags&512u)||!(source.flags&1024u)){
        constexpr std::array<std::uint32_t,3> zero{};
        for(auto& vertex:output.corners)std::memcpy(&vertex.x,zero.data(),sizeof(zero));
        return true;
    }
    const auto x1=corners::bits(source.localRect[0]),y1=corners::bits(source.localRect[1]);
    const auto x2=_mm_add_ss(x1,corners::bits(source.localRect[2]));
    const auto y2=_mm_add_ss(y1,corners::bits(source.localRect[3]));
    const auto xs=_mm_movelh_ps(_mm_unpacklo_ps(x1,x1),_mm_unpacklo_ps(x2,x2));
    const auto y21=_mm_unpacklo_ps(y2,y1),ys=_mm_movelh_ps(y21,y21);
    const auto ax=_mm_mul_ps(broadcast(source.matrix[0]),xs);
    const auto cy=_mm_mul_ps(broadcast(source.matrix[2]^0x80000000u),ys);
    const auto bx=_mm_mul_ps(broadcast(source.matrix[1]),xs);
    const auto dy=_mm_mul_ps(broadcast(source.matrix[3]),ys);
    const auto x=_mm_add_ps(_mm_sub_ps(ax,cy),broadcast(source.matrix[4]));
    // Domain excludes NaNs/overflow: swapping the two finite addends preserves bits and flags.
    const auto y=_mm_add_ps(_mm_add_ps(bx,dy),broadcast(source.matrix[5]));
    const auto low=_mm_unpacklo_ps(x,y),high=_mm_unpackhi_ps(x,y);
    _mm_storel_pi(reinterpret_cast<__m64*>(&output.corners[0].x),low);
    _mm_storeh_pi(reinterpret_cast<__m64*>(&output.corners[1].x),low);
    _mm_storel_pi(reinterpret_cast<__m64*>(&output.corners[2].x),high);
    _mm_storeh_pi(reinterpret_cast<__m64*>(&output.corners[3].x),high);
    for(auto& vertex:output.corners)std::memcpy(&vertex.z,&source.z,4);
    return true;
}
}
