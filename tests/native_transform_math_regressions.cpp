#include "renderer/NativeTransformMath.hpp"
#include "renderer/NativeDecompositionSites.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    using chlorium::native::isDecompositionCaller;
    static_assert(isDecompositionCaller(0x22d179, false) && isDecompositionCaller(0x22d1a1, false));
    static_assert(isDecompositionCaller(0x229b2b, true) && isDecompositionCaller(0x229b51, true));
    static_assert(!isDecompositionCaller(0x229b2b, false) && !isDecompositionCaller(0x22d179, true));
    static_assert(!isDecompositionCaller(0x229b26, true) && !isDecompositionCaller(0x229b50, true));
    chlorium::native::TransformAtanCache<> cache;
    unsigned calls = 0;
    auto original = [&](float y, float x) { ++calls; return std::atan2(y, x); };
    auto same = [](float a, float b) {
        return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
    };
    bool hit = false;
    float first = cache.evaluate(0.5f, -0.75f, 0, original, &hit);
    if (calls != 1 || hit || !same(first, std::atan2(0.5f, -0.75f))) return 1;
    float repeated = cache.evaluate(0.5f, -0.75f, 0, original, &hit);
    if (calls != 1 || !hit || !same(first, repeated)) return 2;
    cache.evaluate(0.5f, -0.75f, 1, original, &hit);
    if (calls != 2 || hit) return 3;
    cache.reset();
    cache.evaluate(0.5f, -0.75f, 1, original, &hit);
    if (calls != 3 || hit) return 4;

    // Both argument order and all four signed-zero quadrants matter to atan2f.
    for (float y : {0.0f, -0.0f}) for (float x : {0.0f, -0.0f}) {
        auto result = cache.evaluate(y, x, 0, original);
        if (!same(result, std::atan2(y, x))) return 5;
    }
    if (same(cache.evaluate(0.25f, 1.0f, 0, original),
             cache.evaluate(1.0f, 0.25f, 0, original))) return 6;

    unsigned before = calls;
    for (int i = 0; i < 2; ++i) {
        float result = cache.evaluate(std::numeric_limits<float>::infinity(), 1.0f,
                                      0, original, &hit);
        if (hit || !same(result, std::atan2(std::numeric_limits<float>::infinity(), 1.0f))) return 7;
    }
    if (calls != before + 2) return 8;
    cache.evaluate(std::numeric_limits<float>::quiet_NaN(), 1.0f, 0, original, &hit);
    if (hit) return 9;

    // Deliberate direct-map collisions must be misses, never wrong results.
    chlorium::native::TransformAtanCache<1> collisions;
    for (int i = 1; i < 100; ++i) {
        float y = static_cast<float>(i) * 0.125f;
        float x = -static_cast<float>(i) * 0.25f;
        if (!same(collisions.evaluate(y, x, 0, original), std::atan2(y, x))) return 10;
    }
    chlorium::native::TransformAtanCache<> nonfiniteResult;
    unsigned invalidCalls = 0;
    auto invalid = [&](float, float) { ++invalidCalls; return std::numeric_limits<float>::quiet_NaN(); };
    nonfiniteResult.evaluate(1, 2, 0, invalid);
    nonfiniteResult.evaluate(1, 2, 0, invalid, &hit);
    if (invalidCalls != 2 || hit) return 11;
    for (auto pair : {std::pair{std::numeric_limits<float>::denorm_min(), 1.0f},
                      std::pair{std::numeric_limits<float>::min(), std::numeric_limits<float>::max()}}) {
        before = calls;
        auto expected = std::atan2(pair.first, pair.second);
        for (int i = 0; i < 2; ++i) {
            auto result = cache.evaluate(pair.first, pair.second, 0, original, &hit);
            if (hit || !same(result, expected)) return 12;
        }
        if (calls != before + 2) return 13;
    }
    // A narrow generation type makes the production wrap logic reachable in
    // a small test. Old generation-one entries must not survive a full wrap.
    chlorium::native::TransformAtanCache<1, std::uint8_t> wrap;
    unsigned wrapCalls = 0;
    auto changing = [&](float, float) { return static_cast<float>(++wrapCalls); };
    if (!same(wrap.evaluate(1, 2, 0, changing), 1.0f)) return 14;
    for (int i = 0; i < 255; ++i) wrap.reset();
    if (!same(wrap.evaluate(1, 2, 0, changing, &hit), 2.0f) || hit || wrapCalls != 2) return 15;
    if (!same(wrap.evaluate(1, 2, 0, changing, &hit), 2.0f) || !hit || wrapCalls != 2) return 16;

    chlorium::native::TransformReusePolicy reuse;
    for (int i = 0; i < 256; ++i) if (!reuse.record(true)) return 17;
    // A good first window must not conceal a bad suffix.
    for (int i = 0; i < 255; ++i) if (!reuse.record(false)) return 18;
    if (reuse.record(false) || !reuse.disabled()) return 19;
    reuse.reset();
    for (int i = 0; i < 256; ++i) reuse.record(i < 64);
    if (reuse.disabled()) return 20;
    reuse.reset();
    for (int i = 0; i < 256; ++i) reuse.record(i < 63);
    if (!reuse.disabled()) return 21;
    reuse.reset();
    if (reuse.disabled() || !reuse.record(true)) return 22;
    std::cout << "Exact transform math cache: bits, controls, epoch reset/wrap, exceptional bypass, collisions and reuse windows passed\n";
}
