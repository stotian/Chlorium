#include "NativeTransformMath.hpp"
#include "NativeTransformStats.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <windows.h>
#include <intrin.h>
#include <float.h>
#include <cstdlib>

using namespace geode::prelude;

namespace chlorium::native {
namespace {
using AtanFunction = float (*)(float, float);
AtanFunction originalAtan = nullptr;
uintptr_t gameBase = 0;
Patch* importPatch = nullptr;
thread_local bool transformPhase = false;
thread_local TransformAtanCache<1024> atanCache;
thread_local TransformMathStats mathStats;
thread_local TransformReusePolicy reusePolicy;
thread_local bool fpControlPending = false, fpControlSupported = false;

bool supportedExecutable() {
    const auto bytes = reinterpret_cast<const unsigned char*>(gameBase);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe->FileHeader.TimeDateStamp != 0x69702da0 || pe->OptionalHeader.SizeOfImage != 0xa3a000) return false;
    // Exclude Geode's entry detour, but require the complete remaining native
    // transform body and its two exact atan2f call sites. Unknown game versions
    // keep their original import; no guessed structure layout is used here.
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0x20; i < 0xf30; ++i) {
        hash ^= bytes[0x22c680 + i]; hash *= 1099511628211ull;
    }
    const unsigned char call1[]{0xe8, 0xcf, 0x46, 0x2a, 0x00};
    const unsigned char call2[]{0xe8, 0xa7, 0x46, 0x2a, 0x00};
    const unsigned char thunk[]{0xff, 0x25, 0x4a, 0x3b, 0x04, 0x00};
    return hash == 0x32a610f5884e6c0cull &&
        !std::memcmp(bytes + 0x22d174, call1, sizeof(call1)) &&
        !std::memcmp(bytes + 0x22d19c, call2, sizeof(call2)) &&
        !std::memcmp(bytes + 0x4d1848, thunk, sizeof(thunk));
}

__declspec(noinline) float cachedNativeAtan(float y, float x) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    // This import is shared by other GD methods. Cache only the two proven
    // transform decomposition calls; every other caller goes straight to CRT.
    if (!transformPhase || (caller != gameBase + 0x22d179 && caller != gameBase + 0x22d1a1))
        return originalAtan(y, x);
    ++mathStats.calls;
    if (reusePolicy.disabled()) { ++mathStats.bypassed; return originalAtan(y, x); }
    if (fpControlPending) {
        unsigned fpControl = 0;
        _controlfp_s(&fpControl, 0, 0);
        fpControlSupported = (fpControl & _MCW_EM) == _MCW_EM && (fpControl & _MCW_RC) == _RC_NEAR;
        fpControlPending = false;
    }
    if (!fpControlSupported) { ++mathStats.bypassed; return originalAtan(y, x); }
    const unsigned csr = _mm_getcsr();
    if ((csr & 0x1f80u) != 0x1f80u) { ++mathStats.bypassed; return originalAtan(y, x); }
    bool hit = false;
    const auto result = atanCache.evaluate(y, x, csr & 0xffc0u, originalAtan, &hit);
    if (hit) ++mathStats.hits;
    else ++mathStats.misses;
    if (!reusePolicy.record(hit)) ++mathStats.lowReusePhases;
    static const bool verify = std::getenv("CHLORIUM_TRANSFORM_VERIFY") != nullptr;
    if (verify && hit) {
        const auto native = originalAtan(y, x);
        ++mathStats.verified;
        if (std::bit_cast<uint32_t>(native) != std::bit_cast<uint32_t>(result)) {
            ++mathStats.failures;
            transformPhase = false;
            return native;
        }
    }
    return result;
}

void installTransformCache() {
    gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!supportedExecutable()) {
        log::warn("Transform cache: unsupported or modified native transform body"); return;
    }
    const auto runtime = GetModuleHandleW(L"ucrtbase.dll");
    const auto expected = runtime ? reinterpret_cast<AtanFunction>(GetProcAddress(runtime, "atan2f")) : nullptr;
    const auto slot = reinterpret_cast<AtanFunction*>(gameBase + 0x515398);
    if (!expected || *slot != expected) {
        log::warn("Transform cache: atan2f import is owned by another implementation"); return;
    }
    originalAtan = *slot;
    const auto replacement = reinterpret_cast<uintptr_t>(&cachedNativeAtan);
    ByteVector bytes(sizeof(replacement));
    std::memcpy(bytes.data(), &replacement, sizeof(replacement));
    auto patched = Mod::get()->patch(slot, std::move(bytes));
    if (!patched) { log::warn("Transform cache import patch failed: {}", patched.unwrapErr()); return; }
    importPatch = patched.unwrap();
    log::info("Exact native transform cache installed (GD 2.2081)");
}
} // namespace
} // namespace chlorium::native

extern "C" __declspec(dllexport) bool ChloriumReadTransformMathStats(
    chlorium::native::TransformMathStats* output, std::size_t size) {
    using namespace chlorium::native;
    if (!output || size != sizeof(*output)) return false;
    *output = mathStats;
    output->installed = importPatch && importPatch->isEnabled();
    return true;
}

$execute { chlorium::native::installTransformCache(); }

class $modify(NativeTransformCacheLayer, GJBaseGameLayer) {
    void processTransformActions(bool visibleFrame) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const bool useCache = importPatch && importPatch->isEnabled() && !baseline && !mathStats.failures &&
            !transformPhase && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == static_cast<GJBaseGameLayer*>(this) &&
            Mod::get()->getSettingValue<bool>("enabled");
        if (!useCache) { GJBaseGameLayer::processTransformActions(visibleFrame); return; }
        struct PhaseScope {
            PhaseScope() {
                atanCache.reset(); reusePolicy.reset();
                fpControlPending = true; fpControlSupported = false;
                transformPhase = true; ++mathStats.phases;
            }
            ~PhaseScope() { transformPhase = false; }
        } scope;
        // Native object updates, history, sections, collisions and trigger
        // order remain intact. Only repeated identical atan2f results are reused.
        GJBaseGameLayer::processTransformActions(visibleFrame);
    }
};
