#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include "renderer/AreaDivisionPatch.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace chlorium::area;
using Calculate = float (*)(float, int, float);
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
int main(int argc, char** argv) try {
    require(argc == 2, "pass installed GD executable path");
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<unsigned char> exe{std::istreambuf_iterator<char>(input), {}};
    require(exe.size() > 0x1000, "read GD executable");
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe.data());
    require(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 && size_t(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) < exe.size(), "PE header");
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exe.data() + dos->e_lfanew);
    require(pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.TimeDateStamp == 0x69702da0, "exact GD 2.2081 build");
    const auto sections = IMAGE_FIRST_SECTION(pe);
    const unsigned char* source = nullptr;
    for (unsigned i=0;i<pe->FileHeader.NumberOfSections;++i) {
        if (functionRva < sections[i].VirtualAddress) continue;
        const auto offset = functionRva - sections[i].VirtualAddress;
        if (offset + functionBytes <= sections[i].SizeOfRawData && sections[i].PointerToRawData + offset + functionBytes <= exe.size())
            source = exe.data() + sections[i].PointerToRawData + offset;
    }
    require(source && hash({source,functionBytes}) == functionHash, "complete original routine fingerprint");
    const auto memory = static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(memory != nullptr, "allocate isolated code comparison page");
    std::memcpy(memory,source,functionBytes); std::memcpy(memory+512,source,functionBytes);
    const float one=1.f; std::memcpy(memory+2048,&one,sizeof(one));
    for (unsigned location : {0u,512u}) for (unsigned instruction : {0x20u,0x3bu}) {
        const unsigned char opcode[]{0xf3,0x0f,0x10,0x2d};
        require(!std::memcmp(memory+location+instruction,opcode,4),"native constant load opcode");
        const int displacement=2048-int(location+instruction+8);
        std::memcpy(memory+location+instruction+4,&displacement,sizeof(displacement));
    }
    for (const auto& patch : patches) {
        require(!std::memcmp(memory+512+patch.offset,patch.expected.data(),patch.length),"native patch bytes");
        std::memcpy(memory+512+patch.offset,patch.replacement.data(),patch.length);
    }
    DWORD oldProtection=0;
    require(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&oldProtection),"make own comparison functions executable");
    FlushInstructionCache(GetCurrentProcess(),memory,4096);
    const auto original=reinterpret_cast<Calculate>(memory);
    const auto optimized=reinterpret_cast<Calculate>(memory+512);
    const auto savedCsr=_mm_getcsr();
    std::mt19937 random(0x227b60);
    unsigned checked=0;
    auto compare = [&](float distance,int length,float deadzone,unsigned controls) {
        _mm_setcsr(controls);
        const auto expected=original(distance,length,deadzone);
        const auto expectedFlags=_mm_getcsr() & 0x3f;
        _mm_setcsr(controls);
        const auto actual=optimized(distance,length,deadzone);
        const auto actualFlags=_mm_getcsr() & 0x3f;
        require(std::bit_cast<uint32_t>(expected)==std::bit_cast<uint32_t>(actual),"result bits differ");
        require(expectedFlags==actualFlags,"floating-point exception flags differ");
        ++checked;
    };
    const uint32_t special[]{0,0x80000000,1,0x80000001,0x00800000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345};
    for (const unsigned rounding : {0u,0x2000u,0x4000u,0x6000u}) for (const unsigned denorm : {0u,0x8040u}) {
        const auto controls=0x1f80u|rounding|denorm;
        for (const auto d:special) for (const auto z:special) for (const int length : {0,1,-1,100,2147483647})
            compare(std::bit_cast<float>(d),length,std::bit_cast<float>(z),controls);
        for (unsigned i=0;i<100000;++i)
            compare(std::bit_cast<float>(uint32_t(random())),std::bit_cast<int32_t>(uint32_t(random())),std::bit_cast<float>(uint32_t(random())),controls);
    }
    _mm_setcsr(savedCsr);
    std::cout << "PASS " << checked << " native/patched bit and FP-flag comparisons\n";
    std::vector<float> distances(65536);
    for (unsigned i=0;i<distances.size();++i) distances[i]=float(i%1000);
    for (const float deadzone : {0.f,.2f,.5f}) {
        std::array<std::vector<double>,2> times;
        std::array<uint64_t,2> checksums{};
        for (int round=0;round<3;++round) for (int order=0;order<2;++order) {
            const int kind=(order+round)%2;
            const auto calculate=kind?optimized:original;
            uint64_t checksum=0;
            const auto begin=std::chrono::steady_clock::now();
            for (unsigned repeat=0;repeat<32;++repeat) for (const auto distance : distances)
                checksum+=std::bit_cast<uint32_t>(calculate(distance,1000,deadzone));
            times[kind].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
            checksums[kind]=checksum;
        }
        require(checksums[0]==checksums[1],"performance workloads have different outputs");
        for (auto& values:times) std::sort(values.begin(),values.end());
        std::cout << "deadzone=" << deadzone << " calls=2097152 native_ms=" << times[0][1]
                  << " patched_ms=" << times[1][1] << " ratio=" << times[0][1]/times[1][1] << '\n';
    }
    VirtualFree(memory,0,MEM_RELEASE);
    std::cout << "Timing is routine throughput, not a GD FPS measurement\n";
} catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
