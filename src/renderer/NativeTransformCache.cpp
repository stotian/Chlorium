#include "NativeTransformMath.hpp"
#include "NativeTransformStats.hpp"
#include "NativeDecompositionSites.hpp"
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
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
thread_local bool areaPhase = false;
thread_local TransformAtanCache<1024> areaAtanCache;
thread_local TransformReusePolicy areaReusePolicy;
thread_local TransformMathStats areaMathStats;
// Optional verification samples the first hit and every Nth subsequent hit.
// Normal alpha.6 retains its existing every-hit verification behaviour.
const unsigned verificationStride = [] {
    const auto text = std::getenv("CHLORIUM_TRANSFORM_VERIFY_STRIDE");
    if (!text) return 1u;
    char* end = nullptr;
    const auto value = std::strtoul(text, &end, 10);
    return end != text && !*end && value >= 1 && value <= 1048576 ? unsigned(value) : 1u;
}();
#endif

bool reuseFailed() {
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    return mathStats.failures || areaMathStats.failures;
#else
    return mathStats.failures != 0;
#endif
}

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
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    uint64_t areaHash = 14695981039346656037ull;
    for (size_t i = 0x20; i < 0x662; ++i) {
        areaHash ^= bytes[0x229720 + i]; areaHash *= 1099511628211ull;
    }
    const unsigned char areaCall1[]{0xe8, 0x1d, 0x7d, 0x2a, 0x00};
    const unsigned char areaCall2[]{0xe8, 0xf7, 0x7c, 0x2a, 0x00};
    if (areaHash != 0x6680410ab39143d4ull ||
        std::memcmp(bytes + 0x229b26, areaCall1, sizeof(areaCall1)) ||
        std::memcmp(bytes + 0x229b4c, areaCall2, sizeof(areaCall2))) return false;
#endif
    return hash == 0x32a610f5884e6c0cull &&
        !std::memcmp(bytes + 0x22d174, call1, sizeof(call1)) &&
        !std::memcmp(bytes + 0x22d19c, call2, sizeof(call2)) &&
        !std::memcmp(bytes + 0x4d1848, thunk, sizeof(thunk));
}

__declspec(noinline) float cachedNativeAtan(float y, float x) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    // This import is shared by other GD methods. Cache only the two proven
    // transform decomposition calls; every other caller goes straight to CRT.
    if (!transformPhase) return originalAtan(y, x);
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    if (!isDecompositionCaller(caller - gameBase, areaPhase)) return originalAtan(y, x);
    auto& selectedStats = areaPhase ? areaMathStats : mathStats;
    auto& selectedPolicy = areaPhase ? areaReusePolicy : reusePolicy;
    auto& selectedCache = areaPhase ? areaAtanCache : atanCache;
#else
    if (!isDecompositionCaller(caller - gameBase, false))
        return originalAtan(y, x);
    auto& selectedStats = mathStats;
    auto& selectedPolicy = reusePolicy;
    auto& selectedCache = atanCache;
#endif
    ++selectedStats.calls;
    if (selectedPolicy.disabled()) { ++selectedStats.bypassed; return originalAtan(y, x); }
    if (fpControlPending) {
        unsigned fpControl = 0;
        _controlfp_s(&fpControl, 0, 0);
        fpControlSupported = (fpControl & _MCW_EM) == _MCW_EM && (fpControl & _MCW_RC) == _RC_NEAR;
        fpControlPending = false;
    }
    if (!fpControlSupported) { ++selectedStats.bypassed; return originalAtan(y, x); }
    const unsigned csr = _mm_getcsr();
    if ((csr & 0x1f80u) != 0x1f80u) { ++selectedStats.bypassed; return originalAtan(y, x); }
    bool hit = false;
    const auto result = selectedCache.evaluate(y, x, csr & 0xffc0u, originalAtan, &hit);
    if (hit) ++selectedStats.hits;
    else ++selectedStats.misses;
    if (!selectedPolicy.record(hit)) ++selectedStats.lowReusePhases;
    static const bool verify = std::getenv("CHLORIUM_TRANSFORM_VERIFY") != nullptr;
    bool verifyHit = verify && hit;
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    verifyHit = verifyHit && (selectedStats.hits - 1) % verificationStride == 0;
#endif
    if (verifyHit) {
        const auto native = originalAtan(y, x);
        ++selectedStats.verified;
        if (std::bit_cast<uint32_t>(native) != std::bit_cast<uint32_t>(result)) {
            ++selectedStats.failures;
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
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    output->phases += areaMathStats.phases;
    output->calls += areaMathStats.calls;
    output->hits += areaMathStats.hits;
    output->misses += areaMathStats.misses;
    output->verified += areaMathStats.verified;
    output->failures += areaMathStats.failures;
    output->bypassed += areaMathStats.bypassed;
    output->lowReusePhases += areaMathStats.lowReusePhases;
#endif
    output->installed = importPatch && importPatch->isEnabled();
    return true;
}

$execute { chlorium::native::installTransformCache(); }

class $modify(NativeTransformCacheLayer, GJBaseGameLayer) {
    void processTransformActions(bool visibleFrame) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const bool useCache = importPatch && importPatch->isEnabled() && !baseline && !reuseFailed() &&
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
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
extern "C" __declspec(dllexport) bool ChloriumReadAreaMathStats(
    chlorium::native::TransformMathStats* output, std::size_t size) {
    using namespace chlorium::native;
    if (!output || size != sizeof(*output)) return false;
    *output = areaMathStats;
    output->installed = importPatch && importPatch->isEnabled();
    return true;
}
class $modify(NativeAreaMathLayer, GJBaseGameLayer) {
    void processAreaActions(float dt, bool visibleFrame) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const bool useCache = importPatch && importPatch->isEnabled() && !baseline &&
            !mathStats.failures && !areaMathStats.failures && !transformPhase &&
            static_cast<GJBaseGameLayer*>(PlayLayer::get()) == static_cast<GJBaseGameLayer*>(this) &&
            Mod::get()->getSettingValue<bool>("enabled");
        if (!useCache) { GJBaseGameLayer::processAreaActions(dt, visibleFrame); return; }
        struct AreaScope {
            AreaScope() {
                areaAtanCache.reset(); areaReusePolicy.reset();
                fpControlPending = true; fpControlSupported = false;
                transformPhase = true; areaPhase = true; ++areaMathStats.phases;
            }
            ~AreaScope() { transformPhase = false; areaPhase = false; }
        } scope;
        GJBaseGameLayer::processAreaActions(dt, visibleFrame);
        // Optional private trace: no file or log traffic unless requested.
        static const bool trace = std::getenv("CHLORIUM_AREA_MATH_TRACE") != nullptr;
        if (trace && (areaMathStats.phases & 255) == 0)
            log::info("Chlorium Area math: phases={} calls={} hits={} misses={} bypassed={} verified={} failures={}",
                areaMathStats.phases, areaMathStats.calls, areaMathStats.hits, areaMathStats.misses,
                areaMathStats.bypassed, areaMathStats.verified, areaMathStats.failures);
    }
};
#endif
