#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace chlorium::snapshot {
// This is a byte-for-byte snapshot of ccV3F_C4B_T2F, not a reconstructed
// rectangle. Keep all four positions, colors and UVs: sprite subclasses and
// other mods can make these values differ independently at every corner.
struct NativeQuadVertex {
    float x = 0, y = 0, z = 0;
    std::uint32_t color = 0;
    float u = 0, v = 0;
};

// Native ccV3F_C4B_T2F_Quad order. The matching triangle strip shares BL/TR,
// the same diagonal as the native atlas's two indexed triangles.
struct NativeQuadState { std::array<NativeQuadVertex, 4> corners{}; }; // TL, BL, TR, BR

// The shader reads scalar std430 fields, with RGBA bytes unpacked low-to-high
// from the color word. Reject a different host ABI instead of silently
// changing positions, texture coordinates or channel order after a port.
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(std::is_standard_layout_v<NativeQuadVertex> && std::is_trivially_copyable_v<NativeQuadVertex>);
static_assert(std::is_standard_layout_v<NativeQuadState> && std::is_trivially_copyable_v<NativeQuadState>);
static_assert(sizeof(NativeQuadVertex) == 24 && alignof(NativeQuadVertex) == 4);
static_assert(offsetof(NativeQuadVertex, x) == 0 && offsetof(NativeQuadVertex, y) == 4 &&
    offsetof(NativeQuadVertex, z) == 8 && offsetof(NativeQuadVertex, color) == 12 &&
    offsetof(NativeQuadVertex, u) == 16 && offsetof(NativeQuadVertex, v) == 20);
static_assert(sizeof(NativeQuadState) == 96 && alignof(NativeQuadState) == 4 &&
    offsetof(NativeQuadState, corners) == 0);
}
