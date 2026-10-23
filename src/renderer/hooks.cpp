#include "Geode/cocos/CCDirector.h"
#include "Renderer.hpp"
#include <decomp/PlayLayer.hpp>
#include <decomp/GJBaseGameLayer.hpp>
#include <BProfiler.hpp>

using namespace geode::prelude;

static bool newPlayLayer = false;
// static CCArray* allGameObjects = nullptr;

#include <Geode/modify/PlayLayer.hpp>
class $modify(RendererPlayLayer, PlayLayer) {
    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        auto renderer = Renderer::get();
        if (renderer && renderer->getPlayLayer() == this) renderer->storeCheckpoint(checkpoint);
    }
    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        auto renderer = Renderer::get();
        if (renderer && renderer->getPlayLayer() == this) renderer->loadCheckpoint(checkpoint);
    }
    void removeAllCheckpoints() {
        PlayLayer::removeAllCheckpoints();
        auto renderer = Renderer::get();
        if (renderer && renderer->getPlayLayer() == this) renderer->clearCheckpoints();
    }
    void setupHasCompleted() {
        log::warn("Chlorium setup reached with {} objects", m_objects ? m_objects->count() : 0);
        newPlayLayer = true;
        PlayLayer::setupHasCompleted();
        newPlayLayer = false;
    }

    bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects) {
        // if (!allGameObjects) {
        //     allGameObjects = CCArray::create();
        //     allGameObjects->retain();
        // }
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }

    // This gets called from inside PlayLayer::setupHasCompleted()
    void resetLevel() {
        if (newPlayLayer) {
            auto batchLayer = this->m_objectLayer;
            if (!batchLayer) {
                log::error("failed to attach renderer: batch layer not found");
                PlayLayer::resetLevel();
                return;
            }

            auto renderer = Renderer::create(this, m_objects);
            if (renderer)
                batchLayer->addChild(renderer, -100000);

            // for (auto obj : CCArrayExt<GameObject, false>(allGameObjects))
            //     obj->release();
            // allGameObjects->removeAllObjects();
        }

        auto renderer = Renderer::get();
        if (renderer)
            renderer->reset();
        // Native reset executes initial toggle/spawn actions. Do not clear
        // their renderer state afterwards, or initially hidden groups reappear.
        PlayLayer::resetLevel();
    }

    void updateVisibility(float dt) {
        auto timer = BProfiler::start("PlayLayer::updateVisibility");
        // Native visibility also advances enter/area effects, animated objects
        // and text. Keep those updates independent of GPU group transforms.
        PlayLayer::updateVisibility(dt);
        timer.end();
    }
};

#include <Geode/modify/GameObject.hpp>
class $modify(MyGameObject, GameObject) {
    /*
    static GameObject* objectFromVector(
        gd::vector<gd::string>& propValues,
        gd::vector<void*>& propIsPresent,
        GJBaseGameLayer* gameLayer,
        bool lowDetail
    ) {
        GameObject* ret = GameObject::objectFromVector(propValues, propIsPresent, gameLayer, lowDetail);
        if (!GameManager::get()->m_playLayer)
            return ret;

        allGameObjects->addObject(ret);
        ret->retain();

        if (ret->m_objectType == GameObjectType::Decoration)
            return ret;
        
        return ret;
    }
    */
};

static void removeGPUUpdatedObjects(CCArray* array, const Renderer& renderer) {
    if (!array) return;
    for (u32 i = 0; i < array->count();) {
        auto object = (GameObject*)array->objectAtIndex(i);
        if (renderer.canSkipNativeObjectUpdates(object))
            array->removeObjectAtIndex(i);
        else
            i++;
    }
}

#include <Geode/modify/GJBaseGameLayer.hpp>
class $modify(RendererGJBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto timer = BProfiler::start("GJBaseGameLayer::update");
        GJBaseGameLayer::update(dt);
        timer.end();

        auto renderer = Renderer::get();
        if (renderer && static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) == this)
            renderer->update(dt);
    }

    void processMoveActions() {
        auto renderer = Renderer::get();
        if (!renderer || static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) != this) {
            GJBaseGameLayer::processMoveActions();
            return;
        }

        m_effectManager->processMoveCalculations();
        for (auto node : m_effectManager->m_unkVector6c0) {
            if (node->m_unk0d0)
                continue;

            int groupId = node->getTag();

            renderer->getGroupManager().moveGroup(groupId, node->m_unk038, node->m_unk040);

            CCArray* objects = getStaticGroup(groupId);
            if (objects)
                moveObjects(objects, node->m_unk038, node->m_unk040, 0);

            // optimizeMoveGroups removes only decorations fully owned by the
            // GPU. Text, physical objects and native fallbacks remain here.
            objects = getOptimizedGroup(groupId);
            if (objects)
                moveObjects(objects, node->m_unk090, node->m_unk098, 0);
        }
    }

    void siphon_processRotationActions() {
        auto renderer = Renderer::get();
        if (!renderer || static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) != this) {
            GJBaseGameLayer::processRotationActions();
            return;
        }

        auto eman = m_effectManager;
        for (auto cmdObj : eman->m_unkVector5b0) {
            if (/* cmdObj->m_unkInt204 != m_gameState.m_unkUint2 ||*/ cmdObj->m_someInterpValue1RelatedFalse)
                continue;

            i32 targetId = cmdObj->m_targetGroupID;
            i32 centerId = cmdObj->m_centerGroupID;

            auto mainObject = tryGetMainObject(centerId);
            auto staticGroup = getStaticGroup(targetId);

            float rotation;
            if (staticGroup && staticGroup->count() != 0)
                rotation = cmdObj->m_someInterpValue1RelatedOne - cmdObj->m_someInterpValue1RelatedZero;
            else
                rotation = cmdObj->m_someInterpValue2RelatedOne - cmdObj->m_someInterpValue2RelatedZero;

            if (rotation == 0.0 && !cmdObj->m_finishRelated)
                continue;

            // Idk what the hell this does but sure
            if (eman->m_unkMap770.find({ targetId, centerId }) != eman->m_unkMap770.end()) {
                for (auto obj : eman->m_unkMap770[{ targetId, centerId }]) {
                    if (obj == cmdObj || obj->m_someInterpValue1RelatedFalse)
                        continue;
                    if (staticGroup && staticGroup->count() != 0)
                        rotation += obj->m_someInterpValue1RelatedOne - obj->m_someInterpValue1RelatedZero;
                    else
                        rotation += obj->m_someInterpValue2RelatedOne - obj->m_someInterpValue2RelatedZero;
                }
            }

            if (mainObject == nullptr)
                renderer->getGroupManager().rotateGroup(targetId, rotation, cmdObj->m_lockObjectRotation);
            else {
                auto pos = mainObject->getUnmodifiedPosition();
                renderer->getGroupManager().rotateGroup(targetId, rotation, cmdObj->m_lockObjectRotation, ccPointToGLM(pos));
            }
        }

        GJBaseGameLayer::processRotationActions();
    }

    void processRotationActions() {
        auto renderer = Renderer::get();
        if (!renderer || static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) != this) {
            GJBaseGameLayer::processRotationActions();
            return;
        }

        if (renderer->useOptimizations())
            ((decomp_GJBaseGameLayer*)this)->optimized_processRotationActions();
        else
            siphon_processRotationActions();
    }

    void processFollowActions() {
        auto renderer = Renderer::get();
        if (!renderer || static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) != this) {
            GJBaseGameLayer::processFollowActions();
            return;
        }

        auto eman = m_effectManager;
        for (auto node : eman->m_unkVector6d8) {
            i32 targetGroupId = node->getTag();
            i32 followGroupId = node->m_unk074;

            auto mainObject = tryGetMainObject(followGroupId);
            if (mainObject == nullptr)
                continue;

            double moveX = 0.0, moveY = 0.0;
            if (mainObject->m_unk4C4 == m_gameState.m_commandIndex) {
                moveX = (mainObject->m_positionX - mainObject->m_lastPosition.x) * node->m_unk080; /* followXMod */
                moveY = (mainObject->m_positionY - mainObject->m_lastPosition.y) * node->m_unk088; /* followYMod */
            }

            renderer->getGroupManager().moveGroup(targetGroupId, moveX, moveY);
        }

        GJBaseGameLayer::processFollowActions();
    }

    void toggleGroup(int id, bool activate) {
        GJBaseGameLayer::toggleGroup(id, activate);
        auto renderer = Renderer::get();
        if (renderer && static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) == this)
            renderer->getGroupManager().toggleGroup(id, activate);
    }

    void optimizeMoveGroups() {
        GJBaseGameLayer::optimizeMoveGroups();

        auto renderer = Renderer::get();
        if (!renderer || static_cast<GJBaseGameLayer*>(renderer->getPlayLayer()) != this || !renderer->useOptimizations())
            return;

        // Capture both native partitions before filtering. This is also run
        // after Renderer finishes classifying the objects it fully owns.
        renderer->getGroupManager().fetchVanilaRotateGroups();
        for (auto array : m_optimizedGroups)
            removeGPUUpdatedObjects(array, *renderer.operator->());
        for (auto array : m_staticGroups)
            removeGPUUpdatedObjects(array, *renderer.operator->());
    }
};

#include <Geode/modify/CCDisplayLinkDirector.hpp>
class $modify(RendererCCDisplayLinkDirector, CCDisplayLinkDirector) {
    void mainLoop() {
        auto ren = Renderer::get();
        if (!ren) {
            CCDisplayLinkDirector::mainLoop();
            return;
        }

        auto timer = BProfiler::start("Frame time");
        CCDisplayLinkDirector::mainLoop();
        timer.end();

#ifdef CHLORIUM_RUNTIME_AUDIT
        if (!std::getenv("CHLORIUM_BENCHMARK_SECONDS")) BProfiler::frameEnd();
#else
        BProfiler::frameEnd();
#endif
    }
};

