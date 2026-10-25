#pragma once
// CPU-only research kernel. No Geode hooks and no suppression of native work.
#include <algorithm>
#include <cstdint>
#include <cfenv>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
#if defined(_M_X64) || defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace chlorium::research {
using Index = std::uint32_t;
struct AlphaLayout {
    Index groupSlots = 0;
    std::vector<std::vector<Index>> combinations;
    std::vector<Index> stateToCombination;
};
struct AlphaWork {
    std::uint64_t polled = 0, edges = 0, combinations = 0, products = 0, stateVisits = 0, writes = 0;
    std::uint64_t densePasses = 0;
};
enum class AlphaStrategy { ReverseCSR, Adaptive };

// Group -> alpha combination -> full state, two immutable CSR reverse indexes.
// Layout changes require a new graph/epoch. Inputs are snapshots AFTER native
// events, never events reordered or skipped by this kernel. Offscreen membership
// is included. Every dirty product is recomputed in its original sorted order;
// no quotient update (which breaks zero, NaN and floating point rounding).
class IncrementalAlpha {
public:
    explicit IncrementalAlpha(AlphaLayout layout, std::uint64_t epoch, AlphaStrategy strategy = AlphaStrategy::Adaptive)
        : layout_(std::move(layout)), epoch_(epoch), strategy_(strategy) {
        if (!epoch_ || layout_.groupSlots > 32768 || layout_.combinations.size() > 65536 ||
            layout_.stateToCombination.size() > 131072)
            throw std::invalid_argument("alpha fixture budget/epoch");
        groupOffsets_.resize(static_cast<std::size_t>(layout_.groupSlots) + 1);
        stateOffsets_.resize(layout_.combinations.size() + 1);
        for (const auto& ids : layout_.combinations) {
            if (ids.size() > 10) throw std::invalid_argument("group count");
            Index previous = 0;
            for (auto id : ids) {
                if (!id || id >= layout_.groupSlots || id <= previous)
                    throw std::invalid_argument("groups must be valid, sorted and unique");
                previous = id;
                ++groupOffsets_[static_cast<std::size_t>(id) + 1];
            }
        }
        for (auto combination : layout_.stateToCombination) {
            if (combination >= layout_.combinations.size()) throw std::invalid_argument("state mapping");
            ++stateOffsets_[static_cast<std::size_t>(combination) + 1];
        }
        for (std::size_t i = 1; i < groupOffsets_.size(); ++i) groupOffsets_[i] += groupOffsets_[i - 1];
        for (std::size_t i = 1; i < stateOffsets_.size(); ++i) stateOffsets_[i] += stateOffsets_[i - 1];
        groupEdges_.resize(groupOffsets_.back());
        stateEdges_.resize(stateOffsets_.back());
        auto groupCursor = groupOffsets_;
        auto stateCursor = stateOffsets_;
        for (Index c = 0; c < layout_.combinations.size(); ++c)
            for (auto id : layout_.combinations[c]) groupEdges_[groupCursor[id]++] = c;
        for (Index state = 0; state < layout_.stateToCombination.size(); ++state)
            stateEdges_[stateCursor[layout_.stateToCombination[state]]++] = state;
        for (Index id = 1; id < layout_.groupSlots; ++id)
            if (groupOffsets_[id] != groupOffsets_[static_cast<std::size_t>(id) + 1]) used_.push_back(id);
        alphas_.resize(layout_.groupSlots);
        products_.resize(layout_.combinations.size());
        outputs_.assign(layout_.stateToCombination.size(), 1.0f);
        visited_.resize(layout_.combinations.size());
        dirty_.reserve(layout_.combinations.size());
        changed_.reserve(outputs_.size());
        changedGroups_.reserve(used_.size());
    }

    bool refresh(std::uint64_t epoch, std::span<const float> native,
                 std::span<const std::uint8_t> disabled, AlphaWork& work) {
        // Reject stale/short snapshots atomically before touching any state.
        if (epoch != epoch_ || native.size() != layout_.groupSlots || disabled.size() != native.size()) return false;
        work = {};
        dirty_.clear(); changed_.clear(); changedGroups_.clear();
        denseRange_ = false; firstChanged_ = std::numeric_limits<Index>::max(); lastChanged_ = 0;
        bool nativeDirty = force_;
        std::size_t edgeCost = 0;
        if (++revision_ == 0) { std::fill(visited_.begin(), visited_.end(), 0); revision_ = 1; }
        for (auto id : used_) {
            ++work.polled;
            const float value = disabled[id] ? 0.0f : native[id];
            // Match the existing GroupManager observer, including NaN != NaN
            // and its unchanged +0/-0 policy. This is not bitwise input dirtying.
            if (alphas_[id] != value) {
                nativeDirty = true;
                alphas_[id] = value;
                changedGroups_.push_back(id);
                edgeCost += groupOffsets_[id + 1] - groupOffsets_[id];
            }
        }
        // Full native recomposition observes the current rounding/denormal
        // controls even in combinations unrelated to the changed group.
        // Delay invalidation until native would actually recompose: a control
        // change alone must not manufacture a visible update absent natively.
        const auto controls = floatingControls();
        // Skipping old products can also suppress sticky FP exception flags.
        // If a consumer cleared a flag observed at the last native-equivalent
        // barrier, recompose fully to regenerate the exact current flags; do
        // not raise cached flags speculatively. This is conservative (old flags
        // may include unrelated work) and can erase the speedup if cleared often.
        const auto currentFlags = floatingStatus();
        const bool flagsCleared = (lastObservedFlags_ & ~currentFlags) != 0;
        const bool trapsUnmasked = floatingTrapsUnmasked();
        if (nativeDirty && (controls != lastControls_ || flagsCleared || trapsUnmasked)) {
            dirty_.clear();
            force_ = true;
        }
        if (nativeDirty) lastControls_ = controls;
        // Dense CSR scatters through every output and pays deduplication too.
        // Preflight the changed groups' adjacency lengths BEFORE walking edges;
        // at high fanout choose contiguous full composition/state propagation.
        // The threshold is an experimental policy, not a universal tuning claim.
        if ((nativeDirty && trapsUnmasked) || (strategy_ == AlphaStrategy::Adaptive &&
            (force_ || edgeCost > layout_.combinations.size() / 2))) {
            ++work.densePasses;
            denseRange_ = true;
            for (Index c = 0; c < layout_.combinations.size(); ++c) products_[c] = evaluate(c, work);
            for (Index state = 0; state < outputs_.size(); ++state) {
                ++work.stateVisits;
                writeState(state, products_[layout_.stateToCombination[state]], work, false);
            }
            force_ = false;
            lastObservedFlags_ = floatingStatus();
            return true;
        }
        if (force_) {
            for (Index c = 0; c < layout_.combinations.size(); ++c) dirty_.push_back(c);
            force_ = false;
        } else for (auto id : changedGroups_) for (auto edge = groupOffsets_[id]; edge < groupOffsets_[id + 1]; ++edge) {
            ++work.edges;
            const auto combination = groupEdges_[edge];
#ifdef CHLORIUM_PRIVATE_DROP_ALPHA_DEPENDENCY
            // N4 private fixture mutation only. Never compiled into a mod.
            if (combination == 0) continue;
#endif
            if (visited_[combination] != revision_) {
                visited_[combination] = revision_;
                dirty_.push_back(combination);
            }
        }
        for (auto c : dirty_) {
            const float opacity = evaluate(c, work);
            for (auto edge = stateOffsets_[c]; edge < stateOffsets_[c + 1]; ++edge) {
                ++work.stateVisits;
                const auto state = stateEdges_[edge];
                writeState(state, opacity, work);
            }
        }
        if (nativeDirty) lastObservedFlags_ = floatingStatus();
        return true;
    }

    bool restore(std::uint64_t epoch, std::span<const float> states) {
        if (epoch != epoch_ || states.size() != outputs_.size()) return false;
        std::copy(states.begin(), states.end(), outputs_.begin());
        force_ = true;
        return true;
    }
    bool invalidate(std::uint64_t epoch) {
        if (epoch != epoch_) return false;
        force_ = true;
        return true;
    }
    std::span<const float> outputs() const { return outputs_; }
    // Ordering of private writes is NOT a native event/submission order.
    // A future transport must canonicalize this set before any GL upload.
    std::span<const Index> changedStates() const { return changed_; }
    bool hasDenseRange() const { return denseRange_; }
    Index firstChanged() const { return firstChanged_; }
    Index lastChanged() const { return lastChanged_; }
    std::size_t dynamicBytes() const {
        std::size_t bytes = (groupOffsets_.capacity() + groupEdges_.capacity() + stateOffsets_.capacity() +
            stateEdges_.capacity() + used_.capacity() + visited_.capacity() + dirty_.capacity() + changed_.capacity() +
            layout_.stateToCombination.capacity() + changedGroups_.capacity()) * sizeof(Index) +
            (alphas_.capacity() + outputs_.capacity() + products_.capacity()) * sizeof(float) +
            layout_.combinations.capacity() * sizeof(std::vector<Index>);
        for (const auto& combination : layout_.combinations) bytes += combination.capacity() * sizeof(Index);
        return bytes;
    }
private:
    float evaluate(Index combination, AlphaWork& work) const {
        ++work.combinations;
        float opacity = 1.0f;
        for (auto id : layout_.combinations[combination]) { opacity *= alphas_[id]; ++work.products; }
        return opacity;
    }
    void writeState(Index state, float opacity, AlphaWork& work, bool collectIndex = true) {
        if (outputs_[state] != opacity) {
            outputs_[state] = opacity;
            if (collectIndex) changed_.push_back(state);
            else { firstChanged_ = std::min(firstChanged_, state); lastChanged_ = std::max(lastChanged_, state); }
            ++work.writes;
        }
    }
    static std::uint64_t floatingControls() {
        std::uint64_t controls = static_cast<std::uint32_t>(std::fegetround());
        controls <<= 32;
#if defined(_M_X64) || defined(__SSE2__)
        controls |= _mm_getcsr() & 0xffc0u; // Controls, not sticky status flags.
#endif
        return controls;
    }
    static bool floatingTrapsUnmasked() {
#if defined(_M_X64) || defined(__SSE2__)
        return (_mm_getcsr() & 0x1f80u) != 0x1f80u;
#else
        return true; // No proof of exception-mask state on this research target.
#endif
    }
    static std::uint64_t floatingStatus() {
        std::uint64_t flags = static_cast<std::uint32_t>(std::fetestexcept(FE_ALL_EXCEPT));
        flags <<= 32;
#if defined(_M_X64) || defined(__SSE2__)
        flags |= _mm_getcsr() & 0x3fu; // Includes raw SSE denormal status too.
#endif
        return flags;
    }
    AlphaLayout layout_;
    std::uint64_t epoch_;
    AlphaStrategy strategy_;
    Index revision_ = 0;
    std::uint64_t lastControls_ = ~std::uint64_t{};
    std::uint64_t lastObservedFlags_ = 0;
    bool denseRange_ = false;
    Index firstChanged_ = std::numeric_limits<Index>::max(), lastChanged_ = 0;
    bool force_ = true;
    std::vector<Index> groupOffsets_, groupEdges_, stateOffsets_, stateEdges_, used_, visited_, dirty_, changed_, changedGroups_;
    std::vector<float> alphas_, outputs_, products_;
};
}
