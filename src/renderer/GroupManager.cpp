#include "GroupManager.hpp"
#include "GroupRotation.hpp"
#include "Renderer.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <Geode/binding/EffectGameObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include "../profiler.hpp"
#include <optional>
#include <string>

using namespace geode::prelude;

#define MOVE_TRIGGER_ID   901
#define ROTATE_TRIGGER_ID 1346
#define FOLLOW_TRIGGER_ID 1347

static std::optional<GroupID> getTargetGroupIdOfTransformingTrigger(GameObject* object) {
    switch (object->m_objectID) {
    case MOVE_TRIGGER_ID:
    case ROTATE_TRIGGER_ID:
    case FOLLOW_TRIGGER_ID:
        return ((EffectGameObject*)object)->m_targetGroupID;
    }
    return std::nullopt;
}

GroupCombination::GroupCombination(std::array<GroupID, 10>* groupIds, i32 count) {
    count = groupIds ? std::clamp(count, 0, 10) : 0;
    *this = GroupCombination(groupIds ? std::vector<GroupID>(groupIds->begin(), groupIds->begin() + count) : std::vector<GroupID>{});
}

GroupCombination::GroupCombination(std::vector<GroupID> groupIds) {
    std::erase_if(groupIds, [](GroupID id) { return id <= 0; });
    std::sort(groupIds.begin(), groupIds.end());
    groupIds.erase(std::unique(groupIds.begin(), groupIds.end()), groupIds.end());
    if (groupIds.size() > ids.size()) groupIds.resize(ids.size());
    count = static_cast<i32>(groupIds.size());
    std::copy(groupIds.begin(), groupIds.end(), ids.begin());
    std::fill(ids.begin() + count, ids.end(), 0);
    std::sort(ids.begin(), ids.begin() + count);
}

void GroupCombination::removeGroupIdsNotInSet(const std::set<GroupID>& maskSet) {
    for (i32 i = 0; i < count; i++) {
        if (!maskSet.contains(ids[i]))
            ids[i] = 0;
    }

    i32 newCount = 0;
    i32 srcI = 0, dstI = 0;
    for (; srcI < count; srcI++) {
        if (ids[srcI] != 0) {
            ids[dstI] = ids[srcI];
            dstI++;
            newCount++;
        }
    }
    for (; dstI < 10; dstI++)
        ids[dstI] = 0;
    count = newCount;
}

GroupCombination::operator std::string() const {
    std::string ret = "[";
    for (u32 i = 0; i < count; i++) {
        if (i != 0) ret += ", ";
        ret += std::to_string(ids[i]);
    }
    return ret + "]";
}

GroupManager::~GroupManager() {
    if (groupStateBuffer) {
        groupStateBuffer->unmap();
        Buffer::destroy(groupStateBuffer);
    }
}


void GroupManager::initWithObjects(cocos2d::CCArray* objects) {
    GroupCombinationIndex groupCombIndex = 0;

    for (auto object : CCArrayExt<GameObject*>(objects)) {
        std::optional<GroupID> targetGroupId = getTargetGroupIdOfTransformingTrigger(object);

        if (targetGroupId.has_value())
            transformGroupIds.insert(targetGroupId.value());

        
        GroupCombination comb = GroupCombination(object);

        if (groupCombinationIndicies.find(comb) == groupCombinationIndicies.end()) {
            addGroupCombination(comb, groupCombIndex);
            groupCombIndex++;
        }
    }

    groupCombinationCount = groupCombIndex;

    // A spawn/remap can target a group that is not the literal target of an
    // alpha/toggle trigger in the level. Every group carried by an object
    // must therefore participate; untouched groups have native opacity 1.
    alphaGroupIds = usedGroupIds;

    u32 transformCombIndex = 0;
    u32 alphaCombIndex     = 0;

    alphaIndiciesPerGroupCombinationIndex.resize(groupCombinationCount);
    haveGroupStatesScaled.resize(groupCombinationCount);

    for (auto object : CCArrayExt<GameObject*>(objects)) {
        GroupCombination comb = GroupCombination(object);
        auto combIndex = groupCombinationIndicies[comb];

        GroupCombination transformComb = comb;
        transformComb.removeGroupIdsNotInSet(transformGroupIds);

        if (transformCombinationIndicies.find(transformComb) == transformCombinationIndicies.end()) {
            transformCombinationIndicies[transformComb] = transformCombIndex;
            firstGroupIndexPerTransformIndex.push_back(combIndex);
            transformCombIndex++;
        }
        
        GroupCombination alphaComb = comb;
        alphaComb.removeGroupIdsNotInSet(alphaGroupIds);

        if (alphaIndicies.find(alphaComb) == alphaIndicies.end()) {
            alphaIndicies[alphaComb] = alphaCombIndex;
            alphaCombIndex++;

            alphaIndexGroupIdFastStructure.push_back(alphaComb.getCount());
            for (auto id : alphaComb.getSpan())
                alphaIndexGroupIdFastStructure.push_back(id);
        }

        alphaIndiciesPerGroupCombinationIndex[combIndex] = alphaIndicies[alphaComb];
    }

    alphaIndiciesCount = alphaCombIndex;

    transformCombinationCount = transformCombIndex;
    log::info("Number of unique group transformations: {}", transformCombinationCount);

    alphaValuesPerGroupId.resize(maxGroupId + 1);
    alphaValuesPerAlphaIndex.resize(alphaIndiciesCount);

    groupStateBuffer = Buffer::createDynamicDraw("Group state buffer", getGroupStateBufferSize());
    groupStates = (GroupCombinationState*)groupStateBuffer->mapWriteOnly();

    // Native rotation membership depends on which objects the renderer can
    // replace. Renderer calls optimizeMoveGroups after that classification.
}
    
GroupCombinationIndex GroupManager::getGroupCombinationIndexForObject(GameObject* object) {
    auto comb = GroupCombination(object);

    auto it = groupCombinationIndicies.find(comb);
    if (it == groupCombinationIndicies.end()) throw std::runtime_error("Unknown object group combination");

    return it->second;
}

GroupCombinationIndex GroupManager::getTransformCombinationIndexForObject(GameObject* object) {
    auto comb = GroupCombination(object);
    comb.removeGroupIdsNotInSet(transformGroupIds);

    auto it = transformCombinationIndicies.find(comb);
    if (it == transformCombinationIndicies.end()) throw std::runtime_error("Unknown transform group combination");

    return it->second;
}

void GroupManager::fetchVanilaRotateGroups() {
    PlayLayer* layer = renderer.getPlayLayer();

    vanillaRotateGroups.clear();
    vanillaRotateGroups.resize(std::max(layer->m_staticGroups.size(), layer->m_optimizedGroups.size()));

    for (const auto* groups : {&layer->m_staticGroups, &layer->m_optimizedGroups}) {
        for (size_t id = 1; id < groups->size() && id <= INT16_MAX; id++) {
            CCArray* group = (*groups)[id];
            if (!group) continue;

            auto& dstGroup = vanillaRotateGroups[id];
            for (auto object : CCArrayExt<GameObject*, false>(group)) {
                if (object && !renderer.canSkipNativeObjectUpdates(object))
                    dstGroup.insert(object);
            }
        }
    }

    for (size_t id = 1; id < vanillaRotateGroups.size() && id <= INT16_MAX; id++) {
        GameObject* mainObject = layer->tryGetMainObject(id);
        if (mainObject)
            vanillaRotateGroups[id].insert(mainObject);
    }
}

void GroupManager::prepareGroupStateBuffer() {
    PROFILER_PUSH("GroupManager::prepareGroupStateBuffer");
    // Iterate only IDs actually carried by objects. Checking their current
    // native opacity also covers trigger remapping without scanning ID gaps.
    for (auto id : alphaGroupIds) {
        if (id <= 0 || id >= alphaValuesPerGroupId.size()) continue;
        const float alpha = disabledGroups.contains(id) ? 0.0f :
            renderer.getPlayLayer()->m_effectManager->opacityModForGroup(id);
        if (alphaValuesPerGroupId[id] != alpha) {
            alphaValuesPerGroupId[id] = alpha;
            alphaStateDirty = true;
        }
    }

    if (alphaStateDirty) {
        const u32* alphaFast = alphaIndexGroupIdFastStructure.data();
        for (u32 alphaIndex = 0; alphaIndex < alphaIndiciesCount; alphaIndex++) {
            u32 combCount = *(alphaFast++);
            float alpha = 1.0;
            while (combCount > 0) {
                alpha *= alphaValuesPerGroupId[*(alphaFast++)];
                combCount--;
            }
            alphaValuesPerAlphaIndex[alphaIndex] = alpha;
        }

        for (u32 combIndex = 0; combIndex < groupCombinationCount; combIndex++) {
            const float alpha = alphaValuesPerAlphaIndex[alphaIndiciesPerGroupCombinationIndex[combIndex]];
            if (groupStates[combIndex].opacity != alpha) {
                groupStates[combIndex].opacity = alpha;
                markGroupStateDirty(combIndex);
            }
        }
        alphaStateDirty = false;
    }

    if (firstDirtyGroupState < groupCombinationCount) {
        const usize offset = static_cast<usize>(firstDirtyGroupState) * sizeof(GroupCombinationState);
        const usize size = static_cast<usize>(lastDirtyGroupState - firstDirtyGroupState + 1) * sizeof(GroupCombinationState);
        groupStateBuffer->flush(offset, size);
        firstDirtyGroupState = UINT32_MAX;
        lastDirtyGroupState = 0;
    }
    PROFILER_POP();
}

void GroupManager::moveGroup(GroupID groupId, float deltaX, float deltaY) {
    if (deltaX == 0.0f && deltaY == 0.0f) return;
    if (groupId < 0 || groupId >= groupCombinationIndiciesPerGroupId.size()) return;
    const auto& combIndicies = groupCombinationIndiciesPerGroupId[groupId];
    for (auto combIndex : combIndicies) {
        groupStates[combIndex].offset += glm::vec2(deltaX, deltaY);
        markGroupStateDirty(combIndex);
    }
}

void GroupManager::rotateGroup(
    GroupID groupId,
    float angle,
    bool lockObjectRotation,
    std::optional<glm::vec2> centerPoint
) {
    if (angle == 0.0f) return;
    if (groupId < 0 || groupId >= groupCombinationIndiciesPerGroupId.size()) return;
    const auto& indices = groupCombinationIndiciesPerGroupId[groupId];
    applyGroupRotation(getGroupStates(), std::span<const u32>(indices), angle,
                       lockObjectRotation, centerPoint);
    if (centerPoint)
        for (auto index : indices) haveGroupStatesScaled[index] = true;
    if (!lockObjectRotation || centerPoint)
        for (auto index : indices) markGroupStateDirty(index);
}

void GroupManager::toggleGroup(GroupID groupId, bool visible) {
    if (visible)
        disabledGroups.erase(groupId);
    else
        disabledGroups.insert(groupId);
}

void GroupManager::resetGroupStates() {
    for (auto& groupState : getGroupStates()) {
        groupState.positionalTransform = glm::identity<glm::mat2>();
        groupState.localTransform = glm::identity<glm::mat2>();
        groupState.offset = glm::vec2(0, 0);
        groupState.opacity = 1.0f;
    }
    disabledGroups.clear();
    std::fill(haveGroupStatesScaled.begin(), haveGroupStatesScaled.end(), false);
    alphaStateDirty = true;
    markAllGroupStatesDirty();
}

void GroupManager::markGroupStateDirty(GroupCombinationIndex index) {
    firstDirtyGroupState = std::min(firstDirtyGroupState, index);
    lastDirtyGroupState = std::max(lastDirtyGroupState, index);
}

void GroupManager::markAllGroupStatesDirty() {
    if (!groupCombinationCount) return;
    firstDirtyGroupState = 0;
    lastDirtyGroupState = groupCombinationCount - 1;
}

void GroupManager::addGroupCombination(GroupCombination& comb, GroupCombinationIndex index) {
    groupCombinationIndicies[comb] = index;

    for (auto groupId : comb.getSpan()) {
        usedGroupIds.insert(groupId);

        if (groupId > maxGroupId)
            maxGroupId = groupId;

        if (groupId >= groupCombinationIndiciesPerGroupId.size())
            groupCombinationIndiciesPerGroupId.resize(groupId + 1);

        groupCombinationIndiciesPerGroupId[groupId].push_back(index);
    }
}
