#pragma once
#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace chlorium::research::samecell {
template<class T> inline T read(const void* object,unsigned offset){T value;std::memcpy(&value,static_cast<const unsigned char*>(object)+offset,sizeof(value));return value;}
inline bool scalesEligible(const float* inverse){
    const auto sx=std::bit_cast<std::uint32_t>(inverse[0]),sy=std::bit_cast<std::uint32_t>(inverse[1]);
    for(auto bits:{sx,sy})if(bits<std::bit_cast<std::uint32_t>(0x1p-20f) || bits>std::bit_cast<std::uint32_t>(1.f))return false;
    return true;
}
inline bool cellsMatch(const void* object,__m128d scale){
    const auto x=read<std::uint64_t>(object,0x3b8),y=read<std::uint64_t>(object,0x3c0);
    for(auto bits:{x,y})if(bits>=std::bit_cast<std::uint64_t>(10000000.) || (bits && bits<std::bit_cast<std::uint64_t>(0x1p-64)))return false;
    const auto positions=_mm_loadu_pd(reinterpret_cast<const double*>(static_cast<const unsigned char*>(object)+0x3b8));
    const auto cells=_mm_cvttpd_epi32(_mm_mul_pd(scale,positions));
    const auto previous=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(static_cast<const unsigned char*>(object)+0x278));
    return (_mm_movemask_epi8(_mm_cmpeq_epi32(cells,previous))&255)==255;
}
inline __m128d loadScale(const float* inverse){return _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(inverse))));}
// Only the verified native no-write/no-callback branch. Rejected speculation restores FP effects.
inline bool unchanged(const void* object,const float* inverse){
    if(read<std::int32_t>(object,0x278)<0)return true;
    const auto fp=_mm_getcsr();if((fp&0x1f80)!=0x1f80 || !scalesEligible(inverse))return false;
    if(cellsMatch(object,loadScale(inverse)))return true;_mm_setcsr(fp);return false;
}
class PacketClassifier {
    const float* inverse_;__m128d scale_{};bool eligible_=false;
public:
    explicit PacketClassifier(const float* inverse):inverse_(inverse){refresh();}
    void refresh(){
        // Supported math can only add inexact; requiring it already set avoids speculative FP saves.
        eligible_=(_mm_getcsr()&0x1fa0)==0x1fa0 && scalesEligible(inverse_);
        if(eligible_)scale_=loadScale(inverse_);
    }
    bool unchanged(const void* object)const{
        return read<std::int32_t>(object,0x278)<0 || (eligible_ && cellsMatch(object,scale_));
    }
};
}
