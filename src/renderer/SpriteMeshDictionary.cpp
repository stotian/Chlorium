#include "SpriteMeshDictionary.hpp"
#include "Geode/cocos/sprite_nodes/CCSpriteFrame.h"
#include "Geode/cocos/sprite_nodes/CCSpriteFrameCache.h"
#include "common.hpp"
#include "glm/fwd.hpp"
#include <matjson.hpp>
#include <optional>
#include <unordered_map>

using namespace geode::prelude;

static std::unordered_map<CCSprite*, CCSpriteFrame*> spriteFramesOfSprites;

#ifdef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
#include <Geode/modify/CCSprite.hpp>
class $modify(SpriteMeshDictionaryCCSprite, CCSprite) {
    void setDisplayFrame(CCSpriteFrame* frame) {
        CCSprite::setDisplayFrame(frame);
        if (frame) spriteFramesOfSprites[this] = frame;
        else spriteFramesOfSprites.erase(this);
    }
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        if (CCSprite::initWithSpriteFrame(frame)) {
            spriteFramesOfSprites[this] = frame;
            return true;
        }
        return false;
    }

    ~SpriteMeshDictionaryCCSprite() {
        spriteFramesOfSprites.erase(this);
    }
};
#endif

static std::unordered_map<CCSpriteFrame*, ConvexList> spriteMeshesPerFrame;
static std::unordered_map<CCSpriteFrame*, IndexedSpriteMesh> indexedSpriteMeshesPerFrame;
static std::vector<Ref<CCSpriteFrame>> retainedMeshFrames;

ConvexList* SpriteMeshDictionary::getSpriteMeshForSprite(cocos2d::CCSprite* sprite) {
    auto frameIt = spriteFramesOfSprites.find(sprite);
    if (frameIt == spriteFramesOfSprites.end())
        return nullptr;

    auto meshIt = spriteMeshesPerFrame.find(frameIt->second);
    if (meshIt == spriteMeshesPerFrame.end())
        return nullptr;

    // setTextureRect/initWithTexture can replace a sprite's image without
    // setDisplayFrame. Never apply an old frame's silhouette to its new crop.
    auto frame = frameIt->second;
    if (sprite->getTexture() != frame->getTexture() || sprite->getTextureRect() != frame->getRect() ||
        sprite->isTextureRectRotated() != frame->isRotated())
        return nullptr;

    return &meshIt->second;
}

const IndexedSpriteMesh* SpriteMeshDictionary::getIndexedSpriteMeshForSprite(CCSprite* sprite) {
    if (!getSpriteMeshForSprite(sprite)) return nullptr;
    const auto frame = spriteFramesOfSprites.find(sprite)->second;
    const auto mesh = indexedSpriteMeshesPerFrame.find(frame);
    return mesh == indexedSpriteMeshesPerFrame.end() ? nullptr : &mesh->second;
}

static std::optional<ConvexList> parseConvexList(const matjson::Value& array);

void SpriteMeshDictionary::loadFromFile(const fs::path& path) {
    spriteMeshesPerFrame.clear();
    indexedSpriteMeshesPerFrame.clear();
    retainedMeshFrames.clear();

    auto source = readResourceFile(path);
    if (!source.has_value())
        return;

    auto result = matjson::parse(source.value());
    if (!result.isOk())
        return;

    auto value = result.unwrap();

    if (!value.isObject())
        return;

    auto frames = value.as<std::map<std::string, matjson::Value>>();
    if (!frames.ok())
        return;

    for (auto [key, value] : frames.unwrap()) {
        CCSpriteFrame* frame = CCSpriteFrameCache::get()->spriteFrameByName(key.c_str());
        if (!frame)
            continue;

        std::optional<ConvexList> convexList = parseConvexList(value);
        if (!convexList.has_value())
            continue;

        // Triangulation uses line intersections. Cache it once per frame,
        // rather than repeating it for every instance in a level, and reuse
        // exactly equal vertices without changing triangle/index order.
        auto mesh = makeIndexedSpriteMesh(*convexList);
        if (!mesh) continue;

        // Retain before publishing either raw-pointer key. If a later map
        // allocation fails, every published frame must still have an owner.
        retainedMeshFrames.emplace_back(frame);
        spriteMeshesPerFrame[frame] = std::move(*convexList);
        indexedSpriteMeshesPerFrame[frame] = std::move(*mesh);
    }
}

static std::optional<ConvexPolygon> parseConvexPolygon(const matjson::Value& array);

static std::optional<ConvexList> parseConvexList(const matjson::Value& array) {
    if (!array.isArray()) return std::nullopt;
    if (array.size() == 0 || array.size() > 256) return std::nullopt;

    ConvexList list;

    for (auto& value : array.as<std::vector<matjson::Value>>().unwrap()) {
        std::optional<ConvexPolygon> polygon = parseConvexPolygon(value);
        if (!polygon.has_value())
            return std::nullopt;

        list.addPolygon(polygon.value());
    }

    return list;
}

// It is assumed that the convex polygons are counter clockwise
static std::optional<ConvexPolygon> parseConvexPolygon(const matjson::Value& array) {
    if (!array.isArray())
        return std::nullopt;

    std::vector<glm::vec2> points;
    auto pointsJson = array.as<std::vector<matjson::Value>>().unwrap();
    if (pointsJson.size() < 3 || pointsJson.size() > 256) return std::nullopt;

    for (auto& pointJson : pointsJson) {
        if (!pointJson.isArray())
            return std::nullopt;

        auto pointJsonArray = pointJson.as<std::vector<matjson::Value>>().unwrap();

        if (pointJsonArray.size() != 2)
            return std::nullopt;

        auto xValue = pointJsonArray[0];
        auto yValue = pointJsonArray[1];

        if (!xValue.isNumber() || !yValue.isNumber())
            return std::nullopt;

        points.push_back(glm::vec2 {
            xValue.as<float>().unwrap(),
            yValue.as<float>().unwrap()
        });
    }

    // Collinear boundary vertices are valid (the bundled slope detail mesh
    // has one). Require nonzero edges and a counter-clockwise convex region;
    // ConvexPolygon removes redundant parallel constraints itself.
    return makeConvexSpritePolygon(points);
}
