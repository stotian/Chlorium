#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
namespace chlorium::research::identity_node {
struct Matrix {float a,b,c,d,tx,ty;};
using Native=Matrix*(*)(void*,Matrix*);
// Only a fingerprinted native CCNode call with disjoint output may use this.
// No cache, callback suppression, angle approximation or FP arithmetic.
inline bool tryIdentity(void* node,Matrix* output) {
    auto p=static_cast<unsigned char*>(node);
    const auto n=reinterpret_cast<std::uintptr_t>(node),o=reinterpret_cast<std::uintptr_t>(output);
    if((o>=n&&o-n<0x120)||(o<n&&n-o<sizeof(Matrix)))return false;
    const auto word=[p](unsigned at){std::uint32_t v;std::memcpy(&v,p+at,4);return v;};
    if(!p[0x111]||(word(0x11c)&0xff00ff00u))return false;
    auto linear=_mm_loadu_si128(reinterpret_cast<const __m128i*>(p+0x38));
    linear=_mm_and_si128(_mm_xor_si128(linear,_mm_set_epi32(0x3f800000,0x3f800000,0,0)),_mm_set_epi32(-1,-1,0x7fffffff,0x7fffffff));
    const auto zeroFields=_mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p+0x54)),_mm_set1_epi32(0x7fffffff));
#ifndef CHLORIUM_IDENTITY_NODE_SKIP_LINEAR_GUARD_MUTANT
    if(_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_or_si128(linear,zeroFields),_mm_setzero_si128()))!=0xffff)return false;
#endif
    const std::array<std::uint32_t,6> matrix{0x3f800000u,0,0x80000000u,0x3f800000u,word(0x4c),word(0x50)};
    std::memcpy(p+0x8c,matrix.data(),sizeof(Matrix));
    std::memcpy(p+0x114,matrix.data()+4,8);p[0x111]=p[0x112]=0;
    std::memcpy(output,matrix.data(),sizeof(Matrix));return true;
}
inline Matrix* dispatch(void* node,Matrix* output,Native native) {
    return tryIdentity(node,output)?output:native(node,output);
}
}
