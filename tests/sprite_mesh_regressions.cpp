#include "math/SpriteMesh.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

static int checks = 0;
static void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static std::optional<ConvexPolygon> polygon(std::initializer_list<glm::vec2> points) {
    return makeConvexSpritePolygon({points.begin(), points.size()});
}

static IndexedSpriteMesh checkExactTriangles(const ConvexList& list) {
    std::vector<glm::vec2> original;
    list.triangulate([&](const auto& a, const auto& b, const auto& c) {
        original.insert(original.end(), {a, b, c});
    });
    auto indexed = makeIndexedSpriteMesh(list);
    require(indexed.has_value(), "nonempty outline produces an indexed mesh");
    require(indexed->indices.size() == original.size(), "triangle/index count remains unchanged");
    for (size_t i = 0; i < original.size(); ++i) {
        require(indexed->indices[i] < indexed->vertices.size(), "every index addresses an emitted vertex");
        const auto reconstructed = indexed->vertices[indexed->indices[i]];
        require(reconstructed.x == original[i].x && reconstructed.y == original[i].y,
            "reconstructed triangles exactly match old coordinates and draw order");
    }
    return *indexed;
}

static float area(const IndexedSpriteMesh& mesh) {
    float sum = 0;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto a = mesh.vertices[mesh.indices[i]];
        const auto u = mesh.vertices[mesh.indices[i + 1]] - a;
        const auto v = mesh.vertices[mesh.indices[i + 2]] - a;
        sum += std::abs(u.x * v.y - u.y * v.x) / 2;
    }
    return sum;
}

#include "sprite_mesh_fixture.inc"

int main() {
    const auto square = polygon({{0, 0}, {1, 0}, {1, 1}, {0, 1}});
    require(square.has_value(), "counter-clockwise square is valid");
    ConvexList squareList;
    squareList.addPolygon(*square);
    auto indexedSquare = checkExactTriangles(squareList);
    require(indexedSquare.vertices.size() == 4 && indexedSquare.indices.size() == 6,
        "square shares four vertices across its two triangles");
    require(std::abs(area(indexedSquare) - 1) < 0.0001f, "shared square keeps unit coverage");

    ConvexList overlapping;
    overlapping.addPolygon(*square);
    const auto triangle = polygon({{0, 0}, {1, 0}, {0, 1}});
    require(triangle.has_value(), "counter-clockwise triangle is valid");
    overlapping.addPolygon(*triangle);
    overlapping.addPolygon(*square);
    auto indexedOverlap = checkExactTriangles(overlapping);
    require(indexedOverlap.vertices.size() < indexedOverlap.indices.size(),
        "vertices are reused across polygons without removing repeated triangles");
    require(std::abs(area(indexedOverlap) - 2.5f) < 0.0001f,
        "overlapping polygon coverage and multiplicity are preserved");

    // Exact source values from block003_slope_02_color_001.png in
    // resources/spriteMeshes.json. The first point lies on the closing edge.
    const auto slope = polygon({
        {0.7263157894736842f, 0.7083333333333334f},
        {0.7263157894736842f, 0.9895833333333334f},
        {0.6421052631578947f, 0.90625f},
        {0.6421052631578947f, 0},
        {0.7263157894736842f, 0}
    });
    require(slope.has_value(), "bundled slope's collinear boundary vertex is valid");
    ConvexList slopeList;
    slopeList.addPolygon(*slope);
    auto indexedSlope = checkExactTriangles(slopeList);
    require(slope->getLines().size() == 4, "redundant collinear slope edge is removed");
    require(area(indexedSlope) > 0, "bundled collinear slope still has visible area");

    require(!polygon({{0, 0}, {1, 0}}), "fewer than three points are rejected");
    require(!polygon({{0, 0}, {1, 0}, {1, 0}, {0, 1}}), "zero-length edge is rejected");
    require(!polygon({{0, 0}, {0.5f, 0}, {1, 0}}), "wholly collinear polygon is rejected");
    require(!polygon({{0, 0}, {0, 1}, {1, 1}, {1, 0}}), "clockwise polygon is rejected");
    require(!polygon({{0, 0}, {1, 0}, {0.3f, 0.3f}, {0, 1}}), "concave polygon is rejected");
    require(!polygon({{0, 0}, {1, 1}, {0, 1}, {1, 0}}), "crossing polygon is rejected");
    require(!polygon({{0.5f, 1}, {0.2f, 0}, {1, 0.7f}, {0, 0.7f}, {0.8f, 0}}),
        "self-crossing star is rejected even when local turns agree");
    require(!polygon({{-0.1f, 0}, {1, 0}, {0, 1}}), "outside unit-sprite coordinates are rejected");
    require(!polygon({{0, 0}, {std::numeric_limits<float>::infinity(), 0}, {0, 1}}),
        "infinite coordinates are rejected");
    require(!polygon({{0, 0}, {1, 0}, {0, std::numeric_limits<float>::quiet_NaN()}}),
        "NaN coordinates are rejected");
    std::vector<glm::vec2> excessive(257, {0, 0});
    require(!makeConvexSpritePolygon(excessive), "oversized outline is rejected");
    require(!makeIndexedSpriteMesh(ConvexList{}), "empty mesh uses the quad fallback");
    checkBundledMeshes();
    std::cout << "PASS: " << checks << " sprite mesh regression checks\n";
}
