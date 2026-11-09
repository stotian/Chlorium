#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "NativeFieldWatch.hpp"
#include "TraceAdapter.hpp"
#include <fstream>
#include <cstring>
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumNativeFieldWatchIdentity[]="CHLORIUM_NATIVE_FIELD_WATCH_R69";
static_assert(offsetof(GameObject,m_positionX)==0x3b8 && offsetof(GameObject,m_positionY)==0x3c0);
namespace chlorium::research::fieldtrace {
namespace {
struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
struct Actor {std::uint64_t address=0;unsigned uid=0,objectId=0;};
struct Header {
    char magic[8]{'C','H','L','R','D','6','9',0};unsigned version=69,rowBytes=sizeof(fieldwatch::NativeFieldWatch::Row),actorBytes=sizeof(Actor),reasons=0,frames=0,rows=0,objects=0,thread=0,features=1;
    std::uint64_t game=0,cocos=0;double percent=0;
};
// Feature 1: capture begins inside the first move call; its first render frame is partial.
static_assert(sizeof(Header)==72 && sizeof(Actor)==16);
Stats stats;std::unique_ptr<fieldwatch::NativeFieldWatch> watch;
std::array<Actor,2> actors{};std::uintptr_t level=0;unsigned levelUid=0,frame=0;DWORD owner=0;
PlayLayer* current=nullptr;bool eligible=false,finished=false,attempted=false;
Header header;
void observerFault()noexcept{
    if(watch)watch->stop();stats.active=0;stats.complete=0;stats.reasons|=ObserverFault;finished=true;
}
bool enabled(){const auto flag=std::getenv("CHLORIUM_FIELD_WATCH_R69");return flag && std::string_view(flag)=="1";}
bool nativeMoveBody(){
    const auto base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)return false;
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp!=0x69702da0)return false;
    std::uint64_t hash=14695981039346656037ull;for(unsigned i=0x22dd70;i<0x22ded0;++i)hash=(hash^base[i])*1099511628211ull;
    return hash==0x98f38c9054902d57ull;
}
bool actorOwned(const Actor& actor){
    if(!current || !current->m_objects || !current->m_objects->data || !current->m_objects->data->arr)return false;
    auto* data=current->m_objects->data;
    for(unsigned i=0;i<data->num;++i){auto* object=data->arr[i];if(reinterpret_cast<std::uintptr_t>(object)==actor.address)return object && object->m_uID==actor.uid;}
    return false;
}
void finish(){
    HostState preserve;if(finished)return;finished=true;
    if(watch){if(!watch->stop())stats.reasons|=OwnerLost;if(watch->failure()){stats.failure=watch->failure();stats.reasons|=StartFailed;}if(watch->overflow())stats.reasons|=Overflow;}
    if(stats.frames!=2)stats.reasons|=IncompleteWindow;
    auto* live=PlayLayer::get();
    if(!live || reinterpret_cast<std::uintptr_t>(live)!=level || live->m_uID!=levelUid){current=nullptr;stats.reasons|=OwnerLost;}else current=live;
    for(unsigned i=0;i<stats.objects;++i)if(!actorOwned(actors[i]))stats.reasons|=OwnerLost;
    stats.active=0;stats.rows=watch?unsigned(watch->rows().size()):0;stats.complete=stats.reasons==0 && stats.objects && stats.rows;
    header.reasons=stats.reasons;header.frames=stats.frames;header.rows=stats.rows;header.objects=stats.objects;
    try{if(const auto path=std::getenv("CHLORIUM_AUDIT_OUTPUT")){
        std::ofstream file(std::string(path)+".pose-r69.bin",std::ios::binary|std::ios::out|std::ios::noreplace);
        if(file){file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(actors.data()),stats.objects*sizeof(Actor));
            if(watch && stats.rows)file.write(reinterpret_cast<const char*>(watch->rows().data()),stats.rows*sizeof(fieldwatch::NativeFieldWatch::Row));
            file.flush();if(!file){stats.complete=0;stats.reasons|=OutputFailed;}
        }else{stats.complete=0;stats.reasons|=OutputFailed;}
    }else{stats.complete=0;stats.reasons|=OutputFailed;}}catch(...){stats.complete=0;stats.reasons|=OutputFailed;}
}
void arm(cocos2d::CCArray* array){
    HostState preserve;if(!eligible || attempted || finished || !array || !array->data || !array->data->arr || array->data->num<8)return;
    const auto p=current->getCurrentPercent();const auto from=std::getenv("CHLORIUM_TARGET_FROM");if(!from || p<std::strtod(from,nullptr))return;
    auto* data=array->data;
    std::array<Actor,2> selected{};unsigned count=0;
    for(unsigned i=0;i<std::min(data->num,128u) && count<2;++i){auto* object=static_cast<GameObject*>(data->arr[i]);if(!object)break;
        if(object->m_objectType!=GameObjectType::Decoration)continue;
        const auto address=reinterpret_cast<std::uintptr_t>(object);if(count && selected[0].address==address)continue;
        Actor actor{address,object->m_uID,unsigned(object->m_objectID)};if(!actorOwned(actor))continue;selected[count++]=actor;
    }
    if(count!=2)return;actors=selected;stats.objects=count;
    attempted=true;header.percent=p;header.thread=owner;header.game=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));header.cocos=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"libcocos2d.dll"));
    std::array<std::uintptr_t,4> addresses{};for(unsigned i=0;i<stats.objects;++i){addresses[i*2]=actors[i].address+0x3b8;addresses[i*2+1]=actors[i].address+0x3c0;}
    watch=std::make_unique<fieldwatch::NativeFieldWatch>();watch->frame(frame);
    if(!nativeMoveBody()){stats.reasons|=StartFailed;stats.failure=ERROR_REVISION_MISMATCH;finish();return;}
    if(!watch->start({addresses.data(),stats.objects*2})){stats.reasons|=StartFailed;finish();return;}
    stats.active=1;
}
}
void begin(PlayLayer* play,bool closed){
    HostState preserve;if(!enabled() || finished)return;
    if(!owner)owner=GetCurrentThreadId();if(owner!=GetCurrentThreadId())return;
    current=play;eligible=closed && play;
    if(level && (!play || reinterpret_cast<std::uintptr_t>(play)!=level || play->m_uID!=levelUid)){stats.reasons|=OwnerLost;finish();return;}
    if(!level && play){level=reinterpret_cast<std::uintptr_t>(play);levelUid=play->m_uID;}
    ++frame;if(watch)watch->frame(frame);
}
void end(){
    HostState preserve;if(owner!=GetCurrentThreadId() || finished || !stats.active)return;
    if(++stats.frames>=2)finish();
}
}
extern "C" __declspec(dllexport) bool ChloriumReadNativeFieldWatchR69(chlorium::research::fieldtrace::Stats* out,std::size_t size){
    using namespace chlorium::research::fieldtrace;
    if(!out || size!=sizeof(*out) || GetCurrentThreadId()!=owner)return false;*out=stats;return true;
}
class $modify(ChloriumNativeFieldMove,GJBaseGameLayer){
    void moveObjects(cocos2d::CCArray* objects,double dx,double dy,bool lockPlayerY){
        using namespace chlorium::research::fieldtrace;
        if(eligible && GetCurrentThreadId()==owner && current && static_cast<GJBaseGameLayer*>(current)==this){try{arm(objects);}catch(...){observerFault();}}
        GJBaseGameLayer::moveObjects(objects,dx,dy,lockPlayerY);
    }
};
