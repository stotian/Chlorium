#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <immintrin.h>

namespace chlorium::matrix {
inline float nativeMultiply(float left, float right) noexcept {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(left), _mm_set_ss(right)));
}
inline float flipSign(float value) noexcept {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) ^ 0x80000000u);
}
inline bool finiteBits(float value) noexcept {
    return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
}
inline bool supportedAngle(float value) noexcept {
    const auto magnitude = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    // Very small rotations stay native because CRT underflow/errno effects
    // are outside the reuse contract. Zero (including -0) is supported.
    return magnitude == 0 || (magnitude >= 0x0da24260u && magnitude < 0x7f800000u);
}
struct RotationPair { float cosine = 1.f, sine = 0.f; };
class RotationCache {
    struct Entry {
        std::uint32_t angle = 0, controls = 0, epoch = 0;
        RotationPair value;
    };
    std::array<Entry, 256> entries{};
    std::uint32_t epoch = 1;
    Entry* recent = nullptr;
    std::uint64_t recentHitCount = 0;
public:
    RotationCache() = default;
    RotationCache(const RotationCache&) = delete;
    RotationCache& operator=(const RotationCache&) = delete;
    std::uint64_t recentHits() const noexcept { return recentHitCount; }
    void reset() noexcept {
        recent = nullptr;
        if (++epoch == 0) { for (auto& entry : entries) entry.epoch = 0; epoch = 1; }
    }
    template<class Math>
    RotationPair evaluate(float angle, std::uint32_t controls, Math cosine, Math sine, bool& hit) {
        const auto bits = std::bit_cast<std::uint32_t>(angle);
        if (recent && recent->epoch == epoch && recent->angle == bits && recent->controls == controls) {
            hit = true; ++recentHitCount; return recent->value;
        }
        const std::uint64_t hash = std::uint64_t(bits ^ controls) * 0x9e3779b97f4a7c15ull;
        auto& entry = entries[(hash ^ (hash >> 32)) & 255];
        hit = entry.epoch == epoch && entry.angle == bits && entry.controls == controls;
        if (hit) { recent = &entry; return entry.value; }
        // Exact native float constant, scalar multiply then sign-bit flip.
        // No approximation, angle quantization, or division by prior scales.
        const float radians = flipSign(nativeMultiply(angle, std::bit_cast<float>(0x3c8efa35u)));
        RotationPair result;
        result.cosine = cosine(radians);
        result.sine = sine(radians);
        if (finiteBits(result.cosine) && finiteBits(result.sine)) {
            entry = {bits, controls, epoch, result}; recent = &entry;
        }
        return result;
    }
};
struct LinearResult {
    std::array<float, 4> coefficients{};
    unsigned hits = 0, misses = 0;
    bool zero = false;
};
// Shared with the independent comparison against the installed Cocos routine.
// Eligibility (no skew/anchor/additional transform) is checked by the caller.
template<class Math>
inline LinearResult calculateLinear(float rx, float ry, float sx, float sy,
    std::uint32_t controls, RotationCache& cache, Math cosine, Math sine) {
    RotationPair x, y;
    LinearResult result;
    if (rx == 0 && ry == 0) result.zero = true;
    else {
        bool hit = false;
        x = cache.evaluate(rx, controls, cosine, sine, hit);
        if (hit) ++result.hits; else ++result.misses;
        if (rx == ry) y = x;
        else {
            y = cache.evaluate(ry, controls, cosine, sine, hit);
            if (hit) ++result.hits; else ++result.misses;
        }
    }
    result.coefficients = {nativeMultiply(sx, y.cosine), nativeMultiply(sx, y.sine),
        nativeMultiply(flipSign(x.sine), sy), nativeMultiply(sy, x.cosine)};
    return result;
}
} // namespace chlorium::matrix
