#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>
#include "HashSubstitution.hpp"

namespace chlorium::research::opacityhash {
inline void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
inline std::uint64_t hashGroup(unsigned group){
    std::uint64_t h=14695981039346656037ull;
    for(unsigned n=0;n<4;++n)h=(h^((group>>(8*n))&255))*1099511628211ull;
    return h;
}
class Image {
    HANDLE file=INVALID_HANDLE_VALUE,mapping=nullptr;void* view=nullptr;
public:
    Image()=default;Image(const Image&)=delete;Image& operator=(const Image&)=delete;
    ~Image(){if(view)UnmapViewOfFile(view);if(mapping)CloseHandle(mapping);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
    void read(unsigned rva,void* output,std::size_t size)const{
        require(rva<=0xa3a000&&size<=0xa3a000-rva,"bounded image read");SIZE_T copied=0;
        require(ReadProcessMemory(GetCurrentProcess(),static_cast<const unsigned char*>(view)+rva,output,size,&copied)&&copied==size,"complete image bytes");
    }
    void open(const std::filesystem::path& path){
        file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);require(file!=INVALID_HANDLE_VALUE,"open owned GD image read-only");
        mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr);require(mapping!=nullptr,"non-executing image mapping");
        view=MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);require(view!=nullptr,"map owned image as data");
        IMAGE_DOS_HEADER dos{};read(0,&dos,sizeof(dos));require(dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0&&dos.e_lfanew<4096,"owned DOS header");
        IMAGE_NT_HEADERS64 pe{};read(dos.e_lfanew,&pe,sizeof(pe));
        require(pe.Signature==IMAGE_NT_SIGNATURE&&pe.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&pe.FileHeader.TimeDateStamp==0x69702da0&&pe.OptionalHeader.SizeOfImage==0xa3a000,"exact GD 2.2081 image");
    }
};
class Oracle {
    std::array<std::uint64_t,10000> full{};
    unsigned char* code=nullptr;std::array<unsigned char,0xbe> original{};
    static std::uint64_t hash(std::span<const unsigned char> bytes){std::uint64_t h=14695981039346656037ull;for(auto v:bytes)h=(h^v)*1099511628211ull;return h;}
public:
    using Getter=float(*)(void*,int);using Product=float(*)(void*);
    Oracle(const Oracle&)=delete;Oracle& operator=(const Oracle&)=delete;
    explicit Oracle(const std::filesystem::path& path){
        Image image;image.open(path);std::array<unsigned char,0x7d> product{};std::uint32_t one=0;
        image.read(0x260b80,original.data(),original.size());image.read(0x19ccb0,product.data(),product.size());image.read(0x622c24,&one,4);
        require(hash(original)==0xc75ba7aaf16ba44cull&&hash(product)==0xe7082c1fd4031c28ull&&one==0x3f800000u,"exact getter/product bodies and constant");
        for(unsigned i=0;i<full.size();++i)full[i]=hashGroup(i);
        std::array<unsigned char,4096> output;output.fill(0xcc);
        auto put=[&](unsigned at,const auto& value){require(at<=output.size()&&sizeof(value)<=output.size()-at,"owned code write");std::memcpy(output.data()+at,&value,sizeof(value));};
        auto rel=[&](unsigned at,unsigned next,unsigned destination){const auto delta=static_cast<std::int32_t>(destination)-static_cast<std::int32_t>(next);put(at,delta);};
        put(0xc00,one);
        for(unsigned mode=0;mode<4;++mode){
            const unsigned base=0x400+mode*0x200;
            std::memcpy(output.data()+base,original.data(),original.size());rel(base+0xb9,base+0xbd,0xc00);
            if(mode){
                const auto block=hashSubstitution(mode==1?full.data():lowHashes.data(),mode==1?HashKind::full:mode==2?HashKind::compact:HashKind::factored);
                std::memcpy(output.data()+base+0x2b,block.data(),block.size());
            }
            std::memcpy(output.data()+mode*0x100,product.data(),product.size());rel(mode*0x100+0x22,mode*0x100+0x26,0xc00);
            rel(mode*0x100+0x47,mode*0x100+0x4b,base);
        }
        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,output.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(code!=nullptr,"owned opacity code page");
        std::memcpy(code,output.data(),output.size());DWORD previous=0;
        if(!VirtualProtect(code,output.size(),PAGE_EXECUTE_READ,&previous)||!FlushInstructionCache(GetCurrentProcess(),code,output.size())){VirtualFree(code,0,MEM_RELEASE);code=nullptr;throw std::runtime_error("owned opacity W then RX");}
    }
    ~Oracle(){if(code)VirtualFree(code,0,MEM_RELEASE);}
    Getter getter(unsigned mode)const{require(mode<4,"getter mode");return reinterpret_cast<Getter>(code+0x400+mode*0x200);}
    Product product(unsigned mode)const{require(mode<4,"product mode");return reinterpret_cast<Product>(code+mode*0x100);}
};
}
