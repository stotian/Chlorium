#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace chlorium::research::maskedcorners {
class OrderedKernel {
    unsigned char* page=nullptr;
    unsigned retAt=0;
public:
    using Method=void(*)(const std::uint32_t*,const std::uint32_t*,std::uint32_t,void*);
    OrderedKernel(const OrderedKernel&)=delete;OrderedKernel& operator=(const OrderedKernel&)=delete;
    OrderedKernel(){
        std::array<unsigned char,4096> code;code.fill(0xcc);unsigned at=0;
        auto bytes=[&](std::initializer_list<unsigned char> values){for(auto value:values)code.at(at++)=value;};
        auto op=[&](unsigned opcode,unsigned to,unsigned from){bytes({0x0f,static_cast<unsigned char>(opcode),static_cast<unsigned char>(0xc0|(to<<3)|from)});};
        auto ss=[&](unsigned to,unsigned base,unsigned offset){bytes({0xf3,0x0f,0x10,static_cast<unsigned char>(0x40|(to<<3)|base),static_cast<unsigned char>(offset)});};
        auto splat=[&](unsigned reg,unsigned offset){ss(reg,1,offset);op(0xc6,reg,reg);bytes({0});};
        auto relative=[&](unsigned destination){const auto displacement=static_cast<std::int32_t>(destination)-static_cast<std::int32_t>(at+4);std::memcpy(code.data()+at,&displacement,4);at+=4;};
        ss(0,2,0);ss(1,2,8);bytes({0xf3});op(0x58,1,0);
        op(0x14,0,0);op(0x14,1,1);op(0x16,0,1); // x1,x1,x2,x2
        ss(1,2,4);ss(3,2,12);bytes({0xf3});op(0x58,3,1);
        op(0x14,3,1);op(0x16,3,3);op(0x28,1,3); // y2,y1,y2,y1
        splat(2,0);op(0x59,2,0);
        splat(3,8);bytes({0x0f,0x57,0x1d});relative(0xf10);op(0x59,3,1);
        op(0x5c,2,3);splat(3,16);op(0x58,2,3); // X in xmm2
        splat(3,4);op(0x59,3,0);splat(4,12);op(0x59,4,1);
        bytes({0x0f,0x28,0x05});relative(0xf00); // top-lane mask
        op(0x28,5,4);op(0x54,5,0);op(0x28,1,0);op(0x55,1,3);op(0x56,5,1);
        op(0x28,1,3);op(0x54,1,0);op(0x55,0,4);op(0x56,1,0);
        op(0x58,5,1);splat(0,20);op(0x58,5,0); // exact corner-specific Y operand order
        op(0x28,0,2);op(0x14,0,5);op(0x15,2,5);
        auto store=[&](unsigned opcode,unsigned reg,unsigned offset){bytes({0x41,0x0f,static_cast<unsigned char>(opcode),static_cast<unsigned char>(0x41|(reg<<3)),static_cast<unsigned char>(offset)});};
        store(0x13,0,0);store(0x17,0,24);store(0x13,2,48);store(0x17,2,72);
        for(unsigned offset:{8u,32u,56u,80u})bytes({0x45,0x89,0x41,static_cast<unsigned char>(offset)});
        retAt=at;bytes({0xc3});
        const std::array<std::uint32_t,4> top{~0u,0,~0u,0},sign{0x80000000u,0x80000000u,0x80000000u,0x80000000u};
        std::memcpy(code.data()+0xf00,top.data(),16);std::memcpy(code.data()+0xf10,sign.data(),16);
        page=static_cast<unsigned char*>(VirtualAlloc(nullptr,code.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!page)throw std::runtime_error("allocate ordered arithmetic page");std::memcpy(page,code.data(),code.size());DWORD old=0;
        if(!VirtualProtect(page,code.size(),PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),page,code.size())){
            VirtualFree(page,0,MEM_RELEASE);page=nullptr;throw std::runtime_error("ordered arithmetic W then RX");}
    }
    Method method()const noexcept{return reinterpret_cast<Method>(page);}
    const unsigned char* image()const noexcept{return page;}
    unsigned returnOffset()const noexcept{return retAt;}
    ~OrderedKernel(){if(page)VirtualFree(page,0,MEM_RELEASE);}
};
}
