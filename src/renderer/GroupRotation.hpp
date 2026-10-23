#pragma once

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <cstdint>
#include <optional>
#include <span>

// Cocos uses clockwise angles. Apply the complete delta to every combination
// containing the target group, including rotations without a centre object.
template <class State>
void applyGroupRotation(std::span<State> states, std::span<const std::uint32_t> indices,
                        float angle, bool lockRotation, std::optional<glm::vec2> centre) {
    const float radians = glm::radians(angle);
    const float cosine = glm::cos(radians);
    const float sine = glm::sin(radians);
    const glm::mat2 rotation{{cosine, -sine}, {sine, cosine}};
    for (const auto index : indices) {
        if (index >= states.size()) continue;
        auto& state = states[index];
        if (!lockRotation) state.localTransform = rotation * state.localTransform;
        if (centre) {
            state.positionalTransform = rotation * state.positionalTransform;
            state.offset = rotation * (state.offset - *centre) + *centre;
        }
    }
}
