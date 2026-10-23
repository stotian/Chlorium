#pragma once

namespace chlorium::native {
// Field operations and their order match GD 2.2081 moveObjects. This does not
// omit updates for decorative objects, combine ticks or change movement math.
template<class Object>
bool advanceNativeMove(Object& object, int command, double dx, double dy) {
    if (!object.m_isDecoration2 && object.m_unk4C4 != command) {
        object.m_lastPosition.x = static_cast<float>(object.m_positionX);
        object.m_lastPosition.y = static_cast<float>(object.m_positionY);
        object.m_unk4C4 = command;
        object.m_isObjectRectDirty = true;
        object.m_isOrientedBoxDirty = true;
    }
    if (dx != 0.0 && !object.m_tempOffsetXRelated) object.m_positionX = dx + object.m_positionX;
    if (dy != 0.0) object.m_positionY = dy + object.m_positionY;
    object.m_isObjectPosDirty = true;
    object.m_isUnmodifiedPosDirty = true;
    // The verified native section function returns immediately for this case.
    return object.m_outerSectionIndex >= 0;
}
struct NativeMovementStats {
    unsigned long long fastCalls = 0, objectVisits = 0, avoidedSectionCalls = 0, fallbackCalls = 0;
};
NativeMovementStats getNativeMovementStats();
}
