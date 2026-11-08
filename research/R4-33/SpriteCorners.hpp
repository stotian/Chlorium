#pragma once
#include "../R4-32/PassCapture.hpp"
#include <array>
#include <bit>
#include <xmmintrin.h>
#include <emmintrin.h>

namespace chlorium::research::corners {
using pass::SpriteRecord;
using snapshot::NativeQuadState;
inline bool bounded(std::uint32_t bits) {
    bits &= 0x7fffffffu;
    // Two local sums, products and cancellation stay normal: minimum 2^-126.
    return !bits || (bits >= 0x2b800000u && bits <= 0x4f800000u);
}
inline bool domain(const SpriteRecord& source) {
    if (source.flags & (64u | 128u)) return false;
    if ((source.flags & 1u) || !(source.flags & 512u) || !(source.flags & 1024u)) return true;
    for (auto value : source.matrix) if (!bounded(value)) return false;
    for (auto value : source.localRect) if (!bounded(value)) return false;
    return (source.z & 0x7fffffffu) < 0x7f800000u;
}
inline __m128 bits(std::uint32_t value) {
    return _mm_castsi128_ps(_mm_cvtsi32_si128(static_cast<int>(value)));
}
inline void store(snapshot::NativeQuadVertex& vertex, __m128 x, __m128 y, std::uint32_t z) {
    _mm_store_ss(&vertex.x,x); _mm_store_ss(&vertex.y,y);
    std::memcpy(&vertex.z,&z,sizeof(z));
}
// Closed arithmetic primitive only. No sprite mutation/callback suppression.
// Rejection leaves output and floating-point state unchanged.
inline bool materialize(const SpriteRecord& source, NativeQuadState& output) {
    if (!domain(source) || (_mm_getcsr() & 0x1f80u) != 0x1f80u) return false;
    if ((source.flags & 1u) || !(source.flags & 512u) || !(source.flags & 1024u)) {
        constexpr std::array<std::uint32_t,3> zero{};
        for(auto& vertex:output.corners) std::memcpy(&vertex.x,zero.data(),sizeof(zero));
        return true;
    }
    const auto x1=bits(source.localRect[0]), y1=bits(source.localRect[1]);
    const auto x2=_mm_add_ss(x1,bits(source.localRect[2])), y2=_mm_add_ss(y1,bits(source.localRect[3]));
    const auto a=bits(source.matrix[0]), b=bits(source.matrix[1]);
    const auto nc=bits(source.matrix[2]^0x80000000u), d=bits(source.matrix[3]);
    const auto tx=bits(source.matrix[4]), ty=bits(source.matrix[5]);
    const auto ax1=_mm_mul_ss(a,x1), ax2=_mm_mul_ss(a,x2);
    const auto bx1=_mm_mul_ss(b,x1), bx2=_mm_mul_ss(b,x2);
    const auto cy1=_mm_mul_ss(nc,y1), cy2=_mm_mul_ss(nc,y2);
    const auto dy1=_mm_mul_ss(d,y1), dy2=_mm_mul_ss(d,y2);
    store(output.corners[0],_mm_add_ss(_mm_sub_ss(ax1,cy2),tx),_mm_add_ss(_mm_add_ss(dy2,bx1),ty),source.z);
    store(output.corners[1],_mm_add_ss(_mm_sub_ss(ax1,cy1),tx),_mm_add_ss(_mm_add_ss(bx1,dy1),ty),source.z);
    store(output.corners[2],_mm_add_ss(_mm_sub_ss(ax2,cy2),tx),_mm_add_ss(_mm_add_ss(dy2,bx2),ty),source.z);
    store(output.corners[3],_mm_add_ss(_mm_sub_ss(ax2,cy1),tx),_mm_add_ss(_mm_add_ss(bx2,dy1),ty),source.z);
    return true;
}
}
