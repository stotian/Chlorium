#pragma once
#include "OwnedScaleProgram.hpp"
#include "../R4-20/LoopComparison.hpp"
#include <chrono>
#include <fstream>
using namespace chlorium::research::transform_shadow;

template<bool Cumulative=false>
inline void compareScale(const OwnedTransformOracle& oracle,World& a,World& b,std::span<const ScaleStep> input,unsigned mode) {
    const std::vector<ScaleStep> expected(input.begin(),input.end());
    _mm_setcsr(mode);errno=1177;
    for(const auto step:expected) {
        a.state.layer.put(0x3e0,step.stamp);if(step.clearTracking)a.state.layer.put(0xee4,0);
        a.native(oracle,step.x,step.y,step.resetVisual);
    }
    const auto flags=_mm_getcsr();const auto error=errno;
    _mm_setcsr(mode);errno=1177;
    require(ownedScaleProgram<Cumulative>(b,oracle,input),"scale program accepted");
    require(flags==_mm_getcsr()&&error==errno,"scale program full FP/errno");equivalent(a,b);
}
template<bool Cumulative=false>
inline void rejectedScale(World& w,const OwnedTransformOracle& oracle,std::span<const ScaleStep> steps) {
    const auto objects=w.state.objects;const auto layer=w.state.layer;const auto center=w.center;
    const auto slots=w.state.slots;const auto parent=w.parent.math(),child=w.child.math();
    const auto events=w.events;const auto callbacks=w.callbacks;
    const auto flags=_mm_getcsr();const auto error=errno;
    require(!ownedScaleProgram<Cumulative>(w,oracle,steps),"unsupported scale program rejected");
    require(flags==_mm_getcsr()&&error==errno,"scale rejection FP/errno");
    require(layer.data==w.state.layer.data&&center.data==w.center.data&&slots==w.state.slots,"scale rejection before state writes");
    require(parent==w.parent.math()&&child==w.child.math()&&events==w.events&&callbacks==w.callbacks,"scale rejection before helpers/callbacks");
    for(unsigned n=0;n<objects.size();++n)require(objects[n].data==w.state.objects[n].data,"scale rejection entire objects");
}
template<bool Cumulative=false>
inline void benchmarkScale(const OwnedTransformOracle& oracle,const char* output) {
    std::ofstream summary(output),raw(std::string(output)+".batches.csv");
    require(bool(summary)&&bool(raw),"scale timing files opened");
    summary<<"objects,phases,batches,native_median_us,program_median_us\n";
    raw<<"objects,phases,batch,native_us,program_us\n";
    for(unsigned count:{4u,64u,4096u})for(unsigned phases:{1u,16u,64u}) {
        std::vector<ScaleStep> steps(phases);
        for(unsigned n=0;n<phases;++n)steps[n]={static_cast<int>((Cumulative?n/4:n)+1),true,n%2==0,n%2?.5f:2.f,n%2?4.f:.25f};
        std::array<std::vector<double>,2> times;
        for(unsigned batch=0;batch<9;++batch) {
            std::array<std::unique_ptr<World>,2> worlds;
            for(auto& w:worlds){w=std::make_unique<World>(oracle,count);w->capture=false;}
            std::array<unsigned,2> flags{};std::array<int,2> errors{};std::array<double,2> elapsed{};
            for(unsigned order=0;order<2;++order) {
                const auto kind=(order+batch)%2;auto& w=*worlds[kind];bool accepted=true;
                _mm_setcsr(0x1f80);errno=1177;const auto begin=std::chrono::steady_clock::now();
                if(kind)accepted=ownedScaleProgram<Cumulative>(w,oracle,steps);
                else for(const auto step:steps) {
                    w.state.layer.put(0x3e0,step.stamp);if(step.clearTracking)w.state.layer.put(0xee4,0);
                    w.native(oracle,step.x,step.y,step.resetVisual);
                }
                const auto end=std::chrono::steady_clock::now();flags[kind]=_mm_getcsr();errors[kind]=errno;
                elapsed[kind]=std::chrono::duration<double,std::micro>(end-begin).count();
                require(accepted,"timed scale program accepted");
            }
            require(flags[0]==flags[1]&&errors[0]==errors[1],"scale benchmark FP/errno");equivalent(*worlds[0],*worlds[1]);
            raw<<count<<','<<phases<<','<<batch<<','<<elapsed[0]<<','<<elapsed[1]<<'\n';
            for(unsigned k=0;k<2;++k)times[k].push_back(elapsed[k]);
        }
        for(auto& samples:times)std::sort(samples.begin(),samples.end());
        summary<<count<<','<<phases<<",9,"<<times[0][4]<<','<<times[1][4]<<'\n';
    }
    summary.flush();raw.flush();require(bool(summary)&&bool(raw),"scale timing files complete");
}
