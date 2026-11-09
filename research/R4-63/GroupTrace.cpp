#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "MovementTrace.hpp"
#include <charconv>
#include <cmath>
#include <atomic>
#include <unordered_map>
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumGroupTraceIdentity[]="CHLORIUM_GROUP_STATE_TRACE_R63";
namespace chlorium::research::grouptrace {
namespace {
Capture capture;
bool configured=false,enabled=false,attempted=false,written=false,active=false;
double threshold=0;std::string output;
std::atomic<DWORD> traceThread{0};std::atomic<bool> foreignThread{false};
std::unordered_map<std::uint64_t,std::uint32_t> observedIds;
struct Preserve {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~Preserve(){_mm_setcsr(fp);SetLastError(error);}};
bool reviewedNativeLayout()noexcept{
    __try{
        const auto base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(!base || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)return false;
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
           pe->FileHeader.TimeDateStamp!=0x69702da0 || pe->OptionalHeader.SizeOfImage<0x22ded0)return false;
        std::uint64_t hash=14695981039346656037ull;
        // Geode owns the entry trampoline; the reviewed remaining body pins the raw field offsets.
        for(unsigned i=0x22dd70;i<0x22ded0;++i)hash=(hash^base[i])*1099511628211ull;
        return hash==0x98f38c9054902d57ull;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static_assert(offsetof(GameObject,m_positionX)==0x3b8 && offsetof(GameObject,m_positionY)==0x3c0 &&
              offsetof(GameObject,m_lastPosition)==0x4d0 && offsetof(GameObject,m_outerSectionIndex)==0x278 &&
              offsetof(GameObject,m_isDecoration2)==0x512 && offsetof(GameObject,m_tempOffsetXRelated)==0x2c8);
static_assert(offsetof(GJBaseGameLayer,m_gameState)+offsetof(decltype(GJBaseGameLayer::m_gameState),m_commandIndex)==0x3e0);
void read(void* receiver,Object& out)noexcept{
    auto object=static_cast<GameObject*>(receiver);const auto bytes=static_cast<const unsigned char*>(receiver);
    out.address=reinterpret_cast<std::uintptr_t>(object);out.uid=object->m_uID;out.section=object->m_outerSectionIndex;
    out.state.position[0]=object->m_positionX;out.state.position[1]=object->m_positionY;
    std::memcpy(out.state.last,&object->m_lastPosition,8);std::memcpy(&out.state.stamp,bytes+0x4dc,4);
    constexpr unsigned offsets[]{0x512,0x2c8,0x368,0x369,0x351,0x352};
    for(unsigned k=0;k<6;++k)out.state.flags|=unsigned(bytes[offsets[k]]!=0)<<k;
    out.objectType=static_cast<unsigned>(object->m_objectType);out.objectId=object->m_objectID;out.groupCount=static_cast<unsigned>(object->m_groupCount);
    out.roles=unsigned(object->m_hasAreaParent!=0)|unsigned(object->m_hasGroupParent!=0)<<1|
        unsigned(object->m_isTrigger!=0)<<2|unsigned(object->m_isUIObject!=0)<<3|
        unsigned(object->m_hasParticles!=0)<<4|unsigned(object->m_usesAudioScale!=0)<<5|
        unsigned(object->m_hasAnimatedChild!=0)<<6|unsigned(object->m_colorGroupCount>0)<<7|
        unsigned(object->m_opacityGroupCount>0)<<8|unsigned(object->m_objectType==GameObjectType::Decoration)<<9;
}
bool previouslyObserved(void* receiver)noexcept{
    const auto at=observedIds.find(reinterpret_cast<std::uintptr_t>(receiver));
    return at!=observedIds.end() && at->second==static_cast<GameObject*>(receiver)->m_uID;
}
void finish(){
    if(written)return;written=true;
    if(!capture.write(output))log::warn("R63 group trace could not be saved; no ownership evidence");
}
}
void begin(PlayLayer* play,bool isolated)noexcept{
    Preserve preserve;
    try{
        if(!configured){
            configured=true;const auto flag=std::getenv("CHLORIUM_GROUP_TRACE_R63");
            const auto path=std::getenv("CHLORIUM_AUDIT_OUTPUT"),from=std::getenv("CHLORIUM_TARGET_FROM");
            if(!flag || std::string_view(flag)!="1" || !path || !*path || !from)return;
            const auto parsed=std::from_chars(from,from+std::strlen(from),threshold);
            if(parsed.ec!=std::errc{} || parsed.ptr!=from+std::strlen(from) || !std::isfinite(threshold) || threshold<0 || threshold>=100)return;
            if(!reviewedNativeLayout())return;
            output=std::string(path)+".groups-r63.bin";enabled=true;
        }
        if(!enabled || written)return;
        if(!play || !isolated){if(attempted){capture.abort();finish();}return;}
        const auto percent=play->getCurrentPercent();
        if(!std::isfinite(percent) || percent<threshold || percent>100){if(attempted){capture.abort();finish();}return;}
        if(!attempted){
            attempted=true;if(!capture.start(static_cast<GJBaseGameLayer*>(play),play->m_uID,percent)){enabled=false;return;}
            traceThread.store(GetCurrentThreadId(),std::memory_order_release);}
        active=capture.beginFrame(static_cast<GJBaseGameLayer*>(play),play->m_uID);
        if(!active){capture.abort();finish();}
    }catch(...){enabled=false;active=false;}
}
void end()noexcept{
    Preserve preserve;
    try{if(active){
        active=false;const auto play=PlayLayer::get();
        if(!play || reinterpret_cast<std::uintptr_t>(static_cast<GJBaseGameLayer*>(play))!=capture.header.owner || play->m_uID!=capture.header.ownerUid){capture.abort();finish();return;}
        // Iterate current owners, never saved native pointers from an earlier callback.
        capture.snapshot(static_cast<GJBaseGameLayer*>(play),play->m_objects,&read,&previouslyObserved);
        capture.endFrame();if(foreignThread.load())capture.header.reasons|=8;if(capture.header.frames==2)finish();
    }}
    catch(...){enabled=false;}
}
void observe(GJBaseGameLayer* layer,CCArray* array,double dx,double dy)noexcept{
    Preserve preserve;
    const auto owner=traceThread.load(std::memory_order_acquire);
    if(!owner)return;
    if(owner!=GetCurrentThreadId()){foreignThread.store(true);return;}
    try{if(active){
        const auto prior=capture.header.rows;
        capture.before(layer,array,dx,dy,&read);
        if(capture.header.rows>prior){const auto& row=capture.row(prior);
            for(unsigned i=0;i<row.observed;++i){const auto& object=capture.object(row.first+i);observedIds[object.address]=object.uid;}}
    }}catch(...){capture.abort();active=false;enabled=false;}
}
}
class $modify(ChloriumGroupTraceMoves,GJBaseGameLayer){
    void moveObjects(CCArray* objects,double dx,double dy,bool lockPlayerY){
        chlorium::research::grouptrace::observe(this,objects,dx,dy);
        GJBaseGameLayer::moveObjects(objects,dx,dy,lockPlayerY);
    }
};
