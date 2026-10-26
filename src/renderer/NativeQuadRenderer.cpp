#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_QUADS
#include "NativeQuadState.hpp"
#include "NativeSnapshotStats.hpp"
#include "DirtyStateUploads.hpp"
#include "Buffer.hpp"
#include "Shader.hpp"
#include "GLState.hpp"
#include "NativeVertexSubmission.hpp"
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
#include "../../research/R4-17/PersistentTransfer.hpp"
#endif
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
#include "AdaptiveUpload.hpp"
#include "NativeVertexLease.hpp"
#include "NativeStoreHydration.hpp"
#ifdef CHLORIUM_NATIVE_GPU_EXCHANGE_V8
#include "NativeGpuReplacement.hpp"
#endif
#endif
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
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
thread_local std::unique_ptr<chlorium::research::PersistentTransfer> transfer;
TransferStatsV9 transferStats;
#endif
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
UploadStatsV6 uploadStats;
#endif
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
    bool active=true;
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
    GLStateSnapshot state = GLStateSnapshot::capture(GLStateScope::NativeVertex);
#else
    GLStateSnapshot state = GLStateSnapshot::capture(GLStateScope::NativeQuad);
#endif
    ~RestoreGL() {
        // This submission section uses raw GL exclusively, so Cocos' cache is
        // untouched. Restore the actual bindings before returning to native.
        // In particular, do not rewrite blend enable/factors or matrix stacks.
        if(active) state.restore();
    }
    void handoff() {state.restore();active=false;}
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
#if defined(CHLORIUM_NATIVE_GPU_EXCHANGE_V8) || defined(CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9)
    const auto cocos=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"libcocos2d.dll"));
#endif
    for (const auto mod : Loader::get()->getAllMods()) {
        if (mod==Mod::get()) continue;
        for (const auto hook : mod->getHooks())
            if (hook->isEnabled() && (hook->getAddress()==address
#if defined(CHLORIUM_NATIVE_GPU_EXCHANGE_V8) || defined(CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9)
                // Moving the upload ahead of the native call requires its
                // preceding texture getter/bind to have no foreign callback.
                || hook->getAddress()==cocos+0x10760 || hook->getAddress()==cocos+0x87fe0
#endif
                )) return conflict=true;
    }
    return false;
}
#endif
struct QuadBatch {
    std::unique_ptr<Buffer> buffer;
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
    AdaptiveUpload uploads;
    std::size_t capacity=0;
#else
    std::vector<NativeQuadState> states;
#endif
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
    template<class Forward> bool draw(CCTextureAtlas* atlas, unsigned amount, unsigned first,Forward&& native) {
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
        if(failed) return false;
        if(!amount) {native();return true;}
        if(!atlas->getTexture()||first>atlas->getCapacity()||amount>atlas->getCapacity()-first||!atlas->m_pQuads)
            return fail("invalid persistent native atlas bounds");
        SubmissionTimer timer;
        if(atlas->m_bDirty) {
            const auto cocos=reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"libcocos2d.dll"));
            const auto table=*reinterpret_cast<const std::uintptr_t* const*>(atlas->getTexture());
            const unsigned char pureGetter[]{0x8b,0x41,0x48,0xc3};
            if(table[12]!=reinterpret_cast<std::uintptr_t>(cocos+0x10760)||
                std::memcmp(cocos+0x10760,pureGetter,sizeof(pureGetter))) return fail("foreign persistent texture getter");
            if(!transfer) transfer=chlorium::research::PersistentTransfer::create();
            if(!transfer) return fail("persistent native transfer unavailable");
            const auto before=transfer->work;
            const auto bytes=static_cast<std::size_t>(amount)*sizeof(NativeQuadState);
            const bool uploaded=transfer->upload(atlas->m_pBuffersVBO[0],static_cast<std::size_t>(first)*sizeof(NativeQuadState),bytes,atlas->m_pQuads+first);
            transferStats.busyFallbacks+=transfer->work.busy-before.busy;
            transferStats.fences+=transfer->work.fences-before.fences;
            if(uploaded) {
                atlas->m_bDirty=false;stats.uploadedBytes+=bytes;
                ++transferStats.uploads;transferStats.bytes+=bytes;
            } else {
                ++stats.fallbacks;++transferStats.nativeFallbacks;
                if(!transfer->current()) transfer.reset();
            }
        } else ++transferStats.cleanDraws;
        // No manual VAO/program/texture handling and no native-store ownership
        // change. On a refused transfer the original dirty upload still runs.
        native();
        const auto frame=CCDirector::get()->getTotalFrames();
        if(frame!=lastFrame) {lastFrame=frame;++stats.frames;stats.sprites=0;}
        stats.sprites+=amount;stats.active=1;timer.committed=true;
        return true;
#else
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
#ifndef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
            glGenVertexArrays(1,&vao);
            if (!vao) return fail("empty vertex array allocation");
#endif
        }
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
#ifdef CHLORIUM_NATIVE_GPU_EXCHANGE_V8
        // Clean draws retain the canonical native GPU contents. For a dirty
        // draw only the requested CPU range is authoritative; copy everything
        // else GPU-to-GPU, then transfer the complete store to native ownership.
        if(atlas->m_bDirty) {
            const auto cocos=reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"libcocos2d.dll"));
            const auto table=*reinterpret_cast<const std::uintptr_t* const*>(atlas->getTexture());
            const unsigned char pureGetter[]{0x8b,0x41,0x48,0xc3};
            if(table[12]!=reinterpret_cast<std::uintptr_t>(cocos+0x10760) ||
                std::memcmp(cocos+0x10760,pureGetter,sizeof(pureGetter))) return fail("foreign native texture getter");
            if(!buffer) buffer.reset(Buffer::createDynamicDraw("Native exchange spare",1));
            const auto rebuilt=rebuildNativeGpuStore(atlas->m_pBuffersVBO[0],buffer->getId(),
                static_cast<std::size_t>(first)*sizeof(NativeQuadState),count*sizeof(NativeQuadState),atlas->m_pQuads+first);
            if(!rebuilt.bytes) return fail("native GPU exchange unsupported or invalid storage");
            buffer->exchangeNativeStore(atlas->m_pBuffersVBO[0],rebuilt.bytes);
            atlas->m_bDirty=false;
            stats.uploadedBytes+=count*sizeof(NativeQuadState);
            ++uploadStats.direct;uploadStats.uploaded+=count;
        } else ++uploadStats.sparse;
#else
        if(capacity<captureCount) {
            std::size_t next=std::max<std::size_t>(256,capacity);
            while(next<captureCount) next*=2;
            auto replacement=std::unique_ptr<Buffer>(Buffer::createDynamicDraw("Adaptive native vertex store",next*sizeof(NativeQuadState)));
            if(!replacement || !replacement->getId() || !glGetBufferParameteri64v) return fail("adaptive vertex storage unavailable");
            replacement->bindAs(GL_ARRAY_BUFFER);
            GLint64 actualBytes=0;glGetBufferParameteri64v(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&actualBytes);
            if(actualBytes<0 || static_cast<std::uint64_t>(actualBytes)<next*sizeof(NativeQuadState)) return fail("adaptive vertex storage allocation failed");
#ifndef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
            configureNativeVertexLayout(vao,replacement->getId());
#endif
            buffer=std::move(replacement);capacity=next;uploads.invalidate();
        }
        const auto source=std::as_bytes(std::span(atlas->m_pQuads,captureCount));
#ifdef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
        buffer->write(atlas->m_pQuads,source.size());
        const UploadWork work{UploadMode::Direct,0,0,captureCount,1};
#else
        const auto work=uploads.update(source,capacity,[&](std::size_t offset,std::size_t count,const void* data) {
            buffer->write(const_cast<void*>(data),count*sizeof(NativeQuadState),offset*sizeof(NativeQuadState));
        });
#endif
        stats.uploadedBytes+=work.uploaded*sizeof(NativeQuadState);
        uploadStats.direct+=work.mode==UploadMode::Direct;uploadStats.seed+=work.mode==UploadMode::Seed;
        uploadStats.sparse+=work.mode==UploadMode::Sparse;uploadStats.compared+=work.compared;
        uploadStats.copied+=work.copied;uploadStats.uploaded+=work.uploaded;
#endif
#else
        bool resized=false;
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
#endif
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
#ifdef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
        restore.handoff();
#ifdef CHLORIUM_NATIVE_GPU_EXCHANGE_V8
        native();
#else
        {
            NativeVertexLease lease(atlas->m_pBuffersVBO[0],atlas->m_bDirty,buffer->getId());
            native();
        }
#endif
#else
        // Keep the actual native program and every uniform (MVP, sampler,
        // effect parameters, mod changes). No shader or matrix reconstruction.
        submitNativeVertices(vao,nativeIndices,atlas->getTexture()->getName(),static_cast<GLsizei>(count),first);
#endif
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
#ifndef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
        CC_INCREMENT_GL_DRAWS(1);
#endif
        const auto frame = CCDirector::get()->getTotalFrames();
        if (frame != lastFrame) { lastFrame = frame; ++stats.frames; stats.sprites = 0; }
        stats.sprites += count;
        stats.active = 1;
        submissionTimer.committed = true;
        return true;
#endif
    }
};
}
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
extern "C" __declspec(dllexport) bool ChloriumReadTransferStatsV9(chlorium::snapshot::TransferStatsV9* output,std::size_t size) {
    if(!output||size!=sizeof(*output)) return false;
    *output=chlorium::snapshot::transferStats;return true;
}
#endif
extern "C" __declspec(dllexport) bool ChloriumReadNativeSnapshotStats(
    chlorium::snapshot::Stats* output,std::size_t size) {
    if (!output || size != sizeof(*output)) return false;
    *output = chlorium::snapshot::stats;
    const auto frame = CCDirector::get()->getTotalFrames();
    output->active = output->active && frame - chlorium::snapshot::lastFrame <= 2 && Mod::get()->getSettingValue<bool>("enabled");
    return true;
}
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
extern "C" __declspec(dllexport) bool ChloriumReadAdaptiveUploadStatsV6(chlorium::snapshot::UploadStatsV6* output,std::size_t size) {
    if(!output||size!=sizeof(*output)) return false;
    *output=chlorium::snapshot::uploadStats;return true;
}
#endif
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
        bool nativeInProgress=false;
        const auto forwardNative=[&] {
#if defined(CHLORIUM_NATIVE_DRAW_BRIDGE_V7) && !defined(CHLORIUM_NATIVE_GPU_EXCHANGE_V8)
            // A partial native draw clears an atlas-wide dirty bit. Populate
            // the entire original store first when switching from a lease.
            if(amount && m_bDirty && (first || amount<getTotalQuads()) && m_pQuads && m_pBuffersVBO[0] &&
                wglGetCurrentContext() && glGetBufferParameteri64v && glIsBuffer(m_pBuffersVBO[0])) {
                const auto required=static_cast<std::uint64_t>(getCapacity())*sizeof(chlorium::snapshot::NativeQuadState);
                (void)hydrateNativeVertexStore(m_pBuffersVBO[0],m_pQuads,static_cast<std::size_t>(required));
            }
#endif
            nativeInProgress=true;
            CCTextureAtlas::drawNumberOfQuads(amount,first);
            nativeInProgress=false;
        };
        auto batch = chlorium::snapshot::activeBatch;
        auto cache = chlorium::snapshot::activeCache;
        if (!batch || !cache) { forwardNative(); return; }
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
        // Never skip another mod's atlas callback or guess a changed engine ABI.
        if (!chlorium::snapshot::nativeAtlasContract() || chlorium::snapshot::otherAtlasHooks()) {
            ++chlorium::snapshot::stats.fallbacks;
            forwardNative(); return;
        }
#endif
        if(batch->getTextureAtlas()!=this) {forwardNative();return;}
#ifndef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
        auto stock = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor);
        GLint currentProgram = 0;
        if (batch && batch->getTextureAtlas() == this && stock && batch->getShaderProgram() == stock)
            glGetIntegerv(GL_CURRENT_PROGRAM,&currentProgram);
        // A later native/mod callback can choose a different program after
        // the batch's shader pointer was read. Do not overwrite its effect.
        if (!batch || !cache || batch->getTextureAtlas() != this || !stock || batch->getShaderProgram() != stock ||
            static_cast<GLuint>(currentProgram) != stock->getProgram()) {
            forwardNative(); return;
        }
#endif
        try {
            if (cache->draw(this,amount,first,forwardNative)) return;
        } catch (const std::exception& error) {
            if(nativeInProgress) throw;
            log::warn("Chlorium native quad allocation/capture failed: {}",error.what());
            cache->fail("native quad allocation/capture exception");
        }
        // Native preparation already ran exactly once. Falling back at the
        // submission boundary avoids repeating updateTransform/mod hooks.
        forwardNative();
    }
};
class $modify(ChloriumNativeQuadLevel,PlayLayer) {
    void setupHasCompleted() {
        chlorium::snapshot::stats = {};
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
        chlorium::snapshot::transferStats={};
        if(chlorium::snapshot::transfer) chlorium::snapshot::transfer->work={};
#endif
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
        chlorium::snapshot::uploadStats={};
#endif
        chlorium::snapshot::lastFrame = ~0u;
        PlayLayer::setupHasCompleted();
    }
};
#endif
