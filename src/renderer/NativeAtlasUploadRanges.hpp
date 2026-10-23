#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace chlorium::native {

struct AtlasUploadPolicy {
    static constexpr unsigned initialCooldown = 120;
    static constexpr unsigned maximumCooldown = 1920;
    unsigned denseRuns = 0, cooldown = 0;
    unsigned nextCooldown = initialCooldown;
    bool skipDraw() {
        if (!cooldown) return false;
        --cooldown;
        return true;
    }
    bool recordDense() {
        // A native fallback is not evidence of useful cache reuse. Repeated
        // dense animation should progressively reduce probing, rather than
        // resume the same compare/copy cost every 120 eligible dirty draws.
        if (cooldown) return false;
        if (++denseRuns < 4) return false;
        denseRuns = 0;
        cooldown = nextCooldown;
        nextCooldown = std::min(maximumCooldown, nextCooldown * 2);
        return true;
    }
    void recordSparse() {
        // Call only after an exact sparse/unchanged upload has succeeded.
        // A newly captured native upload must not erase the backoff history.
        denseRuns = cooldown = 0;
        nextCooldown = initialCooldown;
    }
};

inline bool atlasRangeKnown(std::span<const unsigned char> known,
                            std::size_t start, std::size_t count) {
    if (!count || start > known.size() || count > known.size() - start) return false;
    return std::all_of(known.begin() + start, known.begin() + start + count,
                       [](unsigned char value) { return value != 0; });
}

struct AtlasUploadRange {
    std::size_t offset = 0;
    std::size_t size = 0;
};

struct AtlasUploadPlan {
    std::array<AtlasUploadRange, 32> ranges{};
    std::size_t rangeCount = 0;
    std::size_t changedBytes = 0;
    bool useNativeUpload = false;
};

// Compare every byte, including vertex padding. Hashes cannot establish that
// the GPU already contains precisely the native quad data.
inline AtlasUploadPlan planAtlasUploads(std::span<const std::byte> current,
                                       std::span<const std::byte> previous,
                                       std::size_t blockBytes,
                                       std::size_t maximumRanges = 32) {
    AtlasUploadPlan plan;
    if (current.size() != previous.size() || !blockBytes) {
        plan.useNativeUpload = true;
        return plan;
    }
    for (std::size_t offset = 0; offset < current.size();) {
        const auto size = std::min(blockBytes, current.size() - offset);
        if (std::memcmp(current.data() + offset, previous.data() + offset, size) != 0) {
            plan.changedBytes += size;
            if (plan.rangeCount && plan.ranges[plan.rangeCount - 1].offset +
                    plan.ranges[plan.rangeCount - 1].size == offset) {
                plan.ranges[plan.rangeCount - 1].size += size;
            } else if (plan.rangeCount < std::min(maximumRanges, plan.ranges.size())) {
                plan.ranges[plan.rangeCount++] = {offset, size};
            } else {
                plan.useNativeUpload = true;
                return plan;
            }
            // One ordinary native upload is preferable to many sub-updates
            // when almost the entire active prefix has changed.
            if (plan.changedBytes >= current.size() - current.size() / 4) {
                plan.useNativeUpload = true;
                return plan;
            }
        }
        offset += size;
    }
    return plan;
}

} // namespace chlorium::native
