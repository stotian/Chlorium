#pragma once
#include <cstdint>

namespace chlorium::snapshot {
struct Stats {
    std::uint32_t version = 1, active = 0;
    std::uint64_t frames = 0, sprites = 0, uploadedBytes = 0, fallbacks = 0;
    double captureMilliseconds = 0;
};
static_assert(sizeof(Stats) == 48);
}
