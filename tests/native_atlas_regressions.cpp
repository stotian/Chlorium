#include "renderer/NativeAtlasUploadRanges.hpp"
#include <iostream>
#include <random>
#include <vector>

int main() {
    using namespace chlorium::native;
    std::mt19937 random(1300);
    std::vector<std::byte> current(65539), gpu = current;
    unsigned checks = 0;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        for (int i = 0; i < iteration % 97; ++i)
            current[random() % current.size()] = std::byte(random() & 255);
        const auto plan = planAtlasUploads(current, gpu, 256);
        if (plan.useNativeUpload) gpu = current;
        else for (size_t i = 0; i < plan.rangeCount; ++i) {
            const auto range = plan.ranges[i];
            if (range.offset > gpu.size() || range.size > gpu.size() - range.offset) return 2;
            std::memcpy(gpu.data() + range.offset, current.data() + range.offset, range.size);
        }
        if (gpu != current) return 3;
        ++checks;
    }
    if (!planAtlasUploads(current, gpu, 0).useNativeUpload) return 4;
    if (!planAtlasUploads(current, std::span<const std::byte>{}, 256).useNativeUpload) return 5;
    if (planAtlasUploads(gpu, gpu, 256).rangeCount != 0) return 6;
    std::array<unsigned char, 12> known{};
    if (atlasRangeKnown(known, 0, 12)) return 7;
    std::fill(known.begin() + 2, known.begin() + 8, 1);
    if (!atlasRangeKnown(known, 2, 6)) return 8;
    if (atlasRangeKnown(known, 1, 7)) return 9;
    if (atlasRangeKnown(known, 2, 7)) return 10;
    if (atlasRangeKnown(known, 12, 0)) return 11;
    if (atlasRangeKnown(known, static_cast<size_t>(-1), 2)) return 12;
    AtlasUploadPolicy policy;
    if (policy.skipDraw()) return 13;
    for (int i = 0; i < 3; ++i) if (policy.recordDense()) return 14;
    if (!policy.recordDense() || policy.cooldown != 120) return 15;
    for (int i = 0; i < 120; ++i) if (!policy.skipDraw()) return 16;
    if (policy.skipDraw()) return 17;
    policy.recordDense(); policy.recordSparse();
    if (policy.denseRuns != 0 || policy.cooldown != 0 ||
        policy.nextCooldown != AtlasUploadPolicy::initialCooldown) return 18;
    // Dense cycles must retain their history across native captures. Bound
    // the increasing pauses, then recover promptly after a useful sparse draw.
    constexpr std::array<unsigned, 7> pauses{120, 240, 480, 960, 1920, 1920, 1920};
    for (const auto pause : pauses) {
        for (int i = 0; i < 3; ++i) if (policy.recordDense()) return 25;
        if (!policy.recordDense() || policy.cooldown != pause ||
            policy.denseRuns != 0 || policy.nextCooldown > AtlasUploadPolicy::maximumCooldown) return 26;
        // Repeated observations during a pause neither extend nor shorten it.
        if (policy.recordDense() || policy.cooldown != pause || policy.denseRuns != 0) return 27;
        for (unsigned i = 0; i < pause; ++i) if (!policy.skipDraw()) return 28;
        if (policy.skipDraw() || policy.cooldown != 0) return 29;
    }
    policy.recordDense(); policy.recordDense();
    policy.recordSparse();
    if (policy.denseRuns || policy.cooldown ||
        policy.nextCooldown != AtlasUploadPolicy::initialCooldown) return 30;
    for (int i = 0; i < 3; ++i) if (policy.recordDense()) return 31;
    if (!policy.recordDense() || policy.cooldown != AtlasUploadPolicy::initialCooldown) return 32;
    // Driver-call budget: two separated dirty ranges must retain the native
    // single upload; one contiguous range can still upload fewer bytes.
    std::array<std::byte, 4096> original{}, changed{};
    changed[3] = std::byte{1};
    auto bounded = planAtlasUploads(changed, original, 256, 1);
    if (bounded.useNativeUpload || bounded.rangeCount != 1 || bounded.changedBytes != 256) return 19;
    changed[260] = std::byte{2};
    bounded = planAtlasUploads(changed, original, 256, 1);
    if (bounded.useNativeUpload || bounded.rangeCount != 1 || bounded.changedBytes != 512) return 20;
    changed[2048] = std::byte{3};
    if (!planAtlasUploads(changed, original, 256, 1).useNativeUpload) return 21;
    if (!planAtlasUploads(changed, original, 256, 0).useNativeUpload) return 22;
    current.assign(current.size(), std::byte{}); gpu = current;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        for (int i = 0; i < iteration % 97; ++i)
            current[random() % current.size()] = std::byte(random() & 255);
        const auto plan = planAtlasUploads(current, gpu, 256, 1);
        if (plan.useNativeUpload) gpu = current;
        else {
            if (plan.rangeCount > 1) return 23;
            for (size_t i = 0; i < plan.rangeCount; ++i) {
                const auto r = plan.ranges[i];
                std::memcpy(gpu.data() + r.offset, current.data() + r.offset, r.size);
            }
        }
        if (gpu != current) return 24;
        ++checks;
    }
    std::cout << checks + 30 << " native atlas checks passed\n";
}
