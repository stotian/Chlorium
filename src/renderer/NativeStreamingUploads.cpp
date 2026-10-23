#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING
#include "StreamingUpload.hpp"
#include "NativeAtlasUploads.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/cocos/platform/win32/CCGL.h>
#include <windows.h>
#include <cstdlib>

using namespace geode::prelude;
namespace chlorium::streaming {
namespace {
constexpr std::size_t quadBytes = sizeof(ccV3F_C4B_T2F_Quad);
constexpr std::size_t minimumBytes = 64 * 1024, maximumBytes = 32 * 1024 * 1024;
static_assert(sizeof(void*) == 8 && CC_TEXTURE_ATLAS_USE_VAO == 0 && quadBytes == 96);
static_assert(offsetof(CCTextureAtlas, m_pBuffersVBO) == 0x40);
static_assert(offsetof(CCTextureAtlas, m_bDirty) == 0x48);
static_assert(offsetof(CCTextureAtlas, m_uCapacity) == 0x50);
static_assert(offsetof(CCTextureAtlas, m_pQuads) == 0x60);
chlorium::native::AtlasUploadStats atlasStats;
struct Stats {
    uint64_t eligible = 0, mapped = 0, orphaned = 0, fallbacks = 0;
    uint64_t corruptMappings = 0, bytesMapped = 0;
    unsigned mode = 0;
} stats;
Mode selectedMode() {
    static const auto mode = [] {
        const auto value = std::getenv("CHLORIUM_STREAM_MODE");
        return value && std::strcmp(value, "map") == 0 ? Mode::MappedRange : Mode::OrphanFullStore;
    }();
    return mode;
}
bool compatibleEngine() {
    static const bool result = [] {
        const auto module = GetModuleHandleW(L"libcocos2d.dll");
        if (!module) return false;
        const auto base = reinterpret_cast<const unsigned char*>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
        const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            pe->FileHeader.TimeDateStamp != 0x69702d99 || pe->OptionalHeader.SizeOfImage != 0x1c5000) return false;
        if (reinterpret_cast<const unsigned char*>(GetProcAddress(module,
                "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z")) != base + 0xa6350 ||
            reinterpret_cast<const unsigned char*>(GetProcAddress(module,
                "?mapBuffers@CCTextureAtlas@cocos2d@@AEAAXXZ")) != base + 0xa69c0) return false;
        uint64_t hash = 14695981039346656037ull;
        for (size_t i = 0x20; i < 0x15f; ++i) { hash ^= base[0xa6350 + i]; hash *= 1099511628211ull; }
        return hash == 0xc9d0f08451f64908ull;
    }();
    return result;
}
bool conflictingDrawHook() {
    static unsigned checked = ~0u;
    static bool conflict = true;
    const auto director = CCDirector::get();
    if (!director) return true;
    const auto frame = director->getTotalFrames();
    if (frame == checked) return conflict;
    checked = frame; conflict = false;
    const auto address = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"libcocos2d.dll"),
        "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod == Mod::get()) continue;
        for (const auto hook : mod->getHooks())
            if (hook->isEnabled() && hook->getAddress() == address) return conflict = true;
    }
    return conflict;
}
struct BufferApi {
    void* mapWriteInvalidateRange(size_t bytes) {
        return glMapBufferRange(GL_ARRAY_BUFFER, 0, bytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    }
    bool unmap() { return glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE; }
    void orphan(size_t bytes) { glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW); }
};
}
}
namespace chlorium::native {
AtlasUploadStats getAtlasUploadStats() { return streaming::atlasStats; }
void purgeAtlasUploadCache() {} // No CPU shadows in this backend.
}
extern "C" __declspec(dllexport) bool ChloriumReadAtlasUploadStats(
    chlorium::native::AtlasUploadStats* output, size_t bytes) {
    if (!output || bytes != sizeof(*output)) return false;
    *output = chlorium::native::getAtlasUploadStats(); return true;
}
extern "C" __declspec(dllexport) bool ChloriumReadStreamingStats(
    chlorium::streaming::Stats* output, size_t bytes) {
    if (!output || bytes != sizeof(*output)) return false;
    *output = chlorium::streaming::stats; return true;
}
class $modify(ExperimentalNativeStreamingAtlas, CCTextureAtlas) {
    void drawNumberOfQuads(unsigned n, unsigned start) {
        using namespace chlorium::streaming;
        const auto bytes = static_cast<size_t>(n) * quadBytes;
        static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
        if (!m_bDirty || !n || start || n != m_uTotalQuads || n > m_uCapacity || !m_pQuads || !m_pBuffersVBO[0] ||
            bytes < minimumBytes || bytes > maximumBytes || baseline || !compatibleEngine() ||
            !Mod::get()->getSettingValue<bool>("enabled") || conflictingDrawHook() || !wglGetCurrentContext()) {
            CCTextureAtlas::drawNumberOfQuads(n, start); return;
        }
        const auto mode = selectedMode();
        stats.mode = static_cast<unsigned>(mode);
        if (mode == Mode::MappedRange && (!glMapBufferRange || !glUnmapBuffer)) {
            ++stats.fallbacks; CCTextureAtlas::drawNumberOfQuads(n, start); return;
        }
        ++stats.eligible; ++atlasStats.eligibleUploads; ++atlasStats.draws;
        const auto capacityBytes = static_cast<size_t>(m_uCapacity) * quadBytes;
        if (mode == Mode::OrphanFullStore && bytes < 1024 * 1024) {
            ++stats.fallbacks; CCTextureAtlas::drawNumberOfQuads(n, start); return;
        }
        if (mode == Mode::OrphanFullStore && bytes != capacityBytes) {
            ++stats.fallbacks; CCTextureAtlas::drawNumberOfQuads(n, start); return;
        }
        GLint previous = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
        glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
        BufferApi api;
        const auto result = prepareUpload(api, m_pQuads, bytes, capacityBytes, mode);
        glBindBuffer(GL_ARRAY_BUFFER, previous);
        if (result == chlorium::streaming::Result::Uploaded) {
            ++stats.mapped; stats.bytesMapped += bytes;
            m_bDirty = false;
        } else if (result == chlorium::streaming::Result::CorruptMapping) {
            ++stats.corruptMappings;
            // Failed unmap leaves the store undefined: use Cocos' native
            // storage recovery, then keep the original dirty upload enabled.
            static const auto remap = reinterpret_cast<void(*)(CCTextureAtlas*)>(GetProcAddress(
                GetModuleHandleW(L"libcocos2d.dll"), "?mapBuffers@CCTextureAtlas@cocos2d@@AEAAXXZ"));
            remap(this); m_bDirty = true;
        } else if (mode == Mode::OrphanFullStore && bytes == capacityBytes) ++stats.orphaned;
        else ++stats.fallbacks;
        ++atlasStats.nativeUploads; atlasStats.bytesUploaded += bytes;
        // Preserve native texture, indices, blend, shader, framebuffer and order.
        CCTextureAtlas::drawNumberOfQuads(n, start);
    }
};
#endif
