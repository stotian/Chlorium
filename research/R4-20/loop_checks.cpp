#include "IdentityLinearPhase.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
using namespace chlorium::research::transform_shadow;
unsigned checks=0;
void require(bool b,const char* why){++checks;if(!b){std::cerr<<"FAIL "<<why<<" after "<<checks<<'\n';std::exit(1);}}
void equivalent(const World& a,const World& b) {
    require(a.callbacks==b.callbacks&&a.events==b.events,"native callback order, state, helper caches and FP flags");
    for(const auto w:{&a,&b})for(const auto& e:w->events)require(e.pointersValid,"callback snapshots contain only valid owned pointers");
    require(a.ownedPointers()&&b.ownedPointers(),"all owned pointers remain valid");
    require(a.state.objects.size()==b.state.objects.size(),"object cardinality");
    for(unsigned n=0;n<a.state.objects.size();++n){auto x=a.state.objects[n],y=b.state.objects[n];x.put<std::uintptr_t>(0,0);y.put<std::uintptr_t>(0,0);require(x.data==y.data,"entire object bytes");}
    auto x=a.center,y=b.center;x.put<std::uintptr_t>(0,0);y.put<std::uintptr_t>(0,0);require(x.data==y.data,"center state");
    auto l=a.state.layer,r=b.state.layer;
    for(unsigned at:{0u,0x8c8u,0x8e8u,0xe28u,0xe30u,0xe38u}){l.put<std::uintptr_t>(at,0);r.put<std::uintptr_t>(at,0);}
    require(l.data==r.data,"layer mutations");
    require(a.state.slots.size()==b.state.slots.size(),"tracking cardinality");
    for(unsigned n=0;n<a.state.slots.size();++n)require(a.identity(a.state.slots[n])>=-1&&b.identity(b.state.slots[n])>=-1&&a.identity(a.state.slots[n])==b.identity(b.state.slots[n]),"tracking order and identity");
    require(a.parent.math()==b.parent.math()&&a.child.math()==b.child.math(),"final helper fields/caches");
    require(!a.overflow&&!b.overflow,"callback coverage complete");
}
std::uint64_t bits=0x79e22d178ac4136full;
std::uint64_t next(){bits^=bits<<13;bits^=bits>>7;bits^=bits<<17;return bits;}
void correctness(const OwnedTransformOracle& oracle) {
    {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->members.resize(1);w->groupData.put<unsigned>(0,1);
            w->center.put(0x3b8,16777216.);w->center.put(0x3c0,0.);
            w->state.objects[0].put(0x3b8,.25);w->state.objects[0].put(0x3c0,0.);
        }
        _mm_setcsr(0x1f80);a->native(oracle);
        _mm_setcsr(0x1f80);require(identityLinearPhase(*b,oracle),"quantized identity phase accepted");equivalent(*a,*b);
        require(a->state.objects[0].get<double>(0x3b8)==0.&&a->state.objects[0].get<float>(0x2a0)==-.25f,
                "identity recentering produces real native per-object displacement");
    }
    for(unsigned control=0;control<4;++control)for(unsigned trial=0;trial<256;++trial) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        a->observerWrites=b->observerWrites=trial%2!=0;
        a->observerClearsFlags=b->observerClearsFlags=trial%3==0;
        for(unsigned n=0;n<4;++n) {
            const double x=static_cast<double>(static_cast<int>(next()%4000000)-2000000)/16.;
            const double y=static_cast<double>(static_cast<int>(next()%400000)-200000)/32.;
            for(auto w:{a.get(),b.get()}) {
                auto& o=w->state.objects[n];o.put(0x3b8,trial%3?x:std::nextafter(x,0.));o.put(0x3c0,y);
                o.put(0x2a0,trial%2?.125f:-0.f);o.put(0x2a4,trial%3?-.375f:0.f);
                o.put<unsigned char>(0x2c8,static_cast<unsigned char>(trial%3==0));
                if(trial%5==0&&n==2)o.put<unsigned char>(0x520,1);
            }
        }
        const double cx=static_cast<double>(static_cast<int>(next()%32000000)-16000000)/4.;
        const double cy=static_cast<double>(static_cast<int>(next()%30000000)-15000000)/8.;
        for(auto w:{a.get(),b.get()}) {
            w->center.put(0x3b8,cx);w->center.put(0x3c0,cy);
            if(trial%7==0)w->center.put(0x4e0,-1);
            if(trial%11==0)w->members[2]=nullptr;
            else if(trial%13==0)w->members[2]=w->members[0];
        }
        const auto mode=0x1f80u|((control&1)?0x40u:0)|((control&2)?0x8000u:0)|(trial%4==0?0x3fu:trial%4==1?0x15u:0u);
        for(unsigned phase=0;phase<3;++phase) {
            for(auto w:{a.get(),b.get()}){w->state.layer.put(0x3e0,static_cast<int>(phase+1));w->state.layer.put(0xee4,0);}
            const bool resetVisual=trial%2!=0;
            _mm_setcsr(mode);errno=1177;a->native(oracle,1,1,resetVisual);const auto flags=_mm_getcsr()&63;const auto nativeErrno=errno;
            _mm_setcsr(mode);errno=1177;require(identityLinearPhase(*b,oracle,1,1,resetVisual),"guarded phase accepted");const auto gotFlags=_mm_getcsr()&63;const auto gotErrno=errno;
            equivalent(*a,*b);require(flags==gotFlags&&nativeErrno==gotErrno,"final FP flags and errno");
        }
    }
    const std::array<double,8> boundary{0.,-0.,std::numeric_limits<double>::denorm_min(),
        static_cast<double>(std::numeric_limits<float>::denorm_min()),-static_cast<double>(std::numeric_limits<float>::denorm_min()),
        0x1p40,std::nextafter(0x1p40,0.),-0x1p40};
    for(unsigned control=0;control<4;++control)for(unsigned n=0;n<boundary.size();++n)for(bool resetVisual:{false,true})for(unsigned initialFlags:{0u,63u}) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->state.objects[0].put(0x3b8,boundary[n]);w->state.objects[0].put(0x3c0,-boundary[n]);
            w->state.objects[0].put(0x2a0,std::numeric_limits<float>::denorm_min());w->state.objects[0].put(0x2a4,-std::numeric_limits<float>::denorm_min());
            w->center.put(0x3b8,boundary[(n+1)%boundary.size()]);w->center.put(0x3c0,boundary[(n+2)%boundary.size()]);
            w->observerClearsFlags=true;
        }
        const auto mode=0x1f80u|initialFlags|((control&1)?0x40u:0)|((control&2)?0x8000u:0);
        _mm_setcsr(mode);errno=1177;a->native(oracle,1,1,resetVisual);const auto flags=_mm_getcsr()&63;const auto nativeErrno=errno;
        _mm_setcsr(mode);errno=1177;require(identityLinearPhase(*b,oracle,1,1,resetVisual),"boundary identity phase supported");const auto gotFlags=_mm_getcsr()&63;const auto gotErrno=errno;
        equivalent(*a,*b);require(flags==gotFlags&&nativeErrno==gotErrno,"boundary flags and errno");
    }
    {
        auto a=std::make_unique<World>(oracle,0),b=std::make_unique<World>(oracle,0);
        _mm_setcsr(0x1f80);a->native(oracle);const auto flags=_mm_getcsr()&63;
        _mm_setcsr(0x1f80);require(identityLinearPhase(*b,oracle),"empty phase accepted");equivalent(*a,*b);require((_mm_getcsr()&63)==flags,"empty phase does not publish compiled flags");
    }
    for(unsigned kind=0;kind<7;++kind) {
        auto w=std::make_unique<World>(oracle);
        if(kind==0)w->unknownObserver=true;
        if(kind==1)w->state.objects[1].put(0x488,2.f);
        if(kind==2)w->parent.data[0x54]=1;
        if(kind==3)w->objectTable[0x4a8/8]=nullptr;
        if(kind==4)w->center.put(0x3b8,std::numeric_limits<double>::infinity());
        const auto objects=w->state.objects;const auto helper=w->parent.math();
        const auto mode=kind==5?0x3f80u:0x1fa0u;_mm_setcsr(mode);
        const auto sx=kind==6?std::bit_cast<float>(0x7f800001u):1.f;
        require(!identityLinearPhase(*w,oracle,sx),"unknown phase rejected");
        require(w->events.empty()&&w->parent.math()==helper&&(_mm_getcsr()==mode),"rejection leaves helper/callback/FP state untouched");
        for(unsigned n=0;n<objects.size();++n)require(w->state.objects[n].data==objects[n].data,"rejection leaves objects untouched");
    }
    for(unsigned kind=0;kind<5;++kind) {
        auto w=std::make_unique<World>(oracle);
        const auto foreign=reinterpret_cast<void*>(std::uintptr_t(0x12345678));
        if(kind<2)w->layerTable[(kind?0x498:0x490)/8]=foreign;
        else if(kind<4)w->objectTable[(kind==2?0x508:0x520)/8]=foreign;
        else w->state.slots[7]=foreign;
        const auto objects=w->state.objects;const auto layer=w->state.layer;const auto center=w->center;
        const auto parent=w->parent.math(),child=w->child.math();const auto slots=w->state.slots;
        _mm_setcsr(0x1fbf);errno=1177;
        require(!identityLinearPhase(*w,oracle),"foreign callback or tracking pointer rejected");
        require(w->events.empty()&&w->callbacks==0&&errno==1177&&_mm_getcsr()==0x1fbf,"rejection precedes callbacks/errno/FP mutation");
        require(layer.data==w->state.layer.data&&center.data==w->center.data&&slots==w->state.slots&&parent==w->parent.math()&&child==w->child.math(),"rejection leaves layer/center/slots/helper state unchanged");
        for(unsigned n=0;n<objects.size();++n)require(objects[n].data==w->state.objects[n].data,"rejection leaves every object unchanged");
    }
}
void benchmark(const OwnedTransformOracle& oracle,const char* output) {
    std::ofstream csv(output);csv<<"objects,phases,batches,native_median_us,compiled_median_us,native_max_us,compiled_max_us\n";
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    std::ofstream stages(std::string(output)+".stages.csv");
    stages<<"objects,batch,phases,validation_ns,compilation_ns,preparation_ns,object_loop_ns,publication_ns,candidate_total_us\n";
#endif
    for(unsigned count:{4u,4096u}) {
        std::vector<double> nativeTimes,candidateTimes;
        for(unsigned batch=0;batch<9;++batch) {
            auto a=std::make_unique<World>(oracle,count),b=std::make_unique<World>(oracle,count);
            a->capture=b->capture=false;
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
            stageAudit={};
#endif
            const auto run=[&](World& w,bool fast) {
                bool accepted=true;_mm_setcsr(0x1f80);errno=1177;
                const auto start=std::chrono::steady_clock::now();
                for(unsigned phase=0;phase<16;++phase) {
                    w.state.layer.put(0x3e0,static_cast<int>(phase+1));w.state.layer.put(0xee4,0);
                    if(fast)accepted&=identityLinearPhase(w,oracle);else w.native(oracle);
                }
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
                require(accepted,"benchmark phase accepted");return elapsed;
            };
            double nativeTime,candidateTime;
            if(batch%2){candidateTime=run(*b,true);nativeTime=run(*a,false);}else{nativeTime=run(*a,false);candidateTime=run(*b,true);}
            equivalent(*a,*b);nativeTimes.push_back(nativeTime);candidateTimes.push_back(candidateTime);
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
            stages<<count<<','<<batch<<','<<stageAudit.phases<<','<<stageAudit.validation<<','<<stageAudit.compilation<<','<<stageAudit.preparation<<','<<stageAudit.objects<<','<<stageAudit.publication<<','<<candidateTime<<'\n';
#endif
        }
        std::sort(nativeTimes.begin(),nativeTimes.end());std::sort(candidateTimes.begin(),candidateTimes.end());
        csv<<count<<",16,9,"<<nativeTimes[4]<<','<<candidateTimes[4]<<','<<nativeTimes.back()<<','<<candidateTimes.back()<<'\n';
    }
}
int main(int argc,char** argv) {
    const auto saved=_mm_getcsr();
    _mm_setcsr(0x1f80);
    OwnedTransformOracle oracle;
    auto native=std::make_unique<World>(oracle),candidate=std::make_unique<World>(oracle);
    _mm_setcsr(0x1f80);native->native(oracle);auto expected=_mm_getcsr()&63;
    _mm_setcsr(0x1f80);require(identityLinearPhase(*candidate,oracle),"identity phase supported");auto actual=_mm_getcsr()&63;
    equivalent(*native,*candidate);require(expected==actual,"final FP flags");
    correctness(oracle);
    if(argc>1)benchmark(oracle,argv[1]);
    chlorium::test::SystemInputGuard::assertNoGeode();
    if(argc>1)std::cout<<"PASS alternating full-phase fixture timings; no GD FPS claim\n";
    std::cout<<"Cocos fixture isolated: system XInput selected; Geode absent\n";
    std::cout<<"PASS "<<checks<<" complete native loop identity checks, native_flags="<<expected<<'\n';
    _mm_setcsr(saved);
}
