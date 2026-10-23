#include "Renderer.hpp"
#include <cmath>
#include <cstdlib>
#include "GLState.hpp"
#include <Geode/cocos/shaders/ccGLStateCache.h>
#include <Geode/binding/TextGameObject.hpp>
#include "Geode/cocos/CCDirector.h"
#include "Geode/cocos/kazmath/include/kazmath/mat4.h"
#include "Geode/cocos/platform/win32/CCGL.h"
#include "Geode/cocos/robtop/keyboard_dispatcher/CCKeyboardDelegate.h"
#include "Geode/cocos/sprite_nodes/CCSpriteBatchNode.h"
#include "GroupManager.hpp"
#include "HotKey.hpp"
#include "ObjectBatchNode.hpp"
#include "SpriteMeshDictionary.hpp"
#include "ccTypes.h"
#include "common.hpp"
#include "glm/fwd.hpp"
#include "math/ConvexPolygon.hpp"
#include "math/Line.hpp"
#include <Geode/Enums.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/RingObject.hpp>
#include <Geode/binding/EnhancedGameObject.hpp>
#include <BProfiler.hpp>
#include "profiler.hpp"

using namespace geode::prelude;

static Renderer* currentRenderer;
#ifdef CHLORIUM_RUNTIME_AUDIT
bool chloriumAuditUseNative();
#endif

static void setRendererProfiling(bool enabled) {
#ifdef CHLORIUM_RUNTIME_AUDIT
    // The private benchmark owns its profiler window across temporary scene
    // graph reparenting. Production debug controls never change that window.
    if (std::getenv("CHLORIUM_BENCHMARK_SECONDS")) return;
#endif
    BProfiler::setEnabled(enabled);
}

Renderer::~Renderer() { terminate(); }

bool Renderer::isCompatibleAtlas(SpriteSheet sheet, CCTexture2D* texture) {
    auto expected = getSpriteSheetTexture(sheet);
    if (!expected || !texture) return false;
    if (expected == texture) return true;
    const auto key = std::make_pair(expected, texture);
    if (auto it = textureEquivalences.find(key); it != textureEquivalences.end()) return it->second;
    bool compatible = false;
    if (expected->getPixelsWide() == texture->getPixelsWide() && expected->getPixelsHigh() == texture->getPixelsHigh()) {
        fs::path expectedPath, actualPath;
        for (auto [name, value] : CCDictionaryExt<std::string, CCTexture2D*>(CCTextureCache::get()->m_pTextures)) {
            if (value == expected) expectedPath = name;
            if (value == texture) actualPath = name;
        }
        std::error_code error;
        // The texture cache can load the very same file twice when one path
        // has Windows separators and the other has forward slashes.
        if (!expectedPath.empty() && !actualPath.empty())
            compatible = fs::equivalent(expectedPath, actualPath, error) && !error;
    }
    textureEquivalences.emplace(key, compatible);
    return compatible;
}

static std::string byteSizeToString(usize size) {
    std::string unit = "B";
    double dsize = size;
    if (dsize > 1000.0) { dsize /= 1000.0; unit = "kB"; }
    if (dsize > 1000.0) { dsize /= 1000.0; unit = "MB"; }
    return fmt::format("{:.3f}{}", dsize, unit);
}

class FinishNode : public CCNode {
    Ref<Renderer> owner;
public:
    explicit FinishNode(Renderer* owner) : owner(owner) {}
    void draw() {
        owner->finish();
    }
};

bool Renderer::init(PlayLayer* layer, CCArray* levelObjects) {
    if (!CCNode::init() || !layer || !levelObjects || !layer->m_objectLayer || !layer->m_effectManager)
        return false;
#ifdef CHLORIUM_RUNTIME_AUDIT
    if (chloriumAuditUseNative()) return false;
#endif
    if (currentRenderer || !Mod::get()->getSettingValue<bool>("enabled")) {
        log::warn("Chlorium init skipped: existing renderer={}, enabled={}", currentRenderer != nullptr, Mod::get()->getSettingValue<bool>("enabled"));
        return false;
    }
    GLint glMajor = 0, glMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &glMinor);
    if (glMajor < 4 || (glMajor == 4 && glMinor < 6) || !ObjectBatch::isSupported()) {
        log::warn("Chlorium requires OpenGL 4.6; using the native renderer");
        return false;
    }
    currentRenderer = this;


    this->layer = layer;
    
    auto size = CCDirector::get()->getWinSize();

    ingameEnableDisable = Mod::get()->getSettingValue<bool>("ingame_enable");

    log::info("OpenGL Version: {}", (const char*)glGetString(GL_VERSION));

    // Use the atlases actually owned by this level's native batches. Loading
    // filenames again can pick a different resolution/texture-loader asset,
    // duplicate large textures, and make every UV refer to the wrong image.
    constexpr ZLayer layers[]{ZLayer::Default, ZLayer::B1, ZLayer::B2, ZLayer::B3,
        ZLayer::B4, ZLayer::B5, ZLayer::T1, ZLayer::T2, ZLayer::T3, ZLayer::T4};
    for (int sheet = 0; sheet < static_cast<int>(SpriteSheet::COUNT); ++sheet) {
        if (sheet == static_cast<int>(SpriteSheet::TEXT)) continue;
        for (auto z : layers) {
            auto batch = getSpriteBatchNodeWithLayerId({z, static_cast<SpriteSheet>(sheet), false});
            if (!batch) batch = getSpriteBatchNodeWithLayerId({z, static_cast<SpriteSheet>(sheet), true});
            if (batch && batch->getTexture()) { spriteSheets[sheet] = batch->getTexture(); break; }
        }
    }

    SpriteMeshDictionary::loadFromFile("spriteMeshes.json");

    groupManager.initWithObjects(levelObjects);

    // ObjectSorter sorter;

    std::vector<GameObject*> objects;

    objects.reserve(levelObjects->count());
    objectSRBIndicies.reserve(levelObjects->count());
    log::info("Level contains {} object(s)", levelObjects->count());
    log::info("Sorting objects...");
    for (auto object : CCArrayExt<GameObject*>(levelObjects)) {
        // Text children, visibility and area/enter effects are dynamic. They
        // must stay in the native renderer rather than being baked at load.
        if (typeinfo_cast<TextGameObject*>(object) ||
            object->getParentMode() == static_cast<int>(SpriteSheet::TEXT)) {
            nativeTextPresent = true;
            continue;
        }
        if (object->getParentMode() == static_cast<int>(SpriteSheet::SPECIAL))
            nativeTextPresent = true; // Individually drawn native nodes also need visibility updates.
        if (object == layer->m_anticheatSpike) {
            DEBUG_LOG("- anti-cheat spike");
            continue;
        }

        if (object->isTrigger() || object->m_isHide)
            continue;

        DEBUG_LOG("- {}, id: {}", (void*)object, object->m_objectID);
        DEBUG_LOG("  - zlayer: {}", zLayerAsString(object->getObjectZLayer()));
        DEBUG_LOG("  - zorder: {}", object->getObjectZOrder());
        DEBUG_LOG("  - blending: {}", object->m_baseOrDetailBlending);
        DEBUG_LOG("  - spritesheet: {}", object->getParentMode());
        DEBUG_LOG("  - glowColorIsLBG: {}", object->m_glowColorIsLBG);
        DEBUG_LOG("  - customGlowColor: {}", object->m_customGlowColor);
        DEBUG_LOG("  - opacityMod: {}", object->m_opacityMod);
        DEBUG_LOG("  - isDecoration2: {}", object->m_isDecoration2);
        DEBUG_LOG("  - colorSprite: {}", object->m_colorSprite);
        DEBUG_LOG("  - activeMainColorID: {}", object->m_activeMainColorID);
        DEBUG_LOG("  - activeDetailColorID: {}", object->m_activeDetailColorID);
        if (object->m_baseColor && object->m_baseColor->m_usesHSV)
            DEBUG_LOG("  - baseHSV: {} {} {} {} {}", object->m_baseColor->m_hsv.h, object->m_baseColor->m_hsv.s, object->m_baseColor->m_hsv.v, object->m_baseColor->m_hsv.absoluteSaturation, object->m_baseColor->m_hsv.absoluteBrightness);
        if (object->m_detailColor && object->m_detailColor->m_usesHSV)
            DEBUG_LOG("  - detailHSV: {} {} {} {} {}", object->m_detailColor->m_hsv.h, object->m_detailColor->m_hsv.s, object->m_detailColor->m_hsv.v, object->m_detailColor->m_hsv.absoluteSaturation, object->m_detailColor->m_hsv.absoluteBrightness);
        if (object->getHasRotateAction())
            DEBUG_LOG("  - rotationDelta: {}", ((EnhancedGameObject*)object)->m_rotationDelta);

        objects.push_back(object);
    }

    log::info("Generating static rendering buffer....");

    renderedGameObjectCount = objects.size();
    if (objects.empty()) return false;
    
    generateStaticRenderingBuffer(objects);

    u32 groupCombCount = groupManager.getGroupCombinationCount();
    log::info("{} group combinations detected", groupCombCount);

    log::info("Generating vertex buffer...");

    for (auto& value : colorChannelsIsBlending)
        value = false;

    fetchBlendingStates();

    for (auto object : objects)
        objectBatch.writeGameObject(object);
    if (!objectBatch.finishWriting()) {
        log::warn("Chlorium cannot safely replace this level's texture batches; keeping native rendering");
        return false;
    }

    log::info("Compiling shaders...");

    std::map<std::string, std::string> shaderMacroVariables;

    // Storage buffers use runtime arrays. Per-level object counts are not
    // compile-time constants and must not force new shader programs each load.

    shader = Shader::create("objectDrawing.vert", "objectDrawing.frag", shaderMacroVariables);
    if (!shader)
        return false;

    cullingComputeShader = Shader::createCompute("objectCulling.comp", shaderMacroVariables);
    if (!cullingComputeShader) return false;

    colorChannelBufferObject = Buffer::createDynamicDraw("Color channel buffer", sizeof(ColorChannelBuffer));
    if (!colorChannelBufferObject)
        return false;
    colorChannelBuffer = (ColorChannelBuffer*)colorChannelBufferObject->mapWriteOnly();
    if (!colorChannelBuffer) return false;
    std::memset(colorChannelBuffer, 0, sizeof(ColorChannelBuffer));

    gameStateBuffer = Buffer::createDynamicDraw("Game state buffer", sizeof(GameStateBuffer));
    if (!gameStateBuffer)
        return false;

    debugText = CCLabelBMFont::create("", "chatFont.fnt");
    debugTextOutline1 = CCLabelBMFont::create("", "chatFont.fnt");
    debugTextOutline2 = CCLabelBMFont::create("", "chatFont.fnt");
    if (!debugText || !debugTextOutline1 || !debugTextOutline2) return false;

    debugTextOutline1->setColor({0, 0, 0});
    debugTextOutline1->setOpacity(200);
    debugTextOutline2->setColor({0, 0, 0});
    debugTextOutline2->setOpacity(200);

    debugText->setAnchorPoint(CCPoint(0, 1));
    debugText->setPosition(1, CCDirector::get()->getWinSize().height - 8);
    debugText->setScale(0.5);
    layer->addChild(debugText, 1000);

    debugTextOutline1->setAnchorPoint(CCPoint(0, 1));
    debugTextOutline1->setPosition(1 - 0.5, CCDirector::get()->getWinSize().height - 8 - 0.5);
    debugTextOutline1->setScale(0.5);
    layer->addChild(debugTextOutline1, 999);

    debugTextOutline2->setAnchorPoint(CCPoint(0, 1));
    debugTextOutline2->setPosition(1 + 0.5, CCDirector::get()->getWinSize().height - 8 + 0.5);
    debugTextOutline2->setScale(0.5);
    layer->addChild(debugTextOutline2, 999);

    /*
    if (useOptimizations()) {
        // I hope this doesn't cause any issues
        for (auto& group : layer->m_optimizedGroups) {
            if (group)
                group->release();
            group = nullptr;
        }
    }
    */

    generateBatchNodes();
    setZOrder(-99999);
    setEnabled(true);

    rendererStartTime = getTime();

    auto finishNode = new FinishNode(this);
    finishNode->autorelease();
    layer->m_objectLayer->addChild(finishNode, 99999);

    reset();

    // Native group tables are still complete here. Publish classification
    // before rebuilding/filtering them, and finish this before the native
    // reset can execute its first movement/rotation command.
    classifyNativeObjectUpdates(objects);
    layer->optimizeMoveGroups();

    log::info("Renderer initialized");
    return true;
}

void Renderer::classifyNativeObjectUpdates(const std::vector<GameObject*>& objects) {
    skippableNativeObjects.clear();
    std::unordered_set<GameObject*> groupAnchors;
    for (int id = 1; id <= groupManager.getMaxGroupId(); ++id)
        if (auto mainObject = layer->tryGetMainObject(id)) groupAnchors.insert(mainObject);

    for (auto object : objects) {
        // Rendering coverage alone does not mean native updates are redundant.
        // Interactive objects, group anchors and dynamic visuals keep their
        // original bookkeeping, including their section membership.
        if (!object || object->m_objectType != GameObjectType::Decoration ||
            !objectSRBIndicies.contains(object) || groupAnchors.contains(object) ||
            object->isTrigger() || object->m_isHide || object->m_isInvisibleBlock ||
            object->m_hasAnimatedChild || object->getHasSyncedAnimation() ||
            object->usesSpecialAnimation() || object->usesFreezeAnimation() ||
            object->m_hasAreaParent || object->m_hasGroupParent ||
            object->m_areaOpacityRelated || object->m_isUIObject ||
            object->m_colorGroupCount > 0 || object->m_opacityGroupCount > 0 ||
            object->m_hasParticles || object->m_usesAudioScale ||
            object->getParentMode() == static_cast<int>(SpriteSheet::SPECIAL) ||
            object->getParentMode() == static_cast<int>(SpriteSheet::TEXT) ||
            typeinfo_cast<TextGameObject*>(object)) continue;
        if (auto enhanced = typeinfo_cast<EnhancedGameObject*>(object);
            enhanced && (enhanced->m_frames > 1 || enhanced->m_animateOnTrigger ||
                         enhanced->m_hasCustomAnimation || enhanced->m_usesSpecialAnimation)) continue;

        bool covered = true, hasSprite = false;
        ObjectUtils::unpackObjectIntoSprites(object, [&](const UnpackedSprite& unpacked) {
            const auto crop = unpacked.sprite->getTextureRect();
            if (crop.size.width <= 0 || crop.size.height <= 0 ||
                (unpacked.type == SpriteType::GLOW && object->m_hasNoGlow)) return;
            hasSprite = true;
            const auto sheet = ObjectUtils::getSpritesheetOfObject(object, unpacked.type);
            if (sheet == SpriteSheet::SPECIAL || sheet == SpriteSheet::TEXT) { covered = false; return; }
            const auto channel = ObjectUtils::getSpriteColorChannel(unpacked.type, object, unpacked.sprite);
            const LayerKey key{object->getObjectZLayer(), sheet, isColorChannelBlending(channel)};
            auto nativeBatch = getSpriteBatchNodeWithLayerId(key);
            if (!nativeBatch || !replacedBatches.contains(nativeBatch)) covered = false;
        });
        if (covered && hasSprite) skippableNativeObjects.insert(object);
    }
    log::info("Chlorium can avoid native group updates for {} static decorations", skippableNativeObjects.size());
}

void Renderer::generateBatchNodes() {
    for (const auto& id : objectBatch.getUsedLayerIds()) {
        auto batchNode = ObjectBatchNode::create(this, id);
        if (batchNode) {
            auto node = getSpriteBatchNodeWithLayerId(id);

            if (!node)
                continue;

            layer->m_objectLayer->addChild(batchNode, node->getZOrder());
            batchNodes.push_back(batchNode);
            if (replacedBatches.emplace(node, node->isVisible()).second)
                retainedNativeBatches.emplace_back(node);
        }
    }
}

void Renderer::terminate() {
    // Roll back partial initialization without leaving hidden native batches.
    for (const auto& [node, visible] : replacedBatches) node->setVisible(visible);
    // Nodes belong to the scene; do not mutate its children list from a child
    // destructor while the scene itself may be releasing that list.
    if (currentRenderer == this) setRendererProfiling(false);

    if (shader)
        Shader::destroy(shader);
    shader = nullptr;

    if (basicShader)
        Shader::destroy(basicShader);
    basicShader = nullptr;
    if (cullingComputeShader) Shader::destroy(cullingComputeShader);
    cullingComputeShader = nullptr;

    if (colorChannelBufferObject) {
        colorChannelBufferObject->unmap();
        Buffer::destroy(colorChannelBufferObject);
    }

    if (srbBuffer)
        Buffer::destroy(srbBuffer);

    if (gameStateBuffer)
        Buffer::destroy(gameStateBuffer);

    if (currentRenderer == this) currentRenderer = nullptr;
    log::info("Renderer terminated");
}

void Renderer::prepareShaderUniforms() {
    kmMat4 matrixP;
	kmMat4 matrixMV;
	kmMat4 matrixMVP;
	
	kmGLGetMatrix(KM_GL_PROJECTION, &matrixP);
	kmGLGetMatrix(KM_GL_MODELVIEW, &matrixMV);
	
	kmMat4Multiply(&matrixMVP, &matrixP, &matrixMV);

    // shader->setTextureArray("u_spriteSheets", (i32)SpriteSheet::COUNT, spriteSheets);

    static_assert(sizeof(gameState.u_mvp) == sizeof(matrixMVP));
    std::memcpy(&gameState.u_mvp, &matrixMVP, sizeof(matrixMVP));
    gameState.u_timer = gameTimer;
    gameState.u_cameraViewSize = glm::vec2(layer->m_cameraWidth, layer->m_cameraHeight);
    auto winsize = CCDirector::get()->getWinSize();
    gameState.u_winSize = glm::vec2(winsize.width, winsize.height);
    gameState.u_screenRight = CCDirector::get()->getScreenRight();
    gameState.u_cameraUnzoomedX = layer->m_cameraUnzoomedX;
    ccColor3B specialLightBG = GameToolbox::transformColor(layer->m_effectManager->activeColorForIndex(COLOR_CHANNEL_BG), 0.0, -0.2, 0.2);
    gameState.u_specialLightBGColor = ccColor3BToGLM(specialLightBG);

    auto winSize = CCDirector::get()->getWinSize();

    vec2 cameraBottomLeft  = ccPointToGLM(layer->m_objectLayer->convertToNodeSpace({0, 0}));
    vec2 cameraBottomRight = ccPointToGLM(layer->m_objectLayer->convertToNodeSpace({winSize.width, 0}));
    vec2 cameraTopLeft     = ccPointToGLM(layer->m_objectLayer->convertToNodeSpace({0, winSize.height}));

    vec2 cameraRight = cameraBottomRight - cameraBottomLeft;
    vec2 cameraUp    = cameraTopLeft     - cameraBottomLeft;

    gameState.u_cameraPosition = cameraBottomLeft;

    const float cameraWidth = glm::length(cameraRight);
    const float cameraHeight = glm::length(cameraUp);
    gameState.u_cameraRight = std::isfinite(cameraWidth) && cameraWidth > 0.0f
        ? cameraRight / cameraWidth : glm::vec2{};
    gameState.u_cameraUp = std::isfinite(cameraHeight) && cameraHeight > 0.0f
        ? cameraUp / cameraHeight : glm::vec2{};
    gameState.u_cameraSize = {cameraWidth, cameraHeight};

    gameState.u_gameStateFlags = 0;
    if (layer->m_player1 && layer->m_player1->m_isDead)
        gameState.u_gameStateFlags |= GAME_STATE_IS_PLAYER_DEAD;

    if (layer->m_skipAudioStep)
        gameState.u_audioScale = FMODAudioEngine::sharedEngine()->getMeteringValue();
    else if (layer->m_audioEffectsLayer)
        gameState.u_audioScale = layer->m_audioEffectsLayer->m_audioScale;
    else
        gameState.u_audioScale = 0.5f;
    
    if (layer->m_isSilent || (layer->m_isPracticeMode && !layer->m_practiceMusicSync))
        gameState.u_audioScale = 0.5;

    gameStateBuffer->write(&gameState, sizeof(gameState));
}

void Renderer::prepareColorChannelBuffer() {
    auto timer = BProfiler::start("Generate color channel buffer");
    PROFILER_PUSH("Renderer::prepareColorChannelBuffer");

    auto effectManager = layer->m_effectManager;

    for (usize i = 0; i < effectManager->m_colorActionSpriteVector.size(); i++) {
        auto sprite = effectManager->m_colorActionSpriteVector[i];
        if (sprite == nullptr)
            continue;

        auto id = sprite->m_colorID;
        if (id < 0 || id >= std::size(colorChannelBuffer->u_channelColors)) continue;

        auto& channelColor = colorChannelBuffer->u_channelColors[id];
        channelColor.r = sprite->m_color.r;
        channelColor.g = sprite->m_color.g;
        channelColor.b = sprite->m_color.b;
        channelColor.a = (u8)sprite->m_opacity;
    }

    colorChannelBuffer->u_channelColors[COLOR_CHANNEL_BLACK] = { 0, 0, 0, 255 };
    colorChannelBufferObject->flush(0, colorChannelBufferObject->getSize());
    
    PROFILER_POP();
    timer.end();
}

static ShaderHSV convertToShaderHSV(const ccHSVValue& hsv) {
    return {
        hsv.h, hsv.s, hsv.v,
        (hsv.absoluteSaturation ? HSV_SAT_ADD : 0u) |
        (hsv.absoluteBrightness ? HSV_VAL_ADD : 0u)
    };
}

void Renderer::generateStaticRenderingBuffer(std::vector<GameObject*>& objects) {
    std::vector<ObjectInfo> objectInfos;
    objectInfos.resize(renderedGameObjectCount);

    usize index = 0;

    for (auto object : objects) {
        auto objectInfo = &objectInfos[index];

        objectInfo->startPosition = ccPointToGLM(object->m_startPosition);
        if (object->getHasRotateAction())
            objectInfo->rotationSpeed = ((EnhancedGameObject*)object)->m_rotationDelta; // 'rotationDelta' is incorrect naming
        else
            objectInfo->rotationSpeed = 0.0;

        objectInfo->flags = 0;
        if (object->m_usesAudioScale && !object->m_hasNoAudioScale) {
            objectInfo->flags |= OBJECT_FLAG_USES_AUDIO_SCALE;
        
            if (object->m_customAudioScale) {
                objectInfo->flags |= OBJECT_FLAG_CUSTOM_AUDIO_SCALE;
                objectInfo->audioScaleMin = object->m_minAudioScale;
                objectInfo->audioScaleMax = object->m_maxAudioScale;
            }
            
            if (typeinfo_cast<RingObject*>(object))
                objectInfo->flags |= OBJECT_FLAG_IS_ORB;
        }

        objectInfo->baseColorChannelId = object->m_activeMainColorID;
        objectInfo->detailColorChannelId = object->m_activeDetailColorID;

        if (object->m_baseColor && object->m_baseColor->m_usesHSV) {
            objectInfo->flags |= OBJECT_FLAG_HAS_BASE_HSV;
            objectInfo->baseHSV = convertToShaderHSV(object->m_baseColor->m_hsv);
        }

        if (object->m_detailColor && object->m_detailColor->m_usesHSV) {
            objectInfo->flags |= OBJECT_FLAG_HAS_DETAIL_HSV;
            objectInfo->detailHSV = convertToShaderHSV(object->m_detailColor->m_hsv);
        }

        objectInfo->opacity = (object->m_opacityMod2 > 0.0) ? object->m_opacityMod2 : 1.0;

        objectInfo->groupCombinationIndex = groupManager.getGroupCombinationIndexForObject(object);

        if (object->m_isInvisibleBlock) objectInfo->flags |= OBJECT_FLAG_IS_INVISIBLE_BLOCK;
        if (object->m_customGlowColor)  objectInfo->flags |= OBJECT_FLAG_SPECIAL_GLOW_COLOR;
        if (object->m_glowColorIsLBG) objectInfo->flags |= OBJECT_FLAG_IS_GLOW_COLOR_LBG;
        if (!object->m_canRotateFree)
            objectInfo->flags |= OBJECT_FLAG_IS_STATIC_OBJECT;
        objectInfo->fadeMargin = object->m_fadeMargin;

        objectSRBIndicies[object] = index;
        index++;
    }

    srbBuffer = Buffer::createStaticDraw("Static rendering buffer", objectInfos.data(), objectInfos.size() * sizeof(ObjectInfo));
};

static glm::vec2 linePoint = { 100, 200 };
static float     lineAngle = 0;

static ConvexPolygon polygon;

static isize triangleIndex = 0;

static void drawLine(Renderer* ren, const Line& line, const glm::vec4& color) {
    glm::vec2 npp = { line.normal.y, -line.normal.x };

    glm::vec2 p1 = line.firstPoint() + npp *  10000.f;
    glm::vec2 p2 = line.firstPoint() + npp * -10000.f;

    ren->drawLine(p1, p2, color);
}

static void drawCross(Renderer* ren, const glm::vec2& point, const glm::vec4& color) {
    ren->drawLine(point + glm::vec2(-5, -5), point + glm::vec2( 5,  5), color);
    ren->drawLine(point + glm::vec2(-5,  5), point + glm::vec2( 5, -5), color);
}

float CAMERA_CULL_RECT_SCALE = 1.3;

void Renderer::draw() {
    PROFILER_PUSH("Renderer::draw");
    

    if (fetchBlendingStates()) objectBatch.refreshBlendingStates();

    u64 prevTime = getTime();
    storeGLStates();
    prepareShaderUniforms();
    if (!isPaused() || !preparedFrame) {
        prepareColorChannelBuffer();
    
        // auto timer = BProfiler::start("Generate group state buffer");
        groupManager.prepareGroupStateBuffer();
        // timer.end();

        // glm::vec2 bl = view.bottomLeft;
        // glm::vec2 br = view.bottomLeft + view.rightVector;
        // glm::vec2 tl = view.bottomLeft + view.upVector;
        // glm::vec2 tr = view.bottomLeft + view.rightVector + view.upVector;

        // drawLine(bl, br, glm::vec4(1, 1, 0, 1));
        // drawLine(br, tr, glm::vec4(1, 1, 0, 1));
        // drawLine(tr, tl, glm::vec4(1, 1, 0, 1));
        // drawLine(tl, bl, glm::vec4(1, 1, 0, 1));

        objectBatch.predraw();
        preparedFrame = true;
    }

    renderingTimer.start(debugTextEnabled);

    /*
    glm::vec2 normal { glm::cos(glm::radians(lineAngle)), glm::sin(glm::radians(lineAngle)) };

    drawLine(linePoint, linePoint + normal * 10.f, glm::vec4(0, 1, 0, 1));

    Line line { linePoint, normal };

    ::drawLine(this, line, glm::vec4(1, 0, 0, 1));

    for (const auto& mline : polygon.getLines()) {
        ::drawLine(this, mline, glm::vec4(0.5, 0.5, 0.5, 1));
    
        auto inter = line.intersectionWith(mline);
        if (inter)
            drawCross(this, inter.value(), glm::vec4(0, 0, 1, 1));
    }

    std::vector<glm::vec2> verticies;

    polygon.triangulate([&](const auto& p1, const auto& p2, const auto& p3) {
        verticies.push_back(p1);
        verticies.push_back(p2);
        verticies.push_back(p3);
    });

    if (triangleIndex >= (verticies.size() / 3))
        triangleIndex = 0;

    if (verticies.size() != 0) {
        auto p1 = verticies[triangleIndex * 3 + 0];
        auto p2 = verticies[triangleIndex * 3 + 1];
        auto p3 = verticies[triangleIndex * 3 + 2];

        drawLine(p1, p2, glm::vec4(0, 1, 0, 1));
        drawLine(p2, p3, glm::vec4(0, 1, 0, 1));
        drawLine(p3, p1, glm::vec4(0, 1, 0, 1));
    }
    */

    restoreGLStates();

    PROFILER_POP();
}

void Renderer::finish() {
    if (!enabled) return;
    renderingTimer.stop();

    renderTime = renderingTimer.getTime();

    if (debugText->isVisible())
        updateDebugText();
}

void Renderer::updateDebugText() {
    if (!debugText || !debugTextOutline1 || !debugTextOutline2) return;
    const bool visible = debugTextEnabled || !enabled;
    debugText->setVisible(visible);
    debugTextOutline1->setVisible(visible);
    debugTextOutline2->setVisible(visible);
    if (!visible) return;
    std::string text = "";


    if (!enabled) {
        text += "Chlorium renderer is disabled\n";
        if (canEnableDisableIngame()) text += "Press F8 to enable\n";
    } else {
        if (debugTextEnabled) {
            auto screenSize = CCDirector::get()->getWinSizeInPixels();

            text += fmt::format("Chlorium renderer {}\n", Mod::get()->getVersion().toVString());
            text += fmt::format("OpenGL {}\n", (const char*)glGetString(GL_VERSION));
            text += fmt::format("{}\n", (const char*)glGetString(GL_RENDERER));
            text += fmt::format("Window: {}x{}\n", screenSize.width, screenSize.height);
            text += fmt::format("GPU time: {}ms\n", (double)renderTime / 1000000.0);
            text += fmt::format("Vertex buffer size: {}\n", byteSizeToString(objectBatch.getVertexBufferSize()));
            text += fmt::format("Sprites on screen: {}\n", spritesOnScreen);
            text += fmt::format("Static rendering buffer size: {}\n", byteSizeToString(srbBuffer->getSize()));
            text += fmt::format("Group state buffer size: {}\n", byteSizeToString(groupManager.getGroupStateBufferSize()));
            text += fmt::format("Color channel buffer size: {}\n", byteSizeToString(sizeof(ColorChannelBuffer)));
            text += "\n";
            text += BProfiler::toString();
            text += "\n";
            text += "Press F3 to hide this screen\n";
        }
    }

    debugText->setString(text.c_str());
    debugTextOutline1->setString(text.c_str());
    debugTextOutline2->setString(text.c_str());
}

void Renderer::bindCullingComputeShaderBuffers() {
    gameStateBuffer->bindAsUniformBuffer(GAME_STATE_BUFFER_BINDING);
    srbBuffer->bindAsStorageBuffer(OBJECT_INFO_BUFFER_BINDING);
    colorChannelBufferObject->bindAsStorageBuffer(COLOR_CHANNEL_BUFFER_BINDING);
    groupManager.bindGroupStateBuffer();
}

void Renderer::bindDrawingShaderBuffers() {
    // Vertex/fragment stages consume game state and the compute output only.
    // Compute inputs are rebound once in predraw, never for each texture batch.
    gameStateBuffer->bindAsUniformBuffer(GAME_STATE_BUFFER_BINDING);
}

bool Renderer::fetchBlendingStates() {
    bool changed = false;
    const auto& actions = layer->m_effectManager->m_colorActionVector;
    for (size_t i = 0; i < colorChannelsIsBlending.size(); i++) {
        const auto action = i < actions.size() ? actions[i] : nullptr;
        const bool blending = i == COLOR_CHANNEL_P1 || i == COLOR_CHANNEL_P2 ||
                              i == COLOR_CHANNEL_LBG || (action && action->m_blending);
        changed |= colorChannelsIsBlending[i] != blending;
        colorChannelsIsBlending[i] = blending;
    }
    return changed;
}

Shader* Renderer::prepareDraw() {
    PROFILER_PUSH("Renderer::prepareDraw");
    storeGLStates(GLStateScope::SpriteDraw);
    shader->use();
    PROFILER_POP();
    return shader;
}

void Renderer::finishDraw() {
    restoreGLStates();
}

void Renderer::update(float dt) {
    gameTimer = layer->m_gameState.m_totalTime;

    if (debugText->isVisible())
        updateDebugText();

    auto& gstate = layer->m_gameState;
    cameraCenterPos = ccPointToGLM(gstate.m_cameraPosition2 + CCPoint(layer->m_cameraWidth * 0.5, layer->m_cameraHeight * 0.5));
}

CCSpriteBatchNode* Renderer::getSpriteBatchNodeWithLayerId(LayerKey id) {
    CCNode* node = layer->parentForZLayer((i32)id.zlayer, id.blending, (i32)id.spriteSheet, false);
    
    if (!node || layer->m_batchNodes->indexOfObject(node) == UINT_MAX)
        return nullptr;

    return typeinfo_cast<CCSpriteBatchNode*>(node);
}

Ref<Renderer> Renderer::create(PlayLayer* layer, CCArray* objects) {
#ifndef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
    // The static Bismuth replacement does not yet represent every GD 2.2
    // visual effect. Normal builds keep the original drawing pipeline;
    // native atlas optimizations operate independently without changing it.
    return nullptr;
#endif
    if (!Mod::get()->getSettingValue<bool>("enabled")) return nullptr;
#ifdef CHLORIUM_RUNTIME_AUDIT
    if (chloriumAuditUseNative()) return nullptr;
#endif
    // Capture/restore below uses modern buffer/VAO APIs. Check support before
    // touching them so native fallback also stays safe on older contexts.
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 4 || (major == 4 && minor < 6) || !ObjectBatch::isSupported()) {
        log::warn("Chlorium requires OpenGL 4.6 with suitable storage buffers; using native rendering");
        return nullptr;
    }
    const auto savedState = GLStateSnapshot::capture();
    auto ren = new Renderer;

    bool initialized = false;
    try { initialized = ren->init(layer, objects); }
    catch (const std::exception& error) { log::error("Chlorium initialization failed: {}", error.what()); }
    if (initialized) {
        ren->autorelease();
        // Native font/texture setup can update Cocos' binding cache. Reconcile
        // only the touched bindings; ccGLInvalidateStateCache also calls
        // kmGLFreeAll and destroys the live projection/model-view stacks.
        cocos2d::ccGLUseProgram(savedState.program);
        cocos2d::ccGLBindVAO(savedState.vao);
        cocos2d::ccGLBlendFunc(savedState.blendSrcRGB, savedState.blendDstRGB);
        for (unsigned unit = 0; unit < savedState.textures.size(); ++unit)
            cocos2d::ccGLBindTexture2DN(unit, savedState.textures[unit]);
        savedState.restore();
        return ren;
    }
    
    delete ren;
    cocos2d::ccGLUseProgram(savedState.program);
    cocos2d::ccGLBindVAO(savedState.vao);
    cocos2d::ccGLBlendFunc(savedState.blendSrcRGB, savedState.blendDstRGB);
    for (unsigned unit = 0; unit < savedState.textures.size(); ++unit)
        cocos2d::ccGLBindTexture2DN(unit, savedState.textures[unit]);
    savedState.restore();
    return nullptr;
}

Ref<Renderer> Renderer::get() {
    return currentRenderer;
}

bool Renderer::useOptimizations() {
    if (!enabled || canEnableDisableIngame())
        return false;

    return true;
}

void Renderer::setEnabled(bool enabled) {
    this->enabled = enabled;
    this->setVisible(enabled);
    for (auto node : batchNodes)
        node->setVisible(enabled);
    for (const auto& [batch, originallyVisible] : replacedBatches)
        batch->setVisible(enabled ? false : originallyVisible);
    updateDebugText();
}

void Renderer::reset() {
    groupManager.resetGroupStates();
    preparedFrame = false;
    gameTimer = layer->m_gameState.m_totalTime;
}

void Renderer::onEnter() {
    CCNode::onEnter();
    // Shader/camera layers may temporarily detach and reattach the object
    // layer. onExit is not a destruction notification: resume only this
    // renderer's previous state when its owning level enters again.
    if (!currentRenderer || currentRenderer == this) {
        currentRenderer = this;
        if (hasExited) {
            setEnabled(resumeEnabledOnEnter);
            preparedFrame = false;
            hasExited = false;
        }
    }
}

void Renderer::onExit() {
    resumeEnabledOnEnter = enabled;
    hasExited = true;
    setEnabled(false);
    if (currentRenderer == this) {
        currentRenderer = nullptr;
        setRendererProfiling(false);
    }
    CCNode::onExit();
}

void Renderer::toggleDebugText() {
    debugTextEnabled = !debugTextEnabled;
    setRendererProfiling(debugTextEnabled);
    updateDebugText();
}

void Renderer::drawLine(const glm::vec2& p1, const glm::vec2& p2, const glm::vec4& color) {
    if (!basicShader)
        basicShader = Shader::create("basic.vert", "basic.frag");
    if (!basicShader)
        return;

    glm::vec2 data[2] = { p1, p2 };
    Buffer* buffer = Buffer::createStaticDraw("Line buffer", &data, sizeof(data));
    if (!buffer) return;
    buffer->bindAs(GL_ARRAY_BUFFER);

    u32 vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    kmMat4 matrixP;
	kmMat4 matrixMV;
	kmMat4 matrixMVP;
	kmGLGetMatrix(KM_GL_PROJECTION, &matrixP);
	kmGLGetMatrix(KM_GL_MODELVIEW, &matrixMV);
	kmMat4Multiply(&matrixMVP, &matrixP, &matrixMV);

    basicShader->use();
    basicShader->setMatrix4("u_mvp", matrixMVP.mat);
    basicShader->setVec4("u_color", color);

    glDrawArrays(GL_LINES, 0, 2);

    glDeleteVertexArrays(1, &vao);
    Buffer::destroy(buffer);
}

DECLARE_HOTKEY(KEY_F3, {
    if (currentRenderer)
        currentRenderer->toggleDebugText();
})

DECLARE_HOTKEY(KEY_F8, {
    if (currentRenderer && currentRenderer->canEnableDisableIngame())
        currentRenderer->setEnabled(!currentRenderer->isEnabled());
})


DECLARE_HOTKEY(KEY_F6, {
    BProfiler::useAverages(!BProfiler::isUsingAverages()); 
});

static std::vector<GLStateSnapshot> savedGLStates;

void storeGLStates() {
    storeGLStates(GLStateScope::Full);
}

void storeGLStates(GLStateScope scope) {
    savedGLStates.push_back(GLStateSnapshot::capture(scope));
}

void restoreGLStates() {
    assert(!savedGLStates.empty());
    if (savedGLStates.empty()) return;
    savedGLStates.back().restore();
    savedGLStates.pop_back();
}
