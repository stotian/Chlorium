#include "NativeFieldWatch.hpp"
#include <iostream>
#include <stdexcept>
using chlorium::research::fieldwatch::NativeFieldWatch;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
alignas(64) static volatile double positions[2]{1,2};
__declspec(noinline) static double readX(){return positions[0];}
__declspec(noinline) static double readY(){return positions[1];}
__declspec(noinline) static void writeX(double value){positions[0]=value;}
__declspec(noinline) static void writeY(double value){positions[1]=value;}
int main()try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const auto saved=_mm_getcsr();unsigned rows=0;
    for(unsigned mode:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned round=0;round<12;++round){
        _mm_setcsr(mode);NativeFieldWatch watch;
        const std::array<std::uintptr_t,2> addresses{reinterpret_cast<std::uintptr_t>(&positions[0]),reinterpret_cast<std::uintptr_t>(&positions[1])};
        const std::array<std::uintptr_t,1> misaligned{addresses[0]+1};require(!watch.start(misaligned),"unaligned watch rejected before touching debug state");
        const std::array<std::uintptr_t,2> duplicate{addresses[0],addresses[0]};require(!watch.start(duplicate),"duplicate watch rejected");
        SetLastError(71);require(watch.start(addresses),"arm owned thread fields");require(GetLastError()==71 && _mm_getcsr()==mode,"arm preserves main host state");
        {NativeFieldWatch competing;require(!competing.start(addresses),"competing session cannot take existing debug ownership");}
        watch.frame(19);const auto x=readX(),y=readY();writeX(x+.125);writeY(y-.25);
        require(_mm_getcsr()==mode,"vectored handler preserves FP state");
        require(watch.stop(),"restore and verify debug registers before releasing handler");
        require(GetLastError()==71 && !watch.overflow(),"stop preserves LastError and bounded trace");
        unsigned mask=0;for(const auto& row:watch.rows()){mask|=row.mask;require(row.frame==19 && row.rip && row.exception,"precise native instruction/frame metadata");}
        require(watch.rows().size()==4 && mask==3,"exact two loads and two stores observed");rows+=unsigned(watch.rows().size());
        const auto count=watch.rows().size();writeX(readX());writeY(readY());require(watch.rows().size()==count,"no residual traps after verified disarm");
    }
    _mm_setcsr(saved);std::cout<<"R69 hardware field watch: 48 arm/disarm cycles, "<<rows<<" exact accesses; native FP/LastError, alignment and restored debug state verified; no GD launch\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
