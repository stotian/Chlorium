#pragma once
#include <cstdint>

namespace chlorium::snapshot {
struct Stats {
    std::uint32_t version = 1, active = 0;
    std::uint64_t frames = 0, sprites = 0, uploadedBytes = 0, fallbacks = 0;
    double captureMilliseconds = 0;
};
static_assert(sizeof(Stats) == 48);
struct UploadStatsV6 {
    std::uint32_t version=6,reserved=0;
    std::uint64_t direct=0,seed=0,sparse=0,compared=0,copied=0,uploaded=0;
};
static_assert(sizeof(UploadStatsV6)==56);
}
