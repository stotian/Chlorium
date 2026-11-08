#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
#if !defined(CHLORIUM_MEASUREMENT_ONLY) || !defined(CHLORIUM_CANONICAL_AUDIT_V10)
#error Native pass capture requires its separate canonical diagnostic auditor.
#endif
#include "NativePassCapture.hpp"
#include "PassCapture.hpp"
#include <Geode/Geode.hpp>
#include "../R4-34/NativeQuadReadObservation.hpp"
#include "../R4-35/OwnedRegisterOracle.hpp"
#include "../R4-37/NativeOpacityInvocations.hpp"
#include "../R4-41/NativeInputCensus.hpp"
#include "../R4-42/NativeMovementCensus.hpp"
#include "../R4-45/StartupDeathWatch.hpp"
#include "../R4-54/ProducerShadow.hpp"
#include "../../tests/MeasurementHookPolicy.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <fstream>

using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumNativePassCaptureIdentity[] = "CHLORIUM_NATIVE_PASS_CAPTURE_V3";
extern "C" __declspec(dllexport) const char chloriumNativeMovementCensusIdentity[] = "CHLORIUM_NATIVE_MOVEMENT_CENSUS_R42";
extern "C" __declspec(dllexport) const char chloriumProducerShadowIdentity[] = "CHLORIUM_NATIVE_PRODUCER_SHADOW_R54";
extern "C" __declspec(dllexport) const char chloriumProducerReplayIdentity[] = "CHLORIUM_NATIVE_PRODUCER_REPLAY_R55";
static_assert(offsetof(CCSprite,m_sQuad)==0x1cc && offsetof(CCSprite,m_transformToBatch)==0x17c);
static_assert(offsetof(CCSprite,m_fVertexZ)==0x48 && offsetof(CCSprite,m_bVisible)==0x11e);
static_assert(offsetof(CCSprite,_realOpacity)==0x149 && offsetof(CCSprite,m_bDontDraw)==0x22f);
static_assert(offsetof(CCSprite,m_obRect)==0x1a8 && offsetof(CCSprite,m_obOffsetPosition)==0x1bc);
static_assert(offsetof(CCTextureAtlas,m_bDirty)==0x48);

namespace chlorium::research::pass {
namespace {
std::array<Frame, 2> frames;
std::vector<SpriteRecord> owners;
thread_local Frame* active = nullptr;
CCNode* scene = nullptr;
thread_local CCSpriteBatchNode* batch = nullptr;
unsigned frameCount = 0;
thread_local unsigned scopeOrdinal = 0, activeScope = 0;
bool attempted = false, aborted = false, written = false, successful = false;
std::array<std::array<unsigned,256>,4> controlBytes{};
std::array<std::array<unsigned char,maxDraws>,2> atlasDirty{};
std::atomic<DWORD> producerThread{0};
producers::Shadow producerShadow;
unsigned producerHooks=0,producerActiveHooks=0;
bool producerHooksRunning=false;
bool producerRequested(){static const bool enabled=[] {const auto p=std::getenv("CHLORIUM_NATIVE_PRODUCER_CAPTURE");return p&&std::string_view(p)=="1";}();return enabled;}
bool producerWindow(){return producerRequested() && producerThread.load(std::memory_order_acquire)==GetCurrentThreadId() && attempted && !aborted && !written && (frameCount==1 || (frameCount==2 && active));}
struct ProducerPostState {PreserveFP fp;DWORD error=GetLastError();~ProducerPostState(){SetLastError(error);}};
void toggleProducerHooks(bool enable)noexcept{
    if(!enable && !producerHooksRunning)return;
    ProducerPostState saved;
    if(enable)producerHooksRunning=true;
    try{
        unsigned activeHooks=0;
        for(auto hook:Mod::get()->getHooks())if(chlorium::test::nativeProducerObserverName(hook->getDisplayName())){
            if(hook->isEnabled()!=enable){auto result=enable?hook->enable():hook->disable();if(result.isErr())producerShadow.failed=true;}
            activeHooks+=hook->isEnabled();
        }
        if(enable){producerActiveHooks=activeHooks;if(activeHooks!=3)producerShadow.failed=true;}
        else{producerHooksRunning=activeHooks!=0;if(activeHooks)producerShadow.failed=true;}
    }catch(...){producerShadow.failed=true;}
}
void producerSprite(CCSprite* sprite,bool color)noexcept{
    ProducerPostState saved;if(!producerWindow())return;
    try{
        SpriteRecord source;source.sprite=reinterpret_cast<std::uintptr_t>(sprite);source.id=sprite->m_uID;source.atlasIndex=sprite->m_uAtlasIndex;
        // Read only this live receiver. Parent/atlas addresses are never followed.
        source.flags=nativeFlag(&sprite->m_bShouldBeHidden)|nativeFlag(&sprite->m_bDirty)<<1|nativeFlag(&sprite->m_bRecursiveDirty)<<2|
            nativeFlag(&sprite->m_bFlipX)<<3|nativeFlag(&sprite->m_bFlipY)<<4|nativeFlag(&sprite->m_bRectRotated)<<5|
            nativeFlag(&sprite->m_bUseVertexMod)<<6|nativeFlag(&sprite->m_bDontDraw)<<7|nativeFlag(&sprite->m_bOpacityModifyRGB)<<8|
            nativeFlag(&sprite->_realOpacity)<<9|nativeFlag(&sprite->m_bVisible)<<10;
        std::memcpy(source.matrix,&sprite->m_transformToBatch,sizeof(source.matrix));
        const void* rectangle[]{&sprite->m_obOffsetPosition.x,&sprite->m_obOffsetPosition.y,&sprite->m_obRect.size.width,&sprite->m_obRect.size.height};
        for(unsigned n=0;n<4;++n)std::memcpy(source.localRect+n,rectangle[n],4);
        std::memcpy(source.anchor,&sprite->m_obAnchorPointInPoints,sizeof(source.anchor));std::memcpy(&source.z,&sprite->m_fVertexZ,4);
        const void* mods[]{&sprite->m_fTlVertexMod,&sprite->m_fTrVertexMod,&sprite->m_fBlVertexMod,&sprite->m_fBrVertexMod};
        const void* bounds[]{&sprite->m_fTextureLeft,&sprite->m_fTextureRight,&sprite->m_fTextureBottom,&sprite->m_fTextureTop};
        for(unsigned n=0;n<4;++n){std::memcpy(source.vertexMods+n,mods[n],4);std::memcpy(source.textureBounds+n,bounds[n],4);}
        snapshot::NativeQuadState quad;std::memcpy(&quad,&sprite->m_sQuad,sizeof(quad));producerShadow.sprite(source,quad,color);
    }catch(...){producerShadow.failed=true;}
}
void producerAtlas(CCTextureAtlas* atlas,unsigned index)noexcept{
    ProducerPostState saved;if(!producerWindow())return;
    try{
        if(index>=atlas->m_uCapacity || !atlas->m_pQuads){producerShadow.failed=true;return;}
        snapshot::NativeQuadState quad;std::memcpy(&quad,atlas->m_pQuads+index,sizeof(quad));
        producerShadow.atlas(reinterpret_cast<std::uintptr_t>(atlas),atlas->m_uID,index,quad);
    }catch(...){producerShadow.failed=true;}
}
struct RootEntry { std::uintptr_t node; GameObject* object; unsigned id; };
std::vector<RootEntry> roots;
bool rootsReady = false;

void rootSource(CCNode* node, SpriteRecord& record) {
    if (!rootsReady) return;
    for (unsigned depth = 0; node && depth < 64; node=node->m_pParent, ++depth) {
        const auto address = reinterpret_cast<std::uintptr_t>(node);
        const auto at = std::lower_bound(roots.begin(), roots.end(), address,
            [](const RootEntry& entry, std::uintptr_t key) { return entry.node < key; });
        if (at == roots.end() || at->node != address || at->id != node->m_uID) continue;
        const auto object = at->object;
        record.root = address; record.rootId = at->id;
        record.rootFlags = 0x80000000u | unsigned(object->m_objectType == GameObjectType::Decoration) |
            nativeFlag(&object->m_isTrigger) << 1 | nativeFlag(&object->m_hasAreaParent) << 2 |
            nativeFlag(&object->m_hasGroupParent) << 3 | nativeFlag(&object->m_areaOpacityRelated) << 4 |
            nativeFlag(&object->m_isUIObject) << 5 | nativeFlag(&object->m_hasNoGlow) << 6 |
            nativeFlag(&object->m_isInvisibleBlock) << 7 | nativeFlag(&object->m_isHide) << 8 |
            nativeFlag(&object->m_hasParticles) << 9 | nativeFlag(&object->m_usesAudioScale) << 10 |
            nativeFlag(&object->m_hasAnimatedChild) << 11 | unsigned(object->m_colorGroupCount > 0) << 12 |
            unsigned(object->m_opacityGroupCount > 0) << 13;
        record.groupCount = static_cast<std::uint32_t>(object->m_groupCount);
        record.objectId = static_cast<std::uint32_t>(object->m_objectID);
        record.objectType = static_cast<std::uint32_t>(object->m_objectType);
        if (object->m_groupCount < 0 || object->m_groupCount > 10 || (object->m_groupCount && !object->m_groups))
            record.rootFlags |= 1u << 14;
        else for (int i=0; i<object->m_groupCount; ++i) {
            const auto group = (*object->m_groups)[i];
            record.groups[i] = static_cast<std::uint32_t>(group);
            if (group < 0 || group > 9999) record.rootFlags |= 1u << 14;
        }
        return;
    }
}

void write() {
    if (written) return;
    written = true;
    const auto path = std::getenv("CHLORIUM_AUDIT_OUTPUT");
    if (!path) return;
    std::ofstream output(std::string(path) + ".pass-capture.bin", std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!output) return;
    const std::array<char, 8> magic{'C','H','L','P','A','S','S','3'};
    const std::array<std::uint32_t, 4> layout{frameCount, sizeof(DrawRecord), sizeof(SpriteRecord), sizeof(snapshot::NativeQuadState)};
    output.write(magic.data(), magic.size());
    output.write(reinterpret_cast<const char*>(layout.data()), sizeof(layout));
    successful = frameCount == 2;
    for (unsigned i = 0; i < frameCount; ++i) {
        const auto& f = frames[i];
        successful = successful && !f.header.reasons && !f.draws.empty() && !f.quads.empty();
        output.write(reinterpret_cast<const char*>(&f.header), sizeof(f.header));
        output.write(reinterpret_cast<const char*>(f.draws.data()), f.draws.size() * sizeof(DrawRecord));
        output.write(reinterpret_cast<const char*>(f.quads.data()), f.quads.size() * sizeof(snapshot::NativeQuadState));
        output.write(reinterpret_cast<const char*>(f.sprites.data()), f.sprites.size() * sizeof(SpriteRecord));
    }
    output.close();
    successful = successful && !output.fail();
    std::ofstream controls(std::string(path)+".native-control-bytes.json",std::ios::out|std::ios::noreplace);
    if(controls){
        controls<<"{\"diagnostic_only\":true,\"histograms\":[";
        for(unsigned kind=0;kind<4;++kind){
            if(kind)controls<<',';controls<<'{';bool comma=false;
            for(unsigned value=0;value<256;++value)if(controlBytes[kind][value]){
                if(comma)controls<<',';comma=true;
                controls<<'\"'<<value<<"\":"<<controlBytes[kind][value];
            }
            controls<<'}';
        }
        controls<<"]}\n";
    }
    std::ofstream dirty(std::string(path)+".native-atlas-dirty.json",std::ios::out|std::ios::noreplace);
    if(!dirty)throw std::runtime_error("write native atlas dirty observations");
    dirty<<"{\"diagnostic_only\":true,\"actual_upload_bytes_measured\":false,\"frames\":[";
    for(unsigned f=0;f<frameCount;++f){if(f)dirty<<',';dirty<<"{\"frame\":"<<frames[f].header.frame<<",\"draws\":[";
        for(unsigned d=0;d<frames[f].draws.size();++d){if(d)dirty<<',';const auto& record=frames[f].draws[d];
            dirty<<"{\"ordinal\":"<<d<<",\"atlas_uid\":"<<record.atlasId<<",\"count\":"<<record.count<<",\"total\":"<<record.total<<",\"dirty_before_native_draw\":"<<(atlasDirty[f][d]?"true":"false")<<'}';}
        dirty<<"]}";}
    dirty<<"]}\n";if(!dirty)throw std::runtime_error("serialize native atlas dirty observations");
    if(producerRequested()){
        unsigned enabledHooks=0;
        for(const auto hook:Mod::get()->getHooks())if(hook->isEnabled()){
            const auto& name=hook->getDisplayName();
            enabledHooks+=name=="cocos2d::CCSprite::updateTransform" || name=="cocos2d::CCSprite::updateColor" || name=="cocos2d::CCTextureAtlas::updateQuad";
        }
        std::ofstream out(std::string(path)+".producer-events.json",std::ios::out|std::ios::noreplace);
        if(!out)throw std::runtime_error("new producer observer receipt");
        out<<"{\"diagnostic_only\":true,\"game_fps_evidence\":false,\"native_callbacks_suppressed\":false,\"cpu_consumer_closure_proven\":false,\"lifetime_closure_proven\":false,\"failed\":"<<(producerShadow.failed?"true":"false")
           <<",\"hooks_registered\":"<<producerHooks<<",\"hooks_enabled_at_sample\":"<<producerActiveHooks<<",\"hooks_enabled_at_finish\":"<<enabledHooks<<",\"pose_callbacks\":"<<producerShadow.poseCallbacks<<",\"color_callbacks\":"<<producerShadow.colorCallbacks<<",\"atlas_writes\":"<<producerShadow.atlasWrites
           <<",\"sampled_render_payload_coverage_complete\":"<<(producerHooks==3&&producerActiveHooks==3&&!enabledHooks&&producerShadow.sampledCoverageComplete()?"true":"false")<<",\"rows\":[";
        bool comma=false;for(const auto& r:producerShadow.coverage){if(comma)out<<',';comma=true;
            out<<"{\"frame\":"<<r.frame<<",\"ordinal\":"<<r.ordinal<<",\"count\":"<<r.count<<",\"raw_matched\":"<<r.rawMatched<<",\"raw_missing\":"<<r.rawMissing<<",\"raw_different\":"<<r.rawDifferent
               <<",\"source_index_matched\":"<<r.indexMatched<<",\"source_index_different\":"<<r.indexDifferent<<",\"descendant_linear_order_different\":"<<r.linearOrderDifferent<<",\"source_matched\":"<<r.sourceMatched<<",\"source_missing\":"<<r.sourceMissing<<",\"source_different\":"<<r.sourceDifferent<<",\"incomplete\":"<<r.incomplete<<'}';}
        out<<"]}\n";if(!out)throw std::runtime_error("serialize producer observer receipt");
    }

}
bool inObjectLayer(CCNode* node) {
    auto play = PlayLayer::get();
    if (!play || !play->m_objectLayer) return false;
    for (unsigned depth = 0; node && depth < 64; ++depth) {
        if (node == play->m_objectLayer) return true;
        node = node->m_pParent;
    }
    return false;
}
void observe(CCTextureAtlas* atlas, unsigned count, unsigned first) {
    if (!active || !batch) return;
    PreserveFP preserve;
    if (CCDirector::get()->getRunningScene() != scene) { active->header.reasons |= changedScene; return; }
    if (reinterpret_cast<std::uintptr_t>(wglGetCurrentContext()) != active->header.context) { active->header.reasons |= changedContext; return; }
    if (!validRange(atlas->m_uTotalQuads, atlas->m_uCapacity, first, count) || (count && !atlas->m_pQuads)) {
        active->header.reasons |= invalidRange; return;
    }
    if (count > maxQuads - active->quads.size()) { active->header.reasons |= budget; return; }
    DrawRecord record;
    record.batch = reinterpret_cast<std::uintptr_t>(batch); record.batchId = batch->m_uID;
    record.atlas = reinterpret_cast<std::uintptr_t>(atlas); record.atlasId = atlas->m_uID;
    record.texture = reinterpret_cast<std::uintptr_t>(atlas->m_pTexture);
    record.textureId = atlas->m_pTexture ? atlas->m_pTexture->m_uID : 0;
    record.shader = reinterpret_cast<std::uintptr_t>(batch->m_pShaderProgram);
    record.scope = activeScope; record.first = first; record.count = count;
    record.total = atlas->m_uTotalQuads; record.capacity = atlas->m_uCapacity;
    record.blendSrc = batch->m_blendFunc.src; record.blendDst = batch->m_blendFunc.dst;
    record.mxcsr = preserve.saved;
    if (batch->m_pobTextureAtlas != atlas) record.reasons |= foreignAtlas;
    owners.assign(count, {});
    unsigned producerLinearOrderDifferent=0;
    auto descendants = batch->m_pobDescendants;
    if (descendants && descendants->data && descendants->data->num <= maxQuads &&
        (!descendants->data->num || descendants->data->arr)) {
        for (unsigned i = 0; i < descendants->data->num; ++i) {
            // The original batch draw itself requires CCSprite descendants.
            auto sprite = static_cast<CCSprite*>(descendants->data->arr[i]);
            if (!sprite || sprite->m_uAtlasIndex < first || sprite->m_uAtlasIndex - first >= count) continue;
            auto& owner = owners[sprite->m_uAtlasIndex - first];
            if (owner.sprite) { record.reasons |= duplicateIndex; continue; }
            if (sprite->m_pobBatchNode != batch || sprite->m_pobTextureAtlas != atlas) continue;
            producerLinearOrderDifferent+=sprite->m_uAtlasIndex!=i;
            owner.sprite = reinterpret_cast<std::uintptr_t>(sprite);
            owner.parent = reinterpret_cast<std::uintptr_t>(sprite->m_pParent);
            owner.parentId = sprite->m_pParent ? sprite->m_pParent->m_uID : 0;
            owner.id = sprite->m_uID; owner.atlasIndex = sprite->m_uAtlasIndex;
            const void* fields[]{&sprite->m_bVisible,&sprite->_realOpacity,&sprite->m_bShouldBeHidden,&sprite->m_bDontDraw};
            for(unsigned field=0;field<4;++field)++controlBytes[field][*static_cast<const volatile unsigned char*>(fields[field])];
            owner.quadMatches = std::memcmp(&sprite->m_sQuad, atlas->m_pQuads + sprite->m_uAtlasIndex, sizeof(snapshot::NativeQuadState)) == 0;
            owner.flags = nativeFlag(&sprite->m_bShouldBeHidden) | nativeFlag(&sprite->m_bDirty) << 1 |
                nativeFlag(&sprite->m_bRecursiveDirty) << 2 | nativeFlag(&sprite->m_bFlipX) << 3 |
                nativeFlag(&sprite->m_bFlipY) << 4 | nativeFlag(&sprite->m_bRectRotated) << 5 |
                nativeFlag(&sprite->m_bUseVertexMod) << 6 | nativeFlag(&sprite->m_bDontDraw) << 7 |
                nativeFlag(&sprite->m_bOpacityModifyRGB) << 8 | nativeFlag(&sprite->_realOpacity) << 9 |
                nativeFlag(&sprite->m_bVisible) << 10;
            static_assert(sizeof(sprite->m_transformToBatch) == sizeof(owner.matrix));
            std::memcpy(owner.matrix, &sprite->m_transformToBatch, sizeof(owner.matrix));
            const std::array<float,4> rectangle{sprite->m_obOffsetPosition.x, sprite->m_obOffsetPosition.y,
                sprite->m_obRect.size.width, sprite->m_obRect.size.height};
            std::memcpy(owner.localRect, rectangle.data(), sizeof(owner.localRect));
            const std::array<float,2> anchor{sprite->m_obAnchorPointInPoints.x, sprite->m_obAnchorPointInPoints.y};
            std::memcpy(owner.anchor, anchor.data(), sizeof(owner.anchor));
            owner.z = std::bit_cast<std::uint32_t>(sprite->m_fVertexZ);
            const std::array<float,4> mods{sprite->m_fTlVertexMod,sprite->m_fTrVertexMod,sprite->m_fBlVertexMod,sprite->m_fBrVertexMod};
            std::memcpy(owner.vertexMods, mods.data(), sizeof(owner.vertexMods));
            const std::array<float,4> bounds{sprite->m_fTextureLeft,sprite->m_fTextureRight,sprite->m_fTextureBottom,sprite->m_fTextureTop};
            std::memcpy(owner.textureBounds, bounds.data(), sizeof(owner.textureBounds));
            rootSource(sprite, owner);
            chlorium::research::readwatch::observation->consider(sprite,owner,count,frameCount);
        }
    } else record.reasons |= missingSprites;
    const auto bytes = count ? std::as_bytes(std::span(atlas->m_pQuads + first, count)) : std::span<const std::byte>{};
    if(active->draws.size()<maxDraws)atlasDirty[frameCount-1][active->draws.size()]=static_cast<unsigned char>(nativeFlag(&atlas->m_bDirty));
    active->append(record, bytes, owners);
    if(producerRequested() && !active->draws.empty()){
        ProducerPostState saved;
        try{producerShadow.observeRaw(frameCount,active->draws.back(),owners,bytes,producerLinearOrderDifferent);}catch(...){producerShadow.failed=true;}
    }

}
}
bool producerReplayRequested() noexcept{return producerRequested();}
bool producerReplayPackets(std::span<const SpriteRecord> oracle,std::span<const unsigned char> vertices,
                          std::vector<SpriteRecord>& sources,std::vector<unsigned char>& payload) noexcept{
    ProducerPostState saved;sources.clear();payload.clear();
    if(!producerWindow() || frameCount!=2 || producerActiveHooks!=3)return false;
    try{return producerShadow.assemble(oracle,std::as_bytes(vertices),sources,payload);}
    catch(...){sources.clear();payload.clear();producerShadow.failed=true;return false;}
}
void prepareLevel(PlayLayer* play) noexcept {
    PreserveFP preserve;
    chlorium::research::readwatch::observation->configure();
    chlorium::research::opacityinvocations::observation->configure();
    chlorium::research::opaqueinputs::census->configure();
    chlorium::research::movementcensus::census.configure(static_cast<GJBaseGameLayer*>(play));
    chlorium::research::startupdeath::observation->configure(play);
    chlorium::research::registers::runOwnedRegisterOracle();
    rootsReady = false; roots.clear();
    try {
        const auto objects = play ? play->m_objects : nullptr;
        if (!objects || !objects->data || objects->data->num > 1000000 ||
            (objects->data->num && !objects->data->arr)) return;
        roots.reserve(objects->data->num);
        for (unsigned i=0; i<objects->data->num; ++i) {
            auto object = static_cast<GameObject*>(objects->data->arr[i]);
            if (object) roots.push_back({reinterpret_cast<std::uintptr_t>(static_cast<CCNode*>(object)),object,object->m_uID});
        }
        std::sort(roots.begin(),roots.end(),[](const RootEntry& a,const RootEntry& b){return a.node<b.node;});
        rootsReady = true;
    } catch (...) { roots.clear(); }
}
void beginFrame(bool eligible, std::uint64_t frame, double percent) noexcept {
    PreserveFP preserve;
    active = nullptr;
    if (frameCount >= 2 || written || aborted || (!eligible && !attempted)) return;
    try {
        if (!eligible) { aborted = true; return; }
        if (!attempted) {
            attempted = true;
            if(producerRequested())producerThread.store(GetCurrentThreadId(),std::memory_order_release);
            if(producerRequested())toggleProducerHooks(true);
            for (auto& f : frames) f.reserve();
            owners.reserve(maxQuads);
            scene = CCDirector::get()->getRunningScene();
        }
        active = &frames[frameCount++];
        active->header.frame = frame;
        active->header.percentBits = std::bit_cast<std::uint64_t>(percent);
        active->header.context = reinterpret_cast<std::uintptr_t>(wglGetCurrentContext());
        active->header.scene = reinterpret_cast<std::uintptr_t>(scene);
        scopeOrdinal = activeScope = 0; batch = nullptr;
    } catch (...) { active = nullptr; aborted = true; successful = false; }
}
void endFrame() noexcept {
    PreserveFP preserve;
    active = nullptr; batch = nullptr;
    if(producerRequested()&&(frameCount>=2||aborted||written))toggleProducerHooks(false);
    chlorium::research::readwatch::observation->end(frameCount,aborted);
    chlorium::research::opacityinvocations::observation->end(frameCount,aborted);
    chlorium::research::startupdeath::observation->end(frameCount,aborted);
    // Persist only during finish(), after gameplay has stopped advancing.
}
bool finish() noexcept {
    PreserveFP preserve;
    active = nullptr;
    if(producerRequested())toggleProducerHooks(false);
    try {
        chlorium::research::readwatch::observation->finish(std::getenv("CHLORIUM_AUDIT_OUTPUT"));
        chlorium::research::opacityinvocations::observation->finish(std::getenv("CHLORIUM_AUDIT_OUTPUT"));
        chlorium::research::opaqueinputs::census->finish(std::getenv("CHLORIUM_AUDIT_OUTPUT"));
        chlorium::research::movementcensus::census.finish(std::getenv("CHLORIUM_AUDIT_OUTPUT"));
        chlorium::research::startupdeath::observation->finish(std::getenv("CHLORIUM_AUDIT_OUTPUT"));
        if (!written) write();
        if(!chlorium::research::opaqueinputs::census->comparisonPassed())successful=false;
    } catch (...) { successful = false; written = true; }
    return successful;
}
}

class $modify(ChloriumCaptureMoves, GJBaseGameLayer) {
    void moveObjects(CCArray* objects,double dx,double dy,bool lockPlayerY) {
        using namespace chlorium::research::pass;
        if(active)chlorium::research::movementcensus::census.before<GameObject>(frameCount,this,objects,dx,dy);
        GJBaseGameLayer::moveObjects(objects,dx,dy,lockPlayerY);
    }
};
class $modify(ChloriumCaptureBatch, CCSpriteBatchNode) {
    void draw() {
        using namespace chlorium::research::pass;
        auto previous = batch; const auto previousScope = activeScope;
        batch = active && inObjectLayer(this) ? this : nullptr;
        if (batch) activeScope = scopeOrdinal++;
        struct Restore { CCSpriteBatchNode* node; unsigned scope; ~Restore() { batch=node; activeScope=scope; } } restore{previous,previousScope};
        CCSpriteBatchNode::draw();
    }
};
class $modify(ChloriumCaptureAtlas, CCTextureAtlas) {
    void drawNumberOfQuads(unsigned count, unsigned first) {
        using namespace chlorium::research::pass;
        const auto ordinal=active?active->draws.size():maxDraws;
        try { chlorium::research::pass::observe(this, count, first); }
        catch (...) { if (chlorium::research::pass::active) chlorium::research::pass::active->header.reasons |= chlorium::research::pass::allocationFailure; }
        if(active&&batch&&ordinal<maxDraws&&active->draws.size()==ordinal+1&&!active->draws[ordinal].reasons)
            chlorium::research::opaqueinputs::census->before(this,frameCount,static_cast<unsigned>(ordinal),count,batch->m_pShaderProgram);
        CCTextureAtlas::drawNumberOfQuads(count, first);
        if(active&&batch&&ordinal<maxDraws&&active->draws.size()==ordinal+1&&!active->draws[ordinal].reasons)
            chlorium::research::opaqueinputs::census->observe(this,batch->m_pShaderProgram,frameCount,static_cast<unsigned>(ordinal),count,first,owners,[&]{CCTextureAtlas::drawNumberOfQuads(count,first);});
    }
};
class $modify(ChloriumCaptureSpriteProducers, CCSprite) {
    static void onModify(auto& self){
        const bool enabled=chlorium::research::pass::producerRequested();
        for(auto hook:self.getAllHooks()){hook->setAutoEnable(false);if(enabled)++chlorium::research::pass::producerHooks;}
    }
    void updateTransform(){CCSprite::updateTransform();chlorium::research::pass::producerSprite(this,false);}
    void updateColor(){CCSprite::updateColor();chlorium::research::pass::producerSprite(this,true);}
};
class $modify(ChloriumCaptureAtlasProducer, CCTextureAtlas) {
    static void onModify(auto& self){
        const bool enabled=chlorium::research::pass::producerRequested();
        for(auto hook:self.getAllHooks()){hook->setAutoEnable(false);if(enabled)++chlorium::research::pass::producerHooks;}
    }
    void updateQuad(ccV3F_C4B_T2F_Quad* quad,unsigned index){CCTextureAtlas::updateQuad(quad,index);chlorium::research::pass::producerAtlas(this,index);}
};

#endif
