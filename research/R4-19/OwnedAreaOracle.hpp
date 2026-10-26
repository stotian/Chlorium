#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

// Fixture only: copies complete, fingerprinted functions from the owner's GD.
// Calls between those functions are relocated; vector growth is forbidden.
class OwnedAreaOracle {
    unsigned char* code_ = nullptr;
    static void unsupportedGrowth() { std::abort(); }
public:
    OwnedAreaOracle(const OwnedAreaOracle&) = delete;
    OwnedAreaOracle& operator=(const OwnedAreaOracle&) = delete;
    OwnedAreaOracle() {
        const auto bad=[] { throw std::runtime_error("unsupported owned GD Area oracle"); };
        std::ifstream file("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/GeometryDash.exe",std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        if(bytes.size()<sizeof(IMAGE_DOS_HEADER)) bad();
        IMAGE_DOS_HEADER dos{}; std::memcpy(&dos,bytes.data(),sizeof(dos));
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||
           static_cast<std::size_t>(dos.e_lfanew)>bytes.size()||
           sizeof(IMAGE_NT_HEADERS64)>bytes.size()-dos.e_lfanew) bad();
        IMAGE_NT_HEADERS64 pe{}; std::memcpy(&pe,bytes.data()+dos.e_lfanew,sizeof(pe));
        if(pe.Signature!=IMAGE_NT_SIGNATURE||pe.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
           pe.FileHeader.TimeDateStamp!=0x69702da0||pe.OptionalHeader.SizeOfImage!=0xa3a000||
           pe.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||
           pe.FileHeader.NumberOfSections>64) bad();
        const auto table=static_cast<std::size_t>(dos.e_lfanew)+sizeof(pe);
        if(pe.FileHeader.NumberOfSections>(bytes.size()-table)/sizeof(IMAGE_SECTION_HEADER)) bad();
        std::vector<IMAGE_SECTION_HEADER> sections(pe.FileHeader.NumberOfSections);
        std::memcpy(sections.data(),bytes.data()+table,sections.size()*sizeof(sections[0]));
        const auto at=[&](std::size_t rva,std::size_t length)->std::span<const unsigned char> {
            for(const auto& s:sections) if(rva>=s.VirtualAddress) {
                const auto relative=rva-s.VirtualAddress,raw=static_cast<std::size_t>(s.PointerToRawData);
                if(relative<=s.SizeOfRawData&&length<=s.SizeOfRawData-relative&&raw<=bytes.size()&&
                   relative<=bytes.size()-raw&&length<=bytes.size()-raw-relative)
                    return {bytes.data()+raw+relative,length};
            }
            bad(); return {};
        };
        const auto checked=[&](std::size_t rva,std::size_t length,std::uint64_t expected) {
            auto body=at(rva,length); std::uint64_t hash=14695981039346656037ull;
            for(auto b:body) hash=(hash^b)*1099511628211ull;
            if(hash!=expected) bad(); return body;
        };
        const auto reset=checked(0x227c30,0x279,0xde24cd39a33793b3ull);
        const auto last=checked(0x227eb0,0x94,0x7e900f672011e2d2ull);
        const auto move=checked(0x22aab0,0xe1,0x01de6f3edba821e9ull);
        auto one=at(0x622c24,4); const std::uint32_t oneBits=0x3f800000u;
        if(std::memcmp(one.data(),&oneBits,4)) bad();
        // All targets reside in the same page, so rel32 relocation cannot overflow.
        std::array<unsigned char,4096> image{};
        std::memcpy(image.data(),reset.data(),reset.size());
        std::memcpy(image.data()+0x400,last.data(),last.size());
        std::memcpy(image.data()+0x600,move.data(),move.size());
        const auto call=[&](std::size_t site,std::size_t target) {
            if(image[site]!=0xe8) bad();
            const auto delta=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(site+5);
            std::memcpy(image.data()+site+1,&delta,4);
        };
        call(0x38,0x400); call(0x1d3,0x400); call(0x247,0x800); call(0x623,0);
        image[0x800]=0xff; image[0x801]=0x25;
        const auto trap=reinterpret_cast<std::uintptr_t>(&unsupportedGrowth);
        std::memcpy(image.data()+0x806,&trap,8);
        std::memcpy(image.data()+0x900,&oneBits,4);
        const auto delta=std::int32_t(0x900-(0x143+8));
        std::memcpy(image.data()+0x147,&delta,4);
        code_=static_cast<unsigned char*>(VirtualAlloc(nullptr,image.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!code_) bad();
        std::memcpy(code_,image.data(),image.size());
        DWORD old=0;
        if(!VirtualProtect(code_,image.size(),PAGE_EXECUTE_READ,&old)||
           !FlushInstructionCache(GetCurrentProcess(),code_,image.size())) {
            VirtualFree(code_,0,MEM_RELEASE);code_=nullptr;bad();
        }
    }
    ~OwnedAreaOracle() { if(code_) VirtualFree(code_,0,MEM_RELEASE); }
    void move(void* layer,void* object,float dx,float dy) const {
        reinterpret_cast<void(*)(void*,void*,float,float)>(code_+0x600)(layer,object,dx,dy);
    }
};
