#include "../R4-12/CausalWorldIR.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
namespace ir=chlorium::research::causal;
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
bool same(ir::Position a,ir::Position b) {return !std::memcmp(&a,&b,sizeof(a));}
double elapsed(Clock::time_point begin) {return std::chrono::duration<double,std::micro>(Clock::now()-begin).count();}
double median(std::vector<double> values) {std::sort(values.begin(),values.end());return values[values.size()/2];}
struct Event {unsigned group;ir::Effect effect;};
struct Fixture {
    ir::World world;
    ir::World::Snapshot initial;
    std::vector<ir::Handle> handles;
    std::vector<double> native,read,debt,total;
    std::uint64_t vectors=0,compiled=0,cohorts=0;
    Fixture(unsigned count,ir::Drain drain):world(4096,drain) {
        for(unsigned n=0;n<count;++n) {
            handles.push_back(world.create({float(n),-float(n)}));world.join(handles.back(),1);
            if(n%8==0) world.join(handles.back(),2);
        }
        initial=world.checkpoint();
    }
};
int main(int argc,char** argv) try {
    require(argc==2,"CSV path required");std::ofstream output(argv[1]);
    output<<"objects,strategy,batches,eager_median_us,reads_median_us,debt_median_us,total_median_us,total_max_us,applied_ops,compiled_commands,cohorts,vector_lanes\n";
    const char* names[]{"merge","cohort_scalar","cohort_simd"};
    for(unsigned count:{4096u,32768u}) {
        std::array<std::unique_ptr<Fixture>,3> fixtures;
        for(unsigned n=0;n<3;++n) fixtures[n]=std::make_unique<Fixture>(count,static_cast<ir::Drain>(n));
        std::array<std::vector<unsigned>,3> members;
        std::vector<ir::Position> initial(count),expected(count),eager(count);
        for(unsigned n=0;n<count;++n) {initial[n]={float(n),-float(n)};members[1].push_back(n);if(n%8==0) members[2].push_back(n);}
        std::vector<Event> commands;
        for(unsigned n=0;n<64;++n) {
            commands.push_back({1,{1.0001f,.0002f,-.0003f,.9999f,.125f,-.0625f}});
            if(n%2==0) commands.push_back({2,{.9999f,-.0001f,.0002f,1.0002f,-.25f,.125f}});
        }
        std::uint64_t operations=0;expected=initial;
        for(auto cmd:commands) for(auto index:members[cmd.group]) {expected[index]=ir::apply(expected[index],cmd.effect);++operations;}
        for(unsigned trial=0;trial<9;++trial) for(unsigned order=0;order<3;++order) {
            const auto strategy=(trial+order)%3;auto& f=*fixtures[strategy];
            for(unsigned part=0;part<2;++part) if((trial+part)%2==0) {
                eager=initial;auto begin=Clock::now();
                for(auto cmd:commands) for(auto index:members[cmd.group]) eager[index]=ir::apply(eager[index],cmd.effect);
                f.native.push_back(elapsed(begin));require(same(eager.back(),expected.back()),"eager final state");
            } else {
                f.world.restore(f.initial);for(unsigned n=0;n<count;++n) f.handles[n]=f.world.handleAt(n);
                const auto before=f.world.work();auto begin=Clock::now();
                for(auto cmd:commands) f.world.enqueue(cmd.group,cmd.effect,ir::Capability::PurePositionSnapshot);
                for(unsigned n=0;n<256;++n) require(same(f.world.read(f.handles[n]),expected[n]),"sparse state");
                const auto read=elapsed(begin);begin=Clock::now();f.world.barrier();const auto debt=elapsed(begin);
                f.read.push_back(read);f.debt.push_back(debt);f.total.push_back(read+debt);
                const auto after=f.world.work();require(after.applied-before.applied==operations,"full debt charged exactly once");
                for(unsigned n=0;n<count;++n) require(same(f.world.read(f.handles[n]),expected[n]),"all settled state bits");
                f.vectors=after.vectorLanes-before.vectorLanes;f.compiled=after.compiledCommands-before.compiledCommands;
                f.cohorts=after.cohorts-before.cohorts;
                if(strategy==2) require(f.vectors>0,"SIMD backend active");
            }
        }
        for(unsigned strategy=0;strategy<3;++strategy) {
            auto& f=*fixtures[strategy];
            output<<count<<','<<names[strategy]<<",9,"<<median(f.native)<<','<<median(f.read)<<','<<median(f.debt)<<','
                <<median(f.total)<<','<<*std::max_element(f.total.begin(),f.total.end())<<','<<operations<<','<<f.compiled<<','<<f.cohorts<<','<<f.vectors<<'\n';
        }
    }
    output.flush();require(bool(output),"CSV complete");std::cout<<"PASS matched settlement fixtures; every final object verified, not GD FPS\n";return 0;
} catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
