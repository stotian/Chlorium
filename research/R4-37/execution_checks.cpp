#include "../R4-34/QuadReadWatch.hpp"
#include "../R4-35/RegisterProbe.hpp"
#include <iostream>
#include <stdexcept>
using chlorium::research::readwatch::Watch;
using chlorium::research::registers::Probe;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
__declspec(noinline) std::uint64_t thirdArgument(std::uintptr_t a,std::uintptr_t b,std::uint64_t value){return value^a^b;}
namespace chlorium::research::readwatch {
struct WatchChecks {
    static void ownership(){
        Watch w;w.slotMask=3;w.desired=5;w.execution=true;
        w.addresses={0x101,0x219,0,0};w.saved={0x10,0x20,0x30,0x40,0,0x400};
        CONTEXT c{};c.Dr0=0x101;c.Dr1=0x219;c.Dr2=0x900;c.Dr3=0xa00;c.Dr7=0x405;
        Watch::restore(c,w);
        require(c.Dr0==0x10&&c.Dr1==0x20&&c.Dr2==0x900&&c.Dr3==0xa00&&c.Dr7==0x400&&!w.ownershipLost,"execution cleanup touches only owned slots");
        c.Dr0=0x101;c.Dr1=0x219;c.Dr7=0x100405;
        Watch::restore(c,w);
        require(w.ownershipLost&&c.Dr1==0x219&&(c.Dr7&0xf0000)==0,"foreign slot control is preserved");
    }
};
}
int main()try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    chlorium::research::readwatch::WatchChecks::ownership();
    Probe probe;Watch watch;
    const auto target=reinterpret_cast<std::uintptr_t>(probe.target(0));
    const std::array<std::uintptr_t,2> addresses{target,target+25};
    require(*reinterpret_cast<const unsigned char*>(target+25)==0xc3,"owned return opcode");
    require(!watch.armExecution({}),"empty execution watch rejected");
    SetLastError(0x4c47);require(watch.armExecution(addresses),"arm entry/return execution breakpoints");
    require(GetLastError()==0x4c47,"arming preserves last error");
    for(unsigned i=0;i<600;++i){const auto result=probe.run(0);require(result.result==0x3f800000&&Probe::preserved(result),"native result and live XMM registers unchanged");}
    SetLastError(0x4c48);watch.stop();
    require(GetLastError()==0x4c48&&!watch.armed&&!watch.ownershipLost,"execution cleanup and last error");
    require(watch.hits[0]==600&&watch.hits[1]==600&&watch.count==512&&watch.overflow,"counts continue beyond bounded event log");
    for(unsigned i=0;i<watch.count;++i)require(watch.events[i].afterInstruction==addresses[i%2]&&watch.events[i].slots==(1ull<<(i%2)),"execution trap order and pre-instruction address");
    require(probe.run(0).result==0x3f800000&&watch.hits[0]==600,"stopped execution watch is inert");
    require(watch.armExecution(std::span(addresses).first(1)),"single slot rearm");
    require(probe.run(0).result==0x3f800000,"rearmed call");watch.stop();
    require(watch.hits[0]==1&&watch.hits[1]==0&&watch.count==1&&!watch.overflow,"new interval resets counts");
    const std::array<std::uintptr_t,1> third{reinterpret_cast<std::uintptr_t>(&thirdArgument)};
    require(watch.armExecution(third),"third argument observer arm");
    auto volatile invokeThird=&thirdArgument;
    const auto returned=invokeThird(17,23,0x123456789abcdef0ull);watch.stop();
    require(returned==(0x123456789abcdef0ull^17^23)&&watch.count==1&&watch.events[0].r8==0x123456789abcdef0ull,"R8 captured before native instruction and preserved");
    std::cout<<"PASS 1200 execution traps, bounded overflow, XMM/native return parity, rearm and owned-slot cleanup; no GD\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
