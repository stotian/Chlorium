#include "AlphaMask.hpp"
#include "DepthContract.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <xmmintrin.h>
using chlorium::research::occlusion::AlphaMask;
using chlorium::snapshot::NativeQuadState;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main()try {
    constexpr unsigned w=32,h=17;std::vector<unsigned char> pixels(w*h*4,255);AlphaMask solid(w,h,pixels);
    NativeQuadState quad{};for(auto& v:quad.corners){v.color=0xffffffffu;v.u=.5f;v.v=.5f;}
    require(solid.opaqueFootprint(quad),"solid image and opaque vertices");
    for(auto alpha:{0u,1u,127u,254u}){quad.corners[2].color=alpha<<24;require(!solid.opaqueFootprint(quad),"partly transparent vertices never occlude");}quad.corners[2].color=0xffffffff;
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    for(auto bits:{0x7fc12345u,0x7f812345u,0x7f800000u,0xbf800000u,0x3f800001u}){
        quad.corners[1].u=std::bit_cast<float>(bits);_mm_setcsr(0);
        require(!solid.opaqueFootprint(quad)&&_mm_getcsr()==0,"invalid UV rejects without FP instructions or traps");
    }
    quad.corners[1].u=.5f;
    for(auto bits:{0u,0x80000000u,1u,0x007fffffu,0x3f800000u}){
        for(auto& v:quad.corners)v.u=std::bit_cast<float>(bits);_mm_setcsr(0x1fbf);
        require(solid.opaqueFootprint(quad)&&_mm_getcsr()==0x1fbf,"exact integer UV arithmetic preserves FP state");
    }
    _mm_setcsr(saved);
    pixels[(7*w+11)*4+3]=254;AlphaMask hole(w,h,pixels);
    for(auto& v:quad.corners){v.u=11.5f/w;v.v=7.5f/h;}
    require(!hole.opaqueFootprint(quad),"single nonopaque texel cannot be skipped");
    require(hole.maybeOpaqueFootprint(quad),"partly opaque region is only a GPU alpha-test candidate");
    auto dim=pixels;for(std::size_t i=3;i<dim.size();i+=4)dim[i]=254;AlphaMask glow(w,h,dim);require(!glow.maybeOpaqueFootprint(quad),"texture with no alpha-255 never enters prepass");
    std::mt19937 random(0x441a1fa);unsigned accepted=0;
    for(unsigned test=0;test<20000;++test){
        for(auto& v:quad.corners){v.u=float(random()%1025)/1024.f;v.v=float(random()%1025)/1024.f;}
        if(!hole.opaqueFootprint(quad))continue;++accepted;
        float u0=1,u1=0,v0=1,v1=0;for(const auto& v:quad.corners){u0=std::min(u0,v.u);u1=std::max(u1,v.u);v0=std::min(v0,v.v);v1=std::max(v1,v.v);}
        const auto left=std::clamp(int(std::floor(double(u0)*w-.5)),0,int(w)-1),right=std::clamp(int(std::ceil(double(u1)*w-.5)),0,int(w)-1);
        const auto bottom=std::clamp(int(std::floor(double(v0)*h-.5)),0,int(h)-1),top=std::clamp(int(std::ceil(double(v1)*h-.5)),0,int(h)-1);
        for(int y=bottom;y<=top;++y)for(int x=left;x<=right;++x)require(pixels[(y*w+x)*4+3]==255,"no false-positive bilinear footprint");
    }
    require(accepted>0,"non-vacuous opaque regions remain");
    unsigned rejected=0;for(auto size:{0u,4097u})try{AlphaMask bad(size,h,pixels);}catch(const std::invalid_argument&){++rejected;}
    require(rejected==2,"dimensions rejected before allocation");
    using chlorium::research::occlusion::safelyInsideDepth;
    for(unsigned i=0;i<100000;++i){const auto z=random()&0x7fffffffu,s=random()&0x7fffffffu;
        if(z>=0x7f800000u||s>=0x7f800000u)continue;
        _mm_setcsr(0x1f80);const auto reference=double(std::bit_cast<float>(z))*double(std::bit_cast<float>(s))<=.25;
        _mm_setcsr(0);require(safelyInsideDepth(z,s)==reference&&_mm_getcsr()==0,"exact dyadic clip bound, no FP side effects");
    }
    _mm_setcsr(saved);
    std::cout<<"PASS alpha-254/vertex-alpha/NaN/Inf/subnormal/UV/FP guards and20000 bilinear region checks; accepted "<<accepted<<"; no GD\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
