#pragma once
#include "../../src/renderer/NativeQuadState.hpp"
#include <array>
#include <bit>
#include <span>

namespace chlorium::research::occlusion {
inline bool safelyInsideDepth(std::uint32_t z,std::uint32_t scale)noexcept {
    z&=0x7fffffffu;scale&=0x7fffffffu;
    if(z>=0x7f800000u||scale>=0x7f800000u)return false;
    if(!z||!scale)return true;
    const unsigned ze=z>>23,se=scale>>23;
    const auto zm=(z&0x7fffffu)|(ze?0x800000u:0u),sm=(scale&0x7fffffu)|(se?0x800000u:0u);
    const auto product=std::uint64_t(zm)*sm;
    const int shift=298-int(ze?ze:1)-int(se?se:1);
    // Exact dyadic bound |z*scale| <= 1/4 leaves ample distance to either clip plane.
    return shift>=48||(shift>=0&&product<=(1ull<<shift));
}
inline bool depthContract(std::span<const snapshot::NativeQuadState> quads,const std::array<float,16>& m)noexcept {
    if(quads.empty()||quads.size()>65536)return false;
    for(unsigned i=0;i<m.size();++i){const auto bits=std::bit_cast<unsigned>(m[i])&0x7fffffffu;
        if(bits>=0x7f800000u||(i!=10&&bits>0x4f800000u))return false;
    }
    for(unsigned i:{2u,6u,14u,3u,7u,11u})if((std::bit_cast<unsigned>(m[i])&0x7fffffffu)!=0)return false;
    if(std::bit_cast<unsigned>(m[15])!=0x3f800000u)return false;
    for(const auto& q:quads)for(const auto& v:q.corners){
        for(float p:{v.x,v.y,v.z})if((std::bit_cast<unsigned>(p)&0x7fffffffu)>0x4f800000u)return false;
        if(!safelyInsideDepth(std::bit_cast<unsigned>(v.z),std::bit_cast<unsigned>(m[10])))return false;
    }
    return true;
}
}
