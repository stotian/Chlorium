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
struct TransferStatsV9 {
    std::uint32_t version=9,reserved=0;
    std::uint64_t uploads=0,bytes=0,nativeFallbacks=0,cleanDraws=0,busyFallbacks=0,fences=0;
};
static_assert(sizeof(TransferStatsV9)==56);
// Legacy replacements require zero fallbacks. The bounded V9 transfer path
// instead accounts for exact native fallback draws as part of its measured
// work; unsupported/failed caches must not be mistaken for this typed policy.
inline bool validSubmissionStats(const Stats& stats,bool available,
    const TransferStatsV9* transfer,bool allowHybrid) {
    return available&&stats.version==1&&stats.active&&stats.frames>0&&stats.sprites>0&&
        (stats.fallbacks==0||(allowHybrid&&transfer&&transfer->version==9&&
            transfer->uploads>0&&transfer->nativeFallbacks==stats.fallbacks&&
            transfer->busyFallbacks<=transfer->nativeFallbacks));
}
}
