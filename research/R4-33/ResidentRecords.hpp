#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace chlorium::research::resident {
struct alignas(16) Geometry { std::uint32_t rect[4]{}, uv[8]{}, control[4]{}; };
struct alignas(16) Parameters { std::uint32_t matrix[4]{}, translationZFlags[4]{}, colors[4]{}; };
static_assert(sizeof(Geometry)==64 && sizeof(Parameters)==48);
static_assert(offsetof(Geometry,uv)==16 && offsetof(Geometry,control)==48);
static_assert(offsetof(Parameters,translationZFlags)==16 && offsetof(Parameters,colors)==32);
static_assert(std::is_trivially_copyable_v<Geometry> && std::is_trivially_copyable_v<Parameters>);
}
