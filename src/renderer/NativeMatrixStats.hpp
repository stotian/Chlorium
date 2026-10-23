#pragma once
#include <cstddef>
#include <cstdint>

namespace chlorium::matrix {
// Keep the two older exports layout-compatible with matrix.1 / matrix.2.
struct Stats { std::uint64_t phases = 0, eligible = 0, hits = 0, misses = 0, unsupported = 0; };
struct Details {
    std::uint64_t transformPhases = 0, areaPhases = 0, rotationHits = 0, rotationMisses = 0;
    std::uint64_t zeroRotations = 0, nativeCalls = 0, lowReusePhases = 0, installedSites = 0;
};
struct PersistenceStats {
    std::uint64_t recentMatrixHits = 0, recentRotationHits = 0, invalidations = 0;
    std::uint64_t skippedTransformPhases = 0, skippedAreaPhases = 0;
};
struct Snapshot {
    std::uint32_t version = 1, enabled = 0;
    Stats matrices;
    Details rotations;
    PersistenceStats persistence;
};
static_assert(sizeof(Stats) == 40 && sizeof(Details) == 64 && sizeof(Snapshot) == 152);
using ReadSnapshot = bool (*)(Snapshot*, std::size_t);
} // namespace chlorium::matrix
