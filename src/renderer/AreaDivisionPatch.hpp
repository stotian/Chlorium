#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <algorithm>

namespace chlorium::area {
constexpr std::size_t functionRva = 0x227b60, functionBytes = 0xc5;
constexpr std::uint64_t functionHash = 0xc248a1635fe53fcbull;
struct PatchBytes {
    std::size_t offset, length;
    std::array<unsigned char, 8> expected, replacement;
};
// Keep the first adjusted quotient in xmm6. No branches, comparisons,
// input conversion or floating-point expression are approximated/reordered.
constexpr std::array<PatchBytes, 3> patches{{
    {0x69, 7, {0x0f,0x28,0xf0,0xf3,0x0f,0x5e,0xf1}, {0x0f,0x1f,0x80,0x00,0x00,0x00,0x00}},
    {0x8d, 7, {0x0f,0x28,0xf0,0xf3,0x0f,0x5e,0xf1}, {0x0f,0x1f,0x80,0x00,0x00,0x00,0x00}},
    {0xb8, 8, {0x0f,0x28,0x34,0x24,0xf3,0x0f,0x5e,0xc1}, {0x0f,0x28,0xc6,0x0f,0x28,0x34,0x24,0x90}}
}};
// Validate every source instruction before changing the owned plan. Runtime
// installs one contiguous patch, never a partially enabled set of three.
inline bool rewriteQuotients(std::span<unsigned char> body) {
    if(body.size()!=functionBytes)return false;
    for(const auto& patch:patches)
        if(!std::equal(patch.expected.begin(),patch.expected.begin()+patch.length,body.begin()+patch.offset))return false;
    for(const auto& patch:patches)
        std::copy_n(patch.replacement.begin(),patch.length,body.begin()+patch.offset);
    return true;
}
inline std::uint64_t hash(std::span<const unsigned char> bytes) {
    std::uint64_t result = 14695981039346656037ull;
    for (const auto byte : bytes) { result ^= byte; result *= 1099511628211ull; }
    return result;
}
}
