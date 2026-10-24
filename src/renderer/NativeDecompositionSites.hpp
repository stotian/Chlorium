#pragma once
#include <cstdint>
#include <array>
namespace chlorium::native {
struct DecompositionCall {
    std::uintptr_t rva;
    std::array<unsigned char, 5> expected;
};
inline constexpr std::array<DecompositionCall, 4> decompositionCalls{{
    {0x22d174, {0xe8, 0xcf, 0x46, 0x2a, 0x00}},
    {0x22d19c, {0xe8, 0xa7, 0x46, 0x2a, 0x00}},
    {0x229b26, {0xe8, 0x1d, 0x7d, 0x2a, 0x00}},
    {0x229b4c, {0xe8, 0xf7, 0x7c, 0x2a, 0x00}},
}};
constexpr bool isDecompositionCaller(std::uintptr_t relative, bool area) noexcept {
    return area ? relative == decompositionCalls[2].rva + 5 || relative == decompositionCalls[3].rva + 5
                : relative == decompositionCalls[0].rva + 5 || relative == decompositionCalls[1].rva + 5;
}
}
