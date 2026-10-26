#include "OwnedIdentityProgram.hpp"
#include "../R4-20/LoopComparison.hpp"
#include <climits>
using namespace chlorium::research::transform_shadow;

void rejected(World& w,const OwnedTransformOracle& oracle,std::span<const IdentityStep> steps) {
    const auto objects=w.state.objects;const auto slots=w.state.slots;
    const auto center=w.center;const auto layer=w.state.layer;
    const auto group=w.group;const auto groupData=w.groupData;
    const auto parent=w.parent.math(),child=w.child.math();const auto members=w.members;
    const auto events=w.events;const auto callbacks=w.callbacks;const auto overflow=w.overflow;
    const auto control=_mm_getcsr();const auto previousErrno=errno;
    require(!ownedIdentityProgram(w,oracle,steps),"unsupported program rejected");
    require(control==_mm_getcsr()&&errno==previousErrno,"rejection preserves FP state and errno");
    require(center.data==w.center.data&&layer.data==w.state.layer.data&&group.data==w.group.data&&groupData.data==w.groupData.data,"rejection precedes layer/center/group writes");
    require(parent==w.parent.math()&&child==w.child.math()&&members==w.members&&slots==w.state.slots,"rejection precedes helper/membership/tracking writes");
    require(events==w.events&&callbacks==w.callbacks&&overflow==w.overflow,"rejection precedes observation");
    for(unsigned n=0;n<objects.size();++n)require(objects[n].data==w.state.objects[n].data,"rejection preserves entire objects");
}
void prepareCapture(World& w) {w.eventLimit=1024;w.events.reserve(w.eventLimit);}
void compareProgram(const OwnedTransformOracle& oracle,World& a,World& b,std::span<const IdentityStep> steps,unsigned mode) {
    _mm_setcsr(mode);errno=1177;
    for(const auto step:steps) {
        a.state.layer.put(0x3e0,step.stamp);if(step.clearTracking)a.state.layer.put(0xee4,0);
        a.native(oracle,1,1,step.resetVisual);
    }
    const auto expected=_mm_getcsr();const auto nativeErrno=errno;
    _mm_setcsr(mode);errno=1177;
    require(ownedIdentityProgram(b,oracle,steps),"owned synchronous program accepted");
    require(expected==_mm_getcsr()&&nativeErrno==errno,"whole sequence FP flags/control and errno");
    equivalent(a,b);
}
void correctness(const OwnedTransformOracle& oracle) {
    const std::array<IdentityStep,3> normal{{{1,true,true},{1,false,false},{2,true,true}}};
    for(unsigned kind=0;kind<12;++kind) {
        auto w=std::make_unique<World>(oracle);_mm_setcsr(0x1fbf);errno=1177;
        auto steps=normal;
        if(kind==0)w->center.put(0x2a0,std::bit_cast<float>(0x7fc00000u));
        if(kind==1)w->center.put(0x488,2.f);
        if(kind==2)w->state.objects[0].put(0x3d4,1.f);
        if(kind==3)w->members[1]=reinterpret_cast<void*>(std::uintptr_t(0x12345678));
        if(kind==4)w->objectTable[0x4a8/8]=nullptr;
        if(kind==5)w->layerTable[0x490/8]=nullptr;
        if(kind==6)w->child.data[0x54]=1;
        if(kind==7) {
            w->state.layer.put(0xee4,60);w->state.layer.put(0xef8,60);
            w->state.layer.put(0xe30,w->state.slots.data()+60);
            w->center.put(0x4e0,-1);
            require(w->ownedPointers(),"full-register rejection starts from coherent owned tracking");
        }
        if(kind==8)steps[2].clearTracking=false;
        if(kind==9)steps[1].stamp=INT_MIN;
        if(kind==10)w->unknownObserver=true;
        if(kind==11)_mm_setcsr(0x3fbf);
        rejected(*w,oracle,steps);
    }
    {
        auto w=std::make_unique<World>(oracle);_mm_setcsr(0x1f80);
        std::array<IdentityStep,65> tooMany{};rejected(*w,oracle,tooMany);
        rejected(*w,oracle,{});
        World::active=w.get();rejected(*w,oracle,normal);World::active=nullptr;
    }
    const std::array<double,8> positions{0.,-0.,.25,16777216.,0x1p40,-0x1p40,
        std::numeric_limits<double>::denorm_min(),std::numeric_limits<float>::denorm_min()};
    for(unsigned control=0;control<4;++control)for(unsigned trial=0;trial<24;++trial) {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            prepareCapture(*w);w->observerWrites=trial%2!=0;w->observerClearsFlags=trial%3==0;
            w->center.put(0x3b8,positions[trial%8]);w->center.put(0x3c0,-positions[(trial+1)%8]);
            for(unsigned n=0;n<4;++n) {
                auto& o=w->state.objects[n];o.put(0x3b8,positions[(trial+n+3)%8]);o.put(0x3c0,-positions[(trial+n+4)%8]);
                o.put(0x2a0,trial%2?.125f:-0.f);o.put(0x2a4,std::numeric_limits<float>::denorm_min());
                if(n==2&&trial%5==0)o.put<unsigned char>(0x520,1);
                if(n==1&&trial%7==0)o.put<unsigned char>(0x2c8,1);
            }
            if(trial%11==0)w->members[2]=nullptr;
            else if(trial%13==0)w->members[2]=w->members[0];
        }
        std::array<IdentityStep,8> steps{};
        for(unsigned n=0;n<steps.size();++n)steps[n]={static_cast<int>(1+n/3),n%3==0,n%2!=0};
        compareProgram(oracle,*a,*b,steps,0x1f80u|(control&1?0x40u:0)|(control&2?0x8000u:0)|(trial%2?0x3fu:0u));
        // Any next invocation must validate anew, even after an accepted program.
        b->members[0]=nullptr;b->groupData.put<unsigned>(0,999);rejected(*b,oracle,normal);
    }
    for(unsigned count:{0u,4u})for(int first:{INT_MIN,1,INT_MAX-32}) {
        auto a=std::make_unique<World>(oracle,count),b=std::make_unique<World>(oracle,count);
        prepareCapture(*a);prepareCapture(*b);
        std::array<IdentityStep,64> steps{};
        for(unsigned n=0;n<64;++n)steps[n]={first+static_cast<int>(n/2),n%2==0,n%2!=0};
        compareProgram(oracle,*a,*b,steps,0x1fbf);
    }
    {
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        for(auto w:{a.get(),b.get()}) {
            w->state.layer.put(0xee4,59);w->state.layer.put(0xef8,59);
            w->state.layer.put(0xe30,w->state.slots.data()+59);w->center.put(0x4e0,-1);
            for(unsigned n=0;n<59;++n)w->state.slots[n]=w->state.objects[n%4].data.data();
            require(w->ownedPointers(),"exact-capacity fixture starts coherent");
        }
        const std::array<IdentityStep,2> steps{{{1,false,true},{2,true,false}}};
        compareProgram(oracle,*a,*b,steps,0x1f80);
        require(std::any_of(b->events.begin(),b->events.end(),[](const Event& e){return e.kind==EventKind::Disabled&&e.layer.get<int>(0xee4)==64;}),"first phase really fills all 64 tracking slots");
        require(b->state.layer.get<int>(0xee4)==5&&b->state.layer.get<int>(0xef8)==64,"next stamp reuses full register without growth");
    }
    for(unsigned count:{64u,4096u}) {
        auto a=std::make_unique<World>(oracle,count),b=std::make_unique<World>(oracle,count);
        a->capture=b->capture=false;
        std::array<IdentityStep,16> steps{};
        for(unsigned n=0;n<steps.size();++n)steps[n]={static_cast<int>(n+1),true,n%2!=0};
        compareProgram(oracle,*a,*b,steps,0x1f80);
    }
    {
        // Native reset changes Y during phase one, overwriting command two.
        // The program must already own its immutable copy of both commands.
        static_assert(sizeof(IdentityStep)==8);
        auto a=std::make_unique<World>(oracle),b=std::make_unique<World>(oracle);
        IdentityStep* borrowed=nullptr;
        for(auto w:{a.get(),b.get()}) {
            auto& object=w->state.objects[0];
            auto commands=reinterpret_cast<IdentityStep*>(object.data.data()+0x3b8);
            require(reinterpret_cast<std::uintptr_t>(commands)%alignof(IdentityStep)==0,"aliased commands aligned");
            for(unsigned n=0;n<2;++n) {
                std::construct_at(commands+n);
                commands[n].stamp=static_cast<int>(n+1);
                commands[n].clearTracking=true;commands[n].resetVisual=n==0;
            }
            object.put(0x2a4,-1.f);if(w==b.get())borrowed=commands;
        }
        compareProgram(oracle,*a,*b,{borrowed,2},0x1f80);
        require(borrowed[1].stamp!=2,"native producer really overwrote the borrowed input");
    }
}
int main() try {
    const auto saved=_mm_getcsr();_mm_setcsr(0x1f80);
    // Keep these isolated functional checks below the user's game priority.
    if(!SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS))throw std::runtime_error("fixture priority");
    OwnedTransformOracle oracle;correctness(oracle);
    chlorium::test::SystemInputGuard::assertNoGeode();
    std::cout<<"PASS "<<checks<<" synchronous owned-group checks; native callbacks/state/FP compared\n";
    _mm_setcsr(saved);return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
