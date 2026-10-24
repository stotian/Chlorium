#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "renderer/NativeAffinePoint.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace chlorium::affine;
Apply volatile imported=nullptr;
Apply expectedNative=nullptr;
std::uint64_t expectedEntry=0;
Point* replacement(Point* out,const Point*,const Matrix*) { *out={123.f,-456.f};return out; }
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::uint64_t hash(const unsigned char* bytes, std::size_t size) {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0;i<size;++i) { h^=bytes[i]; h*=1099511628211ull; } return h;
}
__declspec(noinline) Point* candidate(Point* out,const Point* in,const Matrix* matrix) {
    // Includes the runtime contract checks, rather than timing a naked SIMD kernel.
    return applyImportedNonAliased(out,in,matrix,&imported,expectedNative,expectedEntry);
}
int wmain(int argc,wchar_t** argv) try {
    require(argc==2,"pass installed libcocos2d.dll");
    auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(module,"load installed Cocos");
    auto base=reinterpret_cast<const unsigned char*>(module);
    auto native=reinterpret_cast<Apply>(GetProcAddress(module,"?__CCPointApplyAffineTransform@cocos2d@@YA?AVCCPoint@1@AEBV21@AEBUCCAffineTransform@1@@Z"));
    require(native && reinterpret_cast<const unsigned char*>(native)==base+0x65a0,"installed helper address");
    imported=expectedNative=native;
    std::memcpy(&expectedEntry,base+0x65a0,sizeof(expectedEntry));
    std::cout<<"helper_hash="<<std::hex<<hash(base+0x65a0,0xae)<<" ctor_hash="<<hash(base+0xc6d0,9)<<std::dec<<'\n';
    const auto saved=_mm_getcsr();
    unsigned checked=0,flagDifferences=0;
    std::mt19937 random(0x22be5f);
    const std::array<std::uint32_t,17> edge{0,0x80000000,0x3f800000,0xbf800000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0xffc12345,0x7f812345,0xff812345,0x3f000001};
    for(unsigned rounding:{0u,0x2000u,0x4000u,0x6000u}) for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u}) {
        unsigned controls=0x1f80u|rounding|denorm;
        for(unsigned index=0;index<12000;++index) {
            std::array<float,8> inputs;
            for(unsigned j=0;j<8;++j) inputs[j]=std::bit_cast<float>(index<edge.size()*edge.size()?edge[(index+j*7+(index/edge.size())*j)%edge.size()]:std::uint32_t(random()));
            Point point{inputs[0],inputs[1]},expected{},actual{};
            Matrix matrix;std::memcpy(&matrix,inputs.data()+2,sizeof(matrix));
            _mm_setcsr(controls);require(native(&expected,&point,&matrix)==&expected,"native return ABI");
            unsigned expectedFlags=_mm_getcsr()&0x3f;
            _mm_setcsr(controls);require(candidate(&actual,&point,&matrix)==&actual,"candidate return ABI");
            unsigned actualFlags=_mm_getcsr()&0x3f;
            if(expectedFlags!=actualFlags) ++flagDifferences;
            if(std::memcmp(&expected,&actual,sizeof(Point))) {
                std::cerr<<"input="<<index<<" controls="<<controls<<" expected="<<std::hex<<std::bit_cast<std::uint32_t>(expected.x)<<','<<std::bit_cast<std::uint32_t>(expected.y)<<" actual="<<std::bit_cast<std::uint32_t>(actual.x)<<','<<std::bit_cast<std::uint32_t>(actual.y)<<std::dec<<'\n';
                throw std::runtime_error("native point bits differ");
            }
            ++checked;
        }
    }
    _mm_setcsr(saved);
    require(flagDifferences==0,"FP status differs");
    for(unsigned mode=0;mode<8;++mode) {
        std::array<float,16> expected{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},actual=expected;
        const unsigned output=mode;
        native(reinterpret_cast<Point*>(expected.data()+output),reinterpret_cast<Point*>(expected.data()+1),reinterpret_cast<Matrix*>(expected.data()+2));
        applyChecked(reinterpret_cast<Point*>(actual.data()+output),reinterpret_cast<Point*>(actual.data()+1),reinterpret_cast<Matrix*>(actual.data()+2),native);
        require(expected==actual,"overlapping input/output native side effects");
    }
    Point input{3.f,-7.f},out{},reference{};Matrix guardMatrix{1.f,2.f,3.f,4.f,5.f,6.f};
    native(&reference,&input,&guardMatrix);
    expectedEntry^=1;
    candidate(&out,&input,&guardMatrix);
    require(std::memcmp(&out,&reference,sizeof(out))==0,"changed native entry returns through native");
    expectedEntry^=1;
    imported=replacement;
    candidate(&out,&input,&guardMatrix);
    require(out.x==123.f && out.y==-456.f,"later import owner preserved");
    imported=native;
    _mm_setcsr((saved&~0x3fu)&~0x80u);
    applyNonAliased(&out,&input,&guardMatrix,replacement);
    _mm_setcsr(saved);
    require(out.x==123.f && out.y==-456.f,"unmasked exceptions retain fallback");
    std::cout<<"PASS "<<checked<<" exact native point/FP comparisons + 8 overlap cases + 3 compatibility guards\n";
    constexpr unsigned count=8192,repeats=256;
    std::array<Point,count> points,outputs;
    for(unsigned i=0;i<count;++i) points[i]={float(i)*.125f,float(i%97)*-.25f};
    Matrix matrix{.875f,-.375f,.375f,.875f,32.5f,-18.25f};
    volatile float sink=0;
    for(unsigned round=0;round<4;++round) for(unsigned order=0;order<2;++order) {
        bool fast=(order^(round&1))!=0;
        Apply fn=fast?candidate:native;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<repeats;++repeat) for(unsigned i=0;i<count;++i) fn(&outputs[i],&points[i],&matrix);
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        sink=outputs[round].x;
        std::cout<<(fast?"packed":"native")<<" round="<<round<<" ms="<<ms<<'\n';
    }
    FreeLibrary(module);return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n';return 1; }
