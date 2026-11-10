#include "../R4-33/GpuFixture.hpp"
#include <intrin.h>
using Count=unsigned(*)(const void*);
extern "C" unsigned R76CallCount(const void*,const void*,const void*,unsigned);
static Count nativeCount=nullptr;static const void* expectedArray=nullptr;static const void* expectedLayer=nullptr;static const void* expectedSource=nullptr;
static unsigned expectedMask=0,observed=0;static bool valid=true;
extern "C" unsigned R76ObserveFollow(const void* array,const void* layer,const void* source,unsigned x,unsigned y)noexcept{
    const auto result=nativeCount(array);const unsigned fp=_mm_getcsr();const DWORD error=GetLastError();
    valid&=array==expectedArray && layer==expectedLayer && source==expectedSource && x==(expectedMask&1) && y==((expectedMask>>1)&1);++observed;
    _mm_setcsr(0x1f80);SetLastError(999);_mm_setcsr(fp);SetLastError(error);return result;
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==2,"pinned Cocos DLL required");chlorium::test::SystemInputGuard input;const auto cocos=loadNative(argv[1],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};nativeCount=reinterpret_cast<Count>(GetProcAddress(cocos,"?count@CCArray@cocos2d@@QEBAIXZ"));
    require(nativeCount && hash(reinterpret_cast<unsigned char*>(nativeCount),16)==0x947b5236ff403801ull,"pinned native Cocos count");
    const unsigned saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};
    std::array<unsigned char,64> array{};std::array<unsigned,8> data{};const auto* pointer=data.data();std::memcpy(array.data()+0x38,&pointer,sizeof(pointer));
    std::array<unsigned char,80> source{};std::array<unsigned char,64> layer{};expectedArray=array.data();expectedSource=source.data();expectedLayer=layer.data();
    for(unsigned fp:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned i=0;i<128;++i){data[0]=i*513;expectedMask=i%4;_mm_setcsr(fp|unsigned(i%64));SetLastError(17+i);const auto result=nativeCount(array.data());const auto flags=_mm_getcsr();const auto error=GetLastError();
        const auto actual=R76CallCount(array.data(),layer.data(),source.data(),expectedMask);require(actual==result && valid && _mm_getcsr()==flags && GetLastError()==error,"native count result, all gateway arguments, accrued FP flags and LastError");}
    require(observed==512,"one observer per original native call");std::cout<<"PASS 512 native Follow-count gateway ABI witnesses; four FP modes, accrued flags, LastError and exactly-once call results; no GD launch\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
