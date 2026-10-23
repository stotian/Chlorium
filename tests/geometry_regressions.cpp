#include <glm/glm.hpp>
#include "math/ConvexPolygon.hpp"
#include <cstdlib>
#include <iostream>

static int checks = 0;
void require(bool value, const char* name) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
float area(const ConvexPolygon& polygon) {
    float result = 0;
    polygon.triangulate([&](auto a, auto b, auto c) {
        auto u = b - a, v = c - a;
        result += std::abs(u.x * v.y - u.y * v.x) / 2;
    });
    return result;
}
int main() {
    ConvexPolygon square;
    square.addLine(Line(0, {-1, 0}));
    square.addLine(Line(1, {1, 0}));
    square.addLine(Line(0, {0, -1}));
    square.addLine(Line(1, {0, 1}));
    require(square.containsPoint({.5f, .5f}) && !square.containsPoint({2, .5f}), "unit square containment");
    require(std::abs(area(square) - 1) < .0001f, "unit square triangulates to unit area");
    square.addLine(Line(.25f, {1, 0}));
    require(square.containsPoint({.2f, .5f}) && !square.containsPoint({.5f, .5f}), "parallel tighter constraint clips correct side");
    require(std::abs(area(square) - .25f) < .0001f, "parallel tighter constraint preserves ordered triangulation");
    square.addLine(Line(5, {1, 0}));
    require(std::abs(area(square) - .25f) < .0001f, "looser redundant constraint does not expand polygon");
    square.addLine(Line(.25f, {1, 0}));
    require(square.getLines().size() == 4, "duplicate constraint does not add degenerate edge");
    require(!Line(0, {1, 0}).intersectionWith(Line(1, {1, 0})), "parallel lines have no finite intersection");
    std::cout << "PASS: " << checks << " geometry regression checks\n";
}
