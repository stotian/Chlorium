#pragma once
#include "../../src/renderer/NativeQuadState.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace chlorium::research::occlusion {
// Immutable owned RGBA8 image, no mipmaps/aniso; caller must retain sampler/storage identity.
class AlphaMask {
    unsigned width,height;
    std::vector<std::uint32_t> holes;
    static unsigned texelFloor(std::uint32_t bits,unsigned size)noexcept {
        const unsigned exponent=bits>>23;
        const auto mantissa=(bits&0x7fffffu)|(exponent?0x800000u:0u);
        const auto product=std::uint64_t(mantissa)*size;
        const unsigned shift=exponent?150-exponent:149;
        return shift>=64?0:static_cast<unsigned>(product>>shift);
    }
public:
    AlphaMask(unsigned w,unsigned h,std::span<const unsigned char> rgba):width(w),height(h){
        if(!w||!h||w>4096||h>4096||rgba.size()!=std::size_t(w)*h*4)throw std::invalid_argument("bounded complete RGBA8 alpha source");
        holes.resize(std::size_t(w+1)*(h+1));
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const auto i=std::size_t(y+1)*(w+1)+x+1;
            holes[i]=(rgba[(std::size_t(y)*w+x)*4+3]!=255)+holes[i-1]+holes[i-(w+1)]-holes[i-(w+2)];
        }
    }
private:
    bool footprint(const snapshot::NativeQuadState& quad,bool whole)const noexcept {
        std::uint32_t minU=0x3f800000u,minV=minU,maxU=0,maxV=0;
        for(const auto& v:quad.corners){
            const auto u=std::bit_cast<std::uint32_t>(v.u),w=std::bit_cast<std::uint32_t>(v.v);
            const auto ub=u&0x7fffffffu,vb=w&0x7fffffffu;
            if((v.color>>24)!=255||ub>0x3f800000u||vb>0x3f800000u||((u>>31)&&ub)||((w>>31)&&vb))return false;
            minU=std::min(minU,ub);maxU=std::max(maxU,ub);minV=std::min(minV,vb);maxV=std::max(maxV,vb);
        }
        // Include bilinear taps and two extra texels around the entire UV hull.
        const auto x0=std::clamp(int(texelFloor(minU,width))-3,0,int(width)-1);
        const auto x1=std::clamp(int(texelFloor(maxU,width))+3,0,int(width)-1)+1;
        const auto y0=std::clamp(int(texelFloor(minV,height))-3,0,int(height)-1);
        const auto y1=std::clamp(int(texelFloor(maxV,height))+3,0,int(height)-1)+1;
        auto at=[&](int x,int y){return holes[std::size_t(y)*(width+1)+x];};
        const auto count=at(x1,y1)-at(x0,y1)-at(x1,y0)+at(x0,y0);
        return whole?count==0:count<unsigned(x1-x0)*unsigned(y1-y0);
    }
public:
    bool opaqueFootprint(const snapshot::NativeQuadState& quad)const noexcept{return footprint(quad,true);}
    // Candidate selection only: the GPU must still reject every nonopaque fragment.
    bool maybeOpaqueFootprint(const snapshot::NativeQuadState& quad)const noexcept{return footprint(quad,false);}
};
}
