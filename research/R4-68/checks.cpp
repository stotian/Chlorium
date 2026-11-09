#define wmain previousTapeChecks
#include "../R4-42/movement_checks.cpp"
#undef wmain
#include "PackedMirror.hpp"
#include "../R4-64/PacketReader.hpp"
#include <chrono>
#include <algorithm>
namespace pm=chlorium::research::packedmirror;
namespace gp=chlorium::research::groupprogram;
static void publishMovedFields(World& world,std::span<const State> states,std::span<const Range> ranges){
    for(unsigned i=0;i<states.size();++i){const auto& state=states[i];if(!ranges[state.flags>>16][1])continue;
        auto* object=world.objects[i].bytes.data();std::memcpy(object+0x3b8,state.position,16);
        if(!(state.flags&1)){std::memcpy(object+0x4d0,state.last,8);store(object,0x4dc,state.stamp);}
        store(object,0x368,std::uint16_t(((state.flags>>2)&1)|(((state.flags>>3)&1)<<8)));
        store(object,0x351,std::uint16_t(((state.flags>>4)&1)|(((state.flags>>5)&1)<<8)));
    }
}
static void checkCase(chlorium::research::movement::Oracle& oracle,std::span<const State> seed,std::span<const Command> commands,
                      std::span<const Range> ranges,std::span<const unsigned> indices,unsigned mode){
    _mm_setcsr(mode);std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    World world(seed,commands,ranges,indices,table.data());pm::PackedMirror mirror;require(mirror.initialize(seed,commands,ranges,indices),"initialize packed owned states");
    _mm_setcsr(mode);world.run(oracle.native(),commands,unsigned(commands.size()));const auto nativeFp=_mm_getcsr();const auto expected=world.states(seed);
    _mm_setcsr(mode);SetLastError(79);require(mirror.advance(commands,ranges,indices),"advance shared programs in packed SSE2 lanes");
    require(GetLastError()==79 && _mm_getcsr()==nativeFp,"native sticky FP effects and Win32 error contract");
    std::vector<State> actual(seed.size());require(mirror.copyOut(actual),"copy original actor order");
    require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(State)),"all packed state bytes equal actual native moveObjects");
    World published(seed,commands,ranges,indices,table.data());publishMovedFields(published,actual,ranges);
    for(unsigned i=0;i<seed.size();++i)require(published.objects[i].bytes==world.objects[i].bytes,"all 1536 owned object bytes equal native after selective publication");
    auto bad=std::vector<Range>(ranges.begin(),ranges.end());bad[0][1]=65;
    const auto beforeFp=_mm_getcsr();require(!mirror.advance(commands,bad,indices) && _mm_getcsr()==beforeFp,"invalid program rejects before writes or FP effects");
    std::vector<State> unchanged(seed.size());require(mirror.copyOut(unchanged) && !std::memcmp(actual.data(),unchanged.data(),actual.size()*sizeof(State)),"rejected program leaves every byte intact");
    _mm_setcsr(0x3f80);require(!mirror.advance(commands,ranges,indices) && _mm_getcsr()==0x3f80,"unsupported rounding rejects without control changes");_mm_setcsr(mode);
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3 || (argc==4 && std::wstring_view(argv[3])==L"--benchmark"),"owned GD image, R64 compiled packet, optional --benchmark");
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};_mm_setcsr(0x1f80);
    chlorium::research::movement::Oracle oracle(argv[1],reinterpret_cast<void*>(&countArray));
    unsigned checked=0;
    for(unsigned mode:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned count:{1u,2u,3u,31u,257u}){
        std::vector<State> seed(count);std::vector<Command> commands(80);std::vector<Range> ranges(9);std::vector<unsigned> indices;
        for(unsigned i=0;i<commands.size();++i){commands[i].delta[0]=i%4?double(int(i%13)-6)*.1:0;commands[i].delta[1]=i%5?double(int(i%11)-5)*.125:-0.0;commands[i].control[0]=i/7;}
        for(unsigned c=0;c<ranges.size();++c){ranges[c]={unsigned(indices.size()),c?c*7:0};for(unsigned k=0;k<ranges[c][1];++k)indices.push_back(k/2);}
        for(unsigned i=0;i<count;++i){auto& s=seed[i];s.position[0]=double(i)*.1;s.position[1]=-double(i)*.25;s.last[0]=0x7fc02345;s.last[1]=0x80000000;s.stamp=i%5;s.flags=((i%9)<<16)|(i%64);}
        checkCase(oracle,seed,commands,ranges,indices,mode);checked+=count;
    }
    // Masked conversion/addition must not add inexact effects from untouched lanes.
    std::array<State,2> locked{};locked[0].position[0]=0x1p30;locked[0].flags=3;locked[1].flags=1;
    std::array<Command,1> tiny{};tiny[0].delta[0]=0x1p-32;std::array<Range,1> one{{{0,1}}};std::array<unsigned,1> index{0};
    checkCase(oracle,locked,tiny,one,index,0x1f80);checked+=2;
    {
        _mm_setcsr(0x1f80);std::array<State,2> observed{};std::array<Command,1> move{};move[0].delta[0]=1;
        std::array<void*,148> observers{};observers[0x490/8]=reinterpret_cast<void*>(&section);observers[0x498/8]=reinterpret_cast<void*>(&end);
        World world(observed,move,one,index,observers.data());for(auto& object:world.objects)store(object.bytes.data(),0x278,0);world.mutating=true;
        pm::PackedMirror closed;require(closed.initialize(observed,move,one,index),"closed callback counterexample init");
        world.run(oracle.native(),move,1);require(closed.advance(move,one,index),"owned state advance before callback comparison");std::array<State,2> predicted{};require(closed.copyOut(predicted),"counterexample prediction");
        const auto actual=world.states(observed);require(actual[1].position[0]!=predicted[1].position[0],"hostile native callback disproves unordered packed replacement; live adapter must stay disabled");
    }
    _mm_setcsr(0x1f80);auto packet=gp::readPacket(argv[2],_mm_getcsr());
    std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    auto expected=packet.states;const auto& first=packet.frames[0];pm::PackedMirror mirror;
    require(mirror.initialize(packet.states,first.commands,first.ranges,first.indices),"initialize captured shared classes");
    for(const auto& frame:packet.frames){
        World world(expected,frame.commands,frame.ranges,frame.indices,table.data());_mm_setcsr(0x1f80);world.run(oracle.native(),frame.commands,unsigned(frame.commands.size()));const auto fp=_mm_getcsr();expected=world.states(expected);
        _mm_setcsr(0x1f80);require(mirror.advance(frame.commands,frame.ranges,frame.indices),"recurrent packed program");require(_mm_getcsr()==fp,"recurrent native FP flags");
        std::vector<State> actual(expected.size());require(mirror.copyOut(actual) && !std::memcmp(actual.data(),expected.data(),expected.size()*sizeof(State)),"captured packet native state equivalence");
        for(unsigned i=0;i<actual.size();++i)require(!std::memcmp(actual[i].position,frame.expected[i].data(),16),"actual GD end-frame position witness");checked+=unsigned(actual.size());
    }
    std::vector<Range> empty(first.ranges.size());auto preserved=expected;
    require(mirror.advance({},empty,{}) && mirror.copyOut(expected) && !std::memcmp(expected.data(),preserved.data(),expected.size()*sizeof(State)),"empty programs retain all states");
    require(mirror.metrics.actorStores<mirror.metrics.logicalVisits,"state stores coalesce repeated group visits on captured workload");
    std::cout<<"R68 packed mirror states="<<checked<<" loads="<<mirror.metrics.actorLoads<<" stores="<<mirror.metrics.actorStores<<" logical_visits="<<mirror.metrics.logicalVisits<<"; native FP and position witnesses match; NOT live CPU reader ownership or GD FPS\n";
    if(argc==4){
        World native(packet.states,first.commands,first.ranges,first.indices,table.data());
        std::vector<State> materialized(packet.states.size());std::array<std::vector<double>,4> samples;
        for(unsigned round=0;round<10;++round)for(unsigned order=0;order<4;++order){
            const auto variant=(order+round)%4;native.reset(packet.states);pm::PackedMirror candidate;
            require(candidate.initialize(packet.states,first.commands,first.ranges,first.indices),"benchmark mirror initialization");_mm_setcsr(0x1f80);
            const auto start=std::chrono::steady_clock::now();
            for(unsigned repeat=0;repeat<32;++repeat){
                if(!variant)native.run(oracle.native(),first.commands,unsigned(first.commands.size()));
                else{
                    require(candidate.advance(first.commands,first.ranges,first.indices),"bounded benchmark recurrence");
                    if(variant>=2){require(candidate.copyOut(materialized),"benchmark output");if(variant==2)native.scatter(materialized);else publishMovedFields(native,materialized,first.ranges);}
                }
            }
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/32;
            if(round)samples[variant].push_back(ms);
        }
        for(unsigned variant=0;variant<4;++variant){auto values=samples[variant];std::sort(values.begin(),values.end());
            std::cout<<"owned_CPU variant="<<variant<<" median_ms="<<values[values.size()/2]<<" worst_ms="<<values.back()<<" samples="<<values.size()<<"; 0=native closed loop, 1=packed kernel, 2=full object scatter, 3=moved fields only; NOT game FPS or callback/reader ownership\n";}
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
