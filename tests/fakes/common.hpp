#pragma once
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <functional>
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using i32 = std::int32_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;
namespace geode::prelude {
extern bool mappedUploads;
struct Mod {
    static Mod* get() { static Mod instance; return &instance; }
    template <class T> T getSettingValue(const char*) { return static_cast<T>(mappedUploads); }
};
}
