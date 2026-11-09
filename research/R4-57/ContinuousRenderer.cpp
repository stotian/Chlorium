#include <Geode/Geode.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include "../R4-49/NativeReplay.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
#include "TargetBackup.hpp"
#include "ContinuousStats.hpp"
#ifdef CHLORIUM_DIRECT_VERTEX_PAGES_R65
#include "../R4-65/DirectAtlas.hpp"
#include "../R4-65/DirectStats.hpp"
extern "C" __declspec(dllexport) const char chloriumDirectVertexPagesIdentity[]="CHLORIUM_DIRECT_VERTEX_PAGES_R65";
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
extern "C" __declspec(dllexport) const char chloriumAdaptiveVertexPagesIdentity[]="CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66";
#endif
#endif
#ifdef CHLORIUM_GROUP_STATE_TRACE_R63
#include "../R4-63/TraceAdapter.hpp"
#endif
#include <atomic>
#include <unordered_map>
#ifdef CHLORIUM_RESIDENT_FAST_R58
#include "../R4-58/CanonicalIndexCache.hpp"
#include "../R4-58/NativeDrawSuppression.hpp"
extern "C" __declspec(dllexport) const char chloriumFastIdentity[]="CHLORIUM_RESIDENT_FAST_R58";
#endif
#ifdef CHLORIUM_NATIVE_STREAM_DIRECT_R61
extern "C" __declspec(dllexport) const char chloriumNativeStreamDirectIdentity[]="CHLORIUM_NATIVE_STREAM_DIRECT_R61";
#endif
#ifdef CHLORIUM_RENDERER_TIMING_R60
extern "C" __declspec(dllexport) const char chloriumRendererTimingIdentity[]="CHLORIUM_RENDERER_TIMING_R60";
#endif
#ifdef CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59
#include "../R4-59/NativeUploadDeferral.hpp"
extern "C" __declspec(dllexport) const char chloriumUploadDeferralIdentity[]="CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59";
#endif

using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumContinuousIdentity[]="CHLORIUM_RESIDENT_CONTINUOUS_R57";
static_assert(offsetof(CCSprite,m_sQuad)==0x1cc && offsetof(CCSprite,m_transformToBatch)==0x17c);
static_assert(offsetof(CCTextureAtlas,m_pBuffersVBO)==0x40);
#if defined(CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59) || defined(CHLORIUM_DIRECT_VERTEX_PAGES_R65)
static_assert(offsetof(CCTextureAtlas,m_bDirty)==0x48);
#endif
namespace chlorium::research::continuous {
namespace {
struct PostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~PostState(){_mm_setcsr(fp);SetLastError(error);}};
Stats stats;
std::atomic<DWORD> ownerThread{0};
#ifdef CHLORIUM_DIRECT_VERTEX_PAGES_R65
directvertices::DirectAtlas directAtlas;
bool directCallbacks=false;
bool nativeDirectCallbacks(){
    const auto module=GetModuleHandleW(L"libcocos2d.dll");if(!handoff::nativeAtlasBody(module))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    for(const auto mod:Loader::get()->getAllMods())if(mod!=Mod::get())for(const auto hook:mod->getHooks())
        if(hook->isEnabled() && (hook->getAddress()==base+0xa6350 || hook->getAddress()==base+0x10760 || hook->getAddress()==base+0x87fe0))return false;
    return true;
}
#endif
thread_local CCSpriteBatchNode* currentBatch=nullptr;
std::uintptr_t levelAddress=0;unsigned levelId=0;
HGLRC cacheContext=nullptr;
struct Cache {
    unsigned uid=0,vbo=0,ibo=0,nativeCapacity=0,capacity=0;
    bool declined=false;
    std::unique_ptr<resident::NativeReplay> replay;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    CanonicalIndexCache topology;
#endif
};
std::unordered_map<std::uintptr_t,Cache> caches;
batchreplay::TargetBackup backup;
std::vector<pass::SpriteRecord> sources;
std::vector<unsigned char> vertices,indices;
#ifdef CHLORIUM_RESIDENT_FAST_R58
std::span<const unsigned char> indexView;
struct FastStats {
    std::uint64_t version=58,indexReads=0,indexBytes=0,indexReuses=0,nativePointBatches=0,suppressedNativeDraws=0;
    std::uint64_t checkpointsSaved=0,preSubmitDeclines=0;
} fastStats;
NativeDrawSuppression suppression;
bool isolatedIndexWriters=false;
bool indexWriterBody(){
    const auto module=GetModuleHandleW(L"libcocos2d.dll");
    if(!handoff::nativeAtlasBody(module))return false;
    const auto base=reinterpret_cast<const unsigned char*>(module);
    if(reinterpret_cast<const unsigned char*>(GetProcAddress(module,"?mapBuffers@CCTextureAtlas@cocos2d@@AEAAXXZ"))!=base+0xa69c0)return false;
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned n=0;n<156;++n)hash=(hash^base[0xa69c0+n])*1099511628211ull;
    return hash==0xe348dc857131a9f0ull;
}
bool isolatedMods(){
    if(Loader::get()->getLoadingState()!=Loader::LoadingState::Done)return false;
    for(const auto mod:Loader::get()->getAllMods())if(mod->isLoaded()){
        const auto& id=mod->getID();
        if(id!="geode.loader" && id!="geode.node-ids" && id!="local.chlorium" && id!="local.chlorium.audit")return false;
    }
    return true;
}
#endif
#ifdef CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59
bool uploadDeferralUsed=false;
bool nativeUploadUnmodified()noexcept{
    const auto module=GetModuleHandleW(L"libcocos2d.dll"),glew=GetModuleHandleW(L"glew32.dll");
    if(!handoff::nativeAtlasBody(module) || !glew)return false;
    const auto base=reinterpret_cast<unsigned char*>(module);
    const auto source=reinterpret_cast<PFNGLBUFFERSUBDATAPROC*>(GetProcAddress(glew,"__glewBufferSubData"));
    const auto imported=reinterpret_cast<PFNGLBUFFERSUBDATAPROC**>(base+0xe4d48);
    return source && *imported==source && *source==gl::currentProc<PFNGLBUFFERSUBDATAPROC>("glBufferSubData");
}
#endif
unsigned flag(const void* address){return *static_cast<const volatile unsigned char*>(address)!=0;}
bool enabled(){return !stats.fatal && Mod::get()->getSettingValue<bool>("enabled");}
bool objectLayer(CCNode* node){
    auto play=PlayLayer::get();if(!play || !play->m_objectLayer)return false;
    for(unsigned depth=0;node && depth<64;++depth,node=node->m_pParent)if(node==play->m_objectLayer)return true;
    return false;
}
void source(CCSprite* sprite,pass::SpriteRecord& out){
    out.sprite=reinterpret_cast<std::uintptr_t>(sprite);out.id=sprite->m_uID;out.atlasIndex=sprite->m_uAtlasIndex;
    out.flags=flag(&sprite->m_bShouldBeHidden)|flag(&sprite->m_bDirty)<<1|flag(&sprite->m_bRecursiveDirty)<<2|
        flag(&sprite->m_bFlipX)<<3|flag(&sprite->m_bFlipY)<<4|flag(&sprite->m_bRectRotated)<<5|
        flag(&sprite->m_bUseVertexMod)<<6|flag(&sprite->m_bDontDraw)<<7|flag(&sprite->m_bOpacityModifyRGB)<<8|
        flag(&sprite->_realOpacity)<<9|flag(&sprite->m_bVisible)<<10;
    std::memcpy(out.matrix,&sprite->m_transformToBatch,sizeof(out.matrix));
    const void* rectangle[]{&sprite->m_obOffsetPosition.x,&sprite->m_obOffsetPosition.y,&sprite->m_obRect.size.width,&sprite->m_obRect.size.height};
    for(unsigned n=0;n<4;++n)std::memcpy(out.localRect+n,rectangle[n],4);
    std::memcpy(out.anchor,&sprite->m_obAnchorPointInPoints,sizeof(out.anchor));std::memcpy(&out.z,&sprite->m_fVertexZ,4);
    const void* mods[]{&sprite->m_fTlVertexMod,&sprite->m_fTrVertexMod,&sprite->m_fBlVertexMod,&sprite->m_fBrVertexMod};
    const void* bounds[]{&sprite->m_fTextureLeft,&sprite->m_fTextureRight,&sprite->m_fTextureBottom,&sprite->m_fTextureTop};
    for(unsigned n=0;n<4;++n){std::memcpy(out.vertexMods+n,mods[n],4);std::memcpy(out.textureBounds+n,bounds[n],4);}
}
bool collect(CCTextureAtlas* atlas,unsigned count){
#ifdef CHLORIUM_RENDERER_TIMING_R60
    residenttiming::Scope timing(residenttiming::Stage::collect);
#endif
    if(!count || count>16384 || !pass::validRange(atlas->m_uTotalQuads,atlas->m_uCapacity,0,count) || !atlas->m_pQuads ||
       currentBatch->m_pobTextureAtlas!=atlas)return false;
    const auto descendants=currentBatch->m_pobDescendants;
    if(!descendants || !descendants->data || descendants->data->num>pass::maxQuads ||
       (descendants->data->num && !descendants->data->arr))return false;
    sources.assign(count,{});
    for(unsigned n=0;n<descendants->data->num;++n){
        auto sprite=static_cast<CCSprite*>(descendants->data->arr[n]);
        if(!sprite || sprite->m_uAtlasIndex>=count)continue;
        auto& out=sources[sprite->m_uAtlasIndex];
        if(out.sprite || sprite->m_pobBatchNode!=currentBatch || sprite->m_pobTextureAtlas!=atlas)return false;
        source(sprite,out);
        out.quadMatches=std::memcmp(&sprite->m_sQuad,atlas->m_pQuads+out.atlasIndex,96)==0;
    }
    for(const auto& s:sources)if(!s.sprite || !s.quadMatches)return false;
    vertices.resize(std::size_t(count)*96);std::memcpy(vertices.data(),atlas->m_pQuads,vertices.size());
#ifdef CHLORIUM_RESIDENT_FAST_R58
    return true;
}
bool collectIndices(CCTextureAtlas* atlas,unsigned count,Cache& entry){
#ifdef CHLORIUM_RENDERER_TIMING_R60
    residenttiming::Scope timing(residenttiming::Stage::indices);
#endif
    indexView={};
#endif
    const auto ibo=atlas->m_pBuffersVBO[1];if(!ibo || !glIsBuffer(ibo))return false;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    auto read=[&](std::span<unsigned char> out){
        GLint old=0,size=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);glBindBuffer(GL_COPY_READ_BUFFER,ibo);
        glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);
        bool ok=size>=0 && out.size()<=std::size_t(size);
        if(ok){glGetBufferSubData(GL_COPY_READ_BUFFER,0,out.size(),out.data());++fastStats.indexReads;fastStats.indexBytes+=out.size();}
        glBindBuffer(GL_COPY_READ_BUFFER,old);return glGetError()==GL_NO_ERROR && ok;
    };
    static const bool writer=indexWriterBody();
    if(writer && isolatedIndexWriters && atlas->m_pIndices){
        // shortcut: GPU-only index writers are not observed, qualify all writers before general release.
        const auto prior=fastStats.indexReads;
        indexView=entry.topology.acquire({reinterpret_cast<std::uintptr_t>(atlas),reinterpret_cast<std::uintptr_t>(atlas->m_pIndices),
            reinterpret_cast<std::uintptr_t>(wglGetCurrentContext()),atlas->m_uID,ibo,atlas->m_uCapacity},
            {atlas->m_pIndices,std::size_t(std::min(atlas->m_uCapacity,16384u))*6},count,read);
        if(indexView.empty())return false;
        fastStats.indexReuses+=prior==fastStats.indexReads;return true;
    }
    entry.topology.reset();indices.resize(std::size_t(count)*12);
    if(!read(indices))return false;
    indexView=indices;return true;
#else
    GLint old=0,size=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);glBindBuffer(GL_COPY_READ_BUFFER,ibo);
    glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);
    const auto bytes=std::size_t(count)*12;
    if(size<0 || bytes>std::size_t(size)){glBindBuffer(GL_COPY_READ_BUFFER,old);return false;}
    indices.resize(bytes);glGetBufferSubData(GL_COPY_READ_BUFFER,0,bytes,indices.data());glBindBuffer(GL_COPY_READ_BUFFER,old);
    return glGetError()==GL_NO_ERROR;
#endif
}
bool attempt(CCTextureAtlas* atlas,unsigned count,unsigned first)noexcept{
    PostState preserve;bool saved=false;
    try{
        if(first || !currentBatch || !enabled()){++stats.declines[0];return false;}
        static const bool body=handoff::nativeAtlasBody(GetModuleHandleW(L"libcocos2d.dll"));
        if(!body || !glMemoryBarrier || !handoff::standardTexture(GetModuleHandleW(L"libcocos2d.dll"),atlas->m_pTexture)){
            ++stats.declines[1];return false;
        }
        auto play=PlayLayer::get();if(!play){++stats.declines[0];return false;}
        const auto address=reinterpret_cast<std::uintptr_t>(play);const auto context=wglGetCurrentContext();
        if(address!=levelAddress || play->m_uID!=levelId || context!=cacheContext){
#ifdef CHLORIUM_RESIDENT_FAST_R58
            suppression.close();
#endif
            caches.clear();backup.close();levelAddress=address;levelId=play->m_uID;cacheContext=context;
        }
#ifdef CHLORIUM_RESIDENT_FAST_R58
        if(count && count<=16384 && pass::validRange(atlas->m_uTotalQuads,atlas->m_uCapacity,0,count) && atlas->m_pQuads &&
           currentBatch->m_pobTextureAtlas==atlas && std::all_of(atlas->m_pQuads,atlas->m_pQuads+count,[](const auto& quad){
               snapshot::NativeQuadState native{};std::memcpy(&native,&quad,sizeof(native));return batchreplay::pointQuad(native);
           })){
            ++fastStats.nativePointBatches;return false;
        }
#endif
        if(!collect(atlas,count)){++stats.declines[2];return false;}
        const auto key=reinterpret_cast<std::uintptr_t>(atlas);
        auto at=caches.find(key);
        if(at!=caches.end() && (at->second.uid!=atlas->m_uID || at->second.vbo!=atlas->m_pBuffersVBO[0] ||
            at->second.ibo!=atlas->m_pBuffersVBO[1] || at->second.nativeCapacity!=atlas->m_uCapacity || count>at->second.capacity)){
            caches.erase(at);at=caches.end();
        }
        if(at==caches.end()){
            const auto capacity=unsigned(std::min<std::size_t>(16384,std::max<std::size_t>(256,std::size_t(count)*2)));
            std::size_t allocated=0;for(const auto& [_,c]:caches)allocated+=c.capacity;
            if(caches.size()>=48 || capacity>65536-allocated){++stats.declines[3];return false;}
            Cache entry;entry.uid=atlas->m_uID;entry.vbo=atlas->m_pBuffersVBO[0];entry.ibo=atlas->m_pBuffersVBO[1];
            entry.nativeCapacity=atlas->m_uCapacity;entry.capacity=capacity;
            static const std::string residentSource=[] {
                std::ifstream file(Mod::get()->getResourcesDir()/"resident.vert",std::ios::binary);
                std::string text(2057,'\0');file.read(text.data(),text.size());
                if(file.gcount()!=2056 || !file.eof())return std::string{};
                text.resize(2056);return text;
            }();
            if(residentSource.empty()){++stats.declines[1];return false;}
            entry.replay=std::make_unique<resident::NativeReplay>(true,residentSource);entry.replay->requestNativeStream(true);
            at=caches.emplace(key,std::move(entry)).first;
        }
        auto& entry=at->second;
        if(entry.declined){++stats.declines[4];return false;}
#ifdef CHLORIUM_RESIDENT_FAST_R58
        if(!collectIndices(atlas,count,entry)){++stats.declines[2];return false;}
#endif
        auto program=currentBatch->m_pShaderProgram;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        const auto inputIndices=indexView;
#else
        const auto inputIndices=std::span<const unsigned char>(indices);
#endif
        if(!program || !entry.replay->prepare(vertices,inputIndices,sources,program->getProgram(),atlas->m_pTexture->getName())){
            entry.declined=true;++stats.declines[5];return false;
        }
        const auto prior=entry.replay->calls;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        struct Checkpoint {bool& saved;bool refused=false;} checkpoint{saved};
        const auto save=+[](void* opaque){
            auto& state=*static_cast<Checkpoint*>(opaque);
            state.saved=backup.begin();state.refused=!state.saved;
            fastStats.checkpointsSaved+=state.saved;return state.saved;
        };
        const bool drawn=entry.replay->draw(save,&checkpoint);
        if(checkpoint.refused){++stats.declines[6];return false;}
#else
        if(!backup.begin()){++stats.declines[6];return false;}saved=true;
        const bool drawn=entry.replay->draw();
#endif
        if(!drawn){
            if(entry.replay->calls!=prior){++stats.rollbacks;if(!backup.restore())++stats.fatal;}
            else backup.commit();
#ifdef CHLORIUM_RESIDENT_FAST_R58
            // A pre-submit rejection changed no pixels; allow transient state to recover.
            entry.declined=entry.replay->calls!=prior;
            fastStats.preSubmitDeclines+=!entry.declined;
#else
            entry.declined=true;
#endif
            ++stats.declines[7];return false;
        }
        ++stats.submitted;stats.submittedQuads+=count;stats.reconstructed+=entry.replay->reconstructed;
        return true;
    }catch(...){if(saved){++stats.rollbacks;if(!backup.restore())++stats.fatal;}++stats.declines[7];return false;}
}
struct DiscardNative {
    bool active=false;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    bool conditional=false;
#endif
    DiscardNative(){
        PostState preserve;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        if(isolatedIndexWriters && suppression.begin()){
            conditional=active=true;++fastStats.suppressedNativeDraws;return;
        }
#endif
        glEnable(GL_RASTERIZER_DISCARD);active=glGetError()==GL_NO_ERROR;if(!active)glDisable(GL_RASTERIZER_DISCARD);
    }
    ~DiscardNative(){
        PostState preserve;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        if(conditional){if(!suppression.end())++stats.fatal;return;}
#endif
        glDisable(GL_RASTERIZER_DISCARD);
    }
};
}
}
#ifdef CHLORIUM_RESIDENT_FAST_R58
extern "C" __declspec(dllexport) bool ChloriumReadFastStatsR58(void* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(fastStats) || ownerThread.load()!=GetCurrentThreadId())return false;
    std::memcpy(out,&fastStats,size);return true;
}
#endif
#ifdef CHLORIUM_DIRECT_VERTEX_PAGES_R65
#ifdef CHLORIUM_DIRECT_CONTEXT_GUARD_R67
extern "C" __declspec(dllexport) bool ChloriumReadNativeAuthorityStatsR67(chlorium::research::directvertices::NativeAuthorityStats* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(*out) || ownerThread.load()!=GetCurrentThreadId())return false;
    *out={67,directAtlas.canonicalUploads,directAtlas.canonicalBytes};return true;
}
#endif
extern "C" __declspec(dllexport) bool ChloriumReadDirectVertexStatsR65(chlorium::research::directvertices::Stats* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(*out) || ownerThread.load()!=GetCurrentThreadId())return false;
    const auto& m=directAtlas.metrics();*out={65,m.uploads,m.bytes,m.busy,m.fences,m.allocated,directAtlas.fenceFailures};return true;
}
#endif
extern "C" __declspec(dllexport) bool ChloriumReadContinuousStatsR57(chlorium::research::continuous::Stats* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(*out) || (ownerThread.load(std::memory_order_acquire) && ownerThread.load()!=GetCurrentThreadId()))return false;
    *out=stats;out->enabled=enabled();return true;
}
#ifdef CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59
extern "C" __declspec(dllexport) bool ChloriumReadUploadDeferralStatsR59(void* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(NativeUploadDeferral::stats) || ownerThread.load()!=GetCurrentThreadId())return false;
    std::memcpy(out,&NativeUploadDeferral::stats,size);return true;
}
#endif
#ifdef CHLORIUM_RENDERER_TIMING_R60
extern "C" __declspec(dllexport) bool ChloriumReadRendererTimingR60(void* out,std::size_t size){
    using namespace chlorium::research::continuous;
    if(!out || size!=sizeof(chlorium::research::residenttiming::stats) || ownerThread.load()!=GetCurrentThreadId())return false;
    std::memcpy(out,&chlorium::research::residenttiming::stats,size);return true;
}
#endif
class $modify(ChloriumContinuousFrame,CCDisplayLinkDirector){
    void mainLoop(){
        using namespace chlorium::research::continuous;
        DWORD expected=0;ownerThread.compare_exchange_strong(expected,GetCurrentThreadId());
        if(ownerThread.load()==GetCurrentThreadId() && PlayLayer::get())++stats.frames;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        // The mod graph may still change during startup; inspect it only inside a level.
        if(ownerThread.load()==GetCurrentThreadId())isolatedIndexWriters=PlayLayer::get() && isolatedMods();
#ifdef CHLORIUM_DIRECT_VERTEX_PAGES_R65
        if(ownerThread.load()==GetCurrentThreadId()){
            PostState preserve;
            directCallbacks=(PlayLayer::get()?isolatedIndexWriters:isolatedMods()) && nativeDirectCallbacks();
        }
#endif
#endif
#ifdef CHLORIUM_GROUP_STATE_TRACE_R63
        if(ownerThread.load()==GetCurrentThreadId())chlorium::research::grouptrace::begin(PlayLayer::get(),isolatedIndexWriters && enabled());
#endif
        CCDisplayLinkDirector::mainLoop();
#ifdef CHLORIUM_GROUP_STATE_TRACE_R63
        if(ownerThread.load()==GetCurrentThreadId())chlorium::research::grouptrace::end();
#endif
    }
};
class $modify(ChloriumContinuousBatch,CCSpriteBatchNode){
    void draw(){
        using namespace chlorium::research::continuous;
        auto old=currentBatch;
        currentBatch=ownerThread.load()==GetCurrentThreadId() && objectLayer(this)?this:nullptr;
        struct Restore{CCSpriteBatchNode* old;~Restore(){currentBatch=old;}}restore{old};
#ifdef CHLORIUM_RENDERER_TIMING_R60
        chlorium::research::residenttiming::BatchScope timing(currentBatch!=nullptr);
#endif
        CCSpriteBatchNode::draw();
    }
};
class $modify(ChloriumContinuousAtlas,CCTextureAtlas){
    void drawNumberOfQuads(unsigned count,unsigned first){
        using namespace chlorium::research::continuous;
#ifdef CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59
        struct KeepPartialDirty {
            CCTextureAtlas* atlas;bool keep;
            ~KeepPartialDirty(){if(keep)atlas->m_bDirty=true;}
        } keepDirty{this,ownerThread.load()==GetCurrentThreadId() && uploadDeferralUsed &&
            retainPartialDirty(flag(&m_bDirty),first,count,m_uTotalQuads)};
#endif
        if(!currentBatch){CCTextureAtlas::drawNumberOfQuads(count,first);return;}
#ifdef CHLORIUM_RENDERER_TIMING_R60
        chlorium::research::residenttiming::Scope timing(chlorium::research::residenttiming::Stage::atlas);
#endif
        const auto drawNative=[&]{
#ifdef CHLORIUM_RENDERER_TIMING_R60
            chlorium::research::residenttiming::Scope timing(chlorium::research::residenttiming::Stage::native);
#endif
            CCTextureAtlas::drawNumberOfQuads(count,first);
        };
        ++stats.atlasCalls;stats.consideredQuads+=count;
#ifdef CHLORIUM_DIRECT_VERTEX_PAGES_R65
        bool drawn=false;
        {
            PostState preserve;
            drawn=enabled() && directCallbacks && currentBatch->m_pobTextureAtlas==this &&
                chlorium::research::handoff::standardTexture(GetModuleHandleW(L"libcocos2d.dll"),m_pTexture);
        }
        if(drawn && directAtlas.draw(m_pBuffersVBO[0],m_bDirty,m_pQuads,count,first,m_uTotalQuads,m_uCapacity,drawNative)){
            ++stats.committed;stats.committedQuads+=count;
        }else{++stats.nativeFallbacks;drawNative();}
        return;
#endif
        if(!attempt(this,count,first)){++stats.nativeFallbacks;drawNative();return;}
        DiscardNative discard;
        if(!discard.active){
            ++stats.rollbacks;if(!backup.restore())++stats.fatal;
            ++stats.nativeFallbacks;drawNative();return;
        }
        // Preserve native state/cache/counter/callback once; a deferred GPU mirror stays dirty.
#ifdef CHLORIUM_NATIVE_UPLOAD_DEFERRAL_R59
        if(discard.conditional && isolatedIndexWriters && nativeUploadUnmodified()){
            NativeUploadDeferral::Scope deferred(m_pBuffersVBO[0],m_bDirty,std::size_t(count)*96,true);
            uploadDeferralUsed|=deferred.skipped();
            drawNative();
        }else drawNative();
        backup.commit();++stats.committed;stats.committedQuads+=count;
#else
        drawNative();backup.commit();++stats.committed;stats.committedQuads+=count;
#endif
    }
};
