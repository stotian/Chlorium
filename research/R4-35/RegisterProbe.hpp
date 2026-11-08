#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace chlorium::research::registers {
struct Output {std::array<std::uint32_t,12> xmm{};std::uint32_t result=0;};
static_assert(sizeof(Output)==52);
class Probe {
    unsigned char* page=nullptr;bool retained=false;
    using Caller=void(*)(void*,void*,int,Output*);
    static bool invoke(Caller caller,void* target,void* owner,int group,Output* output) noexcept {
        __try {caller(target,owner,group,output);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
public:
    static constexpr std::array<std::uint32_t,3> sentinels{0x11223344,0x55667788,0x99aabbcc};
    Probe(){
        std::array<unsigned char,4096> code;code.fill(0xcc);std::size_t at=0;
        auto bytes=[&](std::initializer_list<unsigned char> values){if(values.size()>code.size()-at)throw std::runtime_error("owned probe code bound");for(auto v:values)code[at++]=v;};
        auto imm=[&](std::uint32_t v){for(unsigned n=0;n<4;++n)bytes({static_cast<unsigned char>(v>>(n*8))});};
        auto setXmm=[&](unsigned n,std::uint32_t v){bytes({0x41,0xb8});imm(v); // mov r8d, immediate
            bytes({0x66,0x41,0x0f,0x6e,static_cast<unsigned char>(0xc0+n*8)}); // movd xmmN,r8d
            bytes({0x66,0x0f,0x70,static_cast<unsigned char>(0xc0+n*9),0});}; // pshufd xmmN,xmmN,0
        // Caller(target, owner, group, output): retain RBX and the Win64 shadow space.
        bytes({0x53,0x48,0x83,0xec,0x20,0x4c,0x89,0xcb,0x48,0x89,0xc8,0x48,0x89,0xd1,0x44,0x89,0xc2});
        for(unsigned n=1;n<=3;++n)setXmm(n,sentinels[n-1]);
        bytes({0xff,0xd0,0x0f,0x11,0x0b,0x0f,0x11,0x53,0x10,0x0f,0x11,0x5b,0x20,0xf3,0x0f,0x11,0x43,0x30,0x48,0x83,0xc4,0x20,0x5b,0xc3});
        for(unsigned signature=0;signature<2;++signature)for(unsigned detour=0;detour<2;++detour){
            at=256+signature*512+detour*256;
            bytes({0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90});
            if(detour){setXmm(1,0xdeadc0de);setXmm(2,0xabad1dea);setXmm(3,0x0badf00d);}
            if(signature){if(detour)bytes({0x83,0xc2,1});bytes({0xf3,0x0f,0x2a,0xc2});} // float(group [+1])
            else{bytes({0xb8});imm(detour?0x40000000:0x3f800000);bytes({0x66,0x0f,0x6e,0xc0});}
            bytes({0xc3});
        }
        page=static_cast<unsigned char*>(VirtualAlloc(nullptr,code.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!page)throw std::runtime_error("allocate owned probe");std::memcpy(page,code.data(),code.size());DWORD previous=0;
        if(!VirtualProtect(page,code.size(),PAGE_EXECUTE_READ,&previous)||!FlushInstructionCache(GetCurrentProcess(),page,code.size())){
            VirtualFree(page,0,MEM_RELEASE);page=nullptr;throw std::runtime_error("make owned probe executable");}
    }
    Probe(const Probe&)=delete;Probe& operator=(const Probe&)=delete;
    ~Probe(){if(page&&!retained)VirtualFree(page,0,MEM_RELEASE);}
    void* target(unsigned signature)const{return page+256+signature*512;}
    void* detour(unsigned signature)const{return page+512+signature*512;}
    Output run(unsigned signature,bool directDetour=false)const{
        if(signature>1)throw std::runtime_error("owned probe signature");alignas(16) Output result;
        if(!invoke(reinterpret_cast<Caller>(page),directDetour?detour(signature):target(signature),reinterpret_cast<void*>(0x1234),7,&result))throw std::runtime_error("owned register caller fault");
        return result;
    }
    Output external(void* target,void* owner,int group)const{
        alignas(16) Output result;if(!invoke(reinterpret_cast<Caller>(page),target,owner,group,&result))throw std::runtime_error("external owned register caller fault");return result;
    }
    static bool preserved(const Output& value){for(unsigned n=0;n<12;++n)if(value.xmm[n]!=sentinels[n/4])return false;return true;}
    // shortcut: Tulip can retain native handler addresses; keep this one 4-KiB page until process exit.
    void retainForLoader()noexcept{retained=true;}
};
}
