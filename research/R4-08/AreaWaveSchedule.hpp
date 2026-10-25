#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace chlorium::research::area_batch {
struct Recipe {
    std::int32_t length;
    std::uint32_t deadzoneBits;
    bool operator==(const Recipe&) const = default;
};
struct Operation {
    Recipe recipe{};
    std::span<const std::uint64_t> reads, writes;
    // Unknown callbacks, aliases, RNG, FP-status observers or unmodelled
    // dependencies MUST remain false. Native GD has not yet met this contract.
    bool snapshotPure = false;
    bool commitOnlyDeclaredWrites = false;
};
struct Wave { std::size_t first, count; bool batchable; };
class WaveSchedule {
public:
    static WaveSchedule compile(std::span<const Operation> operations, std::uint64_t generation) {
        if (!generation) throw std::invalid_argument("Area dependency generation missing");
        WaveSchedule plan; plan.generation_ = generation; plan.operations_ = operations.size();
        std::unordered_set<std::uint64_t> pendingWrites;
        constexpr std::size_t maximumWave = 256;
        for (std::size_t n = 0; n < operations.size(); ++n) {
            const auto& op = operations[n];
            const bool safe = op.snapshotPure && op.commitOnlyDeclaredWrites;
            bool append = safe && !plan.waves_.empty() && plan.waves_.back().batchable &&
                plan.waves_.back().count < maximumWave &&
                operations[plan.waves_.back().first].recipe == op.recipe;
            if (append) for (const auto key : op.reads) {
#ifndef CHLORIUM_PRIVATE_AREA_HAZARD_MUTANT
                if (pendingWrites.contains(key)) { append = false; break; }
#else
                (void)key; // Private mutation: drop a real causal dependency.
#endif
            }
            if (!append) {
                plan.waves_.push_back({n,1,safe});
                pendingWrites.clear();
            } else ++plan.waves_.back().count;
            pendingWrites.insert(op.writes.begin(), op.writes.end());
        }
        return plan;
    }
    std::span<const Wave> checked(std::uint64_t generation, std::size_t operationCount) const {
        // Rebuild after any change to groups, parenting, effect recipe/order,
        // dependencies, object identity, level reset or checkpoint restore.
        // Cardinality alone cannot establish identity: generation is mandatory.
        if (!generation || generation != generation_ || operationCount != operations_)
            throw std::invalid_argument("Area program invalidated before snapshot/commit");
        return waves_;
    }
private:
    std::uint64_t generation_ = 0;
    std::size_t operations_ = 0;
    std::vector<Wave> waves_;
};
}
