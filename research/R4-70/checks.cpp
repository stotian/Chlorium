#include "SameCell.hpp"
#include "SectionOracle.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <algorithm>
using namespace chlorium::research::samecell;
using chlorium::research::opacityhash::require;
template<class T> void store(void* object,unsigned offset,T value){std::memcpy(static_cast<unsigned char*>(object)+offset,&value,sizeof(value));}
struct alignas(64) Object {std::array<unsigned char,0x600> bytes{};};
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==2,"pinned GD image required");SectionOracle oracle(argv[1]);
    std::array<unsigned char,0x4000> layer{};Object object;const auto saved=_mm_getcsr();std::uint64_t checked=0,accepted=0;
    const double values[]{0.,-0.,-1.,0x1p-100,0x1p-64,0x1p-32,.1,1.,99.999,100.,9999999.,10000000.,10000001.,0x1p32,
        std::bit_cast<double>(0x7ff8000000000001ull),std::bit_cast<double>(0x7ff0000000000001ull),std::bit_cast<double>(1ull)};
    for(unsigned fp:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u,0x3f80u,0x5f80u,0x7f80u})for(float sx:{0x1p-20f,.01f,.1f,1.f,2.f})for(double x:values)for(double y:values)for(int delta:{-1,0,1}){
        const float scale[]{sx,.01f};store(layer.data(),0x36a0,scale[0]);store(layer.data(),0x36a4,scale[1]);
        store(object.bytes.data(),0x3b8,x);store(object.bytes.data(),0x3c0,y);
        _mm_setcsr(fp);const auto cell=[](double value,float factor){return value<=0?0:value>=10000000.?int(factor*10000000.f):_mm_cvttsd_si32(_mm_mul_sd(_mm_set_sd(double(factor)),_mm_set_sd(value)));};
        auto cx=cell(x,sx),cy=cell(y,.01f);if(cx!=INT_MIN && cx!=INT_MAX)cx+=delta;
        store(object.bytes.data(),0x278,cx);store(object.bytes.data(),0x27c,cy);store(layer.data(),0x3f00,0u);
        const auto before=object.bytes;_mm_setcsr(fp);oracle.native()(layer.data(),object.bytes.data());const auto expected=_mm_getcsr(),changed=read<unsigned>(layer.data(),0x3f00);
        store(layer.data(),0x3f00,0u);_mm_setcsr(fp);SetLastError(19);const bool skip=unchanged(object.bytes.data(),scale);
        if(skip){require(!changed,"classifier skipped a changed native section");++accepted;}
        else{require(_mm_getcsr()==fp,"rejected speculation changed FP flags");oracle.native()(layer.data(),object.bytes.data());}
        require(_mm_getcsr()==expected && GetLastError()==19,"exact native FP/LastError effects");require(object.bytes==before,"native unchanged path and classifier preserve all object bytes");
        require(read<unsigned>(layer.data(),0x3f00)==(skip?0:changed),"changed-section witness preserved");++checked;
    }
    _mm_setcsr(saved);std::cout<<"PASS "<<checked<<" native section decisions/FP states, "<<accepted<<" exact no-write/no-callback shortcuts; changed/unsupported cases retained\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
