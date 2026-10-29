#include "OwnedScaleProgram.hpp"
#include "../R4-20/LoopComparison.hpp"
#include <climits>
#include <chrono>
#include <fstream>
#include <cmath>
using namespace chlorium::research::transform_shadow;

void compareScale(const OwnedTransformOracle& oracle,World& a,World& b,std::span<const ScaleStep> input,unsigned mode) {
    const std::vector<ScaleStep> expected(input.begin(),input.end());
    _mm_setcsr(mode);errno=1177;
    for(const auto step:expected) {
        a.state.layer.put(0x3e0,step.stamp);if(step.clearTracking)a.state.layer.put(0xee4,0);
        a.native(oracle,step.x,step.y,step.resetVisual);
    }
    const auto flags=_mm_getcsr();const auto error=errno;
    _mm_setcsr(mode);errno=1177;
    require(ownedScaleProgram(b,oracle,input),"scale program accepted");
    require(flags==_mm_getcsr()&&error==errno,"scale program full FP/errno");equivalent(a,b);
}
void rejectedScale(World& w,const OwnedTransformOracle& oracle,std::span<const ScaleStep> steps) {
    const auto objects=w.state.objects;const auto layer=w.state.layer;const auto center=w.center;
    const auto slots=w.state.slots;const auto parent=w.parent.math(),child=w.child.math();
    const auto events=w.events;const auto callbacks=w.callbacks;
    const auto flags=_mm_getcsr();const auto error=errno;
    require(!ownedScaleProgram(w,oracle,steps),"unsupported scale program rejected");
    require(flags==_mm_getcsr()&&error==errno,"scale rejection FP/errno");
    require(layer.data==w.state.layer.data&&center.data==w.center.data&&slots==w.state.slots,"scale rejection before state writes");
    require(parent==w.parent.math()&&child==w.child.math()&&events==w.events&&callbacks==w.callbacks,"scale rejection before helpers/callbacks");
    for(unsigned n=0;n<objects.size();++n)require(objects[n].data==w.state.objects[n].data,"scale rejection entire objects");
}
void benchmarkScale(const OwnedTransformOracle& oracle,const char* output) {
    std::ofstream summary(output),raw(std::string(output)+".batches.csv");
    require(bool(summary)&&bool(raw),"scale timing files opened");
    summary<<"objects,phases,batches,native_median_us,program_median_us\n";
    raw<<"objects,phases,batch,native_us,program_us\n";
    for(unsigned count:{4u,64u,4096u})for(unsigned phases:{1u,16u,64u}) {
        std::vector<ScaleStep> steps(phases);
        for(unsigned n=0;n<phases;++n)steps[n]={static_cast<int>(n+1),true,n%2==0,n%2?.5f:2.f,n%2?4.f:.25f};
        std::array<std::vector<double>,2> times;
        for(unsigned batch=0;batch<9;++batch) {
            std::array<std::unique_ptr<World>,2> worlds;
            for(auto& w:worlds){w=std::make_unique<World>(oracle,count);w->capture=false;}
            std::array<unsigned,2> flags{};std::array<int,2> errors{};std::array<double,2> elapsed{};
            for(unsigned order=0;order<2;++order) {
                const auto kind=(order+batch)%2;auto& w=*worlds[kind];bool accepted=true;
                _mm_setcsr(0x1f80);errno=1177;const auto begin=std::chrono::steady_clock::now();
                if(kind)accepted=ownedScaleProgram(w,oracle,steps);
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
int main(int argc,char** argv)try {
    const auto saved=_mm_getcsr();_mm_setcsr(0x1f80);OwnedTransformOracle oracle;
    const std::array<float,9> scales{.0625f,.125f,.25f,.5f,1,2,4,8,16};
    {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        const std::array<ScaleStep,2> commands{{{1,true,true,2,.5f},{2,true,false,.5f,2}}};
        ScaleStep* borrowed=nullptr;
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);
            auto cache=reinterpret_cast<ScaleStep*>(w->parent.data.data()+0x8c);
            for(unsigned n=0;n<commands.size();++n)std::construct_at(cache+n,commands[n]);
            if(w==b.get())borrowed=cache;
        }
        compareScale(oracle,*a,*b,{borrowed,2},0x1f80);
        require(borrowed[1].stamp!=commands[1].stamp,"native helper really overwrites borrowed command storage");
    }
    for(unsigned controls:{0u,0x40u,0x8000u,0x8040u})for(auto x:scales)for(auto y:scales)for(unsigned observers=0;observers<2;++observers) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);w->observerWrites=observers!=0;w->observerClearsFlags=observers!=0;
            w->state.objects[0].put(0x3b8,std::nextafter(32768.,0.));
            w->state.objects[1].put(0x3b8,0x1p40);w->state.objects[2].put(0x3c0,-0x1p40);
        }
        const std::array<ScaleStep,4> steps{{{1,true,true,x,y},{2,true,false,1,1},{3,true,true,y,x},{4,true,true,.5f,2}}};
        compareScale(oracle,*a,*b,steps,0x1f80u|controls|(observers?63u:0u));
    }
    for(unsigned kind=0;kind<14;++kind) {
        auto w=std::make_unique<World>(oracle);_mm_setcsr(0x1fbf);errno=1177;
        std::array<ScaleStep,2> steps{{{1,true,true,2,.5f},{2,true,false,.5f,2}}};
        if(kind==0)steps[0].x=.3f;if(kind==1)steps[0].x=-1;if(kind==2)steps[0].x=0;
        if(kind==3)steps[0].x=32;if(kind==4)steps[0].x=std::bit_cast<float>(0x7fc00000u);
        if(kind==5)steps[1].stamp=1;if(kind==6)w->members[1]=w->members[0];
        if(kind==7)w->state.objects[0].put(0x4e0,1);if(kind==8)w->unknownObserver=true;
        if(kind==9)steps[1].clearTracking=false;
        if(kind==10)w->objectTable[0x508/8]=nullptr;if(kind==11)_mm_setcsr(0x3fbf);
        if(kind==12)steps[0].x=std::numeric_limits<float>::infinity();if(kind==13)steps[1].stamp=INT_MIN;
        rejectedScale(*w,oracle,steps);
    }
    {
        auto w=std::make_unique<World>(oracle);_mm_setcsr(0x1fbf);errno=1177;
        const std::array<ScaleStep,1> normal{{{1,true,true,2,.5f}}};
        std::array<ScaleStep,65> tooMany{};
        rejectedScale(*w,oracle,{});rejectedScale(*w,oracle,tooMany);
        World::active=w.get();rejectedScale(*w,oracle,normal);World::active=nullptr;
        _mm_setcsr(0x1f00);rejectedScale(*w,oracle,normal);_mm_setcsr(0x1fbf);
        w->state.layer.put(0xee4,60);w->state.layer.put(0xef8,60);
        w->state.layer.put(0xe30,w->state.slots.data()+60);
        rejectedScale(*w,oracle,normal);
    }
    for(int first:{INT_MIN+1,INT_MAX-63}) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=1024;w->events.reserve(1024);
            for(auto& object:w->state.objects)object.put(0x4e0,INT_MIN);
            w->members[2]=nullptr;w->state.objects[1].put<unsigned char>(0x520,1);
        }
        const std::array<ScaleStep,2> steps{{{first,true,true,2,.25f},{first+1,true,false,.5f,4}}};
        compareScale(oracle,*a,*b,steps,0x1fbf);
    }
    for(bool observers:{false,true}) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);w->observerWrites=observers;w->observerClearsFlags=observers;
            w->center.put(0x4e0,-1);w->center.put(0x2a0,.125f);w->center.put(0x2a4,-.25f);
        }
        const std::array<ScaleStep,2> steps{{{1,true,true,2,.5f},{2,true,false,.25f,4}}};
        compareScale(oracle,*a,*b,steps,0x1fbf);
        require(b->center.get<int>(0x4e0)==2&&b->center.get<float>(0x2a0)==0&&b->center.get<float>(0x2a4)==0,"old center stamp resets offsets during scale program");
        require(std::any_of(b->events.begin(),b->events.end(),[](const Event& e){return e.kind==EventKind::Section&&e.object==4;}),"center reset section callback observed");
    }
    for(unsigned count:{0u,64u,4096u}) {
        auto a=std::make_unique<World>(oracle,count),b=std::make_unique<World>(oracle,count);
        a->capture=b->capture=false;std::array<ScaleStep,64> steps{};
        for(unsigned n=0;n<steps.size();++n)steps[n]={static_cast<int>(n+1),true,n%2==0,scales[n%9],scales[(n+3)%9]};
        compareScale(oracle,*a,*b,steps,0x1f80);
    }
    if(argc==3&&std::string(argv[1])=="--benchmark")benchmarkScale(oracle,argv[2]);
    else require(argc==1,"optional --benchmark output.csv");
    chlorium::test::SystemInputGuard::assertNoGeode();_mm_setcsr(saved);
    std::cout<<"PASS "<<checks<<" native scale-group assertions; states/callbacks/FP/errno compared\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
