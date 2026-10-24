#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumPauseGuardIdentity[] = "CHLORIUM_DETACHED_PAUSE_GUARD_V1";

// Installed GD 2.2081 pauseGame dereferences getParent() when adding its pause
// layer. During scene transitions PlayLayer::get() may already return a layer
// that has no parent. A menu opened by another mod must not pause that layer.
// The observed crash was GD+0x3ba38a, reached from ClickAssistPopup; the math
// experiment had not executed any phase or cached call at that point.
class $modify(ChloriumTransitionPauseGuard, PlayLayer) {
    void pauseGame(bool p0) {
        if (!getParent()) {
            log::warn("Chlorium: ignored a pause request on a detached PlayLayer");
            return;
        }
        PlayLayer::pauseGame(p0);
    }
};
