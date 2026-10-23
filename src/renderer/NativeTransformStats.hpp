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
}
