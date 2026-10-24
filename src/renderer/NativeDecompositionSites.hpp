#pragma once
#include <cstdint>
namespace chlorium::native {
constexpr bool isDecompositionCaller(std::uintptr_t relative, bool area) noexcept {
    return area ? relative == 0x229b2b || relative == 0x229b51
                : relative == 0x22d179 || relative == 0x22d1a1;
}
}
