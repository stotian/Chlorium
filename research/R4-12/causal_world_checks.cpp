#include "CausalWorldIR.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_set>
namespace ir=chlorium::research::causal;
unsigned checks=0;
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
template<class F> void rejects(F function,const char* why) {
    bool rejected=false;try {function();}catch(const std::invalid_argument&){rejected=true;}require(rejected,why);
}
bool same(ir::Position a,ir::Position b) {return !std::memcmp(&a,&b,sizeof(a));}
int main(int argc,char** argv) try {
    require(argc==2,"timing output argument");std::mt19937 random(0xca05a17);
    const auto savedCSR=_mm_getcsr();
    for(unsigned mode=0;mode<16;++mode) {
        const auto csr=(savedCSR&~0xe07fu)|0x1f80u|((mode&3)<<13)|((mode&4)?0x8000u:0)|((mode&8)?0x40u:0);
        ir::World world;std::vector<ir::Handle> handles;std::vector<ir::Position> eager;
        const std::uint32_t bits[]{0,0x80000000u,1,0x80000001u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u};
        for(unsigned n=0;n<17;++n) {
            eager.push_back({std::bit_cast<float>(bits[n%9]),std::bit_cast<float>(bits[(n+3)%9])});
            handles.push_back(world.create(eager.back()));world.join(handles.back(),1);world.join(handles.back(),2);
        }
        const std::array<ir::Effect,3> effects{{{1,0,0,1,0,0},{.75f,-.25f,.125f,1.5f,-.0f,10},{-1,0,0,-1,4,-4}}};
        for(unsigned n=0;n<3;++n) world.enqueue(1+n%2,effects[n],ir::Capability::PurePositionSnapshot);
        _mm_setcsr(csr);
        for(auto e:effects) for(auto& p:eager) p=ir::apply(p,e);
        const auto flags=_mm_getcsr()&63;
        _mm_setcsr(csr);world.barrier();
        require((_mm_getcsr()&63)==flags,"SIMD settlement masked FP status matches scalar model");
        for(unsigned n=0;n<17;++n) require(same(world.read(handles[n]),eager[n]),"SIMD tail/NaN/zero/rounding bits match scalar");
        require(world.work().vectorLanes>0,"SIMD path actually executed");
    }
    _mm_setcsr(savedCSR);
    for(unsigned trial=0;trial<64;++trial) {
        ir::World world(19,static_cast<ir::Drain>(trial%3));std::vector<ir::Handle> handles;
        std::vector<ir::Position> eager(128);std::vector<std::unordered_set<unsigned>> groups(128);
        for(unsigned n=0;n<128;++n) {eager[n]={float(n)*.7f,-float(n)*.3f};handles.push_back(world.create(eager[n]));}
        for(unsigned step=0;step<512;++step) {
            const auto n=random()%128,g=random()%9;
            switch(random()%6) {
            case 0:
                if(groups[n].insert(g).second) world.join(handles[n],g);
                else rejects([&]{world.join(handles[n],g);},"duplicate membership rejected");break;
            case 1:
                if(groups[n].erase(g)) world.leave(handles[n],g);
                else rejects([&]{world.leave(handles[n],g);},"unknown membership rejected");break;
            case 2: {
                const ir::Effect effect{1.0001f,.002f,-.001f,.9998f,float(int(random()%100)-50)*.1f,float(int(random()%100)-50)*.2f};
                require(world.enqueue(g,effect,ir::Capability::PurePositionSnapshot),"pure command recorded");
                for(unsigned i=0;i<128;++i) if(groups[i].contains(g)) eager[i]=ir::apply(eager[i],effect);break;
            }
            case 3: require(same(world.read(handles[n]),eager[n]),"causal read matches every eager write");break;
            case 4:
                if(step%17==0) {
                    require(!world.enqueue(g,{},ir::Capability::Unknown),"unknown capability creates barrier, no command");
                    for(unsigned i=0;i<128;++i) require(same(world.read(handles[i]),eager[i]),"observer sees full native model state");
                }break;
            case 5: eager[n]={float(step),-float(n)};world.write(handles[n],eager[n]);break;
            }
        }
        for(unsigned n=0;n<128;++n) require(same(world.read(handles[n]),eager[n]),"final group state equals eager model");
        const auto snapshot=world.checkpoint();const auto stale=handles[0];
        world.enqueue(1,{2,0,0,2,100,200},ir::Capability::PurePositionSnapshot);world.restore(snapshot);
        rejects([&]{world.read(stale);},"checkpoint restore invalidates old handles");
        for(unsigned n=0;n<128;++n) {handles[n]=world.handleAt(n);require(same(world.read(handles[n]),eager[n]),"checkpoint restores exact state");}
        const auto dead=handles[7];world.destroy(dead);const auto replacement=world.create({1,2});
        require(replacement.slot==dead.slot&&replacement.generation!=dead.generation,"slot reuse has fresh generation");
        rejects([&]{world.read(dead);},"destroyed handle cannot read replacement");
    }
    {
        ir::World world(1024);std::vector<ir::Handle> handles;std::vector<ir::Position> eager(512);
        for(unsigned n=0;n<512;++n) {
            eager[n]={float(n),-float(n)};handles.push_back(world.create(eager[n]));
            for(unsigned g=0;g<10;++g) if((n+1)&(1u<<g)) world.join(handles.back(),g);
        }
        std::uint64_t operations=0;
        for(unsigned n=0;n<32;++n) for(unsigned g=0;g<10;++g) {
            const ir::Effect e{1.001f,.002f,-.003f,.999f,float(g),-float(n)};
            world.enqueue(g,e,ir::Capability::PurePositionSnapshot);
            for(unsigned k=0;k<512;++k) if((k+1)&(1u<<g)) {eager[k]=ir::apply(eager[k],e);++operations;}
        }
        world.barrier();require(world.work().compileFallbacks==1,"cohort compile budget forces bounded reference fallback");
        require(world.work().applied==operations,"completed cohorts not applied twice on fallback");
        for(unsigned n=0;n<512;++n) require(same(world.read(handles[n]),eager[n]),"diverse cohort fallback state exact");
    }
    // Dense group events and sparse readers: count eliminated immediate work,
    // then charge the full deferred debt at the final explicit barrier.
    constexpr unsigned objects=32768,events=64,visible=256;
    ir::World dense(events);std::vector<ir::Handle> handles;std::vector<ir::Position> eager(objects);
    for(unsigned n=0;n<objects;++n) {eager[n]={float(n),-float(n)};handles.push_back(dense.create(eager[n]));dense.join(handles.back(),1);}
    const ir::Effect effect{1.0001f,.0002f,-.0003f,.9999f,.125f,-.0625f};
    auto begin=std::chrono::steady_clock::now();
    for(unsigned e=0;e<events;++e) for(auto& p:eager) p=ir::apply(p,effect);
    const auto eagerUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
    begin=std::chrono::steady_clock::now();
    for(unsigned e=0;e<events;++e) dense.enqueue(1,effect,ir::Capability::PurePositionSnapshot);
    for(unsigned n=0;n<visible;++n) require(same(dense.read(handles[n]),eager[n]),"sparse reader state exact");
    const auto sparseUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
    const auto before=dense.work();require(before.applied==std::uint64_t(visible)*events,"immediate work depends on readers, not all objects");
    require(dense.pendingCommands()==events,"deferred debt not hidden");
    begin=std::chrono::steady_clock::now();dense.barrier();
    const auto barrierUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
    require(dense.work().applied==std::uint64_t(objects)*events,"full barrier charges all deferred operations");
    for(unsigned n=0;n<objects;++n) require(same(dense.read(handles[n]),eager[n]),"settled state exact");
    std::ofstream out(argv[1]);out<<"objects,events,readers,eager_us,deferred_read_us,debt_barrier_us,immediate_ops,total_settled_ops\n"
        <<objects<<','<<events<<','<<visible<<','<<eagerUs<<','<<sparseUs<<','<<barrierUs<<','<<before.applied<<','<<dense.work().applied<<'\n';
    out.flush();require(bool(out),"debt timing output complete");
    std::cout<<"PASS "<<checks<<" causal model checks; GD reader/callback coverage unproven\n";return 0;
} catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
