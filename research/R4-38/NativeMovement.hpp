#pragma once
#include "../R4-36/NativeOpacityHash.hpp"
#include <initializer_list>

namespace chlorium::research::movement {
// Closed owned-array experiment: preload three future cache lines, keep every native callback.
class Oracle {
    unsigned char* code=nullptr;
public:
    using Move=void(*)(void*,void*,double,double);
    Oracle(const Oracle&)=delete;Oracle& operator=(const Oracle&)=delete;
    Oracle(const std::filesystem::path& game,void* count){
        opacityhash::Image image;image.open(game);std::array<unsigned char,0x180> native{};
        image.read(0x22dd50,native.data(),native.size());std::uint64_t h=14695981039346656037ull;
        for(auto byte:native)h=(h^byte)*1099511628211ull;
        opacityhash::require(h==0x4a7b0d8f19100568ull,"exact native moveObjects body");
        std::array<unsigned char,4096> output;output.fill(0xcc);
        auto rel=[&](unsigned at,unsigned next,unsigned destination){const auto d=static_cast<std::int32_t>(destination)-static_cast<std::int32_t>(next);std::memcpy(output.data()+at,&d,4);};
        std::memcpy(output.data()+0xe00,&count,sizeof(count));
        for(unsigned mode=0;mode<2;++mode){const unsigned base=mode*0x400;
            std::memcpy(output.data()+base,native.data(),native.size());rel(base+0x5d,base+0x61,0xe00);
        }
        std::size_t at=0xc00;
        auto bytes=[&](std::initializer_list<unsigned char> list){for(auto value:list)output.at(at++)=value;};
        bytes({0x48,0x8d,0x87,0x80,0,0,0}); // lea rax,[rdi+16 pointers]
        bytes({0x4c,0x39,0xf0,0x77,0});const auto outside=at-1;
        bytes({0x48,0x8b,0,0x48,0x85,0xc0,0x74,0});const auto empty=at-1;
        for(unsigned offset:{0x278u,0x3b8u,0x4d0u}){bytes({0x0f,0x18,0x88});std::memcpy(output.data()+at,&offset,4);at+=4;}
        const auto skip=at;output[outside]=static_cast<unsigned char>(skip-outside-1);output[empty]=static_cast<unsigned char>(skip-empty-1);
        const auto preloadBytes=static_cast<unsigned>(skip-0xc00);
        // Inline the same preloads in a second clone; relocate every branch crossing the insertion.
        std::memcpy(output.data()+0x800,native.data(),0x90);
        std::memcpy(output.data()+0x890,output.data()+0xc00,preloadBytes);
        std::memcpy(output.data()+0x890+preloadBytes,native.data()+0x90,native.size()-0x90);
        rel(0x85d,0x861,0xe00);
        rel(0x805,0x809,0x97f+preloadBytes);
        rel(0x871,0x875,0x950+preloadBytes);
        rel(0x88b,0x88f,0x94b+preloadBytes);
        rel(0x947+preloadBytes,0x94b+preloadBytes,0x890);
        // RAX is dead on every native path here; restore the original load/test and its flags.
        bytes({0x48,0x8b,0x17,0x48,0x85,0xd2,0xe9});rel(static_cast<unsigned>(at),static_cast<unsigned>(at+4),0x496);at+=4;
        output[0x490]=0xe9;rel(0x491,0x495,0xc00);output[0x495]=0x90;
        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,output.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        opacityhash::require(code!=nullptr,"owned movement code page");std::memcpy(code,output.data(),output.size());DWORD previous=0;
        if(!VirtualProtect(code,output.size(),PAGE_EXECUTE_READ,&previous)||!FlushInstructionCache(GetCurrentProcess(),code,output.size())){
            VirtualFree(code,0,MEM_RELEASE);code=nullptr;throw std::runtime_error("owned movement W then RX");}
    }
    ~Oracle(){if(code)VirtualFree(code,0,MEM_RELEASE);}
    Move native()const noexcept{return reinterpret_cast<Move>(code);}
    Move trampoline()const noexcept{return reinterpret_cast<Move>(code+0x400);}
    Move prefetch()const noexcept{return reinterpret_cast<Move>(code+0x800);}
};
}
