#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "TraceAdapter.hpp"
#include "../../src/renderer/NearCallSlot.hpp"
#include <fstream>
#include <charconv>
#include <unordered_map>
using namespace geode::prelude;
extern "C" void R72PointGateway();
extern "C" __declspec(dllexport) const char chloriumRotationTraceIdentity[]="CHLORIUM_NATIVE_ROTATION_TRACE_R72";
#ifdef CHLORIUM_MIXED_TRACE_R74
extern "C" __declspec(dllexport) const char chloriumMixedTraceIdentity[]="CHLORIUM_MIXED_NATIVE_TRACE_R74";
#endif
namespace chlorium::research::rotationtrace {
namespace {
enum Reason:unsigned {MissingPost=2,Overflow=4,OwnerLost=8,Setup=16,Output=32,Fault=64,WrongABI=128};
struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
using Apply=chlorium::affine::Apply;
Stats stats;Header header;std::vector<Row> rows;std::array<std::pair<std::uintptr_t,unsigned>,128> actors{};
std::size_t pending=SIZE_MAX;unsigned frame=0;bool finished=false;DWORD ownerThread=0;Patch* patch=nullptr;void* slot=nullptr;Apply native=nullptr;
#ifdef CHLORIUM_MIXED_TRACE_R74
std::unordered_map<std::uintptr_t,unsigned> actorSlots;
unsigned roles(GameObject* o){return unsigned(o->m_hasAreaParent!=0)|unsigned(o->m_hasGroupParent!=0)<<1|unsigned(o->m_isTrigger!=0)<<2|unsigned(o->m_isUIObject!=0)<<3|unsigned(o->m_hasParticles!=0)<<4|unsigned(o->m_usesAudioScale!=0)<<5|unsigned(o->m_hasAnimatedChild!=0)<<6|unsigned(o->m_colorGroupCount>0)<<7|unsigned(o->m_opacityGroupCount>0)<<8;}
struct MoveScope;
MoveScope* currentMove=nullptr;
#endif
static_assert(offsetof(GameObject,m_positionX)==0x3b8 && offsetof(GameObject,m_positionY)==0x3c0 && offsetof(GameObject,m_positionXOffset)==0x2a0);
mixedprogram::State read(GameObject* object){
    mixedprogram::State state;const auto* bytes=reinterpret_cast<const unsigned char*>(object);
    std::memcpy(state.position,bytes+0x3b8,16);std::memcpy(state.last,bytes+0x4d0,8);std::memcpy(&state.stamp,bytes+0x4dc,4);std::memcpy(state.offset,bytes+0x2a0,8);
    constexpr unsigned offsets[]{0x512,0x2c8,0x368,0x369,0x351,0x352,0x350,0x513};for(unsigned k=0;k<8;++k)state.flags|=unsigned(bytes[offsets[k]]!=0)<<k;
    return state;
}
#ifdef CHLORIUM_MIXED_TRACE_R74
struct MoveScope {
    MoveScope* previous=nullptr;GJBaseGameLayer* layer=nullptr;CCObject** members=nullptr;unsigned expected=0,visited=0,stamp=0;double dx=0,dy=0;
    std::array<mixedprogram::State,128> seeds{};std::array<bool,128> seeded{},seen{};
    MoveScope(GJBaseGameLayer* owner,CCArray* array,double x,double y)noexcept:layer(owner),dx(x),dy(y){
        HostState preserve;if(!stats.active || GetCurrentThreadId()!=ownerThread || reinterpret_cast<std::uintptr_t>(owner)!=header.owner || !array || !array->data || !array->data->arr)return;
        if(array->data->num>1000000){stats.reasons|=Overflow;return;}members=array->data->arr;stamp=owner->m_gameState.m_commandIndex;
        for(;expected<array->data->num && members[expected];++expected){auto* object=static_cast<GameObject*>(members[expected]);auto at=actorSlots.find(reinterpret_cast<std::uintptr_t>(object));if(at==actorSlots.end())continue;
            const auto slot=at->second;if(object->m_uID!=actors[slot].second){stats.reasons|=OwnerLost;continue;}if(!seeded[slot]){seeds[slot]=read(object);seeded[slot]=true;}}
        previous=currentMove;currentMove=this;
    }
    ~MoveScope(){HostState preserve;if(currentMove!=this)return;if(visited!=expected)stats.reasons|=MissingPost;currentMove=previous;}
    void post(GameObject* object){
        ++header.calls;if(visited>=expected || members[visited]!=object){stats.reasons|=MissingPost;return;}++visited;
        auto at=actorSlots.find(reinterpret_cast<std::uintptr_t>(object));if(at==actorSlots.end())return;const auto slot=at->second;if(!seeded[slot])return;
        if(object->m_uID!=actors[slot].second){stats.reasons|=OwnerLost;return;}if(rows.size()>=65536){stats.reasons|=Overflow;return;}
        Row row;row.before=seeds[slot];row.after=read(object);row.command.delta[0]=dx;row.command.delta[1]=dy;row.command.control[0]=stamp;
        row.address=reinterpret_cast<std::uintptr_t>(object);row.uid=object->m_uID;row.frame=frame;row.closed=1;row.controls=_mm_getcsr();
        // Move seeds are call-entry snapshots; duplicates continue from the prior pre-callback witness.
        row.reserved=1u|unsigned(seen[slot])<<1|roles(object)<<8;rows.push_back(row);seeds[slot]=row.after;seen[slot]=true;
    }
};
#endif
bool install(){
    wchar_t executable[1024]{};if(!GetModuleFileNameW(nullptr,executable,1024) || _wcsicmp(executable,L"C:\\Users\\stoti\\Chlorium\\test-game\\GeometryDash.exe"))return false;
    const auto* game=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));const auto* cocos=reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"libcocos2d.dll"));
    auto matches=[](const unsigned char* base,unsigned stamp,unsigned size){if(!base)return false;const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)return false;
        const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);return pe->Signature==IMAGE_NT_SIGNATURE && pe->FileHeader.Machine==0x8664 && pe->FileHeader.TimeDateStamp==stamp && pe->OptionalHeader.SizeOfImage==size;};
    if(!matches(game,0x69702da0,0xa3a000) || !matches(cocos,0x69702d99,0x1c5000))return false;
    auto hash=[](const unsigned char* bytes,unsigned length){std::uint64_t h=14695981039346656037ull;for(unsigned i=0;i<length;++i)h=(h^bytes[i])*1099511628211ull;return h;};
    if(hash(game+0x22b810,0x910-32)!=0x7d1426886a014a85ull || hash(cocos+0x65a0,0xae)!=0x1e32bc1900bee292ull)return false;
#ifdef CHLORIUM_MIXED_TRACE_R74
    if(hash(game+0x22dd70,0x160)!=0x98f38c9054902d57ull)return false;
#endif
    native=reinterpret_cast<Apply>(const_cast<unsigned char*>(cocos+0x65a0));if(*reinterpret_cast<const Apply*>(game+0x5165c0)!=native)return false;
    const unsigned char expected[]{0xff,0x15,0x5b,0xa7,0x2e,0};if(std::memcmp(game+0x22be5f,expected,6))return false;
    const std::array<std::uintptr_t,1> sites{reinterpret_cast<std::uintptr_t>(game)+0x22be5f};
    slot=chlorium::matrix::allocateNearCallSlot(sites,reinterpret_cast<std::uintptr_t>(&R72PointGateway),reinterpret_cast<std::uintptr_t>(game)+0xa3a000);if(!slot)return false;
    ByteVector bytes{0xff,0x15,0,0,0,0};const auto displacement=std::int32_t(reinterpret_cast<std::uintptr_t>(slot)-(sites[0]+6));std::memcpy(bytes.data()+2,&displacement,4);
    auto result=Mod::get()->patch(const_cast<unsigned char*>(game+0x22be5f),std::move(bytes));if(!result){VirtualFree(slot,0,MEM_RELEASE);slot=nullptr;return false;}patch=result.unwrap();
    // Geode owns the patch lifetime; keep its single read-only pointer page until process exit.
    return true;
}
void finish()noexcept{
    if(finished)return;finished=true;stats.active=0;if(pending!=SIZE_MAX)stats.reasons|=MissingPost;
    if(patch && !patch->disable())stats.reasons|=Setup;
    stats.rows=unsigned(rows.size());stats.closed=unsigned(std::count_if(rows.begin(),rows.end(),[](const auto& row){return row.closed==1;}));
    if(stats.frames!=2 || !stats.rows || stats.closed!=stats.rows)stats.reasons|=MissingPost;
    header.frames=stats.frames;header.rows=stats.rows;header.actors=stats.actors;header.reasons=stats.reasons;
    try{const auto* path=std::getenv("CHLORIUM_AUDIT_OUTPUT");if(!path){stats.reasons|=Output;return;}
        std::ofstream file(std::string(path)+(traceVersion==74?".mixed-r74.bin":".rotation-r72.bin"),std::ios::binary|std::ios::out|std::ios::noreplace);
        if(!file){stats.reasons|=Output;return;}file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(rows.data()),rows.size()*sizeof(Row));file.flush();if(!file)stats.reasons|=Output;
    }catch(...){stats.reasons|=Output;}
    stats.complete=stats.reasons==0;
}
void observe(chlorium::affine::Point* out,const chlorium::affine::Point* point,const chlorium::affine::Matrix* matrix,GameObject* object,const unsigned char* stack,unsigned finishFlag,GJBaseGameLayer* layer,float angle){
    if(!stats.active || GetCurrentThreadId()!=ownerThread)return;++header.calls;
    if(!object || reinterpret_cast<std::uintptr_t>(layer)!=header.owner || layer->m_uID!=header.ownerUid){stats.reasons|=OwnerLost;return;}
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());const auto address=reinterpret_cast<std::uintptr_t>(stack);
    if(address<reinterpret_cast<std::uintptr_t>(tib->StackLimit)+8 || address>reinterpret_cast<std::uintptr_t>(tib->StackBase)-0xa0 ||
       *reinterpret_cast<const std::uintptr_t*>(stack-8)!=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x22be65 ||
       reinterpret_cast<const unsigned char*>(out)!=stack+0x68 || reinterpret_cast<const unsigned char*>(point)!=stack+0x78 || reinterpret_cast<const unsigned char*>(matrix)!=stack+0x88 || finishFlag>1){stats.reasons|=WrongABI;return;}
    if(object->m_objectType!=GameObjectType::Decoration)return;
    const auto identity=std::pair{reinterpret_cast<std::uintptr_t>(object),object->m_uID};const auto end=actors.begin()+stats.actors;
    if(std::find(actors.begin(),end,identity)==end){if(stats.actors==actors.size())return;actors[stats.actors]=identity;
#ifdef CHLORIUM_MIXED_TRACE_R74
        actorSlots.emplace(identity.first,stats.actors);
#endif
        ++stats.actors;}
    if(rows.size()>=65536){stats.reasons|=Overflow;return;}if(pending!=SIZE_MAX){stats.reasons|=MissingPost;return;}
    Row row;row.before=read(object);row.address=identity.first;row.uid=identity.second;row.frame=frame;row.localRotation=angle;row.controls=_mm_getcsr();
#ifdef CHLORIUM_MIXED_TRACE_R74
    row.reserved=roles(object)<<8;
#endif
    auto& c=row.command;std::memcpy(c.linear,matrix,16);std::memcpy(c.affineCenter,reinterpret_cast<const unsigned char*>(matrix)+16,8);
    std::memcpy(c.affineCenter+2,stack+0x58,8);std::memcpy(c.move,stack+0x60,8);c.control[0]=layer->m_gameState.m_commandIndex;c.control[1]=1;c.control[2]=finishFlag;
    rows.push_back(row);pending=rows.size()-1;
}
void post(GJBaseGameLayer* layer,GameObject* object)noexcept{
    if(!stats.active || GetCurrentThreadId()!=ownerThread)return;
#ifdef CHLORIUM_MIXED_TRACE_R74
    if(pending==SIZE_MAX){if(currentMove)currentMove->post(object);return;}
#else
    if(pending==SIZE_MAX)return;
#endif
    auto& row=rows[pending];
    if(reinterpret_cast<std::uintptr_t>(layer)!=header.owner || !object || reinterpret_cast<std::uintptr_t>(object)!=row.address || object->m_uID!=row.uid){stats.reasons|=OwnerLost;return;}
    row.after=read(object);row.closed=1;pending=SIZE_MAX;
}
}
void begin(PlayLayer* play,bool isolated)noexcept{
    HostState preserve;try{if(finished || !play || !isolated)return;const auto* flag=std::getenv("CHLORIUM_ROTATION_TRACE_R72");if(!flag || std::string_view(flag)!="1")return;
        if(stats.active){if(GetCurrentThreadId()!=ownerThread || reinterpret_cast<std::uintptr_t>(static_cast<GJBaseGameLayer*>(play))!=header.owner || play->m_uID!=header.ownerUid){stats.reasons|=OwnerLost;finish();return;}++frame;return;}
        const auto* from=std::getenv("CHLORIUM_TARGET_FROM");if(!from)return;double threshold=0;const auto parsed=std::from_chars(from,from+std::strlen(from),threshold);
        if(parsed.ec!=std::errc{} || parsed.ptr!=from+std::strlen(from) || !std::isfinite(threshold) || threshold<0 || threshold>=100 || play->getCurrentPercent()<threshold)return;
        ownerThread=GetCurrentThreadId();header.thread=ownerThread;header.owner=reinterpret_cast<std::uintptr_t>(static_cast<GJBaseGameLayer*>(play));header.ownerUid=play->m_uID;header.percent=play->getCurrentPercent();rows.reserve(65536);
#ifdef CHLORIUM_MIXED_TRACE_R74
        actorSlots.reserve(128);
#endif
        if(!install()){stats.reasons|=Setup;finish();return;}stats.active=1;++frame;
    }catch(...){stats.reasons|=Fault;finish();}
}
void end()noexcept{
    HostState preserve;if(!stats.active || GetCurrentThreadId()!=ownerThread)return;auto* play=PlayLayer::get();
    if(!play || reinterpret_cast<std::uintptr_t>(static_cast<GJBaseGameLayer*>(play))!=header.owner || play->m_uID!=header.ownerUid)stats.reasons|=OwnerLost;
    if(++stats.frames>=2 || stats.reasons)finish();
}
}
extern "C" chlorium::affine::Point* R72Observe(chlorium::affine::Point* out,const chlorium::affine::Point* point,const chlorium::affine::Matrix* matrix,GameObject* object,const unsigned char* frame,unsigned finish,GJBaseGameLayer* layer,float angle){
    using namespace chlorium::research::rotationtrace;
    auto* result=native(out,point,matrix);HostState preserve;try{observe(out,point,matrix,object,frame,finish,layer,angle);}catch(...){stats.reasons|=Fault;}return result;
}
extern "C" __declspec(dllexport) bool ChloriumReadRotationTraceR72(chlorium::research::rotationtrace::Stats* out,std::size_t size){
    using namespace chlorium::research::rotationtrace;if(!out || size!=sizeof(*out) || GetCurrentThreadId()!=ownerThread)return false;*out=stats;return true;
}
class $modify(ChloriumRotationTraceSections,GJBaseGameLayer){
#ifdef CHLORIUM_MIXED_TRACE_R74
    void moveObjects(CCArray* objects,double dx,double dy,bool lockPlayerY){
        using namespace chlorium::research::rotationtrace;
        bool capture=false;{HostState preserve;capture=stats.active && GetCurrentThreadId()==ownerThread;}
        if(capture){MoveScope scope(this,objects,dx,dy);GJBaseGameLayer::moveObjects(objects,dx,dy,lockPlayerY);return;}
        GJBaseGameLayer::moveObjects(objects,dx,dy,lockPlayerY);
    }
#endif
    void updateObjectSection(GameObject* object){
        {chlorium::research::rotationtrace::HostState preserve;chlorium::research::rotationtrace::post(this,object);}
        GJBaseGameLayer::updateObjectSection(object);
    }
};
