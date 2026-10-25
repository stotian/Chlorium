#pragma once
#include <emmintrin.h>
#include <xmmintrin.h>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace chlorium::research::area_batch {
using NativeCurve = float (*)(float, int, float);
struct Work { std::size_t packedLanes = 0, nativeLanes = 0; };

// Pure snapshot kernel: no GameObject, callbacks, visibility or world writes.
// Four independent objects share one effect's length/deadzone. Keep native
// division and rounding; do not replace division with a reciprocal or FMA.
// This does NOT authorize gathering positions before native callbacks: the
// caller still needs a proven read/write dependency barrier for integration.
inline Work evaluate(std::span<const float> distance, int length, float deadzone,
                     std::span<float> output, NativeCurve native) {
    if (output.size() != distance.size() || !native)
        throw std::invalid_argument("Area batch cardinality or oracle");
    if (!distance.empty()) {
        const auto a = reinterpret_cast<std::uintptr_t>(distance.data());
        const auto b = reinterpret_cast<std::uintptr_t>(output.data());
        if ((a <= b ? b - a : a - b) < distance.size_bytes())
            throw std::invalid_argument("Area batch requires disjoint snapshots");
    }
    Work work;
    const auto zoneBits = std::bit_cast<std::uint32_t>(deadzone);
    const auto magnitude = zoneBits & 0x7fffffffu;
    // The restricted fast domain cannot create Inf/Inf via 1-deadzone.
    // Other effects remain exact native, including all non-finite inputs.
    const bool fast = distance.size() >= 4 && length != 0 &&
        (magnitude < 0x3f800000u || zoneBits == 0xbf800000u) &&
        (_mm_getcsr() & 0x1f80u) == 0x1f80u;
    std::size_t index = 0;
    if (fast) {
        const auto zero = _mm_setzero_ps(), one = _mm_set1_ps(1.f);
        const auto zone = _mm_set1_ps(deadzone);
        const auto extent = _mm_cvtepi32_ps(_mm_set1_epi32(length));
        const bool zeroZone = _mm_ucomieq_ss(zone, zero) != 0;
        const auto exponent = _mm_set1_epi32(0x7f800000);
        for (; index + 4 <= distance.size(); index += 4) {
            const auto input = _mm_loadu_ps(distance.data() + index);
            const auto bits = _mm_castps_si128(input);
            const auto exceptional = _mm_cmpeq_epi32(_mm_and_si128(bits, exponent), exponent);
            if (_mm_movemask_epi8(exceptional)) {
                for (unsigned lane = 0; lane < 4; ++lane)
                    output[index + lane] = native(distance[index + lane], length, deadzone);
                work.nativeLanes += 4;
                continue;
            }
            const auto quotient = _mm_div_ps(input, extent);
            auto value = quotient;
            if (!zeroZone) {
                const auto numerator = _mm_sub_ps(quotient, zone);
                const auto denominator = _mm_sub_ps(one, zone);
                value = _mm_div_ps(numerator, denominator);
            }
            // min/max would replace a native negative zero with +0 on ties.
            const auto negative = _mm_cmplt_ps(value, zero);
            const auto above = _mm_cmpgt_ps(value, one);
            value = _mm_andnot_ps(negative, value);
            value = _mm_or_ps(_mm_and_ps(above, one), _mm_andnot_ps(above, value));
#ifdef CHLORIUM_PRIVATE_AREA_BATCH_MUTANT
            value = _mm_setzero_ps(); // Private oracle calibration only.
#endif
            _mm_storeu_ps(output.data() + index, value);
            work.packedLanes += 4;
        }
    }
    for (; index < distance.size(); ++index) {
        output[index] = native(distance[index], length, deadzone);
        ++work.nativeLanes;
    }
    return work;
}
}
