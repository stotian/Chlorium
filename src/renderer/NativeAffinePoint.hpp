#pragma once
#include <emmintrin.h>
#include <cstddef>
#include <cstdint>

namespace chlorium::affine {
struct Point { float x, y; };
struct Matrix { float a, b, c, d, tx, ty; };
using Apply = Point* (*)(Point*, const Point*, const Matrix*);

inline bool overlaps(const void* a, std::size_t as, const void* b, std::size_t bs) {
    const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
    return x <= y ? y - x < as : x - y < bs;
}

// GD's installed Cocos helper promotes every operand to double. In particular,
// its X sum is c*y + a*x while its Y sum is b*x + d*y. Preserve both orders:
// no float arithmetic, FMA, reassociation, approximation or persistent cache.
inline Point* applyPacked(Point* output, const Point* point, const Matrix* matrix) {
    const auto xy = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(point));
    const auto abcd = _mm_loadu_ps(&matrix->a);
    const auto firstCoefficients = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(0, 0, 1, 2));
    const auto secondCoefficients = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(0, 0, 3, 0));
    const auto yx = _mm_shuffle_ps(xy, xy, _MM_SHUFFLE(0, 0, 0, 1));
    const auto first = _mm_mul_pd(_mm_cvtps_pd(firstCoefficients), _mm_cvtps_pd(yx));
    const auto second = _mm_mul_pd(_mm_cvtps_pd(secondCoefficients), _mm_cvtps_pd(xy));
    const auto translation = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&matrix->tx));
    const auto result = _mm_cvtpd_ps(_mm_add_pd(_mm_add_pd(first, second), _mm_cvtps_pd(translation)));
    _mm_storel_pi(reinterpret_cast<__m64*>(output), result);
    return output;
}

inline Point* applyNonAliased(Point* output, const Point* point, const Matrix* matrix, Apply native) {
    // Packed lanes cannot preserve the order of unmasked FP exceptions or the
    // native NaN-payload selection. Preserve those cases through native code.
    const auto exponent = _mm_set1_epi32(0x7f800000);
    const auto coefficients = _mm_castps_si128(_mm_loadu_ps(&matrix->a));
    const auto coordinates = _mm_movelh_ps(
        _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(point)),
        _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&matrix->tx)));
    const auto exceptional = _mm_or_si128(
        _mm_cmpeq_epi32(_mm_and_si128(coefficients, exponent), exponent),
        _mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(coordinates), exponent), exponent));
    if (_mm_movemask_epi8(exceptional) || (_mm_getcsr() & 0x1f80u) != 0x1f80u)
        return native(output, point, matrix);
    return applyPacked(output, point, matrix);
}

inline Point* applyChecked(Point* output, const Point* point, const Matrix* matrix, Apply native) {
    // Native zero-initialises output before reading inputs and writes X before
    // rereading inputs for Y. Aliasing must retain those native side effects.
    if (overlaps(output, sizeof(*output), point, sizeof(*point)) ||
        overlaps(output, sizeof(*output), matrix, sizeof(*matrix)))
        return native(output, point, matrix);
    return applyNonAliased(output, point, matrix, native);
}

inline Point* applyImportedNonAliased(Point* output, const Point* point, const Matrix* matrix,
    Apply volatile* slot, Apply expected, std::uint64_t entry) {
    const auto current=*slot;
    if(current!=expected || *reinterpret_cast<volatile const std::uint64_t*>(expected)!=entry)
        return current(output,point,matrix);
    return applyNonAliased(output,point,matrix,expected);
}

struct Stats { std::uint64_t calls = 0, fallbacks = 0; bool installed = false; };
}
