#pragma once

#include <Geode/binding/GameObject.hpp>
#include <common.hpp>
#include <Geode/Geode.hpp>
#include <vector>

#include "Buffer.hpp"
#include "glm/fwd.hpp"
#include "math/ConvexList.hpp"
#include "ObjectUtils.hpp"
#include "ObjectSpriteSorter.hpp"
#include "../utils/GPUTimer.hpp"

using namespace geode;

/*
    These are the vertex attributes of the
    object batch. They are written in this
    macro so you don't have duplicate code
    with the and the attrib pointer calls.

    ATTRIB(attributeLocation, type, name)

    NOTE: The positionOffset attribute is this
          vertex' offset from the position of
          the object this sprite belongs to.
*/
#define OBJECT_VERTEX_ATTRIBUTES(ATTRIB) \
    ATTRIB(0, vec2, positionOffset) \
    ATTRIB(1, vec2, texCoord) \
    ATTRIB(2, u32,  spriteIndex)

////////////////////////////////////////////////

#define VERTEX_ATTRIBUTE_AS_STRUCT_MEMBER(ID, TYPE, NAME) \
    TYPE NAME;

#define VERTICIES_PER_QUAD 4
#define INDICIES_PER_QUAD  6

struct ObjectVertex {
    OBJECT_VERTEX_ATTRIBUTES(VERTEX_ATTRIBUTE_AS_STRUCT_MEMBER);
};

struct ObjectQuad {
    union {
        ObjectVertex verticies[VERTICIES_PER_QUAD];
        struct {
            ObjectVertex bl;
            ObjectVertex br;
            ObjectVertex tl;
            ObjectVertex tr;
        };
    };
};

struct ObjectIndicies {
    u32 indicies[INDICIES_PER_QUAD];
};

////////////////////////////////////////////////

class Renderer;

struct SpriteVertexTransforms {
    glm::vec2 positionBottomLeft;
    glm::vec2 positionRight;
    glm::vec2 positionUp;
    glm::vec2 texCoordBottomLeft;
    glm::vec2 texCoordRight;
    glm::vec2 texCoordUp;
};

class ObjectBatch {
public:
    inline ObjectBatch(Renderer& renderer)
        : renderer(renderer) {}
    ~ObjectBatch();

    SpriteVertexTransforms getSpriteVertexTransform(
        cocos2d::CCSprite* sprite,
        const cocos2d::CCAffineTransform& transform,
        SpriteSheet spriteSheet
    );

    void prepareSpriteMeshWrite(
        GameObject* parentObject,
        cocos2d::CCSprite* sprite,
        SpriteType type,
        const cocos2d::CCAffineTransform& transform
    );

    void writeSpriteVertex(glm::vec2 pos);
    void writeSpriteIndex(u32 index);

    void writeSpriteMeshFromConvexList(const ConvexList& list);

    void addSprite(
        GameObject* parentObject,
        cocos2d::CCSprite* sprite,
        SpriteType type,
        const cocos2d::CCAffineTransform& transform
    );

    void writeGameObject(GameObject* object);

    bool finishWriting();
    static bool isSupported();

    void draw(const LayerKey& layer);

    inline void bind() {
        glBindVertexArray(vao);
        indexBuffer->bindAs(GL_ELEMENT_ARRAY_BUFFER);
    }

    inline usize getVertexBufferSize() {
        return vertexCount * sizeof(ObjectVertex);
    }

    inline void setSpriteSheetFilter(SpriteSheet sheet) {
        spriteSheetFilter = sheet;
    }

    inline const std::set<LayerKey>& getUsedLayerIds() {
        return usedLayerKeys;
    }

    void predraw();
    void refreshBlendingStates();

    void prepareCullingComputeShader();

private:
    void prepareVAO();

private:
    Renderer& renderer;

    SpriteSheet spriteSheetFilter = (SpriteSheet)-1;

    // Static buffers
    Buffer* vertexBuffer = nullptr;
    Buffer* indexBuffer = nullptr;
    Buffer* inputSpriteBuffer = nullptr;
    Buffer* preCulledSpritesBuffer = nullptr;

    // GPU buffers
    Buffer* rawSpriteBuffer = nullptr;
    Buffer* culledIndexBuffer = nullptr;

    GPUTimer cullingComputeShaderTimer { "Culling compute shader time" };

    u32 vao = 0;

    ObjectSpriteSorter spriteSorter;
    UPtr<SortedObjectBatch> sortedBatch;

    std::vector<ObjectVertex> verticies;
    u32 vertexCount = 0;

    std::vector<u32> indicies;
    u32 indexCount = 0;

    std::vector<InputSprite> inputSprites;
    std::vector<PreCulledSprite> preCulledSprites;

    std::set<LayerKey> usedLayerKeys;

    SpriteVertexTransforms currentSpriteVertexTransforms;
    glm::vec2 currentSpriteObjectStartPosition;
    u32 currentSpriteVertexIndex;
    u32 currentSpriteSRBIndex;
    u32 currentSpriteIndex;
    bool validSprites = true;
};
