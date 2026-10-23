#include "renderer/NativeTransformMath.hpp"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

struct Pair { float y, x; };
struct Result { double ms; std::uint64_t checksum; std::size_t calls; };

template <std::size_t Capacity>
Result run(std::vector<Pair> const& inputs) {
    chlorium::native::TransformAtanCache<Capacity> cache;
    std::size_t calls = 0;
    std::uint64_t checksum = 0;
    // An actual function pointer mirrors the original CRT entry in the runtime
    // prototype. No fast-math flags or atan approximations may be used here.
    float (*volatile crt)(float, float) = ::atan2f;
    auto start = std::chrono::steady_clock::now();
    for (auto p : inputs) {
        float result;
        if constexpr (Capacity == 1) {
            ++calls;
            result = crt(p.y, p.x);
        } else {
            result = cache.evaluate(p.y, p.x, 0, [&](float y, float x) {
                ++calls;
                return crt(y, x);
            });
        }
        checksum = (checksum * 0x100000001b3ull) ^ std::bit_cast<std::uint32_t>(result);
    }
    return {std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), checksum, calls};
}

int main() {
    constexpr std::size_t count = 2'000'000;
    std::cout << std::fixed << std::setprecision(2);
    for (int distinct : {2, 16, 64, 256, 0}) {
        std::vector<Pair> pairs;
        pairs.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto n = distinct ? i % distinct : i;
            float angle = static_cast<float>(n + 1) * (distinct ? 0.037f : 0.0000037f);
            pairs.push_back({std::sin(angle) * 1.25f, std::cos(angle) * 0.75f});
        }
        // First pass is warm-up. Each scenario is rerun three times to expose
        // host noise; these are math-loop times, never a prediction of GD FPS.
        (void)run<1>(pairs);
        for (int repeat = 0; repeat < 3; ++repeat) {
            auto base = run<1>(pairs);
            auto small = run<64>(pairs);
            auto medium = run<256>(pairs);
            auto large = run<1024>(pairs);
            if (base.checksum != small.checksum || base.checksum != medium.checksum || base.checksum != large.checksum) return 1;
            std::cout << "distinct=" << distinct << " repeat=" << repeat
                      << " native_ms=" << base.ms
                      << " cache64_ms=" << small.ms << " misses=" << small.calls
                      << " cache256_ms=" << medium.ms << " misses=" << medium.calls
                      << " cache1024_ms=" << large.ms << " misses=" << large.calls
                      << " checksum=" << base.checksum << '\n';
        }
    }
}
