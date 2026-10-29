#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../tests/SystemInputGuard.hpp"
#include "IdentityNode.hpp"
#include <immintrin.h>
#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
using namespace chlorium::research::identity_node;
unsigned checks=0;
Native original=nullptr;
__declspec(noinline) Matrix* candidateCall(void* node,Matrix* output){return dispatch(node,output,original);}
void require(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
struct alignas(16) Node {std::array<unsigned char,512> bytes{};};
template<class T>void put(Node& node,unsigned offset,T value){std::memcpy(node.bytes.data()+offset,&value,sizeof(value));}
Node seed(std::uint32_t x,std::uint32_t y) {
    Node node;put(node,0x40,0x3f800000u);put(node,0x44,0x3f800000u);
    put(node,0x4c,x);put(node,0x50,y);node.bytes[0x110]=node.bytes[0x111]=node.bytes[0x112]=1;
    return node;
}
void compare(Node input,Native native,unsigned mode,bool expectedFast) {
    auto a=input,b=input;Matrix expected{},actual{};
    _mm_setcsr(mode);errno=1177;require(native(a.bytes.data(),&expected)==&expected,"native ABI");
    const auto flags=_mm_getcsr();const auto error=errno;
    _mm_setcsr(mode);errno=1177;
    const bool fast=tryIdentity(b.bytes.data(),&actual);if(!fast)native(b.bytes.data(),&actual);
    require(fast==expectedFast,"eligibility matches declared domain");
    require(flags==_mm_getcsr()&&error==errno,"all FP controls/status and errno");
    require(!std::memcmp(&actual,&expected,sizeof(Matrix)),"matrix bits");
    if(a.bytes!=b.bytes){for(unsigned i=0;i<512;++i)if(a.bytes[i]!=b.bytes[i])std::cerr<<"byte "<<std::hex<<i<<" native "<<unsigned(a.bytes[i])<<" fast "<<unsigned(b.bytes[i])<<std::dec<<'\n';}
    require(a.bytes==b.bytes,"entire native node state");
}
void aliasChecks(Native native,unsigned mode) {
    for(int offset:{-20,0,0x38,0x80,0x8c,0x100,0x110,0x11c,0x120}) {
        alignas(16) std::array<unsigned char,576> a{},b{};
        const auto input=seed(0x7f812345u,0x80000000u);
        std::memcpy(a.data()+32,input.bytes.data(),input.bytes.size());b=a;
        auto outA=reinterpret_cast<Matrix*>(a.data()+32+offset);
        auto outB=reinterpret_cast<Matrix*>(b.data()+32+offset);
        _mm_setcsr(mode);errno=1177;require(native(a.data()+32,outA)==outA,"aliased native return");
        const auto flags=_mm_getcsr();const auto error=errno;
        _mm_setcsr(mode);errno=1177;const auto before=b;
        const bool fast=tryIdentity(b.data()+32,outB);
        require(fast==(offset==0x120),"overlapping output rejected, adjacent output accepted");
        if(!fast){require(b==before,"alias rejection is read only");require(dispatch(b.data()+32,outB,native)==outB,"alias dispatch return");}
        require(a==b&&flags==_mm_getcsr()&&error==errno,"alias fallback full bytes/FP/errno");
    }
}
void benchmark(Native native,const char* path) {
    std::ofstream csv(path);require(bool(csv),"timing file opened");
    std::ofstream batches(std::string(path)+".batches.csv");require(bool(batches),"batch timing file opened");
    batches<<"objects,identity_percent,batch,native_us,candidate_us\n";
    csv<<"objects,identity_percent,phases,batches,native_median_us,candidate_median_us,native_max_us,candidate_max_us\n";
    original=native;
    for(unsigned count:{4u,4096u})for(unsigned share:{0u,25u,75u,100u}) {
        std::vector<double> nativeTimes,candidateTimes;
        for(unsigned batch=0;batch<9;++batch) {
            std::vector<Node> a(count),b(count);
            for(unsigned n=0;n<count;++n) {
                a[n]=seed(std::bit_cast<std::uint32_t>(100.25f+float(n)),0xc0000000u);
                if(n%4>=share/25)put(a[n],0x40,2.f);b[n]=a[n];
            }
            const auto run=[&](std::vector<Node>& nodes,Native call) {
                _mm_setcsr(0x1f80);errno=1177;Matrix output{};
                const auto begin=std::chrono::steady_clock::now();
                for(unsigned phase=0;phase<32;++phase)for(auto& node:nodes) {
                    node.bytes[0x111]=node.bytes[0x112]=1;call(node.bytes.data(),&output);
                }
                const auto end=std::chrono::steady_clock::now();
                require(_mm_getcsr()==0x1f80&&errno==1177,"benchmark FP/errno unchanged");
                return std::chrono::duration<double,std::micro>(end-begin).count();
            };
            double n,c;
            if(batch%2){c=run(b,&candidateCall);n=run(a,native);}else{n=run(a,native);c=run(b,&candidateCall);}
            for(unsigned i=0;i<count;++i)require(a[i].bytes==b[i].bytes,"benchmark full final node state");
            nativeTimes.push_back(n);candidateTimes.push_back(c);
            batches<<count<<','<<share<<','<<batch<<','<<n<<','<<c<<'\n';
        }
        std::sort(nativeTimes.begin(),nativeTimes.end());std::sort(candidateTimes.begin(),candidateTimes.end());
        csv<<count<<','<<share<<",32,9,"<<nativeTimes[4]<<','<<candidateTimes[4]<<','<<nativeTimes.back()<<','<<candidateTimes.back()<<'\n';
    }
}
int main(int argc,char** argv)try {
    const auto saved=_mm_getcsr();chlorium::test::SystemInputGuard isolation;
    const auto module=LoadLibraryExW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Geometry Dash\\libcocos2d.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(module!=nullptr,"Cocos loaded without launching GD");chlorium::test::SystemInputGuard::assertNoGeode();
    const auto base=reinterpret_cast<unsigned char*>(module);std::uint64_t hash=14695981039346656037ull;
    for(unsigned i=0;i<0x468;++i)hash=(hash^base[0x37f0+i])*1099511628211ull;
    require(hash==0x7abc292bc9db4855ull,"complete native transform fingerprint");
    const auto native=reinterpret_cast<Native>(base+0x37f0);std::mt19937 random(0x254e4f44);
    const std::array<std::uint32_t,11> special{0,0x80000000u,1,0x80000001u,0x00800000u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u,0x3e800000u};
    for(unsigned round:{0u,0x2000u,0x4000u,0x6000u})for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u}) {
        const auto mode=0x1f80u|round|denorm;
        aliasChecks(native,mode);
        for(unsigned flags:{0u,63u})for(auto x:special)for(auto y:special)compare(seed(x,y),native,mode|flags,true);
        for(unsigned signs=0;signs<64;++signs) {
            auto node=seed(0x7f812345u,0x80000000u);unsigned bit=0;
            for(unsigned at:{0x38u,0x3cu,0x54u,0x58u,0x5cu,0x60u})put(node,at,(signs&(1u<<bit++))?0x80000000u:0u);
            compare(node,native,mode,true);
        }
        for(unsigned n=0;n<2048;++n) {
            auto node=seed(random(),random());node.bytes[0x110]=n%2;node.bytes[0x112]=n%3!=0;node.bytes[0x11c]=n%2;
            for(unsigned at=0x8c;at<0xa4;at+=4)put(node,at,std::uint32_t(random()));
            compare(node,native,mode,true);
        }
        for(unsigned kind=0;kind<7;++kind) {
            auto node=seed(0x3f800000u,0xc0000000u);
            if(kind==0)put(node,0x38,.25f);if(kind==1)put(node,0x40,2.f);
            if(kind==2)put(node,0x54,.25f);if(kind==3)put(node,0x5c,1.f);
            if(kind==4)node.bytes[0x11d]=1;if(kind==5)node.bytes[0x11f]=1;if(kind==6)node.bytes[0x111]=0;
            compare(node,native,mode,false);
        }
    }
    if(argc==3&&std::string(argv[1])=="--benchmark")benchmark(native,argv[2]);
    else require(argc==1,"optional --benchmark output.csv");
    _mm_setcsr(saved);FreeLibrary(module);chlorium::test::SystemInputGuard::assertNoGeode();
    std::cout<<"PASS "<<checks<<" direct identity node checks; full node/FP/errno compared\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<" at "<<checks<<'\n';return 1;}
