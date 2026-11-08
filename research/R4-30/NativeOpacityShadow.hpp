#pragma once

#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
#error R4-30 observer is blocked until the installed loader native-register contract is verified.
#include <xmmintrin.h>
namespace chlorium::research::opacity {
class SavedOpacityFPState {
    unsigned saved_ = _mm_getcsr();
public:
    SavedOpacityFPState() = default;
    SavedOpacityFPState(const SavedOpacityFPState&) = delete;
    SavedOpacityFPState& operator=(const SavedOpacityFPState&) = delete;
    ~SavedOpacityFPState() { _mm_setcsr(saved_); }
};
bool nativeOpacityShadowPending() noexcept;
bool beginNativeOpacityShadow(bool eligible, unsigned long long frame, double percent) noexcept;
void finishNativeOpacityShadow() noexcept;
void abandonNativeOpacityShadow() noexcept;

class NativeOpacityShadowScope {
    bool active_;
public:
    NativeOpacityShadowScope(bool eligible, unsigned long long frame, double percent) :
        active_(beginNativeOpacityShadow(eligible, frame, percent)) {}
    NativeOpacityShadowScope(const NativeOpacityShadowScope&) = delete;
    NativeOpacityShadowScope& operator=(const NativeOpacityShadowScope&) = delete;
    ~NativeOpacityShadowScope() { if (active_) abandonNativeOpacityShadow(); }
    void finish() {
        if (active_) { active_ = false; finishNativeOpacityShadow(); }
    }
};
}
#endif
