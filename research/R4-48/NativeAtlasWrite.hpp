#pragma once
#include "../../src/renderer/NativeQuadState.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace chlorium::research::atlaswrite {
// Owned reference model for the reviewed Cocos updateQuad body. No hook or
// native write is installed. Failure leaves every caller-owned byte unchanged.
inline bool apply(std::span<std::byte> store,std::uint32_t index,
                  std::span<const std::byte> input,std::uint32_t& total,std::uint8_t& dirty) noexcept {
    constexpr std::size_t quadBytes=sizeof(snapshot::NativeQuadState);
    if(!input.data() || !store.data() || input.size()!=quadBytes || store.size()%quadBytes || index>=store.size()/quadBytes)return false;
    total=std::max(total,index+std::uint32_t{1});
    auto* destination=store.data()+std::size_t(index)*quadBytes;
    // Preserve six forward load/store pairs, including overlapping input/output.
    // A single memcpy/memmove96 would have different overlap semantics.
    for(std::size_t offset=0;offset<quadBytes;offset+=16){
        std::array<std::byte,16> block;
        std::memcpy(block.data(),input.data()+offset,block.size());
        std::memcpy(destination+offset,block.data(),block.size());
    }
    dirty=1;return true;
}
}
