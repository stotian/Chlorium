#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <array>
#include <cstring>
#include <iterator>
// Test-only independent oracle. Extracts a closed translation-reset component
// from the user's owned GD executable at runtime. No game bytes are distributed.
class NativeResetComponentOracle {
    void* executable_=nullptr;
public:
    NativeResetComponentOracle(const NativeResetComponentOracle&)=delete;
    NativeResetComponentOracle& operator=(const NativeResetComponentOracle&)=delete;
    NativeResetComponentOracle() {
        std::ifstream file("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/GeometryDash.exe",std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        auto bad=[] {throw std::runtime_error("unsupported owned GD reset-component oracle");};
        if(bytes.size()<sizeof(IMAGE_DOS_HEADER)+sizeof(IMAGE_NT_HEADERS64)) bad();
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||
            static_cast<std::size_t>(dos->e_lfanew)>bytes.size()-sizeof(IMAGE_NT_HEADERS64)) bad();
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
        if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
            pe->FileHeader.TimeDateStamp!=0x69702da0||pe->OptionalHeader.SizeOfImage!=0xa3a000||
            pe->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||pe->FileHeader.NumberOfSections>64) bad();
        const auto offset=static_cast<std::size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64);
        if(offset>bytes.size()||pe->FileHeader.NumberOfSections>(bytes.size()-offset)/sizeof(IMAGE_SECTION_HEADER)) bad();
        const auto sections=reinterpret_cast<const IMAGE_SECTION_HEADER*>(bytes.data()+offset);
        constexpr std::size_t rva=0x227c6d,length=0x77;
        const unsigned char* code=nullptr;
        for(unsigned n=0;n<pe->FileHeader.NumberOfSections;++n) {
            const auto& s=sections[n];
            if(rva<s.VirtualAddress||rva+length>static_cast<std::size_t>(s.VirtualAddress)+s.SizeOfRawData) continue;
            const auto raw=static_cast<std::size_t>(s.PointerToRawData)+rva-s.VirtualAddress;
            if(raw>bytes.size()||length>bytes.size()-raw) bad();
            code=bytes.data()+raw;break;
        }
        if(!code) bad();
        std::uint64_t hash=14695981039346656037ull;
        for(std::size_t n=0;n<length;++n) hash=(hash^code[n])*1099511628211ull;
        if(hash!=0x84b38d0d06292674ull) bad();
        // Own ABI adapter: save nonvolatile RBX/RBP/R14/XMM6, supply the native
        // component's object pointer and zero registers, restore, return.
        const unsigned char before[]{0x53,0x55,0x41,0x56,0x48,0x83,0xec,0x20,0xf3,0x0f,0x7f,0x34,0x24,
            0x48,0x89,0xca,0x48,0x89,0xcb,0x45,0x31,0xf6,0x31,0xed,0x0f,0x57,0xf6};
        const unsigned char after[]{0xf3,0x0f,0x6f,0x34,0x24,0x48,0x83,0xc4,0x20,0x41,0x5e,0x5d,0x5b,0xc3};
        const auto size=sizeof(before)+length+sizeof(after);
        executable_=VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(!executable_) bad();
        auto target=static_cast<unsigned char*>(executable_);
        std::memcpy(target,before,sizeof(before));std::memcpy(target+sizeof(before),code,length);
        std::memcpy(target+sizeof(before)+length,after,sizeof(after));
        DWORD previous=0;
        if(!VirtualProtect(executable_,size,PAGE_EXECUTE_READ,&previous)) {
            VirtualFree(executable_,0,MEM_RELEASE);executable_=nullptr;bad();
        }
        FlushInstructionCache(GetCurrentProcess(),executable_,size);
    }
    ~NativeResetComponentOracle(){if(executable_)VirtualFree(executable_,0,MEM_RELEASE);}
    chlorium::research::phase::State reset(chlorium::research::phase::State input,bool* dirty=nullptr) const {
        std::array<unsigned char,0x600> object{};
        std::memcpy(object.data()+0x3b8,&input.position,sizeof(input.position));
        std::memcpy(object.data()+0x2a0,&input.offset,sizeof(input.offset));
        reinterpret_cast<void(*)(void*)>(executable_)(object.data());
        std::memcpy(&input.position,object.data()+0x3b8,sizeof(input.position));
        std::memcpy(&input.offset,object.data()+0x2a0,sizeof(input.offset));
        if(dirty) *dirty=object[0x352]!=0;
        return input;
    }
};
