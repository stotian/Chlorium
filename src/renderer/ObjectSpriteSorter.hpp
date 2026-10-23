#pragma once

#include <common.hpp>
#include <ObjectUtils.hpp>
#include "Buffer.hpp"
#include <map>

class Renderer;

struct WorkGroupRange {
    u32 offset, count;
};

struct SortedObjectBatch {
    Buffer* spriteRemapBuffer;
    Buffer* workGroupBuffer;
    Buffer* drawCommandBuffer;

    u32 workGroupCount;

    std::map<LayerKey, WorkGroupRange> workGroupRangePerLayerKey;

    ~SortedObjectBatch();

    void bindBuffers();
};

class ObjectSpriteSorter {
public:
    void addSprite(
        GameObject* object,
        cocos2d::CCSprite* sprite,
        SpriteType type,
        u32 inputSpriteIndex,
        u32 vertexCount
    );

    void sortForCurrentBlendingStates(Renderer& renderer);

    UPtr<SortedObjectBatch> generateSortedBatch();

private:
    void generateWorkGroupsForLayer(
        LayerKey layerKey,
        u32 spritesPerWorkGroup,
        u32& culledIndexBufferOutputIndex,
        std::vector<u32>& inputSpriteIndiciesOut,
        std::vector<WorkGroup>& workGroupsOut
    );

private:
    struct SpriteEntry {
        i32 zOrder;
        ZLayer zLayer;
        SpriteSheet spriteSheet;
        i32 colorChannelId;
        u32 inputSpriteIndex;
        u32 vertexCount;

        LayerKey getFullLayerKey(Renderer& renderer) const;
    };

    std::vector<SpriteEntry> spriteEntries;
    std::vector<SpriteEntry*> sortedSpriteEntries;

    std::unordered_map<LayerKey, std::vector<SpriteEntry*>> sortedSpriteLayers;
};
