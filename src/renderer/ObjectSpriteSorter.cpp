#include "ObjectSpriteSorter.hpp"
#include "Renderer.hpp"

SortedObjectBatch::~SortedObjectBatch() {
    if (spriteRemapBuffer)
        Buffer::destroy(spriteRemapBuffer);
    if (workGroupBuffer)
        Buffer::destroy(workGroupBuffer);
    if (drawCommandBuffer)
        Buffer::destroy(drawCommandBuffer);
}

void SortedObjectBatch::bindBuffers() {
    spriteRemapBuffer->bindAsStorageBuffer(SPRITE_REMAP_BUFFER_BINDING);
    workGroupBuffer->bindAsStorageBuffer(WORK_GROUP_BUFFER_BINDING);
    drawCommandBuffer->bindAsStorageBuffer(DRAW_COMMAND_BUFFER_BINDING);
}

void ObjectSpriteSorter::addSprite(
    GameObject* object,
    cocos2d::CCSprite* sprite,
    SpriteType type,
    u32 inputSpriteIndex,
    u32 vertexCount
) {
    spriteEntries.push_back({
        .zOrder = object->getObjectZOrder(),
        .zLayer = object->getObjectZLayer(),
        .spriteSheet = ObjectUtils::getSpritesheetOfObject(object, type),
        .colorChannelId = static_cast<i32>(ObjectUtils::getSpriteColorChannel(type, object, sprite)),
        .inputSpriteIndex = inputSpriteIndex,
        .vertexCount = vertexCount
    });
}

void ObjectSpriteSorter::sortForCurrentBlendingStates(Renderer& renderer) {
    // Retain layer-vector capacities across color-channel blending changes.
    for (auto& [key, layer] : sortedSpriteLayers) layer.clear();

    for (auto& sprite : spriteEntries) {
        auto key = sprite.getFullLayerKey(renderer);
        sortedSpriteLayers[key].push_back(&sprite);
    }

    for (auto& [key, layer] : sortedSpriteLayers) {
        std::stable_sort(layer.begin(), layer.end(), [](const SpriteEntry* a, const SpriteEntry* b) {
            return a->zOrder < b->zOrder;
        });
    }
}

UPtr<SortedObjectBatch> ObjectSpriteSorter::generateSortedBatch() {
    if (spriteEntries.empty() || sortedSpriteLayers.empty()) return nullptr;
    std::vector<u32> spriteRemap;
    spriteRemap.reserve(spriteEntries.size());

    constexpr u32 approxSpritesPerWorkGroup = 32;
    u32 approxWorkGroupCount = static_cast<u32>((spriteEntries.size() + 31) / 32);

    std::vector<WorkGroup> workGroups;
    workGroups.reserve(approxWorkGroupCount + 100);

    u32 outIndex = 0;
    std::map<LayerKey, WorkGroupRange> workGroupRangesPerLayerKey;

    for (auto& [key, layer] : sortedSpriteLayers) {
        // log::info("Process layer: {}", key.toString());
        u32 workGroupStartIndex = workGroups.size();
        generateWorkGroupsForLayer(key, approxSpritesPerWorkGroup, outIndex, spriteRemap, workGroups);
        u32 workGroupEndIndex = workGroups.size();

        workGroupRangesPerLayerKey[key] = {
            .offset = workGroupStartIndex,
            .count  = workGroupEndIndex - workGroupStartIndex
        };

        // log::info("Layer processed {}: (index={}, size={})", key.toString(), workGroupStartIndex, workGroupEndIndex - workGroupStartIndex);
    }

    if (workGroups.size() > 65535) {
        log::warn("Chlorium level exceeds the supported compute dispatch size");
        return nullptr;
    }
    log::info("Sprites per work group: {}", approxSpritesPerWorkGroup);
    log::info("Work group count: {}", workGroups.size());

    std::unique_ptr<Buffer> spriteRemapBuffer(Buffer::createStaticDraw(
        "Sprite remap buffer",
        spriteRemap.data(),
        spriteRemap.size() * sizeof(u32)
    ));

    if (!spriteRemapBuffer)
        return nullptr;

    std::unique_ptr<Buffer> workGroupBuffer(Buffer::createStaticDraw(
        "Work group buffer",
        workGroups.data(),
        workGroups.size() * sizeof(WorkGroup)
    ));

    if (!workGroupBuffer) return nullptr;

    std::vector<DrawCommand> drawCommands;
    drawCommands.resize(workGroups.size());

    for (auto& cmd : drawCommands) {
        cmd.baseInstance = 0;
        cmd.baseVertex = 0;
        cmd.instanceCount = 1;
    }

    std::unique_ptr<Buffer> drawCommandBuffer(Buffer::createGPUExclusive(
        "Draw command buffer",
        workGroups.size() * sizeof(DrawCommand)
    ));

    if (!drawCommandBuffer) return nullptr;

    drawCommandBuffer->write(drawCommands.data(), drawCommandBuffer->getSize());

    // Keep temporary owners until construction (including the map copy) has
    // succeeded, otherwise a later allocation failure leaks earlier buffers.
    auto result = std::make_unique<SortedObjectBatch>(
        spriteRemapBuffer.get(), workGroupBuffer.get(), drawCommandBuffer.get(),
        workGroups.size(), std::move(workGroupRangesPerLayerKey));
    spriteRemapBuffer.release();
    workGroupBuffer.release();
    drawCommandBuffer.release();
    return result;
}

void ObjectSpriteSorter::generateWorkGroupsForLayer(
    LayerKey layerKey,
    u32 spritesPerWorkGroup,
    u32& culledIndexBufferOutputIndex,
    std::vector<u32>& inputSpriteIndiciesOut,
    std::vector<WorkGroup>& workGroupsOut
) {
    assert(spritesPerWorkGroup > 0);

    const auto& sprites = sortedSpriteLayers[layerKey];

    u32 i = 0;
    while (i < sprites.size()) {
        u32 spritesToAdd = spritesPerWorkGroup;

        u32 culledOutputIndex = culledIndexBufferOutputIndex;
        u32 startIndex = inputSpriteIndiciesOut.size();

        while (spritesToAdd > 0 && i < sprites.size()) {
            auto sprite = sprites[i];
            inputSpriteIndiciesOut.push_back(sprite->inputSpriteIndex);
            culledIndexBufferOutputIndex += sprite->vertexCount;
            spritesToAdd--;
            i++;
        }

        u32 endIndex = inputSpriteIndiciesOut.size();

        // log::info("  - Work group {} = (index={}, count={}), {}", workGroupsOut.size(), startIndex, endIndex - startIndex, culledOutputIndex);

        workGroupsOut.push_back({
            .inputSpriteStart = startIndex,
            .inputSpriteEnd   = endIndex,
            .culledIndexBufferOutputIndex = culledOutputIndex
        });
    }
}

LayerKey ObjectSpriteSorter::SpriteEntry::getFullLayerKey(Renderer& renderer) const {
    return { zLayer, spriteSheet, renderer.isColorChannelBlending(colorChannelId) };
}
