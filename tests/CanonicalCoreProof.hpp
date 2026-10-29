#pragma once
#include "renderer/AreaDivisionPatch.hpp"
#include <cstring>
namespace chlorium::test {
inline bool canonicalGameImage(std::span<const unsigned char> image) {
    if(image.size()!=0xa3a000||image[0]!='M'||image[1]!='Z')return false;
    const auto word=[&](std::size_t at){std::uint16_t v;std::memcpy(&v,image.data()+at,2);return v;};
    const auto dword=[&](std::size_t at){std::uint32_t v;std::memcpy(&v,image.data()+at,4);return v;};
    const auto pe=dword(0x3c);
    if(pe<0x40||pe>image.size()-88)return false;
    return dword(pe)==0x4550&&word(pe+4)==0x8664&&dword(pe+8)==0x69702da0&&
        word(pe+24)==0x20b&&dword(pe+80)==image.size()&&
        chlorium::area::functionRva+chlorium::area::functionBytes<=image.size();
}
inline bool canonicalAreaBody(std::span<const unsigned char> body,bool optimized) {
    using namespace chlorium::area;
    if(body.size()!=functionBytes)return false;
    std::array<unsigned char,functionBytes> original{};
    std::copy(body.begin(),body.end(),original.begin());
    if(optimized)for(const auto& patch:patches) {
        if(std::memcmp(body.data()+patch.offset,patch.replacement.data(),patch.length))return false;
        std::copy_n(patch.expected.begin(),patch.length,original.begin()+patch.offset);
    }
    return hash(original)==functionHash;
}
}
