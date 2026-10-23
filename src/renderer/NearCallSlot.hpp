#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace chlorium::matrix {
// Six-byte virtual calls become six-byte RIP-relative indirect calls. Allocate
// only a nearby read-only pointer page, not an executable assembly trampoline.
inline void* allocateNearCallSlot(std::span<const std::uintptr_t> sites,
                                  std::uintptr_t target, std::uintptr_t preferred) {
    if (sites.empty()) return nullptr;
    std::uintptr_t low = 0x10000, high = (std::numeric_limits<std::uintptr_t>::max)();
    constexpr auto reach = std::uintptr_t(INT32_MAX);
    for (const auto site : sites) {
        low = (std::max)(low, site + 6 > reach ? site + 6 - reach : std::uintptr_t(0x10000));
        high = (std::min)(high, site + 6 + reach);
    }
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const auto granularity = std::uintptr_t(info.dwAllocationGranularity);
    const auto pageSize = std::uintptr_t(info.dwPageSize);
    const auto align = [granularity](std::uintptr_t value) { return (value + granularity - 1) & ~(granularity - 1); };
    low = align(low);
    if (high <= low || high - low < pageSize) return nullptr;
    preferred = (std::clamp)(align(preferred), low, high);
    const auto search = [&](std::uintptr_t begin, std::uintptr_t end) -> void* {
        for (auto cursor = begin; cursor < end;) {
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region))) break;
            const auto regionEnd = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
            const auto candidate = align(cursor);
            if (region.State == MEM_FREE && candidate < end && end - candidate >= pageSize &&
                candidate < regionEnd && regionEnd - candidate >= pageSize) {
                auto memory = VirtualAlloc(reinterpret_cast<void*>(candidate), pageSize,
                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if (memory) {
                    std::memcpy(memory, &target, sizeof(target));
                    DWORD previous = 0;
                    if (VirtualProtect(memory, pageSize, PAGE_READONLY, &previous)) return memory;
                    VirtualFree(memory, 0, MEM_RELEASE);
                    return nullptr;
                }
            }
            if (regionEnd <= cursor) break;
            cursor = regionEnd;
        }
        return nullptr;
    };
    if (auto memory = search(preferred, high)) return memory;
    return search(low, preferred);
}
} // namespace chlorium::matrix
