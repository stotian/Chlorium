#include "renderer/NativeMovementLoop.hpp"
#include <cmath>
#include <iostream>
#include <limits>

struct Object {
    double m_positionX = 12.25, m_positionY = -4.5;
    struct Point { float x = -99, y = -99; } m_lastPosition;
    int m_unk4C4 = -1, m_outerSectionIndex = -1;
    bool m_isDecoration2 = false, m_tempOffsetXRelated = false;
    bool m_isDirty = false, m_isObjectPosDirty = false, m_isUnmodifiedPosDirty = false;
    bool m_isObjectRectDirty = false, m_isOrientedBoxDirty = false;
};
int main() {
    using chlorium::native::advanceNativeMove;
    Object moved;
    if (advanceNativeMove(moved, 7, 2, 3) || moved.m_positionX != 14.25 || moved.m_positionY != -1.5 ||
        moved.m_lastPosition.x != 12.25f || moved.m_lastPosition.y != -4.5f || moved.m_unk4C4 != 7 ||
        moved.m_isDirty || !moved.m_isObjectPosDirty || !moved.m_isUnmodifiedPosDirty || !moved.m_isObjectRectDirty || !moved.m_isOrientedBoxDirty) return 1;
    moved.m_isObjectRectDirty = moved.m_isOrientedBoxDirty = false;
    advanceNativeMove(moved, 7, 0, 0);
    if (moved.m_lastPosition.x != 12.25f || moved.m_isObjectRectDirty || moved.m_isOrientedBoxDirty) return 2;
    Object decoration; decoration.m_isDecoration2 = true;
    advanceNativeMove(decoration, 8, 1, -2);
    if (decoration.m_positionX != 13.25 || decoration.m_positionY != -6.5 || decoration.m_unk4C4 != -1 ||
        decoration.m_lastPosition.x != -99 || decoration.m_isObjectRectDirty) return 3;
    Object locked; locked.m_tempOffsetXRelated = true;
    advanceNativeMove(locked, 2, 100, -4);
    if (locked.m_positionX != 12.25 || locked.m_positionY != -8.5) return 4;
    Object registered; registered.m_outerSectionIndex = 0;
    if (!advanceNativeMove(registered, 2, 0, 0)) return 5;
    registered.m_outerSectionIndex = -2;
    if (advanceNativeMove(registered, 2, 0, 0)) return 6;
    Object zero; zero.m_positionX = -0.0;
    advanceNativeMove(zero, 1, -0.0, 0);
    if (!std::signbit(zero.m_positionX) || !std::signbit(zero.m_lastPosition.x)) return 7;
    Object invalid;
    advanceNativeMove(invalid, 1, std::numeric_limits<double>::quiet_NaN(), 0);
    if (!std::isnan(invalid.m_positionX) || invalid.m_positionY != -4.5) return 8;
    std::cout << "Native movement bookkeeping, repeated commands, decoration, locked X, section and floating-point fixtures passed\n";
}
