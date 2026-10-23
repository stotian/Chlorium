#include "SpriteMesh.hpp"
#include <cmath>
#include <map>

std::optional<ConvexPolygon> makeConvexSpritePolygon(std::span<const glm::vec2> points) {
    if (points.size() < 3 || points.size() > 256) return std::nullopt;
    for (const auto& point : points)
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1)
            return std::nullopt;

    bool hasPositiveTurn = false;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto a = points[(i + 1) % points.size()] - points[i];
        const auto b = points[(i + 2) % points.size()] - points[(i + 1) % points.size()];
        if (a.x == 0 && a.y == 0) return std::nullopt;
        const float turn = a.x * b.y - a.y * b.x;
        if (turn < 0) return std::nullopt;
        hasPositiveTurn |= turn > 0;
        for (const auto& point : points) {
            const auto offset = point - points[i];
            if (a.x * offset.y - a.y * offset.x < 0) return std::nullopt;
        }
    }
    if (!hasPositiveTurn) return std::nullopt;

    ConvexPolygon polygon;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto edge = points[(i + 1) % points.size()] - points[i];
        polygon.addLine(Line{points[i], glm::vec2{edge.y, -edge.x}});
    }
    return polygon;
}

std::optional<IndexedSpriteMesh> makeIndexedSpriteMesh(const ConvexList& list) {
    IndexedSpriteMesh mesh;
    std::map<std::pair<float, float>, u32> vertexIndices;
    bool valid = true;
    list.triangulate([&](const auto& a, const auto& b, const auto& c) {
        for (const auto& point : {a, b, c}) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                valid = false;
                return;
            }
            const auto [index, inserted] = vertexIndices.try_emplace(
                std::make_pair(point.x, point.y), static_cast<u32>(mesh.vertices.size()));
            if (inserted) mesh.vertices.push_back(point);
            mesh.indices.push_back(index->second);
        }
    });
    if (!valid || mesh.indices.empty()) return std::nullopt;
    return mesh;
}
