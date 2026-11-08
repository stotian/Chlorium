#pragma once
#include "ResidentRecords.hpp"
#include "SpriteCorners.hpp"
#include <algorithm>
#include <cstring>

namespace chlorium::research::resident {
inline void encodeParameters(const pass::SpriteRecord& source,
                             const snapshot::NativeQuadState& style,Parameters& p) {
    std::copy_n(source.matrix,4,p.matrix);
    std::copy_n(source.matrix+4,2,p.translationZFlags);
    p.translationZFlags[2]=source.z; p.translationZFlags[3]=source.flags;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    // Only these flags reach corners::domain/materialize and the pinned resident shader.
    p.translationZFlags[3]&=1u|64u|128u|512u|1024u;
#endif
    for(unsigned c=0;c<4;++c)p.colors[c]=style.corners[c].color;
}
// Supported geometry reads only UV/color from the native quad, never positions.
inline void encodeSprite(const pass::SpriteRecord& source,
                         const snapshot::NativeQuadState& style,Geometry& g,Parameters& p) {
    g={}; p={}; encodeParameters(source,style,p);
    std::copy_n(source.localRect,4,g.rect); g.control[0]=corners::domain(source);
    for(unsigned c=0;c<4;++c)std::memcpy(g.uv+c*2,&style.corners[c].u,8);
}
}
