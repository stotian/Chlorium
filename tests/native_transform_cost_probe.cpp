#include "renderer/NativeTransformMath.hpp"
#include <windows.h>
#include <immintrin.h>
#include <algorithm>
#include <chrono>
#include <cstdio>

using Math = float (*)(float, float);
using Clock = std::chrono::steady_clock;
constexpr unsigned Count = 2000000;
unsigned sink = 0;

template<bool Cached>
double run(Math native, bool axis) {
    chlorium::native::TransformAtanCache<1024> cache;
    const auto controls = _mm_getcsr() & 0xffc0u;
    unsigned checksum = 0;
    const auto start = Clock::now();
    for (unsigned i = 0; i < Count; ++i) {
        const auto key = i & 15;
        const float x = 1.f + key * .0625f;
        const float y = axis ? 0.f : .125f + key * .03125f;
        const float result = Cached ? cache.evaluate(y, x, controls, native) : native(y, x);
        checksum += std::bit_cast<unsigned>(result);
    }
    const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    sink ^= checksum;
    return ms;
}
int main() {
    const auto runtime = LoadLibraryExW(L"ucrtbase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    const auto native = runtime ? reinterpret_cast<Math>(GetProcAddress(runtime, "atan2f")) : nullptr;
    if (!native) return 1;
    for (const bool axis : {true, false}) {
        double plain[3]{}, cached[3]{};
        for (unsigned i = 0; i < 3; ++i) {
            if (i & 1) { cached[i] = run<true>(native, axis); plain[i] = run<false>(native, axis); }
            else { plain[i] = run<false>(native, axis); cached[i] = run<true>(native, axis); }
        }
        std::sort(std::begin(plain), std::end(plain));
        std::sort(std::begin(cached), std::end(cached));
        std::printf("%s: %u calls; native median %.3f ms; cache median %.3f ms; ratio %.3f\n",
            axis ? "axis-aligned" : "non-axis repeated", Count, plain[1], cached[1], cached[1] / plain[1]);
    }
    std::printf("checksum %u; helper probe only, not game FPS\n", sink);
}
