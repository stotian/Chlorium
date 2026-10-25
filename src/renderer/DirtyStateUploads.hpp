#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace chlorium::snapshot {
// This plan is ONLY for storage exclusively owned by Chlorium. The GPU store
// must first be initialized from the complete CPU array, and no shader writes
// to it. It must never be used to infer the contents of a native game atlas.
class DirtyStateUploads {
public:
    struct Range { std::size_t first, count; };
    explicit DirtyStateUploads(std::size_t count) : records(count) {}
    void mark(std::size_t record) noexcept {
        if (record >= records) return;
        const auto start = record / blockRecords * blockRecords;
        const auto end = start + std::min(blockRecords, records - start);
        minimum = std::min(minimum, start);
        maximum = std::max(maximum, end);
        if (overflow) return;
        if (used && start < ranges[used - 1].first) { overflow = true; return; }
        if (used && start - ranges[used - 1].first <= ranges[used - 1].count + blockRecords) {
            auto& last = ranges[used - 1];
            last.count = std::max(last.first + last.count, end) - last.first;
        } else if (used < ranges.size()) ranges[used++] = {start, end - start};
        else overflow = true;
    }
    std::span<const Range> finish() noexcept {
        // Dense/scattered/out-of-order changes cost at most one broad upload,
        // instead of hundreds of small driver calls. Unchanged gaps contain
        // authoritative CPU data and are safe to upload again.
        if (overflow) { ranges[0] = {minimum, maximum - minimum}; used = 1; }
        return {ranges.data(), used};
    }
private:
    static constexpr std::size_t blockRecords = 64;
    std::size_t records, minimum = ~std::size_t{}, maximum = 0, used = 0;
    bool overflow = false;
    std::array<Range, 32> ranges{};
};
}
