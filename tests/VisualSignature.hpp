#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace chlorium::test {
// Private diagnostic only. Flags identify candidates, never rendering defects.
// The bottom quarter and top 15% contain the test/user HUD and are excluded.
struct VisualSignature {
    bool valid = false, dark = false;
    unsigned partial = 0; // left=2, right=4, bottom=8, top=16
    std::uint64_t hash = 14695981039346656037ull;
    double centerMean = 0, mean = 0, blackFraction = 0;
};
inline VisualSignature analyzeVisual(const unsigned char* rgba, int width, int height) {
    VisualSignature out;
    if (!rgba || width < 16 || height < 16 ||
        static_cast<std::uint64_t>(width) * height > 4 * 1024 * 1024) return out;
    const int bottom = height / 4, top = height * 85 / 100;
    std::array<unsigned, 4> black{};
    std::array<std::uint64_t, 4> sums{};
    unsigned allBlack = 0;
    std::uint64_t sum = 0;
    constexpr unsigned perHalf = 256, total = 512;
    for (int ty = 0; ty < 4; ++ty) for (int tx = 0; tx < 8; ++tx) {
        for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
            const int x = ((tx * 8 + sx * 2 + 1) * width) / 64;
            const int y = bottom + ((ty * 8 + sy * 2 + 1) * (top - bottom)) / 32;
            const auto p = rgba + (static_cast<std::size_t>(y) * width + x) * 4;
            const unsigned value = p[0] + p[1] + p[2];
            const bool dark = p[0] <= 4 && p[1] <= 4 && p[2] <= 4;
            const auto horizontal = tx < 4 ? 0 : 1, vertical = ty < 2 ? 2 : 3;
            black[horizontal] += dark; black[vertical] += dark; allBlack += dark;
            sums[horizontal] += value; sums[vertical] += value; sum += value;
            for (int c = 0; c < 3; ++c) {
                out.hash ^= p[c] >> 3; out.hash *= 1099511628211ull;
            }
        }
    }
    for (int x = 0; x < width; ++x) {
        const auto p = rgba + (static_cast<std::size_t>(height / 2) * width + x) * 4;
        out.centerMean += p[0] + p[1] + p[2];
    }
    out.centerMean /= width * 3;
    out.mean = static_cast<double>(sum) / (total * 3);
    out.blackFraction = static_cast<double>(allBlack) / total;
    // Confirm a near-black sample against every pixel in the core region.
    if (allBlack >= total * .99) {
        std::uint64_t fullSum = 0, fullBlack = 0;
        const auto count = static_cast<std::size_t>(top - bottom) * width;
        for (std::size_t i = static_cast<std::size_t>(bottom) * width * 4;
             i < static_cast<std::size_t>(top) * width * 4; i += 4) {
            fullSum += rgba[i] + rgba[i+1] + rgba[i+2];
            fullBlack += rgba[i] <= 4 && rgba[i+1] <= 4 && rgba[i+2] <= 4;
        }
        out.mean = static_cast<double>(fullSum) / (count * 3);
        out.blackFraction = static_cast<double>(fullBlack) / count;
        out.dark = out.mean < 4 && out.blackFraction >= .99;
    }
    if (!out.dark) for (unsigned half = 0; half < 4; ++half) {
        if (black[half] >= perHalf * .98 &&
            static_cast<double>(sums[half ^ 1]) / (perHalf * 3) > 12)
            out.partial |= 2u << half;
    }
    out.valid = true;
    return out;
}
}
