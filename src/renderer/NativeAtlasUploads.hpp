#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace chlorium::native {

struct AtlasUploadStats {
    // Dirty draws admitted to the bounded cache path, not all native draws.
    std::uint64_t draws = 0;
    std::uint64_t nativeUploads = 0;
    std::uint64_t partialUploads = 0;
    std::uint64_t unchangedUploads = 0;
    std::uint64_t bytesUploaded = 0;
    std::uint64_t bytesAvoided = 0;
    std::size_t cachedBytes = 0;
    std::uint64_t partialRangeDraws = 0;
    std::uint64_t verifiedUploads = 0;
    std::uint64_t verificationFailures = 0;
    std::uint64_t cacheRejectedMemory = 0, cacheRejectedBuffer = 0;
    std::uint64_t eligibleUploads = 0;
    std::uint64_t postNativeDraws = 0, cacheCaptures = 0, cacheAllocationFailures = 0;
    std::array<std::uint64_t, 8> invalidations{};
    std::array<std::uint64_t, 5> identityChanges{};
    std::uint64_t cooldownDraws = 0;
    std::uint64_t oversizedFallbacks = 0;
    std::uint64_t budgetFallbacks = 0;
};

AtlasUploadStats getAtlasUploadStats();
// Releases CPU shadows only. It never deletes or rewrites native GPU buffers.
void purgeAtlasUploadCache();

} // namespace chlorium::native
