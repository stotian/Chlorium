#pragma once
#include "../../src/renderer/NativeQuadState.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace chlorium::research::pass {
constexpr std::size_t maxQuads = 65536, maxDraws = 2048;
inline unsigned nativeFlag(const void* address) {
    return *static_cast<const volatile unsigned char*>(address) != 0;
}
enum Reason : std::uint32_t {
    budget = 1, invalidRange = 2, foreignAtlas = 4,
    missingSprites = 8, duplicateIndex = 16, unmatchedQuad = 32,
    changedScene = 64, allocationFailure = 128, changedContext = 256
};
struct SpriteRecord {
    std::uint64_t sprite = 0, parent = 0;
    std::uint32_t id = 0, atlasIndex = 0, flags = 0, quadMatches = 0;
    std::uint32_t matrix[6]{}, localRect[4]{}, anchor[2]{}, z = 0, vertexMods[4]{}, parentId = 0, textureBounds[4]{};
    std::uint64_t root = 0;
    std::uint32_t rootId = 0, rootFlags = 0, groupCount = 0, objectId = 0, objectType = 0, reserved = 0, groups[10]{};
};
struct DrawRecord {
    std::uint64_t batch = 0, atlas = 0, texture = 0, shader = 0;
    std::uint32_t batchId = 0, atlasId = 0, textureId = 0, scope = 0;
    std::uint32_t ordinal = 0, first = 0, count = 0, quadOffset = 0, spriteOffset = 0;
    std::uint32_t mapped = 0, reasons = 0, blendSrc = 0, blendDst = 0, total = 0, capacity = 0, mxcsr = 0;
};
struct FrameHeader {
    std::uint64_t frame = 0, percentBits = 0;
    std::uint32_t draws = 0, quads = 0, sprites = 0, reasons = 0;
    std::uint64_t context = 0, scene = 0;
};
static_assert(sizeof(SpriteRecord) == 192 && sizeof(DrawRecord) == 96 && sizeof(FrameHeader) == 48);
static_assert(std::is_trivially_copyable_v<SpriteRecord> && std::is_trivially_copyable_v<DrawRecord>);

inline bool validRange(std::uint32_t total, std::uint32_t capacity,
                       std::uint32_t first, std::uint32_t count) {
    return total <= capacity && first <= total && count <= total - first;
}
struct Frame {
    FrameHeader header;
    std::vector<DrawRecord> draws;
    std::vector<snapshot::NativeQuadState> quads;
    std::vector<SpriteRecord> sprites;

    void reserve() {
        draws.reserve(maxDraws); quads.reserve(maxQuads); sprites.reserve(maxQuads);
    }
    bool append(DrawRecord record, std::span<const std::byte> source,
                std::span<const SpriteRecord> owners) {
        if (!validRange(record.total, record.capacity, record.first, record.count) ||
            source.size() != static_cast<std::size_t>(record.count) * sizeof(snapshot::NativeQuadState) ||
            owners.size() != record.count) {
            header.reasons |= invalidRange; return false;
        }
        if (draws.size() >= maxDraws || owners.size() > maxQuads - quads.size()) {
            header.reasons |= budget; return false;
        }
        // All allocation must have succeeded before observing the native draw.
        if (draws.capacity() < maxDraws || quads.capacity() < maxQuads || sprites.capacity() < maxQuads) {
            header.reasons |= allocationFailure; return false;
        }
        record.ordinal = static_cast<std::uint32_t>(draws.size());
        record.quadOffset = static_cast<std::uint32_t>(quads.size());
        record.spriteOffset = static_cast<std::uint32_t>(sprites.size());
        record.mapped = 0;
        for (std::size_t i = 0; i < owners.size(); ++i) {
            if (owners[i].sprite && owners[i].atlasIndex == record.first + i) {
                ++record.mapped;
                if (!owners[i].quadMatches) record.reasons |= unmatchedQuad;
            } else record.reasons |= missingSprites;
        }
        quads.resize(quads.size() + owners.size());
        if (!source.empty()) std::memcpy(quads.data() + record.quadOffset, source.data(), source.size());
        sprites.insert(sprites.end(), owners.begin(), owners.end());
        draws.push_back(record);
        header.draws = static_cast<std::uint32_t>(draws.size());
        header.quads = static_cast<std::uint32_t>(quads.size());
        header.sprites = static_cast<std::uint32_t>(sprites.size());
        return true;
    }
};
}
