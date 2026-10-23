#include "GJBaseGameLayer.hpp"
#include "../renderer/Renderer.hpp"
#include "../profiler.hpp"

using namespace geode::prelude;

void decomp_GJBaseGameLayer::moveObjects(cocos2d::CCArray* objects, double moveX, double moveY, bool unused) {
    if (objects == nullptr)
        return;

    for (auto object : CCArrayExt<GameObject*>(objects)) {
        if (!object->m_isDecoration2 && object->m_unk4C4 != m_gameState.m_commandIndex) {
            object->m_unk4C4 = m_gameState.m_commandIndex;

            object->m_lastPosition.x = object->m_positionX;
            object->m_lastPosition.y = object->m_positionY;
            object->dirtifyObjectRect();
        }

        if (moveX != 0.0 && !object->m_tempOffsetXRelated)
            object->m_positionX += moveX;
        if (moveY != 0.0)
            object->m_positionY += moveY;
            
        object->dirtifyObjectPos();
        updateObjectSection(object);
    }

    updateDisabledObjectsLastPos(objects);
}

void decomp_GJBaseGameLayer::processMoveActions() {
    m_effectManager->processMoveCalculations();
    for (auto node : m_effectManager->m_unkVector6c0) {
        if (node->m_unk0d0)
            continue;

        int groupId = node->getTag();
        CCArray* objects = getStaticGroup(groupId);
        if (objects)
            moveObjects(objects, node->m_unk038, node->m_unk040, node->m_uIndexInArray == 13); // index in array thing is unused

        objects = getOptimizedGroup(groupId);
        if (objects)
            moveObjects(objects, node->m_unk090, node->m_unk098, node->m_uIndexInArray == 13); // index in array thing is unused
    }
}

void decomp_GJBaseGameLayer::processRotationActions() {
    float objLayerScaleX = m_objectLayer->getScaleX();
    float objLayerScaleY = m_objectLayer->getScaleY();
    CCPoint objLayerPos = m_objectLayer->getPosition();

    m_objectLayer->setScale(1.0f);
    m_objectLayer->setPosition(CCPointZero);

    for (auto& [_, gcos] : m_effectManager->m_unkMap5c8) {
        for (auto gco : gcos)
            gco->m_unkInt204 = m_gameState.m_commandIndex;
    }

    for (auto gco : m_effectManager->m_unkVector5b0) {
        if (gco->m_unkInt204 != m_gameState.m_commandIndex)
            continue;

        if (gco->m_someInterpValue1RelatedFalse)
            continue;

        GameObject* mainObject = tryGetMainObject(gco->m_centerGroupID);
        CCArray* staticGroup = getStaticGroup(gco->m_targetGroupID);

        CCArray* objectsToRotate = getOptimizedGroup(gco->m_targetGroupID);

        float rotation;
        bool ignoreStaticGroups;

        if (staticGroup && staticGroup->count() != 0) {
            objectsToRotate = getGroup(gco->m_targetGroupID);
            rotation = gco->m_someInterpValue1RelatedOne - gco->m_someInterpValue1RelatedZero;
            ignoreStaticGroups = false;
        } else {
            rotation = gco->m_someInterpValue2RelatedOne - gco->m_someInterpValue2RelatedZero;
            ignoreStaticGroups = true;
        }

        if (rotation == 0.0f && !gco->m_finishRelated)
            continue;

        gco->m_someInterpValue1RelatedFalse = true;

        float localRotation = rotation;
        if (gco->m_lockObjectRotation)
            localRotation = 0.0f;

        claimRotationAction(
            gco->m_targetGroupID,
            gco->m_centerGroupID,
            rotation,
            localRotation,
            ignoreStaticGroups,
            true
        );

        if (mainObject) {
            CCPoint centerPoint = mainObject->getUnmodifiedPosition();
            CCPoint unk = claimMoveAction(gco->m_targetGroupID, ignoreStaticGroups);

            m_areaTransformNode->setPosition(centerPoint);
            m_areaTransformNode->setScaleX(1.0f);
            m_areaTransformNode->setScaleY(1.0f);
            m_areaScaleNode->setScaleX(1.0f);
            m_areaScaleNode->setScaleY(1.0f);
            m_areaTransformNode->setSkewX(0.0f);
            m_areaTransformNode->setSkewY(0.0f);
            m_areaSkewNode->setSkewX(0.0f);
            m_areaSkewNode->setSkewY(0.0f);
            m_areaTransformNode->setRotation(rotation);
            prepareTransformParent(false);

            CCAffineTransform transform1 = m_areaTransformNode->nodeToWorldTransform();
            CCAffineTransform transform2 = m_areaSkewNode->nodeToWorldTransform();
            // Huh? Was was the line above for?
            // I've just noticed it doesn't get used as well lmao
            transform2 = m_areaScaleNode->nodeToWorldTransform();

            m_areaTransformNode2->setScaleX(1.0f);
            m_areaTransformNode2->setScaleY(1.0f);
            m_areaTransformNode2->setSkewX(0.0f);
            m_areaTransformNode2->setSkewY(0.0f);
            m_areaTransformNode2->setRotation(0.0f);

            for (auto object : CCArrayExt<GameObject*, false>(objectsToRotate)) {
                if (!object) continue;

                object->m_unk4fb = gco->m_finishRelated;
                if (!object->m_isDecoration2 && object->m_unk4C4 != m_gameState.m_commandIndex) {
                    object->m_lastPosition.x = object->m_positionX;
                    object->m_lastPosition.y = object->m_positionY;
                    object->m_unk4C4 = m_gameState.m_commandIndex;
                    object->dirtifyObjectRect();
                }

                object->m_isDirty = true;
                object->m_isUnmodifiedPosDirty = true;
            
                CCPoint centerToObject = ccp(
                    object->m_positionX - object->m_positionXOffset - centerPoint.x,
                    object->m_positionY - object->m_positionYOffset - centerPoint.y
                );

                CCPoint rotatedCenterToObject = CCPointApplyAffineTransform(centerToObject, transform1);

                object->m_positionX += rotatedCenterToObject.x + unk.x - object->m_positionX + object->m_positionXOffset;
                object->m_positionY += rotatedCenterToObject.y + unk.y - object->m_positionY + object->m_positionYOffset;

                if (localRotation != 0.0f && object->m_canRotateFree) {
                    object->m_rotationXOffset += localRotation;
                    object->m_rotationYOffset += localRotation;
                    object->addRotation(localRotation);
                    if (object->m_objectType != GameObjectType::Decoration && !object->m_shouldUseOuterOb)
                        object->calculateOrientedBox();
                }
                this->updateObjectSection(object);
            }
        } else if (localRotation != 0.0f) {
            for (auto object : CCArrayExt<GameObject*, false>(objectsToRotate)) {
                if (!object || !object->m_canRotateFree) continue;

                object->m_rotationXOffset += localRotation;
                object->m_rotationYOffset += localRotation;
                object->addRotation(localRotation);
                if (object->m_objectType != GameObjectType::Decoration) {
                    object->m_isObjectRectDirty = true;
                    object->m_isOrientedBoxDirty = true;
                    if (!object->m_shouldUseOuterOb)
                        object->calculateOrientedBox();
                }
            }
        }
    }
    m_objectLayer->setScaleX(objLayerScaleX);
    m_objectLayer->setScaleY(objLayerScaleY);
    m_objectLayer->setPosition(objLayerPos);
}

void decomp_GJBaseGameLayer::optimized_processRotationActions() {
    PROFILER_PUSH("optimized_processRotationActions");

    float objLayerScaleX = m_objectLayer->getScaleX();
    float objLayerScaleY = m_objectLayer->getScaleY();
    CCPoint objLayerPos = m_objectLayer->getPosition();

    m_objectLayer->setScale(1.0f);
    m_objectLayer->setPosition(CCPointZero);

    for (auto& [_, gcos] : m_effectManager->m_unkMap5c8) {
        for (auto gco : gcos)
            gco->m_unkInt204 = m_gameState.m_commandIndex;
    }

    for (auto gco : m_effectManager->m_unkVector5b0) {
        if (gco->m_unkInt204 != m_gameState.m_commandIndex)
            continue;

        if (gco->m_someInterpValue1RelatedFalse)
            continue;

        GameObject* mainObject = tryGetMainObject(gco->m_centerGroupID);

        GroupManager& groupManager = Renderer::get()->getGroupManager();

        const auto& objectsToRotate = groupManager.getVanillaRotateGroup(gco->m_targetGroupID);

        CCArray* staticGroup = getStaticGroup(gco->m_targetGroupID);

        float rotation;
        bool ignoreStaticGroups;

        if (staticGroup && staticGroup->count() != 0) {
            rotation = gco->m_someInterpValue1RelatedOne - gco->m_someInterpValue1RelatedZero;
            ignoreStaticGroups = false;
        } else {
            rotation = gco->m_someInterpValue2RelatedOne - gco->m_someInterpValue2RelatedZero;
            ignoreStaticGroups = true;
        }

        if (rotation == 0.0f && !gco->m_finishRelated)
            continue;

        float localRotation = rotation;
        if (gco->m_lockObjectRotation)
            localRotation = 0.0f;

        int targetId = gco->m_targetGroupID;
        int centerId = gco->m_centerGroupID;

        // I just couldn't figure out how to do this any other way
        float rendererRotation = rotation;
        if (m_effectManager->m_unkMap770.find({ targetId, centerId }) != m_effectManager->m_unkMap770.end()) {
            for (auto obj : m_effectManager->m_unkMap770[{ targetId, centerId }]) {
                if (obj == gco || obj->m_someInterpValue1RelatedFalse)
                    continue;
                if (staticGroup && staticGroup->count() != 0)
                    rendererRotation += obj->m_someInterpValue1RelatedOne - obj->m_someInterpValue1RelatedZero;
                else
                    rendererRotation += obj->m_someInterpValue2RelatedOne - obj->m_someInterpValue2RelatedZero;
            }
        }

        gco->m_someInterpValue1RelatedFalse = true;

        claimRotationAction(
            gco->m_targetGroupID,
            gco->m_centerGroupID,
            rotation,
            localRotation,
            ignoreStaticGroups,
            true
        );

        if (mainObject == nullptr)
            groupManager.rotateGroup(gco->m_targetGroupID, rendererRotation, gco->m_lockObjectRotation);
        else {
            auto pos = mainObject->getUnmodifiedPosition();
            groupManager.rotateGroup(gco->m_targetGroupID, rendererRotation, gco->m_lockObjectRotation, ccPointToGLM(pos));
        }

        if (objectsToRotate.empty())
            continue;

        if (mainObject) {
            CCPoint centerPoint = mainObject->getUnmodifiedPosition();
            CCPoint unk = claimMoveAction(gco->m_targetGroupID, ignoreStaticGroups);

            m_areaTransformNode->setPosition(centerPoint);
            m_areaTransformNode->setScaleX(1.0f);
            m_areaTransformNode->setScaleY(1.0f);
            m_areaScaleNode->setScaleX(1.0f);
            m_areaScaleNode->setScaleY(1.0f);
            m_areaTransformNode->setSkewX(0.0f);
            m_areaTransformNode->setSkewY(0.0f);
            m_areaSkewNode->setSkewX(0.0f);
            m_areaSkewNode->setSkewY(0.0f);
            m_areaTransformNode->setRotation(rotation);
            prepareTransformParent(false);

            CCAffineTransform transform1 = m_areaTransformNode->nodeToWorldTransform();
            CCAffineTransform transform2 = m_areaSkewNode->nodeToWorldTransform();
            // Huh? Was was the line above for?
            // I've just noticed it doesn't get used as well lmao
            transform2 = m_areaScaleNode->nodeToWorldTransform();

            m_areaTransformNode2->setScaleX(1.0f);
            m_areaTransformNode2->setScaleY(1.0f);
            m_areaTransformNode2->setSkewX(0.0f);
            m_areaTransformNode2->setSkewY(0.0f);
            m_areaTransformNode2->setRotation(0.0f);

            for (auto object : objectsToRotate) {
                if (!object) continue;

                object->m_unk4fb = gco->m_finishRelated;
                if (!object->m_isDecoration2 && object->m_unk4C4 != m_gameState.m_commandIndex) {
                    object->m_lastPosition.x = object->m_positionX;
                    object->m_lastPosition.y = object->m_positionY;
                    object->m_unk4C4 = m_gameState.m_commandIndex;
                    object->dirtifyObjectRect();
                }

                object->m_isDirty = true;
                object->m_isUnmodifiedPosDirty = true;
            
                CCPoint centerToObject = ccp(
                    object->m_positionX - object->m_positionXOffset - centerPoint.x,
                    object->m_positionY - object->m_positionYOffset - centerPoint.y
                );

                CCPoint rotatedCenterToObject = CCPointApplyAffineTransform(centerToObject, transform1);

                object->m_positionX += rotatedCenterToObject.x + unk.x - object->m_positionX + object->m_positionXOffset;
                object->m_positionY += rotatedCenterToObject.y + unk.y - object->m_positionY + object->m_positionYOffset;

                if (localRotation != 0.0f && object->m_canRotateFree) {
                    object->m_rotationXOffset += localRotation;
                    object->m_rotationYOffset += localRotation;
                    object->addRotation(localRotation);
                    if (object->m_objectType != GameObjectType::Decoration && !object->m_shouldUseOuterOb)
                        object->calculateOrientedBox();
                }
                this->updateObjectSection(object);
            }
        } else if (localRotation != 0.0f) {
            for (auto object : objectsToRotate) {
                if (!object || !object->m_canRotateFree) continue;

                object->m_rotationXOffset += localRotation;
                object->m_rotationYOffset += localRotation;
                object->addRotation(localRotation);
                if (object->m_objectType != GameObjectType::Decoration) {
                    object->m_isObjectRectDirty = true;
                    object->m_isOrientedBoxDirty = true;
                    if (!object->m_shouldUseOuterOb)
                        object->calculateOrientedBox();
                }
            }
        }
    }
    m_objectLayer->setScaleX(objLayerScaleX);
    m_objectLayer->setScaleY(objLayerScaleY);
    m_objectLayer->setPosition(objLayerPos);

    PROFILER_POP();
}
