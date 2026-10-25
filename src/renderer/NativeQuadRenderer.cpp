#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_QUADS
#include "NativeQuadState.hpp"
#include "NativeSnapshotStats.hpp"
#include "DirtyStateUploads.hpp"
#include "Buffer.hpp"
#include "Shader.hpp"
#include "GLState.hpp"
#include "NativeVertexSubmission.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/cocos/shaders/CCShaderCache.h>
#include <Geode/cocos/shaders/ccGLStateCache.h>
#include <Geode/cocos/kazmath/include/kazmath/mat4.h>
#include <chrono>
#include <fstream>
#include <limits>
using namespace geode::prelude;
static_assert(sizeof(cocos2d::ccV3F_C4B_T2F_Quad) == 96);
static_assert(offsetof(cocos2d::ccV3F_C4B_T2F_Quad,tl) == 0 && offsetof(cocos2d::ccV3F_C4B_T2F_Quad,bl) == 24 &&
    offsetof(cocos2d::ccV3F_C4B_T2F_Quad,tr) == 48 && offsetof(cocos2d::ccV3F_C4B_T2F_Quad,br) == 72);

namespace chlorium::snapshot {
Stats stats;
unsigned lastFrame = ~0u;
struct SubmissionTimer {
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    bool committed = false;
    ~SubmissionTimer() {
        if (committed) stats.captureMilliseconds +=
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    }
};
// Keep native batch preparation and every existing draw/updateTransform hook.
// Only replace the final atlas submission, once native quads and GL state are
// ready. Nesting must not let an unrelated atlas inherit its caller's batch.
struct QuadBatch;
thread_local CCSpriteBatchNode* activeBatch = nullptr;
thread_local QuadBatch* activeCache = nullptr;
struct BatchScope {
    CCSpriteBatchNode* previous = activeBatch;
    QuadBatch* previousCache = activeCache;
    BatchScope(CCSpriteBatchNode* batch, QuadBatch* cache) { activeBatch = batch; activeCache = cache; }
    ~BatchScope() { activeBatch = previous; activeCache = previousCache; }
};
struct RestoreGL {
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
    GLStateSnapshot state = GLStateSnapshot::capture(GLStateScope::NativeVertex);
#else
    GLStateSnapshot state = GLStateSnapshot::capture(GLStateScope::NativeQuad);
#endif
    ~RestoreGL() {
        // This submission section uses raw GL exclusively, so Cocos' cache is
        // untouched. Restore the actual bindings before returning to native.
        // In particular, do not rewrite blend enable/factors or matrix stacks.
        state.restore();
    }
};
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
bool nativeAtlasContract() {
    static const bool valid=[] {
        const auto module=GetModuleHandleW(L"libcocos2d.dll");
        if (!module) return false;
        const auto base=reinterpret_cast<const unsigned char*>(module);
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0) return false;
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if (pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp!=0x69702d99 ||
            pe->OptionalHeader.SizeOfImage!=0x1c5000 ||
            reinterpret_cast<const unsigned char*>(GetProcAddress(module,"?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"))!=base+0xa6350) return false;
        std::uint64_t hash=14695981039346656037ull;
        for (std::size_t i=0x20;i<0x15f;++i) { hash^=base[0xa6350+i];hash*=1099511628211ull; }
        return hash==0xc9d0f08451f64908ull;
    }();
    return valid;
}
bool otherAtlasHooks() {
    static unsigned checked=~0u;
    static bool conflict=true;
    const auto frame=CCDirector::get()->getTotalFrames();
    if (checked==frame) return conflict;
    checked=frame; conflict=false;
    const auto address=reinterpret_cast<std::uintptr_t>(GetProcAddress(GetModuleHandleW(L"libcocos2d.dll"),
        "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod==Mod::get()) continue;
        for (const auto hook : mod->getHooks())
            if (hook->isEnabled() && hook->getAddress()==address) return conflict=true;
    }
    return false;
}
#endif
struct QuadBatch {
    std::unique_ptr<Buffer> buffer;
    std::vector<NativeQuadState> states;
    Shader* shader = nullptr;
    GLuint vao = 0;
    bool failed = false;
    std::shared_ptr<chlorium::research::gl::ContextEpoch<>> context;
    ~QuadBatch() {
        if (shader) Shader::destroy(shader);
        // VAO names are context-local, including between shared contexts. A
        // recycled numeric name in a new context may belong to native Cocos.
        if (vao && context && context->isCurrent()) glDeleteVertexArrays(1,&vao);
    }
    bool fail(const char* reason) {
        if (!failed) {
            failed = true;
            ++stats.fallbacks;
            log::warn("Chlorium native quad fallback: {}", reason);
            if (auto report = std::getenv("CHLORIUM_AUDIT_OUTPUT")) {
                std::ofstream trace(std::string(report) + ".snapshot-failure.json",std::ios::trunc);
                trace << matjson::makeObject({{"reason",reason},{"backend","native-quads-v4"},
                    {"frames",stats.frames}}).dump();
            }
        }
        return false;
    }
    bool draw(CCTextureAtlas* atlas, unsigned amount, unsigned first) {
        if (failed) return false;
        if (!amount) return true;
        if (!atlas->getTexture()) return false;
#ifndef CHLORIUM_NATIVE_VERTEX_PARITY
        if (!glDrawArraysInstanced || !glBindBufferBase) return fail("instancing or storage buffers unavailable");
#endif
        auto currentContext = wglGetCurrentContext();
        if (!currentContext) return fail("no current native GL context");
        if (context && !context->isCurrent()) return fail("native GL context generation changed");
        if (!context) {
            GLint major = 0, minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION,&major); glGetIntegerv(GL_MINOR_VERSION,&minor);
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
            if (major < 4 || (major == 4 && minor < 3)) return fail("native vertex ownership requires OpenGL 4.3");
#else
            if (major < 4 || (major == 4 && minor < 6)) return fail("native quad shaders require OpenGL 4.6");
#endif
        }
        // Declared before RestoreGL: its destructor measures restoration too.
        // Failed attempts remain separate fallbacks, not successful frames.
        SubmissionTimer submissionTimer;
        RestoreGL restore;
        const std::size_t count = amount;
        // Honor subrange submissions too. Native drawNumberOfQuads permits
        // any initialized range within capacity, not only the total count.
        if (first > atlas->getCapacity() || count > atlas->getCapacity() - first || !atlas->m_pQuads)
            return fail("invalid native atlas bounds");
        if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) return fail("native batch exceeds instancing limit");
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
        if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())/6)
            return fail("native vertex indexing exceeds representation");
        const auto nativeIndices=atlas->m_pBuffersVBO[1];
        if (!nativeIndices || !glIsBuffer(nativeIndices)) return fail("native element store unavailable");
        // Keep native vertex IDs/subrange addressing, including shaders that
        // inspect gl_VertexID. Never rebase a subrange to vertex zero.
        const auto captureCount=static_cast<std::size_t>(first)+count;
#else
        const auto captureCount=count;
#endif
        if (!context) {
            context = chlorium::research::gl::acquireContextEpoch();
            if (!context) return fail("native GL context identity unavailable");
#ifndef CHLORIUM_NATIVE_VERTEX_PARITY
            shader = Shader::create("nativeQuad.vert","nativeQuad.frag");
            if (!shader) return fail("native quad shader compilation");
#endif
            glGenVertexArrays(1,&vao);
            if (!vao) return fail("empty vertex array allocation");
        }
        bool resized = false;
        if (states.size() < captureCount) {
            std::size_t capacity = std::max<std::size_t>(256,states.size());
            while (capacity < captureCount) capacity *= 2;
            states.resize(capacity);
            auto replacement = std::unique_ptr<Buffer>(Buffer::createDynamicDraw("Owned native quad states",states.size()*sizeof(NativeQuadState)));
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
            static_assert(kCCVertexAttrib_Position==0 && kCCVertexAttrib_Color==1 && kCCVertexAttrib_TexCoords==2);
            configureNativeVertexLayout(vao,replacement->getId());
#endif
            buffer = std::move(replacement);
            resized = true;
        }
        DirtyStateUploads uploads(states.size());
        // getQuads() dirties the native atlas merely by reading it. Leave its
        // dirty bit untouched so a later native fallback uploads exactly what
        // native updates require, and copying does not manufacture new work.
        const auto quads = atlas->m_pQuads
#ifndef CHLORIUM_NATIVE_VERTEX_PARITY
            + first
#endif
            ;
        for (std::size_t index = 0; index < captureCount; ++index) {
            NativeQuadState next{};
            static_assert(sizeof(next) == sizeof(quads[index]));
            std::memcpy(&next,&quads[index],sizeof(next));
            if (std::memcmp(&states[index],&next,sizeof(next))) {
                states[index] = next;
                uploads.mark(index);
            }
        }
        // New storage is initialized completely. Existing owned storage only
        // receives changed blocks, and is never invalidated/orphaned partially.
        if (resized) {
            buffer->write(states.data(),states.size()*sizeof(NativeQuadState));
            stats.uploadedBytes += states.size()*sizeof(NativeQuadState);
        } else for (const auto range : uploads.finish()) {
            const auto bytes = range.count*sizeof(NativeQuadState);
            buffer->write(states.data()+range.first,bytes,range.first*sizeof(NativeQuadState));
            stats.uploadedBytes += bytes;
        }
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
        // Keep the actual native program and every uniform (MVP, sampler,
        // effect parameters, mod changes). No shader or matrix reconstruction.
        submitNativeVertices(vao,nativeIndices,atlas->getTexture()->getName(),static_cast<GLsizei>(count),first);
#else
        kmMat4 projection,model,mvp;
        kmGLGetMatrix(KM_GL_PROJECTION,&projection); kmGLGetMatrix(KM_GL_MODELVIEW,&model);
        kmMat4Multiply(&mvp,&projection,&model);
        shader->use(); shader->setMatrix4("u_mvp",mvp.mat);
        shader->setTexture("u_spriteSheetTexture",0,atlas->getTexture());
        buffer->bindAsStorageBuffer(4); glBindVertexArray(vao);
        // Native CCSpriteBatchNode::draw has already set blend enable/factors.
        // Preserve that exact state, including changes from other draw hooks.
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,static_cast<GLsizei>(count));
#endif
        CC_INCREMENT_GL_DRAWS(1);
        const auto frame = CCDirector::get()->getTotalFrames();
        if (frame != lastFrame) { lastFrame = frame; ++stats.frames; stats.sprites = 0; }
        stats.sprites += count;
        stats.active = 1;
        submissionTimer.committed = true;
        return true;
    }
};
}
extern "C" __declspec(dllexport) bool ChloriumReadNativeSnapshotStats(
    chlorium::snapshot::Stats* output,std::size_t size) {
    if (!output || size != sizeof(*output)) return false;
    *output = chlorium::snapshot::stats;
    const auto frame = CCDirector::get()->getTotalFrames();
    output->active = output->active && frame - chlorium::snapshot::lastFrame <= 2 && Mod::get()->getSettingValue<bool>("enabled");
    return true;
}
class $modify(ChloriumNativeQuadBatch,CCSpriteBatchNode) {
    // Geode fields require a CCNode. CCTextureAtlas is only a CCObject, so it
    // cannot own m_fields; keep the cache on its owning sprite batch instead.
    struct Fields { std::unique_ptr<chlorium::snapshot::QuadBatch> cache; bool allocationFailed = false; };
    void draw() {
        auto play = PlayLayer::get();
        bool inLevel = false;
        if (play && play->m_objectLayer) {
            auto node = getParent();
            for (unsigned depth = 0; node && depth < 64; node = node->getParent(),++depth)
                if (node == play->m_objectLayer) { inLevel = true; break; }
        }
        bool eligible = inLevel && Mod::get()->getSettingValue<bool>("enabled") && !m_fields->allocationFailed;
        if (eligible && !m_fields->cache) {
            try { m_fields->cache = std::make_unique<chlorium::snapshot::QuadBatch>(); }
            catch (const std::exception&) {
                m_fields->allocationFailed = true; eligible = false;
                ++chlorium::snapshot::stats.fallbacks;
            }
        }
        chlorium::snapshot::BatchScope scope(eligible ? this : nullptr, eligible ? m_fields->cache.get() : nullptr);
        CCSpriteBatchNode::draw();
    }
};
class $modify(ChloriumNativeQuadAtlas,CCTextureAtlas) {
    void drawNumberOfQuads(unsigned int amount,unsigned int first) {
        auto batch = chlorium::snapshot::activeBatch;
        auto cache = chlorium::snapshot::activeCache;
        if (!batch || !cache) { CCTextureAtlas::drawNumberOfQuads(amount,first); return; }
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
        // Never skip another mod's atlas callback or guess a changed engine ABI.
        if (!chlorium::snapshot::nativeAtlasContract() || chlorium::snapshot::otherAtlasHooks()) {
            ++chlorium::snapshot::stats.fallbacks;
            CCTextureAtlas::drawNumberOfQuads(amount,first); return;
        }
#endif
        auto stock = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor);
        GLint currentProgram = 0;
        if (batch && batch->getTextureAtlas() == this && stock && batch->getShaderProgram() == stock)
            glGetIntegerv(GL_CURRENT_PROGRAM,&currentProgram);
        // A later native/mod callback can choose a different program after
        // the batch's shader pointer was read. Do not overwrite its effect.
        if (!batch || !cache || batch->getTextureAtlas() != this || !stock || batch->getShaderProgram() != stock ||
            static_cast<GLuint>(currentProgram) != stock->getProgram()) {
            CCTextureAtlas::drawNumberOfQuads(amount,first); return;
        }
        try {
            if (cache->draw(this,amount,first)) return;
        } catch (const std::exception& error) {
            log::warn("Chlorium native quad allocation/capture failed: {}",error.what());
            cache->fail("native quad allocation/capture exception");
        }
        // Native preparation already ran exactly once. Falling back at the
        // submission boundary avoids repeating updateTransform/mod hooks.
        CCTextureAtlas::drawNumberOfQuads(amount,first);
    }
};
class $modify(ChloriumNativeQuadLevel,PlayLayer) {
    void setupHasCompleted() {
        chlorium::snapshot::stats = {};
        chlorium::snapshot::lastFrame = ~0u;
        PlayLayer::setupHasCompleted();
    }
};
#endif
