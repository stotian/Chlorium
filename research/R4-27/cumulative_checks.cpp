#include "../R4-26/ScaleFixture.hpp"
#include <climits>
#include <cmath>
int main(int argc,char** argv)try {
    const auto saved=_mm_getcsr();_mm_setcsr(0x1f80);OwnedTransformOracle oracle;
    const std::array<float,9> scales{.0625f,.125f,.25f,.5f,1,2,4,8,16};
    for(unsigned controls:{0u,0x40u,0x8000u,0x8040u})for(auto x:scales)for(auto y:scales)for(unsigned observers=0;observers<2;++observers) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);w->observerWrites=observers!=0;w->observerClearsFlags=observers!=0;
            w->center.put(0x4e0,-1);w->center.put(0x2a0,.125f);w->center.put(0x2a4,-.25f);
            w->state.objects[0].put(0x3b8,std::nextafter(32768.,0.));
            w->state.objects[1].put(0x3b8,0x1p40);w->state.objects[2].put(0x3c0,-0x1p40);
        }
        const std::array<ScaleStep,4> steps{{{1,true,true,x,y},{1,false,false,1.f/x,1.f/y},{1,true,true,y,x},{2,true,true,.5f,2}}};
        compareScale<true>(oracle,*a,*b,steps,0x1f80u|controls|(observers?63u:0u));
    }
    for(unsigned kind=0;kind<8;++kind) {
        auto w=std::make_unique<World>(oracle);_mm_setcsr(0x1fbf);errno=1177;
        std::array<ScaleStep,2> steps{{{1,true,true,2,.5f},{1,false,true,.5f,2}}};
        if(kind==0){steps[0].x=16;steps[1].x=2;}
        if(kind==1){steps[0].y=.0625f;steps[1].y=.5f;}
        if(kind==2)steps[1].stamp=0;
        if(kind==3){steps[1].stamp=2;steps[1].clearTracking=false;}
        if(kind==4)w->members[1]=w->members[0];
        if(kind==5)w->state.objects[0].put(0x4e0,1);
        if(kind==6)w->unknownObserver=true;
        if(kind==7)steps[0].x=std::bit_cast<float>(0x7f812345u);
        rejectedScale<true>(*w,oracle,steps);
    }
    for(unsigned count:{0u,64u,4096u}) {
        auto a=std::make_unique<World>(oracle,count),b=std::make_unique<World>(oracle,count);
        a->capture=b->capture=false;std::array<ScaleStep,64> steps{};
        for(unsigned n=0;n<steps.size();++n)steps[n]={static_cast<int>(1+n/4),n%4==0,n%2==0,n%2?.5f:2.f,n%2?4.f:.25f};
        compareScale<true>(oracle,*a,*b,steps,0x1f80);
    }
    for(int stamp:{INT_MIN+1,INT_MAX}) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);
            for(auto& object:w->state.objects)object.put(0x4e0,INT_MIN);
            w->members[2]=nullptr;w->state.objects[1].put<unsigned char>(0x520,1);
        }
        const std::array<ScaleStep,2> steps{{{stamp,true,true,2,.25f},{stamp,false,false,.5f,4}}};
        compareScale<true>(oracle,*a,*b,steps,0x1fbf);
    }
    {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        const std::array<ScaleStep,2> commands{{{1,true,true,2,.5f},{1,false,true,.5f,2}}};ScaleStep* borrowed=nullptr;
        for(auto w:{a.get(),b.get()}) {
            w->eventLimit=512;w->events.reserve(512);
            auto cache=reinterpret_cast<ScaleStep*>(w->parent.data.data()+0x8c);
            for(unsigned n=0;n<commands.size();++n)std::construct_at(cache+n,commands[n]);
            if(w==b.get())borrowed=cache;
        }
        compareScale<true>(oracle,*a,*b,{borrowed,2},0x1f80);
        require(borrowed[1].stamp!=1,"cumulative borrowed commands really overwritten");
    }
    if(argc==3&&std::string(argv[1])=="--benchmark")benchmarkScale<true>(oracle,argv[2]);
    else require(argc==1,"optional --benchmark output.csv");
    chlorium::test::SystemInputGuard::assertNoGeode();_mm_setcsr(saved);
    std::cout<<"PASS "<<checks<<" native cumulative scale assertions; same/new stamps, callbacks, state, FP and errno\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
