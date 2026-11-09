#pragma once
#include "../R4-36/NativeOpacityHash.hpp"

namespace chlorium::research::samecell {
// Original arithmetic/branches; changed-section mutations terminate at an owned witness.
class SectionOracle {
    unsigned char* code_=nullptr;
public:
    using Call=void(*)(void*,void*);
    SectionOracle(const SectionOracle&)=delete;SectionOracle& operator=(const SectionOracle&)=delete;
    explicit SectionOracle(const std::filesystem::path& game){
        opacityhash::Image image;image.open(game);std::array<unsigned char,4096> out{};
        image.read(0x227f50,out.data(),0x117);std::uint64_t hash=14695981039346656037ull;
        for(unsigned i=0;i<0x117;++i)hash=(hash^out[i])*1099511628211ull;
        opacityhash::require(hash==0x1c1d7f366edbe2f4ull,"exact native section body");
        image.read(0x6230c8,out.data()+0x200,8);image.read(0x62369c,out.data()+0x208,4);
        auto rel=[&](unsigned at,unsigned next,unsigned target){const auto value=std::int32_t(target)-std::int32_t(next);std::memcpy(out.data()+at,&value,4);};
        rel(0x31,0x35,0x200);rel(0x39,0x3d,0x208);
        // Changed branch: increment an owned layer counter, then execute the native epilogue.
        const unsigned at=0xa2;const unsigned char witness[]{0xff,0x87,0x00,0x3f,0,0,0xe9};std::memcpy(out.data()+at,witness,sizeof(witness));rel(at+7,at+11,0x10c);
        code_=static_cast<unsigned char*>(VirtualAlloc(nullptr,out.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        opacityhash::require(code_!=nullptr,"owned section oracle page");std::memcpy(code_,out.data(),out.size());DWORD old=0;
        if(!VirtualProtect(code_,out.size(),PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),code_,out.size())){
            VirtualFree(code_,0,MEM_RELEASE);code_=nullptr;throw std::runtime_error("section oracle W then RX");}
    }
    ~SectionOracle(){if(code_)VirtualFree(code_,0,MEM_RELEASE);}
    Call native()const noexcept{return reinterpret_cast<Call>(code_);}
};
}
