#pragma once
#include "NearCallSlot.hpp"
#include <array>

namespace chlorium::native {
// The native E8 instructions have only five bytes available. Their destination
// is a leaf tail-jump, FF 25 00000000 + an absolute pointer. It touches no stack,
// flags, register or return address, so the bridge sees the native caller.
// This is not used by the normal package; it is an unvalidated experiment.
inline void* allocateNearCallThunk(std::span<const std::uintptr_t> sites,
                                  std::uintptr_t target, std::uintptr_t preferred) {
    static_assert(sizeof(std::uintptr_t) == 8);
    // Reuse the existing bounded nearby allocator. It initially publishes only
    // a read-only pointer page, never executable code or a game patch.
    auto memory = chlorium::matrix::allocateNearCallSlot(sites, target, preferred);
    if (!memory) return nullptr;
    for (const auto site : sites) {
        const auto delta = std::int64_t(reinterpret_cast<std::uintptr_t>(memory)) -
                           std::int64_t(site + 5);
        if (delta < INT32_MIN || delta > INT32_MAX) {
            VirtualFree(memory, 0, MEM_RELEASE);
            return nullptr;
        }
    }
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    DWORD previous = 0;
    if (!VirtualProtect(memory, info.dwPageSize, PAGE_READWRITE, &previous)) {
        VirtualFree(memory, 0, MEM_RELEASE);
        return nullptr;
    }
    std::array<unsigned char, 14> code{0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(code.data() + 6, &target, sizeof(target));
    std::memcpy(memory, code.data(), code.size());
    // Write first, then make read/execute. No writable/executable mapping.
    if (!VirtualProtect(memory, info.dwPageSize, PAGE_EXECUTE_READ, &previous) ||
        !FlushInstructionCache(GetCurrentProcess(), memory, code.size())) {
        VirtualFree(memory, 0, MEM_RELEASE);
        return nullptr;
    }
    return memory;
}
}
