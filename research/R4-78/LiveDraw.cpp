#include <Geode/Geode.hpp>
#include "TraceAdapter.hpp"
#include "RootRuntime.hpp"
#include "../R4-75/GameGeometry.hpp"
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumLiveRootDrawIdentity[]="CHLORIUM_LIVE_ROOT_DRAW_R78";
namespace chlorium::research::liveroot {
namespace {
Stats stats;DWORD owner=0;unsigned preparedFrame=0;bool failed=false;
liveprogram::Roster roster;std::vector<mixedprogram::State> previous,actual;std::vector<unsigned char> disabled;
std::unique_ptr<canonicaldraw::RootRuntime> roots;std::unique_ptr<canonicaldraw::CanonicalWrite> writer;
std::map<liveprogram::Key,unsigned> slots;
}
Status prepare(CCTextureAtlas* atlas,CCSpriteBatchNode* batch,std::span<const rotationtrace::Row> rows,unsigned frame)noexcept{
    canonicaldraw::HostState preserve;if(!owner)owner=GetCurrentThreadId();if(owner!=GetCurrentThreadId() || failed || !atlas || !batch || frame<1 || frame>2)return Status::native;
    try{
        if(preparedFrame!=frame){
            if(!roots){roster=liveprogram::seed(rows,frame);if(roster.keys.empty())return Status::native;previous=roster.initial;disabled.assign(roster.keys.size(),0);for(unsigned i=0;i<roster.keys.size();++i)slots.emplace(roster.keys[i],i);
                roots=std::make_unique<canonicaldraw::RootRuntime>(Mod::get()->getResourcesDir()/"shaders/r78-mixed.comp",roster.initial);
                writer=std::make_unique<canonicaldraw::CanonicalWrite>(Mod::get()->getResourcesDir()/"shaders/r78-canonical.comp");}
            const auto program=liveprogram::compile(rows,frame,roster,previous,disabled);actual=roots->advance(program);
            for(unsigned i=0;i<actual.size();++i)if(!disabled[i] && std::memcmp(actual[i].position,program.expected[i].position,16))throw std::runtime_error("live GPU/native root mismatch");
            previous=program.expected;preparedFrame=frame;++stats.frames;
        }
        auto* descendants=batch->getDescendants();if(!descendants || !descendants->data || !descendants->data->arr || descendants->data->num>1000000)return Status::native;
        std::vector<rootgeometry::Row> sources;std::vector<unsigned> poseSlots;sources.reserve(roster.keys.size());poseSlots.reserve(roster.keys.size());
        for(unsigned i=0;i<descendants->data->num;++i){auto* sprite=static_cast<CCSprite*>(descendants->data->arr[i]);if(!sprite)continue;auto found=slots.find({reinterpret_cast<std::uintptr_t>(sprite),sprite->m_uID});if(found==slots.end() || disabled[found->second] || sprite->m_pobTextureAtlas!=atlas)continue;
            auto* object=static_cast<GameObject*>(sprite);mixedprogram::State current;std::memcpy(current.position,&object->m_positionX,16);auto captured=rootgeometry::capture(object,current,frame,0);sources.push_back(captured.row);poseSlots.push_back(found->second);}
        if(sources.empty())return Status::inactive;std::vector<const rootgeometry::Row*> pointers;for(const auto& source:sources)pointers.push_back(&source);
        const auto failures=writer->failures,committed=writer->committed;
        const bool ok=writer->prepare(atlas->m_pBuffersVBO[0],roots->buffer(),{reinterpret_cast<const snapshot::NativeQuadState*>(atlas->m_pQuads),atlas->m_uTotalQuads},actual,pointers,poseSlots);
        stats.failures+=writer->failures-failures;if(!ok){++stats.declines;return Status::native;}
        canonicaldraw::check(writer->committed==committed+1,"exactly one verified live canonical preparation");stats.quads=writer->quads;stats.verifiedBytes=writer->verifiedBytes;return Status::gpu;
    }catch(...){++stats.failures;failed=true;return Status::native;}
}
void submitted()noexcept{++stats.draws;if(preparedFrame>=1 && preparedFrame<=2)stats.drawnMask|=1ull<<(preparedFrame-1);}
}
extern "C" __declspec(dllexport) bool ChloriumReadLiveRootDrawR78(chlorium::research::liveroot::Stats* out,std::size_t size){
    using namespace chlorium::research::liveroot;if(!out || size!=sizeof(*out) || GetCurrentThreadId()!=owner)return false;*out=stats;return true;
}
