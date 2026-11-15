#include <Geode/Geode.hpp>
#include "TraceAdapter.hpp"
#include "RootRuntime.hpp"
#include "../R4-75/GameGeometry.hpp"
#include <algorithm>
#include <cstring>
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumLiveRootDrawIdentity[]="CHLORIUM_LIVE_ROOT_DRAW_R78";
namespace chlorium::research::liveroot {
namespace {
Stats stats;DWORD owner=0;unsigned preparedFrame=0;bool failed=false;
liveprogram::Roster roster;std::vector<mixedprogram::State> previous,actual;std::vector<unsigned char> disabled;
std::unique_ptr<canonicaldraw::RootRuntime> roots;std::unique_ptr<canonicaldraw::CanonicalWrite> writer;
std::map<liveprogram::Key,unsigned> slots;
void copyText(char* destination,std::size_t capacity,const char* source)noexcept{
    if(!capacity)return;const auto count=std::min(capacity-1,std::strlen(source));std::memcpy(destination,source,count);destination[count]=0;
}
}
Status prepare(CCTextureAtlas* atlas,CCSpriteBatchNode* batch,std::span<const rotationtrace::Row> rows,unsigned frame)noexcept{
    canonicaldraw::HostState preserve;if(!owner)owner=GetCurrentThreadId();if(owner!=GetCurrentThreadId() || failed || !atlas || !batch || frame<1 || frame>2)return Status::native;const char* stage="root-seed";
    try{
        if(preparedFrame!=frame){
            if(!roots){roster=liveprogram::seed(rows,frame);if(roster.keys.empty())return Status::native;previous=roster.initial;disabled.assign(roster.keys.size(),0);for(unsigned i=0;i<roster.keys.size();++i)slots.emplace(roster.keys[i],i);
                stage="root-runtime-init";
                roots=std::make_unique<canonicaldraw::RootRuntime>(Mod::get()->getResourcesDir()/"r78-mixed.comp",roster.initial);
                stage="canonical-writer-init";
                writer=std::make_unique<canonicaldraw::CanonicalWrite>(Mod::get()->getResourcesDir()/"r78-canonical.comp");}
            stage="frame-compile";
            const auto program=liveprogram::compile(rows,frame,roster,previous,disabled);stage="root-advance";actual=roots->advance(program);
            stage="root-verify";
            for(unsigned i=0;i<actual.size();++i)if(!disabled[i] && std::memcmp(actual[i].position,program.expected[i].position,16))throw std::runtime_error("live GPU/native root mismatch");
            previous=program.expected;preparedFrame=frame;++stats.frames;
        }
        stage="descendant-scan";
        auto* descendants=batch->getDescendants();if(!descendants || !descendants->data || !descendants->data->arr || descendants->data->num>1000000)return Status::native;
        std::vector<rootgeometry::Row> sources;std::vector<unsigned> poseSlots;sources.reserve(roster.keys.size());poseSlots.reserve(roster.keys.size());
        stage="geometry-capture";
        for(unsigned i=0;i<descendants->data->num;++i){auto* sprite=static_cast<CCSprite*>(descendants->data->arr[i]);if(!sprite)continue;auto found=slots.find({reinterpret_cast<std::uintptr_t>(sprite),sprite->m_uID});if(found==slots.end() || disabled[found->second] || sprite->m_pobTextureAtlas!=atlas)continue;
            auto* object=static_cast<GameObject*>(sprite);mixedprogram::State current;std::memcpy(current.position,&object->m_positionX,16);auto captured=rootgeometry::capture(object,current,frame,0);sources.push_back(captured.row);poseSlots.push_back(found->second);}
        if(sources.empty())return Status::inactive;std::vector<const rootgeometry::Row*> pointers;for(const auto& source:sources)pointers.push_back(&source);
        stage="canonical-write";
        const auto failures=writer->failures,committed=writer->committed;
        const bool ok=writer->prepare(atlas->m_pBuffersVBO[0],roots->buffer(),{reinterpret_cast<const snapshot::NativeQuadState*>(atlas->m_pQuads),atlas->m_uTotalQuads},actual,pointers,poseSlots);
        stats.failures+=writer->failures-failures;if(!ok){++stats.declines;return Status::native;}
        stage="commit-check";
        canonicaldraw::check(writer->committed==committed+1,"exactly one verified live canonical preparation");stats.quads=writer->quads;stats.verifiedBytes=writer->verifiedBytes;return Status::gpu;
    }catch(const std::exception& error){++stats.failures;copyText(stats.failureStage,sizeof(stats.failureStage),stage);copyText(stats.failureMessage,sizeof(stats.failureMessage),error.what());failed=true;return Status::native;}
    catch(...){++stats.failures;copyText(stats.failureStage,sizeof(stats.failureStage),stage);copyText(stats.failureMessage,sizeof(stats.failureMessage),"unknown exception");failed=true;return Status::native;}
}
void submitted()noexcept{++stats.draws;if(preparedFrame>=1 && preparedFrame<=2)stats.drawnMask|=1ull<<(preparedFrame-1);}
}
extern "C" __declspec(dllexport) bool ChloriumReadLiveRootDrawR78(chlorium::research::liveroot::Stats* out,std::size_t size){
    using namespace chlorium::research::liveroot;if(!out || size!=sizeof(*out) || GetCurrentThreadId()!=owner)return false;*out=stats;return true;
}
