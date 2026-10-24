#ifdef CHLORIUM_EXPERIMENTAL_MATRIX_REUSE
#include "MatrixReusePolicy.hpp"
#include "NativeMatrixStats.hpp"
#include "NativeRotationReuse.hpp"
#include "NearCallSlot.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <float.h>
#include <cstdlib>
#include "OwnedMathBodyHash.hpp"
using namespace geode::prelude;

namespace chlorium::matrix {
namespace {
static_assert(sizeof(CCAffineTransform) == 24);
static_assert(offsetof(CCNode, m_fRotationX) == 0x38);
static_assert(offsetof(CCNode, m_fRotationY) == 0x3c);
static_assert(offsetof(CCNode, m_fScaleX) == 0x40);
static_assert(offsetof(CCNode, m_fScaleY) == 0x44);
static_assert(offsetof(CCNode, m_obPosition) == 0x4c);
static_assert(offsetof(CCNode, m_fSkewX) == 0x54);
static_assert(offsetof(CCNode, m_obAnchorPointInPoints) == 0x5c);
static_assert(offsetof(CCNode, m_sTransform) == 0x8c);
static_assert(offsetof(CCNode, m_bTransformDirty) == 0x111);
static_assert(offsetof(CCNode, m_bPositionDirty) == 0x112);
static_assert(offsetof(CCNode, m_fTransformX) == 0x114);
static_assert(offsetof(CCNode, m_bAdditionalTransformDirty) == 0x11d);
static_assert(offsetof(CCNode, m_bIgnoreAnchorPointForPosition) == 0x11f);
static_assert(offsetof(GJBaseGameLayer, m_areaTransformNode2) == 0x8e8);

// Win64 member-return ABI: this RCX, caller's 24-byte result buffer RDX,
// return buffer address RAX. A free function returning a struct reverses
// those first two arguments, so this bridge uses explicit pointer arguments.
using NodeTransform = CCAffineTransform* (*)(CCNode*, CCAffineTransform*);
using TrigFunction = float (*)(float);
TrigFunction nativeCosine = nullptr, nativeSine = nullptr;
std::uintptr_t gameBase = 0, cocosBase = 0;
std::array<Patch*, 2> callPatches{};
void* callSlot = nullptr; // retained while Geode can enable the owned patches
bool installationComplete = false, enabled = false;
const bool forceNative = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;

struct Entry {
    std::array<std::uint32_t, 4> inputs{};
    std::uint32_t controls = 0, epoch = 0;
    CCAffineTransform value{};
};
thread_local Stats stats;
thread_local Details details;
thread_local PersistenceStats persistence;
thread_local std::array<Entry, 256> entries;
thread_local Entry* recentMatrix = nullptr;
thread_local std::uint32_t epoch = 1;
thread_local RotationCache rotations;
thread_local CCNode* activeMatrixNode = nullptr;
thread_local std::array<StreamReusePolicy, 2> streams;
thread_local StreamReusePolicy* activePolicy = nullptr;
thread_local std::uint32_t activeFrame = 0, lastFrame = 0;
thread_local GJBaseGameLayer* cacheOwner = nullptr;
thread_local CCNode* cacheNode = nullptr;
thread_local bool contextValid = false;
void invalidateCaches();

void recordReuse(bool hit) {
    if (activePolicy->record(hit, activeFrame)) ++details.lowReusePhases;
}

std::uint64_t hash(const unsigned char* bytes, std::size_t count) {
    std::uint64_t value = 14695981039346656037ull;
    for (std::size_t i = 0; i < count; ++i) { value ^= bytes[i]; value *= 1099511628211ull; }
    return value;
}
bool validModule(std::uintptr_t address, unsigned timestamp, unsigned imageBytes) {
    if (!address) return false;
    const auto base = reinterpret_cast<const unsigned char*>(address);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
        pe->FileHeader.TimeDateStamp == timestamp && pe->OptionalHeader.SizeOfImage == imageBytes;
}
bool nativeBodiesMatch() {
    if (!validModule(cocosBase, 0x69702d99, 0x1c5000) || !validModule(gameBase, 0x69702da0, 0xa3a000)) return false;
    const auto cocos = reinterpret_cast<const unsigned char*>(cocosBase);
    const auto game = reinterpret_cast<const unsigned char*>(gameBase);
    // No CCNode hook: require its whole body. Only the entry detours of our
    // two GJBaseGameLayer hooks are excluded from their respective hashes.
    if (hash(cocos + 0x37f0, 0x468) != 0x7abc292bc9db4855ull ||
        hash(cocos + 0x6560, 0x33) != 0x5b70839e8e476f88ull ||
        chlorium::native::mathBodyHash(game + 0x22c680 + 32, 0xf30 - 32) != 0x32a610f5884e6c0cull ||
        chlorium::native::mathBodyHash(game + 0x229720 + 32, 0x662 - 32) != 0x6680410ab39143d4ull ||
        hash(cocos + 0xe0564, 12) != 0x829282b09b619ee7ull) return false;
    const auto constant = [cocos](std::size_t rva) {
        std::uint32_t value; std::memcpy(&value, cocos + rva, 4); return value;
    };
    return constant(0xe61a4) == 0x3c8efa35u && constant(0xe61a8) == 0x3f800000u && constant(0xe61c0) == 0x80000000u;
}
bool conflictingHooks() {
    static unsigned checked = ~0u;
    static bool conflict = true;
    const auto director = CCDirector::get();
    if (!director) return true;
    const auto frame = director->getTotalFrames();
    if (frame == checked) return conflict;
    checked = frame; conflict = false;
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod == Mod::get()) continue;
        // The independent audit's named timer only forwards this outer call.
        // Accept that specific wrapper, not every hook owned by the audit mod.
        bool passiveTransformTimer = false;
        if (mod->getID() == "local.chlorium.audit") {
            const auto audit = GetModuleHandleW(mod->getBinaryPath().c_str());
            const auto marker = audit ? GetProcAddress(audit, "chloriumPassiveTransformTimingIdentity") : nullptr;
            constexpr char expected[] = "CHLORIUM_PASSIVE_TRANSFORM_TIMER_V1";
            passiveTransformTimer = marker && !std::memcmp(reinterpret_cast<const void*>(marker), expected, sizeof(expected));
        }
        for (const auto hook : mod->getHooks()) {
            const auto address = hook->getAddress();
            if (passiveTransformTimer && address == gameBase + 0x22c680 &&
                hook->getDisplayName() == "GJBaseGameLayer::processTransformActions") continue;
            if (hook->isEnabled() && (address == cocosBase + 0x37f0 || address == cocosBase + 0x6560 ||
                address == reinterpret_cast<std::uintptr_t>(nativeCosine) || address == reinterpret_cast<std::uintptr_t>(nativeSine) ||
                address == gameBase + 0x22c680 || address == gameBase + 0x229720)) return conflict = true;
        }
    }
    return conflict = *reinterpret_cast<TrigFunction*>(cocosBase + 0xe4ac8) != nativeCosine ||
                      *reinterpret_cast<TrigFunction*>(cocosBase + 0xe4ad0) != nativeSine;
}

CCAffineTransform* cachedNodeTransform(CCNode* node, CCAffineTransform* output) {
    const auto original = reinterpret_cast<NodeTransform>((*reinterpret_cast<std::uintptr_t**>(node))[0x308 / 8]);
    const auto fallback = [&]() { ++details.nativeCalls; return original(node, output); };
    if (!enabled || node != activeMatrixNode || reinterpret_cast<std::uintptr_t>(original) != cocosBase + 0x37f0 ||
        !node->m_bTransformDirty || !activePolicy || activePolicy->disabled()) return fallback();
    if (node->m_bAdditionalTransformDirty || node->m_bIgnoreAnchorPointForPosition ||
        node->m_obAnchorPointInPoints.x != 0 || node->m_obAnchorPointInPoints.y != 0 ||
        node->m_fSkewX != 0 || node->m_fSkewY != 0 ||
        !supportedAngle(node->m_fRotationX) || !supportedAngle(node->m_fRotationY) ||
        !finiteBits(node->m_fScaleX) || !finiteBits(node->m_fScaleY)) {
        ++stats.unsupported; recordReuse(false); return fallback();
    }
    const auto controls = _mm_getcsr() & 0xffc0u;
    if ((controls & 0x1f80u) != 0x1f80u) { recordReuse(false); return fallback(); }
    const std::array<std::uint32_t, 4> key{std::bit_cast<std::uint32_t>(node->m_fRotationX),
        std::bit_cast<std::uint32_t>(node->m_fRotationY), std::bit_cast<std::uint32_t>(node->m_fScaleX),
        std::bit_cast<std::uint32_t>(node->m_fScaleY)};
    const auto matches = [&](const Entry* entry) {
        return entry && entry->epoch == epoch && entry->controls == controls && entry->inputs == key;
    };
    const bool recent = matches(recentMatrix);
    Entry* selected = recentMatrix;
    if (!recent) {
        std::uint64_t index = controls;
        for (const auto word : key) index = (index ^ word) * 0x9e3779b97f4a7c15ull;
        selected = &entries[(index ^ (index >> 32)) & 255];
    }
    auto& entry = *selected;
    ++stats.eligible;
    CCAffineTransform result;
    bool usefulReuse = false;
    if (recent || matches(selected)) {
        ++stats.hits; usefulReuse = true; result = entry.value; recentMatrix = selected;
        if (recent) ++persistence.recentMatrixHits;
    } else {
        ++stats.misses;
        const auto linear = calculateLinear(node->m_fRotationX, node->m_fRotationY,
            node->m_fScaleX, node->m_fScaleY, controls, rotations, nativeCosine, nativeSine);
        details.rotationHits += linear.hits; details.rotationMisses += linear.misses;
        if (linear.zero) ++details.zeroRotations;
        usefulReuse = linear.zero || linear.hits != 0;
        result.a = linear.coefficients[0]; result.b = linear.coefficients[1];
        result.c = linear.coefficients[2]; result.d = linear.coefficients[3];
        result.tx = result.ty = 0;
        if (finiteBits(result.a) && finiteBits(result.b) && finiteBits(result.c) && finiteBits(result.d)) {
            entry = {key, controls, epoch, result}; recentMatrix = selected;
        }
    }
    recordReuse(usefulReuse);
    result.tx = node->m_obPosition.x; result.ty = node->m_obPosition.y;
    node->m_sTransform = result;
    node->m_fTransformX = node->m_obPosition.x; node->m_fTransformY = node->m_obPosition.y;
    node->m_bTransformDirty = node->m_bPositionDirty = false;
    *output = result;
    return output;
}

void install() {
    gameBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    cocosBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"libcocos2d.dll"));
    if (!nativeBodiesMatch()) { log::warn("Matrix experiment: unknown/modified native body; no patches installed"); return; }
    const auto runtime = GetModuleHandleW(L"ucrtbase.dll");
    nativeCosine = runtime ? reinterpret_cast<TrigFunction>(GetProcAddress(runtime, "cosf")) : nullptr;
    nativeSine = runtime ? reinterpret_cast<TrigFunction>(GetProcAddress(runtime, "sinf")) : nullptr;
    if (!nativeCosine || !nativeSine || *reinterpret_cast<TrigFunction*>(cocosBase + 0xe4ac8) != nativeCosine ||
        *reinterpret_cast<TrigFunction*>(cocosBase + 0xe4ad0) != nativeSine) {
        log::warn("Matrix experiment: native math imports are unavailable/modified"); return;
    }
    const std::array<std::uintptr_t, 2> sites{gameBase + 0x22d00d, gameBase + 0x229a59};
    const unsigned char expected[]{0xff, 0x90, 0x08, 0x03, 0x00, 0x00};
    for (auto site : sites) if (std::memcmp(reinterpret_cast<void*>(site), expected, sizeof(expected))) return;
    callSlot = allocateNearCallSlot(sites, reinterpret_cast<std::uintptr_t>(&cachedNodeTransform), gameBase + 0xa3a000);
    if (!callSlot) { log::warn("Matrix experiment: no nearby pointer slot; native calls retained"); return; }
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto delta = std::int64_t(reinterpret_cast<std::uintptr_t>(callSlot)) - std::int64_t(sites[i] + 6);
        if (delta < INT32_MIN || delta > INT32_MAX) break;
        ByteVector bytes{0xff, 0x15, 0, 0, 0, 0};
        const auto relative = static_cast<std::int32_t>(delta);
        std::memcpy(bytes.data() + 2, &relative, sizeof(relative));
        auto patch = Mod::get()->patch(reinterpret_cast<void*>(sites[i]), std::move(bytes));
        if (!patch) { log::warn("Matrix call-site patch failed: {}", patch.unwrapErr()); break; }
        callPatches[i] = patch.unwrap();
    }
    installationComplete = callPatches[0] && callPatches[1];
    const auto toggle = [](bool requested) {
        enabled = false;
        invalidateCaches();
        for (const auto patch : callPatches) {
            if (!patch) continue;
            if (auto result = patch->toggle(requested && installationComplete && !forceNative); !result) {
                for (const auto previous : callPatches) if (previous) (void)previous->disable();
                log::warn("Matrix experiment: call-site toggle failed; reuse disabled"); return;
            }
        }
        enabled = requested && installationComplete && !forceNative;
    };
    toggle(Mod::get()->getSettingValue<bool>("enabled"));
    if (!installationComplete) { log::warn("Matrix experiment: partial installation rolled back"); return; }
    listenForSettingChanges<bool>("enabled", toggle);
    log::info("Matrix experiment: two targeted call sites, rotation reuse and Area transform scope installed");
}
bool canReuse(GJBaseGameLayer* layer, unsigned stream) {
    if (!enabled || activeMatrixNode || !layer->m_areaTransformNode2 ||
        layer != static_cast<GJBaseGameLayer*>(PlayLayer::get())) return false;
    const auto director = CCDirector::get();
    if (!director) return false;
    const auto frame = static_cast<std::uint32_t>(director->getTotalFrames());
    const bool sameContext = contextValid && cacheOwner == layer && cacheNode == layer->m_areaTransformNode2 &&
        std::bit_cast<std::int32_t>(frame - lastFrame) >= 0;
    // A bypassed phase does not consume cached values. Avoid FP queries and
    // hook inventory work until a probe is due; that probe validates both.
    if (sameContext && streams[stream].blockedAt(frame)) {
        lastFrame = frame;
        if (stream == 0) ++persistence.skippedTransformPhases; else ++persistence.skippedAreaPhases;
        return false;
    }
    if (conflictingHooks()) { if (contextValid) invalidateCaches(); return false; }
    unsigned fp = 0; _controlfp_s(&fp, 0, 0);
    if ((fp & _MCW_EM) != _MCW_EM || (fp & _MCW_RC) != _RC_NEAR) {
        if (contextValid) invalidateCaches(); return false;
    }
    if (!sameContext) {
        invalidateCaches(); contextValid = true; cacheOwner = layer; cacheNode = layer->m_areaTransformNode2;
    }
    lastFrame = frame;
    if (!streams[stream].begin(frame)) {
        if (stream == 0) ++persistence.skippedTransformPhases; else ++persistence.skippedAreaPhases;
        return false;
    }
    return true;
}
void invalidateCaches() {
    if (++epoch == 0) { for (auto& entry : entries) entry.epoch = 0; epoch = 1; }
    recentMatrix = nullptr; rotations.reset();
    for (auto& stream : streams) stream.reset();
    contextValid = false; cacheOwner = nullptr; cacheNode = nullptr;
    ++persistence.invalidations;
}
struct Scope {
    CCNode* previous;
    StreamReusePolicy* previousPolicy;
    std::uint32_t previousFrame;
    Scope(CCNode* node, unsigned stream) : previous(activeMatrixNode), previousPolicy(activePolicy), previousFrame(activeFrame) {
        activeMatrixNode = node; activePolicy = &streams[stream]; activeFrame = lastFrame; ++stats.phases;
    }
    // Cached values depend only on math inputs/FP controls, never on an object
    // address, its translation, a group ID, or a previous frame's visibility.
    ~Scope() { activeMatrixNode = previous; activePolicy = previousPolicy; activeFrame = previousFrame; }
};
struct Suspend {
    CCNode* previous = activeMatrixNode;
    Suspend() { activeMatrixNode = nullptr; }
    ~Suspend() {
        // Reentrant code may change math hooks or FP state. Keep the remainder
        // of the outer native loop on its original path until the next scope
        // can validate the environment again.
        if (previous) { invalidateCaches(); activePolicy = nullptr; }
        activeMatrixNode = nullptr;
    }
};
} // namespace
} // namespace chlorium::matrix

extern "C" __declspec(dllexport) bool ChloriumReadMatrixStats(chlorium::matrix::Stats* output, std::size_t size) {
    if (!output || size != sizeof(*output)) return false;
    *output = chlorium::matrix::stats; return true;
}
extern "C" __declspec(dllexport) bool ChloriumReadMatrixDetails(chlorium::matrix::Details* output, std::size_t size) {
    using namespace chlorium::matrix;
    if (!output || size != sizeof(*output)) return false;
    *output = details;
    output->installedSites = 0;
    for (const auto patch : callPatches) if (patch && patch->isEnabled()) ++output->installedSites;
    return true;
}
$execute { chlorium::matrix::install(); }

extern "C" __declspec(dllexport) bool ChloriumReadMatrixSnapshotV1(chlorium::matrix::Snapshot* output, std::size_t size) {
    using namespace chlorium::matrix;
    if (!output || size != sizeof(*output)) return false;
    *output = Snapshot{}; output->enabled = enabled ? 1u : 0u;
    output->matrices = stats; output->rotations = details; output->persistence = persistence;
    output->persistence.recentRotationHits = rotations.recentHits();
    for (const auto patch : callPatches) if (patch && patch->isEnabled()) ++output->rotations.installedSites;
    return true;
}

class $modify(ExperimentalMatrixReuseLayer, GJBaseGameLayer) {
    void processTransformActions(bool visible) {
        using namespace chlorium::matrix;
        if (!canReuse(this, 0)) { Suspend suspend; GJBaseGameLayer::processTransformActions(visible); return; }
        Scope scope(m_areaTransformNode2, 0); ++details.transformPhases;
        GJBaseGameLayer::processTransformActions(visible);
    }
    void transformAreaObjects(GameObject* object, CCArray* objects, float scaleX, float scaleY, bool reset) {
        using namespace chlorium::matrix;
        if (!canReuse(this, 1)) { Suspend suspend; GJBaseGameLayer::transformAreaObjects(object, objects, scaleX, scaleY, reset); return; }
        Scope scope(m_areaTransformNode2, 1); ++details.areaPhases;
        GJBaseGameLayer::transformAreaObjects(object, objects, scaleX, scaleY, reset);
    }
};
#endif
