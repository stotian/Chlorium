#pragma once
#include <cstddef>
#include <cstdint>

namespace chlorium::native {
struct TransformMathStats {
    std::uint64_t phases = 0, calls = 0, hits = 0, misses = 0;
    std::uint64_t verified = 0, failures = 0;
    std::uint64_t bypassed = 0, lowReusePhases = 0;
    bool installed = false;
};
using ReadTransformMathStats = bool (*)(TransformMathStats*, std::size_t);
// Targeted Area revision 3 makes per-call counters optional. Callers must read
// this state before treating zero calls as evidence that no math was reused.
struct MathPatchStateV1 {
    std::uint32_t version = 1, revision = 3, requestedSites = 4, activeSites = 0;
    std::uint32_t cacheEnabled = 0, perCallCounters = 0, verificationStride = 0, ownershipLost = 0;
};
static_assert(sizeof(MathPatchStateV1) == 32);
using ReadMathPatchStateV1 = bool (*)(MathPatchStateV1*, std::size_t);
}
