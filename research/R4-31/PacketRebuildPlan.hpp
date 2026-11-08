#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace chlorium::research {
// Render-thread only. Packet indices MUST follow the complete native pass order.
// This plans persistent geometry; it neither skips gameplay nor submits draws.
class PacketRebuildPlan {
public:
    struct Ticket {
        std::uint64_t owner;
        std::uint32_t packet;
        std::uint64_t revision;
    };
private:
    static constexpr std::size_t maxPackets = 262144, maxEdges = 2097152;
    inline static std::atomic<std::uint64_t> nextOwner_{0};
    const std::uint64_t owner_ = acquireOwner();
    std::vector<std::vector<std::uint32_t>> dependents_;
    std::vector<std::uint64_t> wanted_, published_;
    std::vector<std::uint32_t> dirty_;
    std::vector<bool> queued_;
    std::uint64_t revision_ = 0;
    bool valid_ = false;

    static std::uint64_t acquireOwner() {
        auto value = nextOwner_.load(std::memory_order_relaxed);
        while (value != std::numeric_limits<std::uint64_t>::max())
            if (nextOwner_.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value + 1;
        return 0;
    }

    bool advance() {
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) {
            valid_ = false;
            return false;
        }
        ++revision_;
        return true;
    }
    void queue(std::uint32_t packet) {
        if (!queued_[packet]) {
            dirty_.push_back(packet);
            queued_[packet] = true;
        }
    }
public:
    PacketRebuildPlan() = default;
    PacketRebuildPlan(const PacketRebuildPlan&) = delete;
    PacketRebuildPlan& operator=(const PacketRebuildPlan&) = delete;
    PacketRebuildPlan(PacketRebuildPlan&&) = delete;
    PacketRebuildPlan& operator=(PacketRebuildPlan&&) = delete;

    // Compile once per pass topology. Failure disables custom submission,
    // including the old topology, rather than leaving a usable stale plan.
    bool compile(std::span<const std::vector<std::uint32_t>> inputs, std::uint32_t inputCount) {
        valid_ = false;
        if (!owner_ || inputs.size() > maxPackets || inputCount > 65536 || !advance()) return false;
        std::size_t edges = 0;
        for (const auto& packet : inputs) {
            if (packet.size() > maxEdges - edges) return false;
            edges += packet.size();
            for (auto input : packet) if (input >= inputCount) return false;
        }
        std::vector<std::vector<std::uint32_t>> dependents(inputCount);
        for (std::uint32_t packet = 0; packet < inputs.size(); ++packet)
            for (auto input : inputs[packet]) dependents[input].push_back(packet);
        for (auto& list : dependents) list.erase(std::unique(list.begin(), list.end()), list.end());
        std::vector<std::uint64_t> wanted(inputs.size(), revision_), published(inputs.size(), 0);
        std::vector<bool> queued(inputs.size(), true);
        std::vector<std::uint32_t> dirty;
        dirty.reserve(inputs.size());
        for (std::uint32_t packet = 0; packet < inputs.size(); ++packet) dirty.push_back(packet);
        dependents_.swap(dependents);
        wanted_.swap(wanted); published_.swap(published);
        queued_.swap(queued); dirty_.swap(dirty);
        valid_ = true;
        return true;
    }

    bool invalidateInput(std::uint32_t input) {
        if (!valid_ || input >= dependents_.size()) { valid_ = false; return false; }
        if (!advance()) return false;
        for (auto packet : dependents_[input]) { wanted_[packet] = revision_; queue(packet); }
        return true;
    }
    bool invalidateAll() {
        if (!valid_ || !advance()) return false;
        for (std::uint32_t packet = 0; packet < wanted_.size(); ++packet) {
            wanted_[packet] = revision_; queue(packet);
        }
        return true;
    }
    std::size_t packetCount() const { return wanted_.size(); }
    // The caller reserves packetCount() once, outside the rendering loop.
    // Refusal leaves both the output and pending queue unchanged.
    bool takePending(std::vector<Ticket>& jobs) {
        if (!valid_ || jobs.capacity() < dirty_.size()) return false;
        jobs.clear();
        for (auto packet : dirty_) {
            if (published_[packet] != wanted_[packet]) jobs.push_back({owner_, packet, wanted_[packet]});
            queued_[packet] = false;
        }
        dirty_.clear();
        return true;
    }
    // Upload to separate staging storage, NEVER overwrite a live allocation
    // before this validation. Stale completion must only discard staging.
    bool publish(const Ticket& job, bool uploadSucceeded) {
        if (!valid_ || job.owner != owner_ || job.packet >= wanted_.size()) return false;
        if (job.revision != wanted_[job.packet]) return false;
        if (!uploadSucceeded) { published_[job.packet] = 0; queue(job.packet); return false; }
        published_[job.packet] = job.revision;
        return true;
    }
    // Decide ONCE before the first custom draw: any missing/stale packet means
    // the entire native pass. Never draw a prefix then replay the native pass.
    std::uint64_t revision() const { return revision_; }
    bool canSubmit(std::span<const std::uint32_t> visible, std::uint64_t visibilityRevision, bool completeCoverageProven) const {
        if (!valid_ || visibilityRevision != revision_ || !completeCoverageProven) return false;
        std::optional<std::uint32_t> previous;
        for (auto packet : visible) {
            if (packet >= wanted_.size() || (previous && packet <= *previous) ||
                published_[packet] != wanted_[packet]) return false;
            previous = packet;
        }
        return true;
    }
};
}
