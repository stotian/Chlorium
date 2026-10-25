#pragma once
// Research-only GL identity prototype. Include Windows and GLEW first.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <limits>

namespace chlorium::research::gl {
using LabelProc = void (APIENTRY*)(GLenum,GLuint,GLsizei,const char*);
using ReadLabelProc = void (APIENTRY*)(GLenum,GLuint,GLsizei,GLsizei*,char*);
template <class Function> Function currentProc(const char* name) noexcept {
    const auto address = wglGetProcAddress(name);
    if (!address || address == reinterpret_cast<PROC>(1) || address == reinterpret_cast<PROC>(2) ||
        address == reinterpret_cast<PROC>(3) || address == reinterpret_cast<PROC>(-1)) return nullptr;
    return reinterpret_cast<Function>(address);
}
inline bool labelObject(GLenum kind,GLuint name,GLsizei length,const char* label) noexcept {
    const auto function = currentProc<LabelProc>("glObjectLabel");
    if (!function) return false;
    function(kind,name,length,label);
    return true;
}
struct NativeContext { static HGLRC current() noexcept { return wglGetCurrentContext(); } };
inline std::atomic<std::uint64_t> lastEpoch{0};
inline std::uint64_t nextEpoch() noexcept {
    auto previous = lastEpoch.load(std::memory_order_relaxed);
    while (previous != UINT64_MAX) {
        if (lastEpoch.compare_exchange_weak(previous, previous + 1, std::memory_order_relaxed)) return previous + 1;
    }
    return 0; // Saturate, never recycle an epoch.
}

// HGLRC equality alone is insufficient after handle reuse. The labelled VAO is
// context-local even when buffers/programs are shared. A foreign or relabelled
// sentinel invalidates the identity; no owned resource may then be deleted.
template <class Provider = NativeContext>
class ContextEpoch {
public:
    ~ContextEpoch() noexcept {
        const auto destroy = currentProc<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
        if (destroy && isCurrent()) destroy(1, &sentinel_);
    }
    ContextEpoch(const ContextEpoch&) = delete;
    ContextEpoch& operator=(const ContextEpoch&) = delete;
    bool isCurrent() const noexcept {
        if (!native_ || Provider::current() != native_ || !sentinel_) return false;
        // Resolve in the CURRENT context; old WGL function addresses are not
        // a portable interface to a replacement context on a different driver.
        const auto exists = currentProc<PFNGLISVERTEXARRAYPROC>("glIsVertexArray");
        const auto readLabel = currentProc<ReadLabelProc>("glGetObjectLabel");
        if (!exists || !readLabel || exists(sentinel_) != GL_TRUE) return false;
        std::array<char, 64> actual{};
        GLsizei written = 0;
        readLabel(GL_VERTEX_ARRAY, sentinel_, static_cast<GLsizei>(actual.size()), &written, actual.data());
        return written == labelLength_ && std::memcmp(actual.data(), label_.data(), static_cast<std::size_t>(labelLength_)) == 0;
    }
    std::uint64_t epoch() const noexcept { return epoch_; }
    GLuint sentinel() const noexcept { return sentinel_; }
    HGLRC nativeHandle() const noexcept { return native_; }
    // Exactly one caller-owned resource. Never use this to delete Cocos data.
    bool destroyOwnedBuffer(GLuint buffer) const noexcept {
        if (!isCurrent()) return false;
        const auto destroy = currentProc<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
        if (!destroy) return false;
        destroy(1, &buffer);
        return true;
    }
    static std::shared_ptr<ContextEpoch> capture() {
        if (!Provider::current()) return {};
        const auto create = currentProc<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
        const auto bind = currentProc<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
        const auto label = currentProc<LabelProc>("glObjectLabel");
        if (!create || !bind || !label || !currentProc<ReadLabelProc>("glGetObjectLabel")) return {};
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major < 4 || (major == 4 && minor < 3)) return {};
        auto result = std::shared_ptr<ContextEpoch>(new ContextEpoch());
        result->native_ = Provider::current(); result->epoch_ = nextEpoch();
        if (!result->epoch_) return {};
        // Header code is instantiated in both the mod DLL and the independent
        // measurement DLL. Their counters are separate: include a module nonce
        // so a new stamp in the other DLL cannot impersonate an old generation.
        result->labelLength_ = std::snprintf(result->label_.data(), result->label_.size(),
            "chlorium:%016llx:%016llx",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&lastEpoch)),
            static_cast<unsigned long long>(result->epoch_));
        if (result->labelLength_ <= 0 || result->labelLength_ >= static_cast<int>(result->label_.size())) return {};
        GLint previous = 0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous);
        create(1, &result->sentinel_);
        if (!result->sentinel_) return {};
        bind(result->sentinel_);
        label(GL_VERTEX_ARRAY, result->sentinel_, result->labelLength_, result->label_.data());
        bind(previous);
        if (!result->isCurrent()) return {};
        return result;
    }
private:
    ContextEpoch() = default;
    HGLRC native_ = nullptr;
    std::uint64_t epoch_ = 0;
    GLuint sentinel_ = 0;
    int labelLength_ = 0;
    std::array<char, 64> label_{};
};

template <class Provider = NativeContext>
class EpochRegistry {
public:
    std::shared_ptr<ContextEpoch<Provider>> acquire() {
        const auto current = Provider::current();
        if (!current) return {};
        std::lock_guard lock(mutex_);
        if (auto found = contexts_.find(current); found != contexts_.end()) {
            if (auto live = found->second.lock(); live && live->isCurrent()) return live;
            contexts_.erase(found);
        }
        // Bounded weak cache, not an ever-growing list of destroyed contexts.
        std::erase_if(contexts_, [](const auto& item) { return item.second.expired(); });
        if (contexts_.size() >= 16) return {};
        auto identity = ContextEpoch<Provider>::capture();
        if (identity) contexts_.emplace(current, identity);
        return identity;
    }
private:
    std::mutex mutex_;
    std::unordered_map<HGLRC, std::weak_ptr<ContextEpoch<Provider>>> contexts_;
};
inline std::shared_ptr<ContextEpoch<>> acquireContextEpoch() {
    static EpochRegistry<> registry;
    return registry.acquire();
}
}
