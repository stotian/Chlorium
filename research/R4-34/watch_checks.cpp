#include <algorithm>
#include "QuadReadWatch.hpp"
#include <iostream>
#include <stdexcept>
using chlorium::research::readwatch::Watch;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
namespace chlorium::research::readwatch {
struct WatchChecks {
    static void ownership(){
        Watch w;w.addresses={0x100,0x200,0x300,0x400};w.saved={0x10,0x20,0x30,0x40,0,0x400};
        CONTEXT c{};c.Dr0=0x100;c.Dr1=0x200;c.Dr2=0x300;c.Dr3=0x400;c.Dr7=Watch::ours|0x400;
        Watch::restore(c,w);require(c.Dr0==0x10&&c.Dr3==0x40&&c.Dr7==0x400&&!w.ownershipLost,"restore owned slots");
        c.Dr0=0x900;c.Dr1=0x200;c.Dr2=0x300;c.Dr3=0x400;c.Dr7=Watch::ours|0x400;
        Watch::restore(c,w);require(c.Dr0==0x900&&c.Dr1==0x20&&c.Dr7==0xf0401&&w.ownershipLost,"preserve changed foreign debug slot");
    }
};
}
__declspec(noinline) unsigned read(const volatile unsigned* value){return *value;}
__declspec(noinline) void write(volatile unsigned* value,unsigned n){*value=n;}
int main()try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    chlorium::research::readwatch::WatchChecks::ownership();
    alignas(16) std::array<unsigned,8> memory{};
    SetLastError(0x4c43);Watch watch;require(watch.arm({reinterpret_cast<std::uintptr_t>(&memory[0]),reinterpret_cast<std::uintptr_t>(&memory[2]),reinterpret_cast<std::uintptr_t>(&memory[4]),reinterpret_cast<std::uintptr_t>(&memory[6])}),"arm isolated hardware watch");
    require(GetLastError()==0x4c43,"arm preserves last error");
    for(unsigned i=0;i<4;++i){write(&memory[i*2],i+1);require(read(&memory[i*2])==i+1,"observed memory unchanged");}
    const auto observed=watch.count;
    require(observed==8,"four native writes and four reads observed");
    for(unsigned i=0;i<4;++i)require(watch.events[i*2].slots==(1ull<<i)&&watch.events[i*2+1].slots==(1ull<<i),"correct watched slot");
    write(&memory[1],99);require(read(&memory[1])==99&&watch.count==observed,"unwatched memory must not produce events");
    Watch nested;require(!nested.arm({1,2,3,4}),"nested observer rejected");
    SetLastError(0x4c44);watch.stop();require(!watch.armed&&GetLastError()==0x4c44,"watch disarmed and last error preserved");write(&memory[0],77);require(read(&memory[0])==77&&watch.count==observed,"stopped observer has no events");
    require(watch.arm({reinterpret_cast<std::uintptr_t>(&memory[0]),reinterpret_cast<std::uintptr_t>(&memory[2]),reinterpret_cast<std::uintptr_t>(&memory[4]),reinterpret_cast<std::uintptr_t>(&memory[6])}),"watch can rearm");
    for(unsigned i=0;i<600;++i)write(&memory[0],i);
    watch.stop();require(watch.overflow&&!watch.armed&&memory[0]==599,"bounded overflow and cleanup preserve execution");
    std::cout<<"PASS hardware read/write slots, untouched memory, cleanup and bounded overflow; no GD process\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
