#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <utility>
#include <vector>
#include <xmmintrin.h>

namespace chlorium::research::opacity {
// Closed snapshot experiment only: no engine hooks, object pointers or publication.
class GroupOpacityProgram {
    struct Node { std::size_t parent, input; };
    std::vector<Node> nodes_{{0, 0}};
    std::vector<std::size_t> tails_;
    std::vector<int> groups_;
    std::vector<std::uint32_t> inputs_, products_;
    std::size_t multiplications_ = 0;
public:
    explicit GroupOpacityProgram(std::span<const std::vector<int>> membership) {
        std::array<int, 10000> slots;
        slots.fill(-1);
        std::map<std::pair<std::size_t, int>, std::size_t> prefixes;
        for (const auto& object : membership) {
            std::size_t parent = 0;
            for (const auto rawGroup : object) {
                const int group = std::clamp(rawGroup, 0, 9999);
                const auto [at, inserted] = prefixes.try_emplace({parent, group}, nodes_.size());
                if (inserted) {
                    auto& slot = slots[group];
                    if (slot < 0) {
                        slot = static_cast<int>(groups_.size());
                        groups_.push_back(group);
                    }
                    nodes_.push_back({parent, static_cast<std::size_t>(slot)});
                }
                parent = at->second;
            }
            tails_.push_back(parent);
        }
        inputs_.resize(groups_.size());
        products_.resize(nodes_.size());
    }

    std::span<const int> groups() const { return groups_; }
    std::size_t objectCount() const { return tails_.size(); }
    std::size_t prefixCount() const { return nodes_.size() - 1; }
    std::size_t multiplications() const { return multiplications_; }

    // Values follow groups(), from one immutable producer snapshot. Rejection
    // leaves caller outputs and MXCSR untouched; caller must retain native work.
    bool evaluate(std::span<const std::uint32_t> values, std::span<std::uint32_t> output) {
        multiplications_ = 0;
        if (values.size() != inputs_.size() || output.size() != tails_.size() ||
            (_mm_getcsr() & 0x1f80u) != 0x1f80u) return false;
        // Own inputs before writing: caller input/output spans may overlap.
        std::copy(values.begin(), values.end(), inputs_.begin());
        for (const auto bits : inputs_)
            if (bits > 0x3f800000u && bits != 0x80000000u) return false;
        products_[0] = 0x3f800000u;
        for (std::size_t i = 1; i < nodes_.size(); ++i) {
            const auto& node = nodes_[i];
            const auto parent = products_[node.parent];
            if (!parent) { products_[i] = 0; continue; }
            const auto product = _mm_mul_ss(_mm_set_ss(std::bit_cast<float>(parent)),
                                           _mm_set_ss(std::bit_cast<float>(inputs_[node.input])));
            // COMISS preserves denormal flags and DAZ's comparison against zero.
            products_[i] = _mm_comige_ss(_mm_setzero_ps(), product) ? 0u :
                std::bit_cast<std::uint32_t>(_mm_cvtss_f32(product));
            ++multiplications_;
        }
        for (std::size_t i = 0; i < tails_.size(); ++i) output[i] = products_[tails_[i]];
        return true;
    }
};
}
