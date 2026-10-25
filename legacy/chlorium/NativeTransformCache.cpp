#include "renderer/NativeTransformMath.hpp"
#include "renderer/NativeTransformStats.hpp"
#include "renderer/NativeDecompositionSites.hpp"
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
#include "renderer/NearCallThunk.hpp"
#endif
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <windows.h>
#include <intrin.h>
#include <float.h>
#include <cstdlib>
#include "renderer/OwnedMathBodyHash.hpp"

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
std::array<Patch*, decompositionCalls.size()> callPatches{};
void* callThunk = nullptr;
bool installationComplete = false, targetedCallsEnabled = false, ownershipChanged = false;
std::array<unsigned char, 16> originalEntry{};
thread_local bool areaPhase = false;
thread_local TransformAtanCache<1024> areaAtanCache;
thread_local TransformReusePolicy areaReusePolicy;
thread_local TransformMathStats areaMathStats;
const bool verifyRequested = std::getenv("CHLORIUM_TRANSFORM_VERIFY") != nullptr;
const bool areaTraceRequested = std::getenv("CHLORIUM_AREA_MATH_TRACE") != nullptr;
const bool collectCallStats = verifyRequested || areaTraceRequested ||
    std::getenv("CHLORIUM_MATH_DIAGNOSTICS") != nullptr;
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

bool cacheInstalled() {
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    return installationComplete && targetedCallsEnabled && !ownershipChanged;
#else
    return importPatch && importPatch->isEnabled();
#endif
}

#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
AtanFunction currentNativeAtan();
AtanFunction nativeAtanRoute();
bool originalEntryMatches();
bool originalThunkMatches();
bool nativeRouteMatches();
void ownershipLost();
#endif

bool reportedCacheInstalled() {
    if (!cacheInstalled()) return false;
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    if (!nativeRouteMatches()) {
        ownershipLost();
        return false;
    }
    for (const auto patch : callPatches) {
        if (!patch || !patch->isEnabled()) return false;
        const auto& bytes = patch->getBytes();
        if (std::memcmp(reinterpret_cast<const void*>(patch->getAddress()), bytes.data(), bytes.size())) {
            ownershipChanged = true;
            targetedCallsEnabled = false;
            return false;
        }
    }
#endif
    return true;
}

#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
AtanFunction currentNativeAtan() {
    // Unlike alpha.6's global interception, this experiment never writes IAT.
    // Each of the four original calls would follow the *current* import too.
    return *reinterpret_cast<AtanFunction volatile*>(gameBase + 0x515398);
}
AtanFunction nativeAtanRoute() {
    // Follow the original game thunk on fallback. Another mod can hook that
    // thunk itself, replace IAT, or hook CRT; a captured CRT pointer alone
    // would bypass the first of those changes.
    return reinterpret_cast<AtanFunction>(gameBase + 0x4d1848);
}
bool originalThunkMatches() {
    constexpr unsigned char expected[]{0xff, 0x25, 0x4a, 0x3b, 0x04, 0x00};
    return !std::memcmp(reinterpret_cast<const void*>(gameBase + 0x4d1848), expected, sizeof(expected));
}
bool originalEntryMatches() {
    return !std::memcmp(reinterpret_cast<const void*>(originalAtan),
        originalEntry.data(), originalEntry.size());
}
bool nativeRouteMatches() {
    return currentNativeAtan() == originalAtan && originalThunkMatches() && originalEntryMatches();
}
void ownershipLost() {
    ownershipChanged = true;
    targetedCallsEnabled = false;
    // Redirected calls can remain during rollback/unload failure: the bridge
    // bypasses all caches and follows the current native import in that case.
}
#endif

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
    const auto hash = mathBodyHash(bytes + 0x22c680 + 32, 0xf30 - 32);
    const unsigned char thunk[]{0xff, 0x25, 0x4a, 0x3b, 0x04, 0x00};
    std::size_t requiredCalls = 2;
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    const auto areaHash = mathBodyHash(bytes + 0x229720 + 32, 0x662 - 32);
    if (areaHash != 0x6680410ab39143d4ull) return false;
    requiredCalls = decompositionCalls.size();
#endif
    for (std::size_t i = 0; i < requiredCalls; ++i) {
        const auto& site = decompositionCalls[i];
        if (std::memcmp(bytes + site.rva, site.expected.data(), site.expected.size())) return false;
    }
    return hash == 0x32a610f5884e6c0cull &&
        !std::memcmp(bytes + 0x4d1848, thunk, sizeof(thunk));
}

template<bool CollectStats = true>
__declspec(noinline) float cachedNativeAtan(float y, float x) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    const auto native = nativeAtanRoute();
    if (!cacheInstalled() || !transformPhase) return native(y, x);
#else
    const auto native = originalAtan;
    // Historical normal build: filter the two callers of the shared import.
    if (!transformPhase) return originalAtan(y, x);
#endif
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    if (!isDecompositionCaller(caller - gameBase, areaPhase)) return native(y, x);
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
    if constexpr (CollectStats) ++selectedStats.calls;
    if (selectedPolicy.disabled()) {
        if constexpr (CollectStats) ++selectedStats.bypassed;
        return native(y, x);
    }
    if (fpControlPending) {
        unsigned fpControl = 0;
        _controlfp_s(&fpControl, 0, 0);
        fpControlSupported = (fpControl & _MCW_EM) == _MCW_EM && (fpControl & _MCW_RC) == _RC_NEAR;
        fpControlPending = false;
    }
    if (!fpControlSupported) {
        if constexpr (CollectStats) ++selectedStats.bypassed;
        return native(y, x);
    }
    const unsigned csr = _mm_getcsr();
    if ((csr & 0x1f80u) != 0x1f80u) {
        if constexpr (CollectStats) ++selectedStats.bypassed;
        return native(y, x);
    }
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    // Owner checks protect elided native calls. A disabled/unsupported cache
    // above already follows the current native thunk, so these checks would
    // only add cost to a path that cannot reuse anything (24M calls in Orbit).
    if (!nativeRouteMatches()) { ownershipLost(); return native(y, x); }
#endif
    bool hit = false;
    const auto result = selectedCache.evaluate(y, x, csr & 0xffc0u, originalAtan, &hit);
    if constexpr (CollectStats) {
        if (hit) ++selectedStats.hits;
        else ++selectedStats.misses;
    }
    if (!selectedPolicy.record(hit)) {
        if constexpr (CollectStats) ++selectedStats.lowReusePhases;
    }
    if constexpr (CollectStats) {
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
        const bool verifyHit = verifyRequested && hit && (selectedStats.hits - 1) % verificationStride == 0;
#else
        static const bool verify = std::getenv("CHLORIUM_TRANSFORM_VERIFY") != nullptr;
        const bool verifyHit = verify && hit;
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
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod == Mod::get()) continue;
        for (const auto hook : mod->getHooks()) {
            if (hook->isEnabled() && hook->getAddress() == reinterpret_cast<std::uintptr_t>(expected)) {
                log::warn("Area math: CRT atan2f is hooked; targeted reuse not installed");
                return;
            }
        }
    }
    std::memcpy(originalEntry.data(), reinterpret_cast<const void*>(originalAtan), originalEntry.size());
    std::array<std::uintptr_t, decompositionCalls.size()> sites{};
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto& site = decompositionCalls[i];
        sites[i] = gameBase + site.rva;
        if (std::memcmp(reinterpret_cast<const void*>(sites[i]), site.expected.data(), site.expected.size())) {
            log::warn("Area math: a targeted native call was modified; no patch installed");
            return;
        }
    }
    const auto bridge = collectCallStats ? &cachedNativeAtan<true> : &cachedNativeAtan<false>;
    callThunk = allocateNearCallThunk(sites, reinterpret_cast<std::uintptr_t>(bridge), gameBase + 0xa3a000);
    if (!callThunk) { log::warn("Area math: nearby call bridge unavailable; native calls retained"); return; }
    const auto disableAll = [] {
        targetedCallsEnabled = false;
        // Geode's disable() writes the saved original bytes unconditionally.
        // Refuse that write if an external raw patch has since taken a site.
        for (const auto patch : callPatches) {
            if (!patch || !patch->isEnabled()) continue;
            const auto& bytes = patch->getBytes();
            if (std::memcmp(reinterpret_cast<const void*>(patch->getAddress()), bytes.data(), bytes.size())) {
                ownershipChanged = true;
                log::warn("Area math: a call site changed owner; cache disabled without overwriting it");
                return;
            }
        }
        for (const auto patch : callPatches) {
            if (patch && patch->isEnabled()) {
                if (auto result = patch->disable(); !result)
                    log::warn("Area math: call rollback failed; remaining bridge stays in native bypass mode");
            }
        }
    };
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto delta = static_cast<std::int32_t>(std::int64_t(reinterpret_cast<std::uintptr_t>(callThunk)) -
                                                    std::int64_t(sites[i] + 5));
        ByteVector bytes{0xe8, 0, 0, 0, 0};
        std::memcpy(bytes.data() + 1, &delta, sizeof(delta));
        auto patched = Mod::get()->patch(reinterpret_cast<void*>(sites[i]), std::move(bytes));
        if (!patched) {
            disableAll();
            log::warn("Area math: partial call installation rolled back; reuse disabled");
            return;
        }
        callPatches[i] = patched.unwrap();
    }
    installationComplete = true;
    const auto toggle = [disableAll](bool requested) {
        targetedCallsEnabled = false;
        if (!requested || ownershipChanged || !installationComplete) { disableAll(); return; }
        // A mod may have claimed the import or CRT while reuse was disabled.
        // Recheck before enabling any call, not only at the next cached call.
        if (!nativeRouteMatches()) { ownershipLost(); disableAll(); return; }
        for (std::size_t i = 0; i < callPatches.size(); ++i) {
            const auto patch = callPatches[i];
            const auto& activeBytes = patch->getBytes();
            const auto& nativeBytes = decompositionCalls[i].expected;
            const auto bytes = patch->isEnabled() ? activeBytes.data() : nativeBytes.data();
            const auto count = patch->isEnabled() ? activeBytes.size() : nativeBytes.size();
            if (std::memcmp(reinterpret_cast<const void*>(patch->getAddress()), bytes, count)) {
                ownershipChanged = true;
                disableAll();
                return;
            }
        }
        for (const auto patch : callPatches) {
            if (auto result = patch->enable(); !result) { disableAll(); return; }
        }
        targetedCallsEnabled = true;
    };
    toggle(Mod::get()->getSettingValue<bool>("enabled"));
    listenForSettingChanges<bool>("enabled", toggle);
    // Retain the immutable thunk while Geode owns patches referring to it.
    // Freeing it after a failed rollback would leave an executable dangling call.
    log::info("Area math: four targeted calls prepared; cache enabled={}, per-call counters={}",
        cacheInstalled(), collectCallStats);
#else
    const auto replacement = reinterpret_cast<uintptr_t>(&cachedNativeAtan<>);
    ByteVector bytes(sizeof(replacement));
    std::memcpy(bytes.data(), &replacement, sizeof(replacement));
    auto patched = Mod::get()->patch(slot, std::move(bytes));
    if (!patched) { log::warn("Transform cache import patch failed: {}", patched.unwrapErr()); return; }
    importPatch = patched.unwrap();
    log::info("Exact native transform cache installed (GD 2.2081)");
#endif
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
    output->installed = reportedCacheInstalled();
    return true;
}

$execute { chlorium::native::installTransformCache(); }

class $modify(NativeTransformCacheLayer, GJBaseGameLayer) {
    void processTransformActions(bool visibleFrame) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const bool useCache = cacheInstalled() && !baseline && !reuseFailed() &&
            !transformPhase && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == static_cast<GJBaseGameLayer*>(this)
#ifndef CHLORIUM_EXPERIMENTAL_AREA_MATH
            && Mod::get()->getSettingValue<bool>("enabled")
#endif
            ;
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
extern "C" __declspec(dllexport) bool ChloriumReadMathPatchStateV1(
    chlorium::native::MathPatchStateV1* output, std::size_t size) {
    using namespace chlorium::native;
    if (!output || size != sizeof(*output)) return false;
    *output = MathPatchStateV1{};
    for (const auto patch : callPatches) {
        if (!patch || !patch->isEnabled()) continue;
        const auto& bytes = patch->getBytes();
        if (!std::memcmp(reinterpret_cast<const void*>(patch->getAddress()), bytes.data(), bytes.size()))
            ++output->activeSites;
    }
    output->cacheEnabled = reportedCacheInstalled() && !reuseFailed();
    output->perCallCounters = collectCallStats;
    output->verificationStride = verifyRequested ? verificationStride : 0;
    output->ownershipLost = ownershipChanged;
    return true;
}
extern "C" __declspec(dllexport) bool ChloriumReadAreaMathStats(
    chlorium::native::TransformMathStats* output, std::size_t size) {
    using namespace chlorium::native;
    if (!output || size != sizeof(*output)) return false;
    *output = areaMathStats;
    output->installed = reportedCacheInstalled();
    return true;
}
class $modify(NativeAreaMathLayer, GJBaseGameLayer) {
    void processAreaActions(float dt, bool visibleFrame) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const bool useCache = cacheInstalled() && !baseline &&
            !mathStats.failures && !areaMathStats.failures && !transformPhase &&
            static_cast<GJBaseGameLayer*>(PlayLayer::get()) == static_cast<GJBaseGameLayer*>(this);
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
        if (areaTraceRequested && (areaMathStats.phases & 255) == 0)
            log::info("Chlorium Area math: phases={} calls={} hits={} misses={} bypassed={} verified={} failures={}",
                areaMathStats.phases, areaMathStats.calls, areaMathStats.hits, areaMathStats.misses,
                areaMathStats.bypassed, areaMathStats.verified, areaMathStats.failures);
    }
};
#endif
