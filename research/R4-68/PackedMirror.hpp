#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <emmintrin.h>
#include "../R4-64/GroupProgram.hpp"
#include <algorithm>

namespace chlorium::research::packedmirror {
using namespace groupprogram;
// Owned arithmetic mirror only. No native pointers, skipped callbacks or live reader ownership.
class PackedMirror {
    struct WinError {DWORD value=GetLastError();~WinError(){SetLastError(value);}};
    std::vector<State> states_;
    std::vector<unsigned> offsets_,original_;
    double bound_=0;
    static __m128d select(__m128d mask,__m128d yes,__m128d no){return _mm_or_pd(_mm_and_pd(mask,yes),_mm_andnot_pd(mask,no));}
    static __m128d mask(bool a,bool b){return _mm_castsi128_pd(_mm_set_epi64x(b?-1ll:0,a?-1ll:0));}
public:
    struct Metrics {std::uint64_t frames=0,actorLoads=0,actorStores=0,logicalVisits=0;} metrics;
    bool initialize(std::span<const State> states,std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices){
        WinError preserve;
        if(!states_.empty() || !seedEligible(states,commands,ranges,indices,_mm_getcsr()))return false;
        std::vector<unsigned> offsets(ranges.size()+1),original(states.size());std::vector<State> packed(states.size());
        for(const auto& state:states)++offsets[(state.flags>>16)+1];
        for(unsigned i=1;i<offsets.size();++i)offsets[i]+=offsets[i-1];
        auto cursor=offsets;double bound=0;
        for(unsigned i=0;i<states.size();++i){const auto at=cursor[states[i].flags>>16]++;packed[at]=states[i];original[i]=at;
            bound=std::max(bound,std::max(std::abs(states[i].position[0]),std::abs(states[i].position[1])));}
        states_.swap(packed);original_.swap(original);offsets_.swap(offsets);bound_=bound;
        return true;
    }
    bool copyOut(std::span<State> destination)const{
        if(destination.size()!=original_.size())return false;
        for(unsigned i=0;i<destination.size();++i)destination[i]=states_[original_[i]];return true;
    }
    bool advance(std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices){
        WinError preserve;const auto fp=_mm_getcsr();
        if(states_.empty() || ranges.size()+1!=offsets_.size() || !programEligible(commands,ranges,indices,fp))return false;
        const auto next=boundAfter(bound_,commands,ranges,indices);_mm_setcsr(fp);
        if(next>0x1p32)return false;
        for(unsigned group=0;group<ranges.size();++group){const auto range=ranges[group];
            if(!range[1])continue;
            for(unsigned i=offsets_[group];i<offsets_[group+1];i+=2){
                auto a=states_[i];const bool pair=i+1<offsets_[group+1];auto b=pair?states_[i+1]:a;
                auto x=_mm_set_pd(b.position[0],a.position[0]),y=_mm_set_pd(b.position[1],a.position[1]);
                const auto unlocked=mask(!(a.flags&2),!(b.flags&2));
                for(unsigned step=0;step<range[1];++step){const auto& command=commands[indices[range[0]+step]];
                    const bool lastA=!(a.flags&1) && a.stamp!=command.control[0],lastB=!(b.flags&1) && b.stamp!=command.control[0];
                    if(lastA || lastB){const auto lanes=mask(lastA,lastB);alignas(16) unsigned xx[4],yy[4];
                        _mm_store_si128(reinterpret_cast<__m128i*>(xx),_mm_castps_si128(_mm_cvtpd_ps(_mm_and_pd(lanes,x))));
                        _mm_store_si128(reinterpret_cast<__m128i*>(yy),_mm_castps_si128(_mm_cvtpd_ps(_mm_and_pd(lanes,y))));
                        if(lastA){a.last[0]=xx[0];a.last[1]=yy[0];a.stamp=command.control[0];a.flags|=12;}
                        if(lastB){b.last[0]=xx[1];b.last[1]=yy[1];b.stamp=command.control[0];b.flags|=12;}
                    }
                    // Locked lanes add delta to zero, so masked-off work cannot raise extra inexact flags.
                    if(command.delta[0]!=0)x=select(unlocked,_mm_add_pd(_mm_set1_pd(command.delta[0]),_mm_and_pd(unlocked,x)),x);
                    if(command.delta[1]!=0)y=_mm_add_pd(_mm_set1_pd(command.delta[1]),y);
                    a.flags|=48;b.flags|=48;
                }
                alignas(16) double xx[2],yy[2];_mm_store_pd(xx,x);_mm_store_pd(yy,y);
                a.position[0]=xx[0];a.position[1]=yy[0];states_[i]=a;
                if(pair){b.position[0]=xx[1];b.position[1]=yy[1];states_[i+1]=b;}
            }
            metrics.logicalVisits+=std::uint64_t(offsets_[group+1]-offsets_[group])*range[1];
            metrics.actorLoads+=offsets_[group+1]-offsets_[group];metrics.actorStores+=offsets_[group+1]-offsets_[group];
        }
        bound_=next;++metrics.frames;return true;
    }
};
}
