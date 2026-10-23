#pragma once

#include <Geode/binding/GameObject.hpp>
#include <common.hpp>
#include "Geode/cocos/sprite_nodes/CCSpriteBatchNode.h"
#include "ObjectBatch.hpp"
#include "ObjectUtils.hpp"
#include "Shader.hpp"
#include <unordered_map>
#include <array>
#include <unordered_set>

#include "GroupManager.hpp"
#include "ObjectBatchNode.hpp"
#include "../../resources/shaders/headers/shared.h"

using namespace geode;

enum class GLStateScope;

class Renderer : public cocos2d::CCNode {
private:
    inline Renderer()
        : groupManager(*this), objectBatch(*this) {}
    ~Renderer() override;

    bool init(PlayLayer* layer, cocos2d::CCArray* objects);

    void generateBatchNodes();

    void terminate();

    void prepareShaderUniforms();

    void prepareColorChannelBuffer();

    void generateStaticRenderingBuffer(std::vector<GameObject*>& objects);
    void classifyNativeObjectUpdates(const std::vector<GameObject*>& objects);

    void draw() override;
    void onEnter() override;
    void onExit() override;

    void finish();

    void updateDebugText();

    void bindCullingComputeShaderBuffers();
    void bindDrawingShaderBuffers();

    bool fetchBlendingStates();

    friend class FinishNode;
    friend class ObjectBatch;

protected:
    Shader* prepareDraw();

    void finishDraw();

    friend class GroupManager;
    friend class ObjectBatchNode;

public:
    void storeCheckpoint(CheckpointObject* checkpoint) {
        if (!checkpoint) return;
        // Keep only checkpoints still owned by this PlayLayer. Pointer keys
        // are never dereferenced and are refreshed whenever a slot is reused.
        std::erase_if(checkpoints, [&](const auto& item) {
            return !layer->m_checkpointArray || !layer->m_checkpointArray->containsObject(item.first);
        });
        checkpoints.insert_or_assign(checkpoint, groupManager.saveState());
    }
    void loadCheckpoint(CheckpointObject* checkpoint) {
        auto it = checkpoints.find(checkpoint);
        if (it != checkpoints.end()) groupManager.restoreState(it->second);
        preparedFrame = false;
    }
    void clearCheckpoints() { checkpoints.clear(); }
    void update(float dt) override;

    cocos2d::CCSpriteBatchNode* getSpriteBatchNodeWithLayerId(LayerKey id);

    inline bool isColorChannelBlending(i32 channel) {
        return channel >= 0 && channel < colorChannelsIsBlending.size() && colorChannelsIsBlending[channel];
    }

    inline cocos2d::CCTexture2D* getSpriteSheetTexture(SpriteSheet sheet) {
        if ((i32)sheet < 0 || (i32)sheet >= (i32)SpriteSheet::COUNT)
            return nullptr;
        return spriteSheets[(i32)sheet];
    }
    bool isCompatibleAtlas(SpriteSheet sheet, cocos2d::CCTexture2D* texture);

    inline PlayLayer* getPlayLayer() {
        return layer;
    }

    inline usize getObjectSRBIndex(GameObject* object) {
        return objectSRBIndicies.at(object);
    }

    inline bool isEnabled() { return enabled; }


    void toggleDebugText();


    inline bool canEnableDisableIngame() const { return ingameEnableDisable; }
    inline bool needsNativeVisibility() const { return nativeTextPresent; }

    inline bool isPaused() {
        return layer->m_isPaused;
    }

    inline bool isUseIndexCulling() const { return useIndexCulling; }

    inline ObjectBatch& getObjectBatch() { return objectBatch; }

    inline Shader* getCullingComputeShader() const {
        return cullingComputeShader;
    }

    bool useOptimizations();
    bool canSkipNativeObjectUpdates(GameObject* object) const {
        return enabled && skippableNativeObjects.contains(object);
    }

    void setEnabled(bool enabled);

    inline GroupManager& getGroupManager() { return groupManager; }
    
    void reset();

    void drawLine(const glm::vec2& p1, const glm::vec2& p2, const glm::vec4& color);

public:
    static Ref<Renderer> create(PlayLayer* layer, cocos2d::CCArray* objects);

    /**
     * Returns the current renderer. If the game
     * is not in a PlayLayer. This returns nullptr.
     */
    static Ref<Renderer> get();

private:
    PlayLayer* layer = nullptr;
    std::unordered_map<CheckpointObject*, GroupManager::Snapshot> checkpoints;

    bool enabled = false;
    bool resumeEnabledOnEnter = false;
    bool hasExited = false;
    bool debugTextEnabled = false;
    bool ingameEnableDisable = false;
    bool nativeTextPresent = false;

    bool useIndexCulling = false;

    u64 rendererStartTime = 0;

    usize spritesOnScreen = 0;

    std::unordered_map<GameObject*, usize> objectSRBIndicies;
    std::unordered_set<GameObject*> skippableNativeObjects;

    bool preparedFrame = false;

    glm::vec2 cameraCenterPos;

    std::vector<Ref<ObjectBatchNode>> batchNodes;
    std::map<cocos2d::CCNode*, bool> replacedBatches;
    std::vector<Ref<cocos2d::CCNode>> retainedNativeBatches;

    cocos2d::CCTexture2D* spriteSheets[(i32)SpriteSheet::COUNT] = { nullptr };
    std::map<std::pair<cocos2d::CCTexture2D*, cocos2d::CCTexture2D*>, bool> textureEquivalences;

    std::array<bool, 1101> colorChannelsIsBlending;

    Ref<cocos2d::CCLabelBMFont> debugText;
    Ref<cocos2d::CCLabelBMFont> debugTextOutline1;
    Ref<cocos2d::CCLabelBMFont> debugTextOutline2;

    GPUTimer renderingTimer { "Rendering time" };

    GroupManager groupManager;

    ObjectBatch objectBatch;
    Shader* shader = nullptr;
    Shader* basicShader = nullptr;
    Shader* cullingComputeShader = nullptr;

    ColorChannelBuffer* colorChannelBuffer = nullptr;
    Buffer* colorChannelBufferObject = nullptr;

    Buffer* srbBuffer = nullptr;

    GameStateBuffer gameState{};
    Buffer* gameStateBuffer = nullptr;


    float gameTimer = 0.0;

    i64 groupStateCount = 0;
    i64 renderedGameObjectCount = 0;
    i64 renderTime = 0;
};

void storeGLStates();
void storeGLStates(GLStateScope scope);

void restoreGLStates();
