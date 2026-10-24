#include "renderer/NearCallThunk.hpp"
#include <intrin.h>
#include <bit>
#include <cmath>
#include <cstdio>

namespace {
using Function = float (*)(float, float);
void* observedReturn = nullptr;
unsigned observedY = 0, observedX = 0;
__declspec(noinline) float probe(float y, float x) {
    observedReturn = _ReturnAddress();
    observedY = std::bit_cast<unsigned>(y);
    observedX = std::bit_cast<unsigned>(x);
    return std::atan2(y, x);
}
}

int main() {
    auto callers = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!callers) return 1;
    std::array<std::uintptr_t, 4> sites{};
    for (std::size_t i = 0; i < sites.size(); ++i)
        sites[i] = reinterpret_cast<std::uintptr_t>(callers + i * 32 + 4);
    auto thunk = chlorium::native::allocateNearCallThunk(sites,
        reinterpret_cast<std::uintptr_t>(&probe), sites[0] + 65536);
    if (!thunk) return 2;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(thunk, &region, sizeof(region)) || region.Protect != PAGE_EXECUTE_READ) return 3;
    for (std::size_t i = 0; i < sites.size(); ++i) {
        // A real Win64 caller: reserve shadow space, E8, restore and return.
        std::array<unsigned char, 14> code{
            0x48, 0x83, 0xec, 0x28, 0xe8, 0, 0, 0, 0, 0x48, 0x83, 0xc4, 0x28, 0xc3};
        const auto offset = std::int64_t(reinterpret_cast<std::uintptr_t>(thunk)) - std::int64_t(sites[i] + 5);
        if (offset < INT32_MIN || offset > INT32_MAX) return 4;
        const auto relative = static_cast<std::int32_t>(offset);
        std::memcpy(code.data() + 5, &relative, sizeof(relative));
        std::memcpy(callers + i * 32, code.data(), code.size());
    }
    DWORD previous = 0;
    if (!VirtualProtect(callers, 4096, PAGE_EXECUTE_READ, &previous) ||
        !FlushInstructionCache(GetCurrentProcess(), callers, 4096)) return 5;
    constexpr std::array<unsigned, 12> bits{
        0, 0x80000000, 0x3f800000, 0xbf800000, 0x3eaaaaab, 0x41a12345,
        1, 0x80000001, 0x7f7fffff, 0x7f800000, 0xff800000, 0x7fc12345};
    unsigned checks = 0;
    const auto savedControl = _mm_getcsr();
    for (std::size_t i = 0; i < sites.size(); ++i) {
        auto call = reinterpret_cast<Function>(callers + i * 32);
        for (auto yBits : bits) for (auto xBits : bits) {
            const auto y = std::bit_cast<float>(yBits), x = std::bit_cast<float>(xBits);
            const auto control = (savedControl | 0x1f80u) & ~0x3fu;
            _mm_setcsr(control);
            const auto expected = probe(y, x);
            const auto expectedFlags = _mm_getcsr();
            _mm_setcsr(control);
            const auto result = call(y, x);
            const auto actualFlags = _mm_getcsr();
            if (observedReturn != reinterpret_cast<void*>(sites[i] + 5) ||
                observedY != yBits || observedX != xBits || actualFlags != expectedFlags ||
                std::bit_cast<unsigned>(result) != std::bit_cast<unsigned>(expected)) return 6;
            ++checks;
        }
    }
    _mm_setcsr(savedControl);
    VirtualFree(callers, 0, MEM_RELEASE);
    VirtualFree(thunk, 0, MEM_RELEASE);
    std::printf("Native E8 bridge: %u ABI/result/FP comparisons, four return addresses and RX protection passed\n", checks);
}
