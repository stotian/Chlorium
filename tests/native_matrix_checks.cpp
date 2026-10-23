#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "renderer/NativeRotationReuse.hpp"
#include "renderer/MatrixReusePolicy.hpp"
#include "renderer/NearCallSlot.hpp"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace chlorium::matrix;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Matrix { float a,b,c,d,tx,ty; };
struct alignas(16) NodeMemory { std::array<unsigned char, 0x200> bytes{}; };
template<class T> void put(NodeMemory& node, unsigned offset, const T& value) {
    std::memcpy(node.bytes.data()+offset, &value, sizeof(value));
}
std::uint64_t hash(const unsigned char* bytes, std::size_t size) {
    std::uint64_t h=14695981039346656037ull;
    for (std::size_t i=0;i<size;++i) { h^=bytes[i]; h*=1099511628211ull; }
    return h;
}
int wmain(int argc, wchar_t** argv) try {
    require(argc == 2, "pass the installed libcocos2d.dll path");
    auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) { std::cerr << "LoadLibrary error " << GetLastError() << '\n'; return 2; }
    auto base=reinterpret_cast<const unsigned char*>(module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    require(pe->FileHeader.TimeDateStamp==0x69702d99 && pe->OptionalHeader.SizeOfImage==0x1c5000,
        "expected installed GD 2.2081 Cocos build");
    require(hash(base+0x37f0,0x468)==0x7abc292bc9db4855ull, "exact native matrix body");
    using Transform=Matrix* (*)(void*,Matrix*);
    using Math=float (*)(float);
    auto native=reinterpret_cast<Transform>(const_cast<unsigned char*>(base+0x37f0));
    auto cosine=*reinterpret_cast<Math const*>(base+0xe4ac8);
    auto sine=*reinterpret_cast<Math const*>(base+0xe4ad0);
    RotationCache cache;
    std::mt19937 rng(0x229a59);
    unsigned compared=0, flagsDifferent=0;
    const auto saved=_mm_getcsr();
    const auto compare=[&](float rx,float ry,float sx,float sy,unsigned controls) {
        if (!supportedAngle(rx)||!supportedAngle(ry)||!finiteBits(sx)||!finiteBits(sy)) return;
        NodeMemory original;
        put(original,0x38,rx); put(original,0x3c,ry); put(original,0x40,sx); put(original,0x44,sy);
        const float px=(compared&1)?-0.f:123.125f, py=-32768.75f;
        put(original,0x4c,px); put(original,0x50,py);
        original.bytes[0x111]=original.bytes[0x112]=1;
        original.bytes[0x110]=1; // inverseDirty must not be cleared
        NodeMemory predicted=original;
        Matrix expected{};
        _mm_setcsr(controls);
        require(native(original.bytes.data(),&expected)==&expected,"member return-buffer ABI");
        const auto nativeFlags=_mm_getcsr()&0x3f;
        _mm_setcsr(controls);
        const auto linear=calculateLinear(rx,ry,sx,sy,controls&0xffc0u,cache,cosine,sine);
        Matrix actual{linear.coefficients[0],linear.coefficients[1],linear.coefficients[2],linear.coefficients[3],px,py};
        const auto candidateFlags=_mm_getcsr()&0x3f;
        if (nativeFlags!=candidateFlags) ++flagsDifferent;
        if (std::memcmp(&expected,&actual,sizeof(actual))) {
            std::cerr<<"mismatch rx="<<rx<<" ry="<<ry<<" sx="<<sx<<" sy="<<sy<<" csr="<<controls<<'\n';
            for(unsigned i=0;i<6;++i) {
                std::uint32_t a,b; std::memcpy(&a,reinterpret_cast<char*>(&expected)+4*i,4);
                std::memcpy(&b,reinterpret_cast<char*>(&actual)+4*i,4);
                std::cerr<<i<<": "<<std::hex<<a<<" vs "<<b<<std::dec<<'\n';
            }
            throw std::runtime_error("matrix bits differ from installed native code");
        }
        put(predicted,0x8c,actual); put(predicted,0x114,px); put(predicted,0x118,py);
        predicted.bytes[0x111]=predicted.bytes[0x112]=0;
        require(original.bytes==predicted.bytes,"native node state differs");
        ++compared;
    };
    const float angles[]{0.f,-0.f,1.f,-1.f,90.f,-90.f,180.f,360.f,1e-29f,-1e-29f,1e10f,3.402823466e38f};
    const float scales[]{0.f,-0.f,1.f,-1.f,2.f,-.125f,1.17549435e-38f,
        std::bit_cast<float>(1u),std::bit_cast<float>(0x80000001u),3.402823466e38f};
    for(unsigned round:{0u,0x2000u,0x4000u,0x6000u}) for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u}) {
        const auto controls=0x1f80u|round|denorm;
        for(float rx:angles) for(float ry:angles) for(float sx:scales) for(float sy:scales) {
            compare(rx,ry,sx,sy,controls); compare(rx,ry,sx,sy,controls);
        }
        for(unsigned i=0;i<15000;++i) {
            float rx=std::bit_cast<float>(std::uint32_t(rng())), ry=std::bit_cast<float>(std::uint32_t(rng()));
            if(i%3==0) ry=rx;
            const float sx=std::bit_cast<float>(std::uint32_t(rng())),sy=std::bit_cast<float>(std::uint32_t(rng()));
            compare(rx,ry,sx,sy,controls); compare(rx,ry,sx,sy,controls);
            if(i%128==0) cache.reset();
        }
    }
    _mm_setcsr(saved);
    std::cout<<"PASS "<<compared<<" native matrix bit/state comparisons; FP status differs in "<<flagsDifferent<<" cases (known cache contract)\n";
    StreamReusePolicy policy;
    require(policy.begin(100),"first probe");
    for(unsigned i=0;i<255;++i) require(!policy.record(false,100),"do not back off before full window");
    require(policy.record(false,100),"poor window starts cooldown");
    require(!policy.begin(100)&&!policy.begin(101)&&policy.begin(102),"one full cooldown frame");
    for(unsigned i=0;i<256;++i) policy.record(false,102);
    require(!policy.begin(104)&&policy.begin(105),"second cooldown grows to two frames");
    policy.reset(); require(policy.begin(0xfffffffeu),"wrap probe");
    for(unsigned i=0;i<256;++i) policy.record(false,0xfffffffeu);
    require(!policy.begin(0xffffffffu)&&policy.begin(0u),"frame wrap cooldown boundary");
    policy.reset();
    for(unsigned i=0;i<256;++i) policy.record(i<64,5);
    require(!policy.disabled(),"25 percent reuse remains enabled");
    const std::array<std::uintptr_t,2> sites{reinterpret_cast<std::uintptr_t>(base)+0x1000,reinterpret_cast<std::uintptr_t>(base)+0x2000};
    auto slot=allocateNearCallSlot(sites,reinterpret_cast<std::uintptr_t>(native),reinterpret_cast<std::uintptr_t>(base)+0x1c5000);
    require(slot,"near call slot allocation");
    MEMORY_BASIC_INFORMATION info{}; VirtualQuery(slot,&info,sizeof(info));
    require(info.Protect==PAGE_READONLY,"pointer page read-only, non-executable");
    require(*reinterpret_cast<Transform*>(slot)==native,"slot target");
    for(auto site:sites) {
        auto delta=std::int64_t(reinterpret_cast<std::uintptr_t>(slot))-std::int64_t(site+6);
        require(delta>=INT32_MIN&&delta<=INT32_MAX,"RIP-relative reach");
    }
    VirtualFree(slot,0,MEM_RELEASE);
    std::cout<<"PASS adaptive cooldown boundaries, modular wrap and near-call pointer allocation\n";
    FreeLibrary(module);
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
