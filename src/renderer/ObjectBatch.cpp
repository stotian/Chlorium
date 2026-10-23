#include "ObjectBatch.hpp"
#include "BProfiler.hpp"
#include "Renderer.hpp"
#include "SpriteMeshDictionary.hpp"
#include "common.hpp"
#include "glm/fwd.hpp"
#include "math/ConvexList.hpp"
#include "math/ConvexPolygon.hpp"
#include <limits>
#include <string>
#include "../profiler.hpp"

using namespace geode::prelude;

#define QUAD_BL 0
#define QUAD_BR 1
#define QUAD_TL 2
#define QUAD_TR 3

ObjectBatch::~ObjectBatch() {
    if (vertexBuffer)
        Buffer::destroy(vertexBuffer);
    if (indexBuffer)
        Buffer::destroy(indexBuffer);
    for (auto buffer : {inputSpriteBuffer, preCulledSpritesBuffer, rawSpriteBuffer, culledIndexBuffer})
        if (buffer) Buffer::destroy(buffer);
    if (vao)
        glDeleteVertexArrays(1, &vao);
}

SpriteVertexTransforms ObjectBatch::getSpriteVertexTransform(
    cocos2d::CCSprite* sprite,
    const cocos2d::CCAffineTransform& transform,
    SpriteSheet spriteSheet
) {
    CCRect crop = sprite->getTextureRect();
    
    CCTexture2D* texture = renderer.getSpriteSheetTexture(spriteSheet);
    if (texture == nullptr) return {};

    glm::vec2 posBottomLeft  = ccPointToGLM(CCPointApplyAffineTransform(sprite->getOffsetPosition(), transform));
    glm::vec2 posRightVector = glm::vec2(transform.a, transform.b) * crop.size.width;
    glm::vec2 posUpVector    = glm::vec2(transform.c, transform.d) * crop.size.height;

    glm::vec2 texBottomLeft;
    glm::vec2 texRightVector;
    glm::vec2 texUpVector;

    if (!sprite->isTextureRectRotated()) {
        texBottomLeft  = { crop.origin.x, crop.origin.y + crop.size.height };
        texRightVector = { crop.size.width, 0 };
        texUpVector    = { 0, -crop.size.height };
    } else {
        texBottomLeft  = { crop.origin.x, crop.origin.y };
        texRightVector = { 0, crop.size.width };
        texUpVector    = { crop.size.height, 0 };
    }
    
    if (sprite->isFlipX()) {
        posBottomLeft += posRightVector;
        posRightVector = -posRightVector;
    }
    if (sprite->isFlipY()) {
        posBottomLeft += posUpVector;
        posUpVector = -posUpVector;
    }

    float contentScaleFactor = CCDirector::get()->getContentScaleFactor();
    glm::vec2 texCoordFactor = glm::vec2(
        contentScaleFactor / texture->getPixelsWide(),
        contentScaleFactor / texture->getPixelsHigh()
    );

    texBottomLeft  *= texCoordFactor;
    texRightVector *= texCoordFactor;
    texUpVector    *= texCoordFactor;

    return {
        posBottomLeft, posRightVector, posUpVector,
        texBottomLeft, texRightVector, texUpVector
    };
}

void ObjectBatch::prepareSpriteMeshWrite(
    GameObject* object,
    cocos2d::CCSprite* sprite,
    SpriteType type,
    const cocos2d::CCAffineTransform& transform
) {
    SpriteSheet spriteSheet = ObjectUtils::getSpritesheetOfObject(object, type);
    if (spriteSheetFilter != (SpriteSheet)-1 && spriteSheet != spriteSheetFilter)
        return;

    u32 colorChannel = ObjectUtils::getSpriteColorChannel(type, object, sprite);
    if (type == SpriteType::DETAIL)
        colorChannel |= A_COLOR_CHANNEL_IS_SPRITE_DETAIL;

    currentSpriteVertexTransforms    = getSpriteVertexTransform(sprite, transform, spriteSheet);
    currentSpriteObjectStartPosition = ccPointToGLM(object->m_startPosition);

    currentSpriteSRBIndex    = renderer.getObjectSRBIndex(object);
    currentSpriteVertexIndex = verticies.size();
}

void ObjectBatch::writeSpriteVertex(glm::vec2 pos) {
    isize index = verticies.size();
    verticies.resize(index + 1);
    ObjectVertex& vertex = verticies[index];

    auto& transforms = currentSpriteVertexTransforms;

    vertex.positionOffset = transforms.positionRight * pos.x +
                            transforms.positionUp    * pos.y +
                            transforms.positionBottomLeft - currentSpriteObjectStartPosition;
    
    vertex.texCoord = transforms.texCoordRight * pos.x +
                      transforms.texCoordUp    * pos.y +
                      transforms.texCoordBottomLeft;

    vertex.spriteIndex = currentSpriteIndex;
}

void ObjectBatch::writeSpriteIndex(u32 index) {
    indicies.push_back(currentSpriteVertexIndex + index);
}

void ObjectBatch::writeSpriteMeshFromConvexList(const ConvexList& list) {
    u32 vertexIndex = 0;
    list.triangulate([&](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
        writeSpriteVertex(p1);
        writeSpriteVertex(p2);
        writeSpriteVertex(p3);
        writeSpriteIndex(vertexIndex + 0);
        writeSpriteIndex(vertexIndex + 1);
        writeSpriteIndex(vertexIndex + 2);
        vertexIndex += 3;
    });
}


void ObjectBatch::addSprite(
    GameObject* object,
    cocos2d::CCSprite* sprite,
    SpriteType type,
    const cocos2d::CCAffineTransform& transform
) {
    if (!validSprites) return;
    if (!object) { validSprites = false; return; }
    if (object->m_hasNoGlow && type == SpriteType::GLOW)
        return;
    if (!sprite) { validSprites = false; return; }
    const auto crop = sprite->getTextureRect();
    // Animated/container objects can have a zero-area placeholder root with
    // an unrelated dummy texture. It contributes no native pixels.
    if (crop.size.width <= 0 || crop.size.height <= 0) return;
    const auto sheet = ObjectUtils::getSpritesheetOfObject(object, type);
    // SPECIAL roots are individual native nodes, not texture batches. Keep
    // their native draw and still allow their separately batched glow here.
    if (sheet == SpriteSheet::SPECIAL) return;
    if (spriteSheetFilter != (SpriteSheet)-1 && sheet != spriteSheetFilter) return;
    if (!sprite || !renderer.isCompatibleAtlas(sheet, sprite->getTexture())) {
        if (validSprites) {
            log::warn("Chlorium texture mismatch: object {}, sprite type {}, sheet {}, actual texture {}, expected {}",
                object->m_objectID, static_cast<int>(type), static_cast<int>(sheet),
                sprite && sprite->getTexture() ? sprite->getTexture()->getName() : 0,
                renderer.getSpriteSheetTexture(sheet) ? renderer.getSpriteSheetTexture(sheet)->getName() : 0);
            log::warn("Chlorium mismatched sprite size={}x{}, root={}, glow={}", crop.size.width, crop.size.height,
                sprite == object, sprite == object->m_glowSprite);
            for (auto [name, texture] : CCDictionaryExt<std::string, CCTexture2D*>(CCTextureCache::get()->m_pTextures)) {
                if (texture && (texture == sprite->getTexture() || texture == renderer.getSpriteSheetTexture(sheet)))
                    log::warn("Chlorium atlas {} = {}", texture->getName(), name);
            }
        }
        validSprites = false;
        return;
    }

    const auto* spriteMesh = SpriteMeshDictionary::getIndexedSpriteMeshForSprite(sprite);
    const auto addedVertices = spriteMesh ? spriteMesh->vertices.size() : VERTICIES_PER_QUAD;
    const auto addedIndices = spriteMesh ? spriteMesh->indices.size() : INDICIES_PER_QUAD;
    constexpr auto maxIndex = std::numeric_limits<u32>::max();
    if (inputSprites.size() >= maxIndex || addedVertices > maxIndex || addedIndices > maxIndex ||
        verticies.size() > maxIndex - addedVertices || indicies.size() > maxIndex - addedIndices) {
        log::warn("Chlorium sprite geometry exceeds 32-bit buffer indexing; using native rendering");
        validSprites = false;
        return;
    }

    prepareSpriteMeshWrite(object, sprite, type, transform);

    usize indexStart = indicies.size();

    currentSpriteIndex = inputSprites.size();

    if (spriteMesh) {
        for (const auto& point : spriteMesh->vertices)
            writeSpriteVertex(point);
        for (const auto index : spriteMesh->indices)
            writeSpriteIndex(index);
    } else {
        writeSpriteVertex({ 0, 0 });
        writeSpriteVertex({ 1, 0 });
        writeSpriteVertex({ 0, 1 });
        writeSpriteVertex({ 1, 1 });

        writeSpriteIndex(QUAD_BL);
        writeSpriteIndex(QUAD_TL);
        writeSpriteIndex(QUAD_TR);
        writeSpriteIndex(QUAD_BL);
        writeSpriteIndex(QUAD_TR);
        writeSpriteIndex(QUAD_BR);
    }

    usize indexCount = indicies.size() - indexStart;

    inputSprites.push_back({
        .objectIndex = (u32)currentSpriteSRBIndex,
        .spriteType = (u32)type,
        .indexStart = (u32)indexStart,
        .indexCount = (u32)indexCount
    });

    const auto& transformData = currentSpriteVertexTransforms;
    const auto origin = transformData.positionBottomLeft - currentSpriteObjectStartPosition;
    const float radius = std::max({glm::length(origin),
        glm::length(origin + transformData.positionRight),
        glm::length(origin + transformData.positionUp),
        glm::length(origin + transformData.positionRight + transformData.positionUp)});
    preCulledSprites.push_back({
        .groupCombinationIndex = renderer.getGroupManager().getGroupCombinationIndexForObject(object),
        .radius = radius,
        .startPosition = ccPointToGLM(object->getStartPos())
    });

    ZLayer zLayer = object->getObjectZLayer();
    SpriteSheet spriteSheet = ObjectUtils::getSpritesheetOfObject(object, type);

    usedLayerKeys.insert({zLayer, spriteSheet, false});
    usedLayerKeys.insert({zLayer, spriteSheet, true});

    spriteSorter.addSprite(object, sprite, type, currentSpriteIndex, indexCount);
}

void ObjectBatch::writeGameObject(GameObject* object) {
    if (!validSprites) return;
    struct ScaleRestorer {
        GameObject* object = nullptr;
        float x = 1, y = 1;
        ~ScaleRestorer() {
            if (!object) return;
            object->setScaleX(x);
            object->setScaleY(y);
        }
    } restoreScale;

    if (object->m_usesAudioScale) {
        // An allocation failure during mesh writing must also restore the
        // scale before Renderer falls back to the native scene.
        restoreScale.object = object;
        restoreScale.x = object->getScaleX();
        restoreScale.y = object->getScaleY();
        object->setScaleX(object->m_scaleX);
        object->setScaleY(object->m_scaleY);
    }

    ObjectUtils::unpackObjectIntoSprites(object, [&](const UnpackedSprite& sprite) {
        addSprite(sprite.parentObject, sprite.sprite, sprite.type, sprite.transform);
    });
}

bool ObjectBatch::finishWriting() {
    assert(vertexBuffer == nullptr);
    if (!validSprites || verticies.empty() || indicies.empty() || inputSprites.empty()) return false;

    storeGLStates();

    vertexCount = verticies.size();
    indexCount  = indicies.size();
    const auto spriteCount = inputSprites.size();

    vertexBuffer           = Buffer::createStaticDraw("Object vertex buffer", verticies.data(), vertexCount * sizeof(ObjectVertex));
    // These arrays are upload staging, never read by a later frame. clear()
    // alone retains large allocations on dense levels, including shared GPUs.
    std::vector<ObjectVertex>().swap(verticies);
    indexBuffer            = Buffer::createStaticDraw("Object index buffer", indicies.data(), indexCount * sizeof(u32));
    std::vector<u32>().swap(indicies);
    inputSpriteBuffer      = Buffer::createStaticDraw("Input sprite buffer", inputSprites.data(), inputSprites.size() * sizeof(InputSprite));
    std::vector<InputSprite>().swap(inputSprites);
    preCulledSpritesBuffer = Buffer::createStaticDraw("Pre-culled sprites buffer", preCulledSprites.data(), preCulledSprites.size() * sizeof(PreCulledSprite));
    std::vector<PreCulledSprite>().swap(preCulledSprites);

    rawSpriteBuffer   = Buffer::createGPUExclusive("Raw sprite buffer", spriteCount * sizeof(RawSprite));
    culledIndexBuffer = Buffer::createGPUExclusive("Culled index buffer", indexCount * sizeof(u32));

    spriteSorter.sortForCurrentBlendingStates(renderer);
    sortedBatch = spriteSorter.generateSortedBatch();
    if (!sortedBatch) { restoreGLStates(); return false; }

    prepareVAO();
    restoreGLStates();
    return true;
}

typedef void (*PFNGLMULTIDRAWELEMENTSINDIRECTPROC)(GLenum mode, GLenum type, const void * indirect, GLsizei drawcount, GLsizei stride);
static PFNGLMULTIDRAWELEMENTSINDIRECTPROC glMultiDrawElementsIndirect;
typedef void (*PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
static PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = nullptr;

bool ObjectBatch::isSupported() {
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute");
    glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC)wglGetProcAddress("glMultiDrawElementsIndirect");
    auto valid = [](const void* address) {
        const auto value = reinterpret_cast<uintptr_t>(address);
        return value > 3 && value != UINTPTR_MAX;
    };
    GLint bindings = 0, blocks = 0;
    glGetIntegerv(0x90DD, &bindings); // GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
    glGetIntegerv(0x90DB, &blocks);   // GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS
    return valid((const void*)glDispatchCompute) && valid((const void*)glMultiDrawElementsIndirect) &&
           glMemoryBarrier && bindings >= 12 && blocks >= 11;
}

void ObjectBatch::draw(const LayerKey& layer) {
    PROFILER_PUSH("ObjectBatch::draw");

    if (!sortedBatch || !glMultiDrawElementsIndirect || !sortedBatch->workGroupRangePerLayerKey.contains(layer)) {
        PROFILER_POP();
        return;
    }
    const auto& range = sortedBatch->workGroupRangePerLayerKey[layer];


    // The draw program reads only raw sprite SSBO 4 and game-state UBO 3.
    // Compute inputs/commands retain their own bindings during predraw, and
    // need not disturb native indexed buffer state for every texture layer.
    rawSpriteBuffer->bindAsStorageBuffer(RAW_SPRITE_BUFFER_BINDING);
    renderer.bindDrawingShaderBuffers();
    sortedBatch->drawCommandBuffer->bindAs(GL_DRAW_INDIRECT_BUFFER);
    glBindVertexArray(vao);
    culledIndexBuffer->bindAs(GL_ELEMENT_ARRAY_BUFFER);

    glMultiDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        (void*)(range.offset * sizeof(int) * 5),
        range.count,
        sizeof(int) * 5
    );

    PROFILER_POP();
}

void ObjectBatch::predraw() {
    PROFILER_PUSH("ObjectBatch::predraw");
    cullingComputeShaderTimer.start();

    Shader* computeShader = renderer.getCullingComputeShader();

    computeShader->use();

    prepareCullingComputeShader();
    renderer.bindCullingComputeShaderBuffers();
    sortedBatch->bindBuffers();


    // These buffers were read by the previous frame's draw calls.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDispatchCompute(sortedBatch->workGroupCount, 1, 1);
    // The compute shader writes SSBO data, element indices and indirect draw
    // commands. All three consumers need visibility before the draw calls.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT |
                    GL_COMMAND_BARRIER_BIT);

    cullingComputeShaderTimer.stop();
    PROFILER_POP();
}

void ObjectBatch::refreshBlendingStates() {
    spriteSorter.sortForCurrentBlendingStates(renderer);
    auto replacement = spriteSorter.generateSortedBatch();
    if (replacement) sortedBatch = std::move(replacement);
}

void ObjectBatch::prepareCullingComputeShader() {
    indexBuffer->bindAsStorageBuffer(INPUT_INDEX_BUFFER_BINDING);
    inputSpriteBuffer->bindAsStorageBuffer(INPUT_SPRITE_BUFFER_BINDING);
    rawSpriteBuffer->bindAsStorageBuffer(RAW_SPRITE_BUFFER_BINDING);
    culledIndexBuffer->bindAsStorageBuffer(CULLED_INDEX_BUFFER_BINDING);
    preCulledSpritesBuffer->bindAsStorageBuffer(PRE_CULLED_SPRITE_BUFFER_BINDING);
}

struct AttribTypeInfo {
    i32 openGlType;
    i32 componentCount;
    u32 size;
};

static AttribTypeInfo getInfoOfAttributeTypeString(std::string type) {
    if (type == "float") return { GL_FLOAT, 1, sizeof(float) * 1 };
    if (type == "vec2")  return { GL_FLOAT, 2, sizeof(float) * 2 };
    if (type == "vec3")  return { GL_FLOAT, 3, sizeof(float) * 3 };
    if (type == "vec4")  return { GL_FLOAT, 4, sizeof(float) * 4 };

    if (type == "i8")                   return { GL_BYTE,  1, sizeof(i8)  };
    if (type == "i16")                  return { GL_SHORT, 1, sizeof(i16) };
    if (type == "int" || type == "i32") return { GL_INT,   1, sizeof(i32) };

    if (type == "u8")  return { GL_UNSIGNED_BYTE,  1, sizeof(u8)  };
    if (type == "u16") return { GL_UNSIGNED_SHORT, 1, sizeof(u16) };
    if (type == "u32") return { GL_UNSIGNED_INT,   1, sizeof(u32) };
    
    return { 0, 0 };
}

static void vertexAttribPointer(u32 id, const AttribTypeInfo& info, usize stride, usize offset) {
    if (info.openGlType == GL_FLOAT)
        glVertexAttribPointer(id, info.componentCount, info.openGlType, GL_FALSE, sizeof(ObjectVertex), (void*)offset);
    else if (info.openGlType == GL_DOUBLE)
        glVertexAttribLPointer(id, info.componentCount, info.openGlType, sizeof(ObjectVertex), (void*)offset);
    else
        glVertexAttribIPointer(id, info.componentCount, info.openGlType, sizeof(ObjectVertex), (void*)offset);
}

#define VERTEX_ATTRIBUTE_AS_ATTRIB_POINTER_CALL(ID, TYPE, NAME) \
    { \
        auto info = getInfoOfAttributeTypeString(#TYPE); \
        vertexAttribPointer(ID, info, sizeof(ObjectVertex), offsetof(ObjectVertex, NAME)); \
        glEnableVertexAttribArray(ID); \
    }

void ObjectBatch::prepareVAO() {
    if (vao == 0)
        glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);
    vertexBuffer->bindAs(GL_ARRAY_BUFFER);

    OBJECT_VERTEX_ATTRIBUTES(VERTEX_ATTRIBUTE_AS_ATTRIB_POINTER_CALL)
}
