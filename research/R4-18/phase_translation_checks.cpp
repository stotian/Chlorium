#include "ExactPhaseTranslation.hpp"
#include "NativeResetComponentOracle.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <fstream>
#include <immintrin.h>
#include <cmath>
#include <cstdlib>
using namespace chlorium::research::phase;
std::uint64_t randomState=0xd4270ec183a76291ull;
std::uint64_t next(){randomState^=randomState<<13;randomState^=randomState>>7;randomState^=randomState<<17;return randomState;}
unsigned checks=0;
void require(bool b,const char* why) {++checks;if(!b){std::cerr<<"FAIL "<<why<<" after "<<checks<<'\n';std::exit(1);}}
bool same(State a,State b) {return std::bit_cast<std::uint64_t>(a.position)==std::bit_cast<std::uint64_t>(b.position)&&std::bit_cast<std::uint32_t>(a.offset)==std::bit_cast<std::uint32_t>(b.offset);}
void correctness() {
    const auto saved=_mm_getcsr();_mm_setcsr(0x1f80);
    NativeResetComponentOracle nativeReset;
    // Ordinary magnitudes can lose a double ULP when crossing a binade. A
    // last-phase-only rewrite without an exactness proof is therefore invalid.
    const std::array<float,2> crossing{.125f,0};
    const State edge{std::nextafter(32768.,0.),0};
    const auto reference=eager(edge,crossing);
    const auto proved=Program(crossing).evaluate(edge);
    require(reference.position!=edge.position,"native-style reset/apply can change canonical base");
    require(!proved.coalesced&&same(proved.state,reference),"rounded cancellation cannot be superseded");
    const std::array<float,4> normal{.125f,-.5f,.375f,0};
    require(Program(normal).evaluate({32767.5,0}).coalesced,"exact dyadic translation stream is coalesced");
    auto mutableCommands=normal;Program immutable(mutableCommands);mutableCommands.back()=1000000000.f;
    require(same(immutable.evaluate({100,0}).state,eager({100,0},normal)),"proof owns immutable commands rather than a mutable source");
    require(same(Program({}).evaluate(edge).state,edge),"empty program preserves unobserved initial state");
    bool bounded=false;try {std::vector<float> many(4097);Program rejected(many);}catch(const std::length_error&){bounded=true;}
    require(bounded,"oversized program rejected before command allocation");
    for(unsigned control=0;control<16;++control) {
        const auto mode=0x1f80u|((control&3)<<13)|((control&4)?0x40:0)|((control&8)?0x8000:0);
        for(unsigned trial=0;trial<4096;++trial) {
            _mm_setcsr(mode);
            std::array<float,17> offsets;
            for(auto& f:offsets) f=trial%3?static_cast<float>(static_cast<int>(next()%4000)-2000)/128.f:std::bit_cast<float>(static_cast<std::uint32_t>(next()));
            State input{trial%2?static_cast<double>(static_cast<float>(next()%1000000)/8.f):std::bit_cast<double>(next()),static_cast<float>(static_cast<int>(next()%100)-50)/16.f};
            Program p(offsets);
            _mm_setcsr(mode);const auto expected=eager(input,offsets);const auto flags=_mm_getcsr()&63;
            _mm_setcsr(mode);const auto actual=p.evaluate(input);const auto observed=_mm_getcsr()&63;
            require(same(actual.state,expected),"phase coalescing matches ordered reference bits");
            require(flags==observed,"masked FP status flags preserved");
            const auto split=next()%offsets.size();
            const auto prefix=Program(std::span(offsets).first(split)).evaluate(input).state;
            require(same(prefix,eager(input,std::span(offsets).first(split))),"reader observes exact eager prefix, not just final compensation");
            const auto suffix=Program(std::span(offsets).subspan(split)).evaluate(prefix).state;
            require(same(suffix,expected),"reader barrier preserves eager intermediate state");
            if(normalDouble(input.position)&&normalOrZero(input.offset)) {
                _mm_setcsr(mode);auto reset=input;
                if(reset.offset!=0) {reset.position-=static_cast<double>(reset.offset);reset.offset=0;}
                const auto resetFlags=_mm_getcsr()&63;
                _mm_setcsr(mode);bool dirty=false;const auto native=nativeReset.reset(input,&dirty);const auto nativeFlags=_mm_getcsr()&63;
                require(same(native,reset),"actual owned GD reset component matches position and offset bits");
                require(nativeFlags==resetFlags,"actual GD reset component matches masked FP flags");
                require(dirty==(input.offset!=0),"native position-dirty byte matches exact reset condition");
            }
        }
    }
    _mm_setcsr(saved);std::cout<<"PASS "<<checks<<" exact phase checks; native callbacks/read sets unproved\n";
}
int main(int argc,char** argv) {
    correctness();if(argc<2)return 0;
    std::ofstream out(argv[1]);out<<"case,objects,phases,batches,eager_median_us,proved_median_us,eager_max_us,proved_max_us,coalesced_objects\n";
    using Clock=std::chrono::steady_clock;
    for(const auto kind:{0,1,2}) for(const std::size_t count:{4096u,32768u}) {
        std::vector<State> initial(count),reference,actual;
        for(std::size_t n=0;n<count;++n)initial[n]={static_cast<double>(static_cast<float>(n+1)*3.125f),0};
        if(kind) for(std::size_t n=0;n<count;++n) if(kind==2||n%2) initial[n].position=std::nextafter(65536.,0.);
        std::vector<float> offsets(64);for(auto& f:offsets)f=static_cast<float>(static_cast<int>(next()%8000)-4000)/128.f;
        std::vector<double> eagerTimes,provedTimes;std::size_t coalesced=0;
        for(unsigned batch=0;batch<9;++batch) {
            reference=initial;actual=initial;
            auto runEager=[&]{auto start=Clock::now();for(auto f:offsets)for(auto& s:reference)s=eager(s,std::span(&f,1));eagerTimes.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count());};
            auto runProved=[&]{auto start=Clock::now();Program p(offsets);coalesced=0;for(auto& s:actual){const auto r=p.evaluate(s);s=r.state;coalesced+=r.coalesced;}provedTimes.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count());};
            if(batch%2){runProved();runEager();}else{runEager();runProved();}
            for(std::size_t n=0;n<count;++n)require(same(actual[n],reference[n]),"all benchmark debt settled and compared");
        }
        std::sort(eagerTimes.begin(),eagerTimes.end());std::sort(provedTimes.begin(),provedTimes.end());
        out<<(kind==0?"exact_grid":kind==1?"mixed":"full_precision")<<','<<count<<",64,9,"<<eagerTimes[4]<<','<<provedTimes[4]<<','<<eagerTimes.back()<<','<<provedTimes.back()<<','<<coalesced<<'\n';
    }
    std::cout<<"PASS complete-state CPU comparison; no GD FPS claim\n";
}
