#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_MOVES
#include "NativeMovementLoop.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <intrin.h>
#include <windows.h>
#include <cstdlib>
#include <bit>

using namespace geode::prelude;
namespace chlorium::native {
namespace {
NativeMovementStats stats;
static_assert(offsetof(GameObject, m_outerSectionIndex) == 0x278);
static_assert(offsetof(GameObject, m_tempOffsetXRelated) == 0x2c8);
static_assert(offsetof(GameObject, m_isObjectPosDirty) == 0x351);
static_assert(offsetof(GameObject, m_isUnmodifiedPosDirty) == 0x352);
static_assert(offsetof(GameObject, m_isObjectRectDirty) == 0x368);
static_assert(offsetof(GameObject, m_isOrientedBoxDirty) == 0x369);
static_assert(offsetof(GameObject, m_positionX) == 0x3b8);
static_assert(offsetof(GameObject, m_positionY) == 0x3c0);
static_assert(offsetof(GameObject, m_lastPosition) == 0x4d0);
static_assert(offsetof(GameObject, m_unk4C4) == 0x4dc);
static_assert(offsetof(GameObject, m_isDecoration2) == 0x512);
static_assert(offsetof(GJBaseGameLayer, m_movedCount) == 0x3724);
static_assert(offsetof(GJBaseGameLayer, m_gameState) +
    offsetof(decltype(GJBaseGameLayer::m_gameState), m_commandIndex) == 0x3e0);

bool nativeContract() {
    static const bool valid = [] {
        const auto base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != 0x69702da0 ||
            pe->OptionalHeader.SizeOfImage != 0xa3a000) return false;
        auto hash = [&](size_t rva, size_t size) {
            uint64_t value = 14695981039346656037ull;
            for (size_t i = 32; i < size; ++i) { value ^= base[rva + i]; value *= 1099511628211ull; }
            return value;
        };
        return hash(0x22dd50, 0x180) == 0x98f38c9054902d57ull &&
               hash(0x227f50, 0x117) == 0x36c19a0a704c9a34ull;
    }();
    return valid;
}
bool otherMovementHooks() {
    static unsigned checked = ~0u;
    static bool conflict = true;
    const auto frame = CCDirector::get()->getTotalFrames();
    if (frame == checked) return conflict;
    checked = frame; conflict = false;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod == Mod::get()) continue;
        for (const auto hook : mod->getHooks())
            if (hook->isEnabled() && (hook->getAddress() == base + 0x22dd50 ||
                hook->getAddress() == base + 0x227f50)) return conflict = true;
    }
    return conflict;
}
}
NativeMovementStats getNativeMovementStats() { return stats; }
}

class $modify(NativeMovementLayer, GJBaseGameLayer) {
    void moveObjects(CCArray* objects, double dx, double dy, bool lockPlayerY) {
        using namespace chlorium::native;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        const auto vtable = *reinterpret_cast<const uintptr_t* const*>(this);
        static const auto sectionFunction = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + 0x227f50;
        if (baseline || !objects || objects->count() < 64 ||
            static_cast<GJBaseGameLayer*>(this) != static_cast<GJBaseGameLayer*>(PlayLayer::get()) ||
            !Mod::get()->getSettingValue<bool>("enabled") || !nativeContract() ||
            vtable[0x490 / sizeof(void*)] != sectionFunction || otherMovementHooks()) {
            ++stats.fallbackCalls;
            GJBaseGameLayer::moveObjects(objects, dx, dy, lockPlayerY); return;
        }
        const auto count = objects->count();
        const auto data = objects->data->arr;
        // Native instrumentation wraps its 32-bit counter, even at a null entry.
        m_movedCount = std::bit_cast<int>(std::bit_cast<unsigned>(m_movedCount) + count);
        unsigned long long visited = 0, skipped = 0;
        for (unsigned i = 0; i < count; ++i) {
            auto object = static_cast<GameObject*>(data[i]);
            if (!object) break; // Native loop stops at the first null.
            if (i + 4 < count && data[i + 4]) {
                const auto next = reinterpret_cast<const char*>(data[i + 4]);
                _mm_prefetch(next + 0x240, _MM_HINT_T0);
                _mm_prefetch(next + 0x340, _MM_HINT_T0);
                _mm_prefetch(next + 0x380, _MM_HINT_T0);
                _mm_prefetch(next + 0x3c0, _MM_HINT_T0);
                _mm_prefetch(next + 0x4c0, _MM_HINT_T0);
                _mm_prefetch(next + 0x500, _MM_HINT_T0);
            }
            ++visited;
            if (advanceNativeMove(*object, m_gameState.m_commandIndex, dx, dy)) updateObjectSection(object);
            else ++skipped;
        }
        updateDisabledObjectsLastPos(objects);
        ++stats.fastCalls; stats.objectVisits += visited; stats.avoidedSectionCalls += skipped;
    }
};
#endif
