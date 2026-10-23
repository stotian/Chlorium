#pragma once
#include <bit>
#include <cstdint>

namespace chlorium::matrix {
// Accumulate evidence across small Area groups as well as large loops. A bad
// window bypasses the rest of the current frame and 1/2/4/.../32 further frames.
// Physics and object updates still run; only optional math reuse is suspended.
class StreamReusePolicy {
    unsigned calls = 0, hits = 0, delay = 1;
    std::uint32_t retryFrame = 0;
    bool suspended = false;
public:
    void reset() noexcept { calls = hits = 0; delay = 1; retryFrame = 0; suspended = false; }
    bool blockedAt(std::uint32_t frame) const noexcept {
        return suspended && std::bit_cast<std::int32_t>(frame - retryFrame) < 0;
    }
    bool begin(std::uint32_t frame) noexcept {
        if (blockedAt(frame)) return false;
        suspended = false;
        return true;
    }
    bool disabled() const noexcept { return suspended; }
    // Returns true only when this sample starts a new cooldown.
    bool record(bool reused, std::uint32_t frame) noexcept {
        if (suspended) return false;
        hits += unsigned(reused);
        if (++calls < 256) return false;
        const bool poor = hits < 64;
        calls = hits = 0;
        if (!poor) { delay = 1; return false; }
        suspended = true;
        retryFrame = frame + delay + 1;
        if (delay < 32) delay *= 2;
        return true;
    }
};
} // namespace chlorium::matrix
