#pragma once

#include <Geode/binding/GameObject.hpp>
#include <common.hpp>
#include <span>
#include "../../resources/shaders/headers/shared.h"
#include "Buffer.hpp"

/*
    Objects can have up to 10 group ids. Alpha, move, rotate
    and scale triggers can then change opacity or transformation
    depending on which group id they have.
    
    In GD, this is done by keeping track of the opacity and
    transformation of every object. And when a trigger gets
    triggered, it goes through every object with the target
    group id and applies these properties.

    But, we're going to do it differently.

    We are going to get every combination of group ids of all
    the objects. We then keep track of opacity and transformation
    per group combination rather than per object. In the case
    of a trigger. The properties of that trigger get only applied
    to the group combination that contains the target group id.

    The state of every group combination is then sent to the shader
    to be applied to every object in parallel.

    To do that, every group combination is assigned an index.
*/

using GroupID = i16;
using GroupCombinationIndex = u32;

class GroupCombination {
public:
    GroupCombination(std::array<GroupID, 10>* groupIds, i32 count);
    GroupCombination(std::vector<GroupID> groupIds);
    inline GroupCombination(const GroupCombination& c) {
        ids = c.ids;
        count = c.count;
    }

    void removeGroupIdsNotInSet(const std::set<GroupID>& maskSet);

    inline GroupCombination(GameObject* object)
        : GroupCombination(object->m_groups, object->m_groupCount) {}

    // This function is important for this class to be used as a key in std::map
    GEODE_INLINE bool operator<(const GroupCombination& o) const {
        if (count < o.count) return true;
        if (count > o.count) return false;

        for (i32 i = 0; i < count; i++) {
            if (ids[i] < o.ids[i]) return true;
            if (ids[i] > o.ids[i]) return false;
        }
        return false;
    }

    GEODE_INLINE std::span<GroupID> getSpan() {
        return std::span<GroupID>(ids.data(), static_cast<size_t>(count));
    }

    GEODE_INLINE std::span<const GroupID> getSpan() const {
        return std::span<const GroupID>(ids.data(), static_cast<size_t>(count));
    }

    GEODE_INLINE i32 getCount() const { return count; }

    operator std::string() const;

private:
    i32 count;
    std::array<GroupID, 10> ids;
};

class Renderer;

class GroupManager {
public:
    struct Snapshot {
        std::vector<GroupCombinationState> states;
        std::set<GroupID> disabled;
        std::vector<bool> scaled;
    };
    Snapshot saveState() {
        auto states = getGroupStates();
        return {{states.begin(), states.end()}, disabledGroups, haveGroupStatesScaled};
    }
    bool restoreState(const Snapshot& snapshot) {
        if (snapshot.states.size() != groupCombinationCount) return false;
        std::copy(snapshot.states.begin(), snapshot.states.end(), groupStates);
        disabledGroups = snapshot.disabled;
        haveGroupStatesScaled = snapshot.scaled;
        alphaStateDirty = true;
        markAllGroupStatesDirty();
        return true;
    }
    inline GroupManager(Renderer& renderer)
        : renderer(renderer) {}

    ~GroupManager();

    void initWithObjects(cocos2d::CCArray* objects);
    
    /*
        Objects with the same group ids always have
        the same group combination index
    */
    GroupCombinationIndex getGroupCombinationIndexForObject(GameObject* object);

    /*
        Objects that are affected by the same transforming
        triggers (move, rotate, scale, follow, ...) always
        have the same transform combination index
    */
    GroupCombinationIndex getTransformCombinationIndexForObject(GameObject* object);

    inline GroupCombinationIndex getFirstGroupIndexOfTransformIndex(GroupCombinationIndex transformIndex) const {
        return firstGroupIndexPerTransformIndex[transformIndex];
    }

    inline GroupID getMaxGroupId() const { return maxGroupId; }

    inline u32 getGroupCombinationCount() const { return groupCombinationCount; }
    inline u32 getTransformCombinationCount() const { return transformCombinationCount; }

    inline void bindGroupStateBuffer() {
        groupStateBuffer->bindAsStorageBuffer(GROUP_STATE_BUFFER_BINDING);
    }

    inline isize getGroupStateBufferSize() {
        return groupCombinationCount * sizeof(GroupCombinationState);
    }

    inline std::span<GroupCombinationState> getGroupStates() {
        return { groupStates, groupStates + getGroupCombinationCount() };
    }

    inline bool hasGroupStateScaled(i32 index) { return haveGroupStatesScaled[index]; }

    inline const std::set<GameObject*>& getVanillaRotateGroup(GroupID id) {
        static const std::set<GameObject*> empty;
        if (id < 0 || id >= vanillaRotateGroups.size()) return empty;
        return vanillaRotateGroups[id];
    }

    void fetchVanilaRotateGroups();

    void prepareGroupStateBuffer();

    //// TRIGGER ACTIONS ////

    void moveGroup(GroupID groupId, float deltaX, float deltaY);

    void rotateGroup(
        GroupID groupId,
        float angle,
        bool lockObjectRotation,
        std::optional<glm::vec2> centerPoint = std::nullopt
    );

    void toggleGroup(GroupID groupId, bool visible);

    //// RESET ////

    void resetGroupStates();

private:
    void addGroupCombination(GroupCombination& comb, GroupCombinationIndex index);
    void markGroupStateDirty(GroupCombinationIndex index);
    void markAllGroupStatesDirty();

private:
    Renderer& renderer;

    // GroupCombinationState struct can be found in resources/shaders/shared.h
    // std::vector<GroupCombinationState> groupStates;
    GroupCombinationState* groupStates = nullptr;
    Buffer* groupStateBuffer = nullptr;

    // The CPU copy remains mapped between frames. Upload only the range that
    // changed; most groups are static even while the level keeps updating.
    GroupCombinationIndex firstDirtyGroupState = UINT32_MAX;
    GroupCombinationIndex lastDirtyGroupState = 0;
    bool alphaStateDirty = true;

    std::vector<bool> haveGroupStatesScaled;

    u32 groupCombinationCount = 0;
    std::map<GroupCombination, GroupCombinationIndex> groupCombinationIndicies;

    u32 transformCombinationCount = 0;
    std::map<GroupCombination, u32> transformCombinationIndicies;

    u32 alphaIndiciesCount = 0;
    std::map<GroupCombination, u32> alphaIndicies;

    std::vector<u32> alphaIndiciesPerGroupCombinationIndex;
    std::vector<u32> alphaIndexGroupIdFastStructure;
    std::vector<float> alphaValuesPerGroupId;
    std::vector<float> alphaValuesPerAlphaIndex;

    std::vector<GroupCombinationIndex> firstGroupIndexPerTransformIndex;

    /*
        This is a vector with the index being a group id and the value being
        an array of all group combination indicies it belongs to.

        This is used for fast lookup to see which group combinations
        need to be changed when a group id gets affected.
    */
    std::vector<std::vector<GroupCombinationIndex>> groupCombinationIndiciesPerGroupId;

    GroupID maxGroupId = 0;

    // This is all the group ids used by objects
    std::set<GroupID> usedGroupIds;

    /*
        This is all the group ids that are the target
        of a transforming trigger like move, rotate and scale.
    */
    std::set<GroupID> transformGroupIds;

    std::set<GroupID> alphaGroupIds;

    std::set<GroupID> disabledGroups;

    /*
        These are groups with objects that have to rotate
        with vanilla code. Otherwise, it may cause physics
        inconsistencies.
    */
    std::vector<std::set<GameObject*>> vanillaRotateGroups;
};
