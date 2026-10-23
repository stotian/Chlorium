#pragma once

#include "ConvexList.hpp"
#include <optional>
#include <span>

struct IndexedSpriteMesh {
    std::vector<glm::vec2> vertices;
    std::vector<u32> indices;
};

// Sprite outlines are normalized, convex, counter-clockwise polygons.
// Redundant collinear boundary vertices are allowed; zero-length edges are not.
std::optional<ConvexPolygon> makeConvexSpritePolygon(std::span<const glm::vec2> points);

// Preserve the old triangulation's triangle order and exact coordinates.
std::optional<IndexedSpriteMesh> makeIndexedSpriteMesh(const ConvexList& list);
