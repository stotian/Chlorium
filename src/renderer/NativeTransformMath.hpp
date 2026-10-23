#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace chlorium::native {

// Exact math cache; this header itself installs no native hook.
// GD 2.2081 processTransformActions calls atan2f twice per transformed object
// (RVA 0x22d174 / 0x22d19c). Their inputs are the affine linear coefficients,
// which may repeat across many objects even when their positions differ.
// Cache the exact original result; do not substitute a faster approximation.
//
// The caller must scope this cache to a single native transform phase and reset
// it at phase entry. A control tag must include the FP rounding/FTZ/DAZ modes.
// Use only with masked FP exceptions and no caller-observable errno/fenv flags:
// repeated calls can otherwise have observable side effects beyond the result.
// All non-finite inputs/results bypass storage. Signed zeros remain distinct.
template <std::size_t Capacity = 64, class Generation = std::uint32_t>
class TransformAtanCache {
    static_assert(Capacity && !(Capacity & (Capacity - 1)));
    static_assert(std::is_integral_v<Generation> && std::is_unsigned_v<Generation> &&
                  !std::is_same_v<Generation, bool>);
    struct Entry {
        std::uint64_t key = 0;
        std::uint32_t controls = 0;
        float value = 0;
        Generation generation = 0;
    };
    std::array<Entry, Capacity> m_entries{};
    Generation m_generation = 1;

    static bool finiteBits(std::uint32_t bits) noexcept {
        return (bits & 0x7f800000u) != 0x7f800000u;
    }
    static bool subnormalBits(std::uint32_t bits) noexcept {
        return (bits & 0x7f800000u) == 0 && (bits & 0x007fffffu) != 0;
    }

public:
    void reset() noexcept {
        // Most native transform phases are small or empty. Advancing an epoch
        // avoids touching every cache line 240 times/second regardless of use.
        // Zero is never live, and wrap must erase old epoch-one entries before
        // epoch one can become live again.
        if (++m_generation == 0) {
            for (auto& entry : m_entries) entry.generation = 0;
            m_generation = 1;
        }
    }

    // original(y, x) is evaluated exactly once on a miss and never on a hit.
    // Optional hit output permits diagnostic hit-rate measurement without
    // changing the original function's result.
    template <class Original>
    float evaluate(float y, float x, std::uint32_t controls,
                   Original&& original, bool* hit = nullptr) {
        if (hit) *hit = false;
        const auto yBits = std::bit_cast<std::uint32_t>(y);
        const auto xBits = std::bit_cast<std::uint32_t>(x);
        if (!finiteBits(yBits) || !finiteBits(xBits) ||
            subnormalBits(yBits) || subnormalBits(xBits) ||
            ((yBits & 0x7fffffffu) == 0 && (xBits & 0x7fffffffu) == 0))
            return std::forward<Original>(original)(y, x);
        const auto key = (std::uint64_t(yBits) << 32) | xBits;
        auto hash = key ^ (key >> 29);
        hash *= 0x9e3779b97f4a7c15ull;
        auto& entry = m_entries[(hash ^ (hash >> 32)) & (Capacity - 1)];
        if (entry.generation == m_generation && entry.key == key && entry.controls == controls) {
            if (hit) *hit = true;
            return entry.value;
        }
        const auto result = std::forward<Original>(original)(y, x);
        const auto resultBits = std::bit_cast<std::uint32_t>(result);
        // Underflow may set errno. Keep those cases on the original CRT path.
        if (finiteBits(resultBits) && !subnormalBits(resultBits) &&
            ((resultBits & 0x7fffffffu) != 0 || (yBits & 0x7fffffffu) == 0))
            entry = {key, controls, result, m_generation};
        return result;
    }
};

// Reconsider reuse throughout a phase: a repetitive prefix does not justify
// paying for cache misses in a much larger, mostly unique suffix. Disabling
// affects only optional caching; the caller still invokes the original math.
class TransformReusePolicy {
    unsigned m_calls = 0, m_hits = 0;
    bool m_disabled = false;
public:
    void reset() noexcept { m_calls = m_hits = 0; m_disabled = false; }
    bool disabled() const noexcept { return m_disabled; }
    bool record(bool hit) noexcept {
        if (m_disabled) return false;
        m_hits += unsigned(hit);
        if (++m_calls == 256) {
            m_disabled = m_hits < 64;
            m_calls = m_hits = 0;
        }
        return !m_disabled;
    }
};

} // namespace chlorium::native
