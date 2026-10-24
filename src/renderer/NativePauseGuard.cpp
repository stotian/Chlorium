#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
using namespace geode::prelude;

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
#endif
