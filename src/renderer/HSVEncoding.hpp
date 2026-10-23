#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace chlorium {
inline std::uint32_t packHSV(float hue, float saturation, float value, bool absoluteSaturation, bool absoluteBrightness) {
    hue = std::isfinite(hue) ? std::remainder(hue, 360.f) : 0.f;
    if (!std::isfinite(saturation)) saturation = absoluteSaturation ? 0.f : 1.f;
    if (!std::isfinite(value)) value = absoluteBrightness ? 0.f : 1.f;
    const auto h = static_cast<std::uint32_t>(hue + 256.f);
    const auto s = static_cast<std::uint32_t>(std::clamp(saturation + (absoluteSaturation ? 1.f : 0.f), 0.f, 2.f) * 127.5f);
    const auto v = static_cast<std::uint32_t>(std::clamp(value + (absoluteBrightness ? 1.f : 0.f), 0.f, 2.f) * 127.5f);
    return ((h & 0x1ff) << 16) | ((s & 0xff) << 8) | (v & 0xff) |
           (absoluteSaturation ? 0x20000000 : 0) | (absoluteBrightness ? 0x10000000 : 0);
}
}
