#include "NativeAtlasUploads.hpp"
#include "NativeAtlasUploadRanges.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/cocos/platform/win32/CCGL.h>

#include <cstdlib>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <windows.h>

using namespace geode::prelude;
extern "C" __declspec(dllexport) extern const char chloriumAtlasCacheImplementation[] =
    "CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1";

namespace chlorium::native {
namespace {

constexpr std::size_t quadBytes = sizeof(ccV3F_C4B_T2F_Quad);
constexpr std::size_t blockBytes = 64 * quadBytes;
constexpr std::size_t memoryBudget = 32 * 1024 * 1024;
constexpr std::size_t maximumEntries = 256;
constexpr std::size_t minimumAtlasBytes = 4 * 1024;
// CPU shadow work is optional. Large native uploads must never cause an
// equally large extra scan/copy on top of the original engine work.
constexpr std::size_t maximumAtlasBytes = 512 * 1024;
constexpr std::size_t frameWorkBytes = 2 * 1024 * 1024;
constexpr unsigned maximumWorkOperations = 64;

// These offsets and the complete draw body were checked in the installed
// GD 2.2081 libcocos2d.dll, not inferred solely from upstream Cocos sources.
static_assert(sizeof(void*) == 8 && CC_TEXTURE_ATLAS_USE_VAO == 0);
static_assert(sizeof(ccV3F_C4B_T2F) == 24 && quadBytes == 96);
static_assert(offsetof(CCTextureAtlas, m_pBuffersVBO) == 0x40);
static_assert(offsetof(CCTextureAtlas, m_bDirty) == 0x48);
static_assert(offsetof(CCTextureAtlas, m_uTotalQuads) == 0x4c);
static_assert(offsetof(CCTextureAtlas, m_uCapacity) == 0x50);
static_assert(offsetof(CCTextureAtlas, m_pQuads) == 0x60);

struct AtlasIdentity {
    const ccV3F_C4B_T2F_Quad* quads = nullptr;
    HGLRC context = nullptr;
    GLuint vertexBuffer = 0, indexBuffer = 0;
    unsigned capacity = 0, total = 0;
    bool operator==(const AtlasIdentity& other) const {
        // Changing the active count does not reallocate the VBO or erase its
        // old bytes. Per-quad coverage below tracks which bytes are known.
        return quads == other.quads && context == other.context &&
            vertexBuffer == other.vertexBuffer && indexBuffer == other.indexBuffer && capacity == other.capacity;
    }
};
AtlasIdentity identityOf(const CCTextureAtlas* atlas) {
    return {atlas->m_pQuads, wglGetCurrentContext(), atlas->m_pBuffersVBO[0],
            atlas->m_pBuffersVBO[1], atlas->m_uCapacity, atlas->m_uTotalQuads};
}
struct CachedAtlas {
    AtlasIdentity identity;
    AtlasUploadPolicy policy;
    std::vector<std::byte> uploaded;
    std::vector<unsigned char> knownQuads;
    std::uint64_t lastUsed = 0;
    bool coverageUnknown = false;
};
struct UploadCache {
    std::mutex mutex;
    std::unordered_map<const CCTextureAtlas*, CachedAtlas> atlases;
    AtlasUploadStats stats;
    std::uint64_t serial = 0;
    unsigned workFrame = std::numeric_limits<unsigned>::max(), workOperations = 0;
    std::size_t remainingWork = 0;
};
UploadCache& cache() {
    // Cocos teardown can outlive other static destructors. Store no Cocos
    // references here; explicit purge/destruction frees all shadow storage.
    static auto* instance = new UploadCache;
    return *instance;
}
void eraseAtlas(UploadCache& storage, const CCTextureAtlas* atlas) {
    const auto found = storage.atlases.find(atlas);
    if (found == storage.atlases.end()) return;
    storage.stats.cachedBytes -= found->second.uploaded.capacity() + found->second.knownQuads.capacity();
    storage.atlases.erase(found);
}
void invalidateAtlas(const CCTextureAtlas* atlas, unsigned reason = 0) {
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    if (storage.atlases.contains(atlas)) ++storage.stats.invalidations[reason];
    eraseAtlas(storage, atlas);
}

// An original upload which we do not capture invalidates the byte proof,
// but keeping the entry preserves its adaptive dense-upload cooldown.
void forgetAtlasCoverage(const CCTextureAtlas* atlas) {
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    if (const auto found = storage.atlases.find(atlas); found != storage.atlases.end())
        found->second.coverageUnknown = true;
}

// Called under storage.mutex. Charge an upper bound before scanning, copying,
// allocating shadow bytes, or querying GL. Denial always retains native I/O.
bool reserveAtlasWork(UploadCache& storage, std::size_t bytes) {
    const auto director = CCDirector::get();
    if (!director) { ++storage.stats.budgetFallbacks; return false; }
    const auto frame = director->getTotalFrames();
    if (storage.workFrame != frame) {
        storage.workFrame = frame;
        storage.workOperations = 0;
        storage.remainingWork = frameWorkBytes;
    }
    if (bytes > storage.remainingWork || storage.workOperations >= maximumWorkOperations) {
        ++storage.stats.budgetFallbacks;
        return false;
    }
    storage.remainingWork -= bytes;
    ++storage.workOperations;
    return true;
}

bool matchesNativeDrawContract() {
    static const bool compatible = [] {
        auto module = GetModuleHandleW(L"libcocos2d.dll");
        if (!module) return false;
        const auto base = reinterpret_cast<const std::byte*>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
        const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (pe->Signature != IMAGE_NT_SIGNATURE ||
            pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            pe->FileHeader.TimeDateStamp != 0x69702d99 ||
            pe->OptionalHeader.SizeOfImage != 0x1c5000) return false;
        auto draw = GetProcAddress(module, "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z");
        if (reinterpret_cast<const std::byte*>(draw) != base + 0xa6350) return false;
        // Geode replaces the entry prologue with its own detour. Check the
        // remaining original body, whose RIP-relative operands do not change
        // with ASLR. Unknown engine builds or patched bodies keep native I/O.
        std::uint64_t hash = 14695981039346656037ull;
        for (std::size_t i = 0x20; i < 0x15f; ++i) {
            hash ^= std::to_integer<unsigned char>(base[0xa6350 + i]);
            hash *= 1099511628211ull;
        }
        return hash == 0xc9d0f08451f64908ull;
    }();
    return compatible;
}

bool otherModsHookDraw() {
    // Check once per native frame. If another mod changes this draw contract,
    // preserve its dirty/upload behavior and do not assume its GPU contents.
    static unsigned checkedFrame = std::numeric_limits<unsigned>::max();
    static bool incompatible = true;
    const auto director = CCDirector::get();
    if (!director) return true;
    const auto frame = director->getTotalFrames();
    if (checkedFrame == frame) return incompatible;
    checkedFrame = frame;
    incompatible = false;
    const auto module = GetModuleHandleW(L"libcocos2d.dll");
    const auto draw = reinterpret_cast<uintptr_t>(GetProcAddress(module,
        "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod == Mod::get()) continue;
        for (const auto hook : mod->getHooks()) {
            if (hook->isEnabled() && hook->getAddress() == draw) {
                incompatible = true;
                return true;
            }
        }
    }
    return false;
}

struct ArrayBufferBinding {
    GLint previous = 0;
    explicit ArrayBufferBinding(GLuint buffer) {
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
        if (previous != static_cast<GLint>(buffer)) glBindBuffer(GL_ARRAY_BUFFER, buffer);
    }
    ~ArrayBufferBinding() { glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous)); }
};
bool bufferCanAccept(const AtlasIdentity& identity, std::size_t bytes) {
    if (!identity.context || !identity.vertexBuffer || !glIsBuffer(identity.vertexBuffer)) return false;
    ArrayBufferBinding binding(identity.vertexBuffer);
    GLint size = 0, mapped = GL_TRUE;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &mapped);
    return size >= 0 && static_cast<std::size_t>(size) >= bytes && mapped == GL_FALSE;
}

bool trySparseUpload(CCTextureAtlas* atlas, const AtlasIdentity& identity, unsigned n, unsigned start) {
    const auto bytes = static_cast<std::size_t>(n) * quadBytes;
    const auto offset = static_cast<std::size_t>(start) * quadBytes;
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    const auto found = storage.atlases.find(atlas);
    if (found == storage.atlases.end()) return false;
    auto& entry = found->second;
    if (entry.identity != identity) {
        const auto& prior = entry.identity;
        storage.stats.identityChanges[0] += prior.quads != identity.quads;
        storage.stats.identityChanges[1] += prior.context != identity.context;
        storage.stats.identityChanges[2] += prior.vertexBuffer != identity.vertexBuffer;
        storage.stats.identityChanges[3] += prior.indexBuffer != identity.indexBuffer;
        storage.stats.identityChanges[4] += prior.capacity != identity.capacity;
        eraseAtlas(storage, atlas);
        return false;
    }
    if (entry.policy.skipDraw()) { ++storage.stats.cooldownDraws; return false; }
    if (entry.coverageUnknown) return false;
    // Charge both input reads and coverage checks before comparing. A partial
    // shadow copy is charged separately, before any GPU write is attempted.
    if (!reserveAtlasWork(storage, 2 * bytes + n)) {
        entry.coverageUnknown = true;
        return false;
    }
    if (!atlasRangeKnown(entry.knownQuads, start, n)) return false;
    entry.lastUsed = ++storage.serial;
    const std::span<const std::byte> current{reinterpret_cast<const std::byte*>(identity.quads) + offset, bytes};
    const std::span<const std::byte> previous{entry.uploaded.data() + offset, bytes};
    // A native dirty draw submits one upload. Fragmenting it into many
    // driver calls can cost more than the saved bytes on integrated GPUs.
    // Keep at most one contiguous partial upload; otherwise stay native.
    const auto plan = planAtlasUploads(current, previous, blockBytes, 1);
    if (plan.useNativeUpload) {
        // Animated/moving atlases often change every block. After four dense
        // draws, pause queries/copies/comparisons with adaptive backoff.
        // Unknown coverage prevents using any shadow from before the pause.
        if (entry.policy.recordDense()) entry.coverageUnknown = true;
        return false;
    }
    if (plan.rangeCount && !reserveAtlasWork(storage, 2 * plan.changedBytes)) {
        entry.coverageUnknown = true;
        return false;
    }
    // Dense changes use the original upload; validate once when remembering
    // that upload instead of querying the driver twice on the common path.
    if (!bufferCanAccept(identity, offset + bytes)) { eraseAtlas(storage, atlas); return false; }
    if (plan.rangeCount) {
        ArrayBufferBinding binding(identity.vertexBuffer);
        for (std::size_t i = 0; i < plan.rangeCount; ++i) {
            const auto& range = plan.ranges[i];
            glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset + range.offset),
                            static_cast<GLsizeiptr>(range.size), current.data() + range.offset);
            std::memcpy(entry.uploaded.data() + offset + range.offset, current.data() + range.offset, range.size);
        }
    }
#ifdef CHLORIUM_RUNTIME_AUDIT
    static std::array<unsigned, 10> verifiedBySegment{};
    const auto verifyingLevel = std::getenv("CHLORIUM_VERIFY_NATIVE_UPLOADS") ? PlayLayer::get() : nullptr;
    const auto verificationSegment = verifyingLevel ? std::clamp(static_cast<int>(verifyingLevel->getCurrentPercent() / 10), 0, 9) : 0;
    if (verifyingLevel && verifiedBySegment[verificationSegment] < 12) {
        std::vector<std::byte> actual;
        try { actual.resize(bytes); }
        catch (const std::bad_alloc&) { return false; }
        ArrayBufferBinding binding(identity.vertexBuffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, bytes, actual.data());
        ++storage.stats.verifiedUploads;
        ++verifiedBySegment[verificationSegment];
        if (!std::equal(actual.begin(), actual.end(), current.begin())) {
            ++storage.stats.verificationFailures;
            eraseAtlas(storage, atlas);
            return false; // Native dirty upload repairs the entire requested range.
        }
    }
#endif
    if (plan.rangeCount) ++storage.stats.partialUploads;
    else ++storage.stats.unchangedUploads;
    entry.policy.recordSparse();
    storage.stats.bytesUploaded += plan.changedBytes;
    storage.stats.bytesAvoided += bytes - plan.changedBytes;
    // The original draw still binds texture, attributes and indices and
    // issues exactly its original draw. Only its redundant upload is skipped.
    atlas->m_bDirty = false;
    return true;
}

void rememberNativeUpload(CCTextureAtlas* atlas, const AtlasIdentity& identity, unsigned n, unsigned start) {
    const auto bytes = static_cast<std::size_t>(n) * quadBytes;
    const auto offset = static_cast<std::size_t>(start) * quadBytes;
    // Large native atlases reserve far more quads than one draw uses. Shadow
    // only the observed prefix, rounded to 256 quads, rather than the entire
    // reserved GPU capacity. Coverage bits still reject every unknown range.
    const auto end = static_cast<std::size_t>(start) + n;
    const auto coveredQuads = std::min({static_cast<std::size_t>(identity.capacity),
        (end + 255) / 256 * 256, maximumAtlasBytes / quadBytes});
    const auto capacityBytes = coveredQuads * quadBytes;
    if (end > coveredQuads || capacityBytes > memoryBudget || coveredQuads > memoryBudget - capacityBytes) {
        auto& storage = cache();
        const std::lock_guard lock(storage.mutex);
        ++storage.stats.cacheRejectedMemory;
        eraseAtlas(storage, atlas);
        return;
    }
    {
        auto& storage = cache();
        const std::lock_guard lock(storage.mutex);
        const auto found = storage.atlases.find(atlas);
        const bool reuse = found != storage.atlases.end() && found->second.identity == identity &&
            found->second.knownQuads.size() >= end;
        if (reuse && found->second.policy.cooldown) return;
        const auto work = reuse ? 2 * bytes + n + (found->second.coverageUnknown ? found->second.knownQuads.size() : 0) :
            capacityBytes + coveredQuads + 2 * bytes + n;
        if (!reserveAtlasWork(storage, work)) {
            if (found != storage.atlases.end()) found->second.coverageUnknown = true;
            return;
        }
    }
    if (atlas->m_bDirty || identityOf(atlas) != identity || !bufferCanAccept(identity, offset + bytes)) {
        auto& storage = cache();
        const std::lock_guard lock(storage.mutex);
        ++storage.stats.cacheRejectedBuffer;
        eraseAtlas(storage, atlas);
        return;
    }
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    if (const auto found = storage.atlases.find(atlas); found != storage.atlases.end() &&
        found->second.identity == identity && found->second.knownQuads.size() >= end) {
        // Only a dirty native upload establishes these exact GPU bytes.
        if (found->second.coverageUnknown) {
            std::fill(found->second.knownQuads.begin(), found->second.knownQuads.end(), 0);
            found->second.coverageUnknown = false;
        }
        std::memcpy(found->second.uploaded.data() + offset,
            reinterpret_cast<const std::byte*>(identity.quads) + offset, bytes);
        std::fill_n(found->second.knownQuads.begin() + start, n, 1);
        found->second.lastUsed = ++storage.serial;
        return;
    }
    eraseAtlas(storage, atlas);
    // Evict before allocating so the strict budget also holds during capture.
    while (!storage.atlases.empty() &&
           (storage.stats.cachedBytes > memoryBudget - capacityBytes - coveredQuads || storage.atlases.size() >= maximumEntries)) {
        auto oldest = storage.atlases.begin();
        for (auto it = storage.atlases.begin(); it != storage.atlases.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        eraseAtlas(storage, oldest->first);
    }
    try {
        CachedAtlas entry;
        entry.identity = identity;
        entry.lastUsed = ++storage.serial;
        entry.uploaded.resize(capacityBytes);
        entry.knownQuads.assign(coveredQuads, 0);
        std::memcpy(entry.uploaded.data() + offset,
            reinterpret_cast<const std::byte*>(identity.quads) + offset, bytes);
        std::fill_n(entry.knownQuads.begin() + start, n, 1);
        const auto allocation = entry.uploaded.capacity() + entry.knownQuads.capacity();
        if (allocation > memoryBudget - storage.stats.cachedBytes) return;
        storage.atlases.emplace(atlas, std::move(entry));
        storage.stats.cachedBytes += allocation;
        ++storage.stats.cacheCaptures;
    } catch (const std::bad_alloc&) {
        ++storage.stats.cacheAllocationFailures;
        // Native drawing already completed. A failed optional CPU cache must
        // never interrupt the game or change its next dirty upload.
    }
}

bool enabled() {
    static const bool baseline = std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
    return !baseline && Mod::get()->getSettingValue<bool>("enabled");
}

} // namespace

AtlasUploadStats getAtlasUploadStats() {
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    return storage.stats;
}
void purgeAtlasUploadCache() {
    auto& storage = cache();
    const std::lock_guard lock(storage.mutex);
    storage.atlases.clear();
    storage.stats.cachedBytes = 0;
}

} // namespace chlorium::native

using namespace chlorium::native;

class $modify(NativeAtlasTextureAtlas, CCTextureAtlas) {
    void drawNumberOfQuads(unsigned n, unsigned start) {
        // A clean draw and a zero-count draw cannot establish new GPU bytes.
        // Avoid setting lookups, context queries, cache locks and hook scans.
        if (!m_bDirty || !n) {
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        const auto bytes = static_cast<std::size_t>(n) * quadBytes;
        const bool validRange = start <= m_uTotalQuads && n <= m_uTotalQuads - start &&
            start <= m_uCapacity && n <= m_uCapacity - start && m_pQuads;
        if (!validRange) {
            invalidateAtlas(this, 2);
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        if (bytes < minimumAtlasBytes) {
            // A partial native upload may overlap an existing shadow even
            // though this small draw is not worth caching itself.
            forgetAtlasCoverage(this);
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        const auto end = static_cast<std::size_t>(start) + n;
        if (bytes > maximumAtlasBytes || end > maximumAtlasBytes / quadBytes) {
            auto& storage = cache();
            {
                const std::lock_guard lock(storage.mutex);
                ++storage.stats.oversizedFallbacks;
                eraseAtlas(storage, this);
            }
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        if (!enabled() || !matchesNativeDrawContract() || otherModsHookDraw()) {
            invalidateAtlas(this, 1);
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        const auto identity = identityOf(this);
        const bool dirtyBefore = m_bDirty;
        {
            auto& storage = cache();
            const std::lock_guard lock(storage.mutex);
            ++storage.stats.draws;
            if (validRange && dirtyBefore && bytes >= minimumAtlasBytes) ++storage.stats.eligibleUploads;
            if (start != 0 || n != identity.total) ++storage.stats.partialRangeDraws;
        }
        if (validRange && bytes >= minimumAtlasBytes && dirtyBefore && trySparseUpload(this, identity, n, start)) {
            CCTextureAtlas::drawNumberOfQuads(n, start);
            return;
        }
        if (dirtyBefore && n > 0) {
            auto& storage = cache();
            const std::lock_guard lock(storage.mutex);
            ++storage.stats.nativeUploads;
            storage.stats.bytesUploaded += bytes;
        }
        CCTextureAtlas::drawNumberOfQuads(n, start);
        // Partial draws establish only their own per-quad coverage. Unknown
        // regions can never suppress a later native dirty upload.
        if (validRange && dirtyBefore) {
            auto& storage = cache();
            bool tracked;
            { const std::lock_guard lock(storage.mutex); ++storage.stats.postNativeDraws; tracked = storage.atlases.contains(this); }
            if (bytes >= minimumAtlasBytes || tracked) rememberNativeUpload(this, identity, n, start);
        }
    }

    bool initWithTexture(CCTexture2D* texture, unsigned capacity) {
        invalidateAtlas(this, 3);
        return CCTextureAtlas::initWithTexture(texture, capacity);
    }
    bool resizeCapacity(unsigned capacity) {
        invalidateAtlas(this, 4);
        return CCTextureAtlas::resizeCapacity(capacity);
    }
    void mapBuffers() {
        // This native function replaces GPU storage with glBufferData while
        // keeping the atlas, CPU pointer, capacity and buffer names unchanged.
        // A shadow from before that replacement no longer proves GPU contents.
        invalidateAtlas(this, 0);
        // The SDK declares this DLL export private. Calling its entry from
        // the hook uses Geode's normal next-hook/original dispatch on Win64.
        static const auto original = reinterpret_cast<void(*)(CCTextureAtlas*)>(GetProcAddress(
            GetModuleHandleW(L"libcocos2d.dll"), "?mapBuffers@CCTextureAtlas@cocos2d@@AEAAXXZ"));
        original(this);
    }
    void setQuads(ccV3F_C4B_T2F_Quad* quads) {
        invalidateAtlas(this, 5);
        CCTextureAtlas::setQuads(quads);
    }
    void listenBackToForeground(CCObject* object) {
        invalidateAtlas(this, 6);
        CCTextureAtlas::listenBackToForeground(object);
    }
    ~NativeAtlasTextureAtlas() { invalidateAtlas(this, 7); }
};

extern "C" __declspec(dllexport) bool ChloriumReadAtlasUploadStats(
    AtlasUploadStats* output, std::size_t size) {
    if (!output || size != sizeof(*output)) return false;
    *output = getAtlasUploadStats();
    return true;
}

// Do not hook CCDirector::purgeCachedData on this engine. Its Windows export
// is the shared empty function at libcocos2d+0x1fb0, folded with unrelated
// methods. Hooking it clears the atlas cache during ordinary scene updates.
// Atlas destruction, storage changes and context recovery own invalidation.
