#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace chlorium::research::opacityhash {
inline constexpr std::uint64_t prime=1099511628211ull;
enum class HashKind {full,compact,factored};
inline constexpr auto lowHashes=[] {std::array<std::uint64_t,256> values{};for(unsigned i=0;i<values.size();++i)values[i]=(14695981039346656037ull^i)*prime;return values;}();
inline std::array<unsigned char,0x3e> hashSubstitution(const std::uint64_t* table,HashKind kind) {
    std::array<unsigned char,0x3e> output;output.fill(0xcc);unsigned at=0;
    auto bytes=[&](std::initializer_list<unsigned char> values){for(auto value:values){if(at>=output.size())throw std::runtime_error("hash substitution bounds");output[at++]=value;}};
    auto imm=[&](std::uint64_t value){for(unsigned n=0;n<8;++n)bytes({static_cast<unsigned char>(value>>(8*n))});};
    if(kind==HashKind::full){bytes({0x48,0xba});imm(reinterpret_cast<std::uintptr_t>(table));bytes({0x4a,0x8b,0x14,0xca});}
    else if(kind==HashKind::compact){
        bytes({0x41,0x0f,0xb6,0xc1,0x48,0xba});imm(reinterpret_cast<std::uintptr_t>(table));
        bytes({0x48,0x8b,0x14,0xc2,0x44,0x89,0xc8,0xc1,0xe8,8,0x48,0x31,0xc2,0x49,0xba});imm(prime*prime*prime);
        bytes({0x49,0x0f,0xaf,0xd2,0x31,0xc0}); // Native missing-key handling requires RAX=0.
    }
    else if(kind==HashKind::factored){
        bytes({0x41,0x0f,0xb6,0xd1,0x48,0xb9});imm(14695981039346656037ull);
        bytes({0x48,0x31,0xca,0x49,0x0f,0xaf,0xd2,0x44,0x89,0xc8,0xc1,0xe8,8,0x48,0x31,0xc2,0x49,0xba});imm(prime*prime*prime);
        bytes({0x49,0x0f,0xaf,0xd2,0x31,0xc0});
    }
    else throw std::runtime_error("unknown hash substitution");
    bytes({0x44,0x89,0x4c,0x24,0x10}); // Retain the original caller-home scratch write.
    const auto distance=output.size()-(at+2);if(distance>127)throw std::runtime_error("hash continuation distance");
    bytes({0xeb,static_cast<unsigned char>(distance)});return output;
}
}
