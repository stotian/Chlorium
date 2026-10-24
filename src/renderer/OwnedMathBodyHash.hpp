#pragma once
#include <Geode/Geode.hpp>
#include "NativeDecompositionSites.hpp"
#include <vector>

namespace chlorium::native {
// Cold installation-only hashing. A combined experiment may already own a
// known call in the other component's body. Normalize ONLY a complete patch
// owned by this mod whose published bytes still match the live instruction.
// Unknown modifications remain in the hash and reject the native body.
inline std::uint64_t mathBodyHash(const unsigned char* bytes, std::size_t size) {
#ifdef CHLORIUM_EXPERIMENTAL_MATH_PIPELINE
    std::vector<unsigned char> original(bytes, bytes + size);
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto begin = reinterpret_cast<std::uintptr_t>(bytes);
    const auto patches = geode::Mod::get()->getPatches();
    const auto restore = [&](std::uintptr_t rva, const auto& expected) {
        const auto address = base + rva;
        if (address < begin || address - begin > size || expected.size() > size - (address - begin)) return;
        for (const auto patch : patches) {
            if (patch->getAddress() != address || !patch->isEnabled()) continue;
            const auto& replacement = patch->getBytes();
            if (replacement.size() == expected.size() &&
                !std::memcmp(reinterpret_cast<const void*>(address), replacement.data(), replacement.size()))
                std::memcpy(original.data() + (address - begin), expected.data(), expected.size());
        }
    };
    for (const auto& call : decompositionCalls) restore(call.rva, call.expected);
    constexpr std::array<unsigned char, 6> virtualCall{0xff, 0x90, 0x08, 0x03, 0, 0};
    restore(0x22d00d, virtualCall);
    restore(0x229a59, virtualCall);
    bytes = original.data();
#endif
    std::uint64_t value = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) { value ^= bytes[i]; value *= 1099511628211ull; }
    return value;
}
}
