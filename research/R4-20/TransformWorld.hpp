#pragma once
#include "OwnedTransformOracle.hpp"
#include "../R4-19/StampedAreaShadow.hpp"
#include "../R4-11/PoseProgram.hpp"
#include <memory>

namespace chlorium::research::transform_shadow {
namespace area=chlorium::research::area_shadow;
namespace pose=chlorium::research::pose;
struct Point {float x,y;};
template<unsigned Slot,class Return=void,class... Args> Return nodeCall(void* self,Args... args) {
    const auto table=*reinterpret_cast<void***>(self);
    return reinterpret_cast<Return(*)(void*,Args...)>(table[Slot/8])(self,args...);
}
struct Node {
    alignas(16) std::array<unsigned char,1024> data{};
    void (*destroy)(void*)=nullptr;
    explicit Node(const OwnedTransformOracle& oracle) {
        destroy=reinterpret_cast<void(*)(void*)>(oracle.cocosExport("??1CCNode@cocos2d@@UEAA@XZ"));
        const auto create=reinterpret_cast<void*(*)(void*)>(oracle.cocosExport("??0CCNode@cocos2d@@QEAA@XZ"));
        create(data.data());
    }
    Node(const Node&)=delete;Node& operator=(const Node&)=delete;
    ~Node(){if(destroy)destroy(data.data());}
    template<class T> T get(unsigned at)const {T value;std::memcpy(&value,data.data()+at,sizeof(value));return value;}
    template<class T> void put(unsigned at,const T& value){std::memcpy(data.data()+at,&value,sizeof(value));}
    std::array<unsigned char,0xbd> math() const {
        std::array<unsigned char,0xbd> out{};
        std::memcpy(out.data(),data.data()+0x38,0x84);
        std::memcpy(out.data()+0x84,data.data()+0x110,0x11);
        std::memcpy(out.data()+0x95,data.data()+0x10,0x28);
        // Arrival order is a process-global ID; compare owned topology instead.
        std::memset(out.data()+0x95+0x20,0,4);return out;
    }
};
enum class EventKind {Section,Disabled,Rotation,Scale};
struct Event {
    EventKind kind{};unsigned object=0;std::uint32_t argument=0,flags=0;
    std::array<area::Object,4> objects;area::Object center;area::Bytes<0xf10> layer;
    std::array<int,64> tracking{};std::array<unsigned char,0xbd> parent{},child{};
    bool pointersValid=false;
    bool operator==(const Event& o)const {
        return kind==o.kind&&object==o.object&&argument==o.argument&&flags==o.flags&&
            std::equal(objects.begin(),objects.end(),o.objects.begin(),[](const auto& a,const auto& b){return a.data==b.data;})&&
            center.data==o.center.data&&layer.data==o.layer.data&&tracking==o.tracking&&parent==o.parent&&child==o.child&&pointersValid==o.pointersValid;
    }
};
struct World {
    // Destruction order releases the parent's child ownership before child dtor.
    Node child,parent;
    struct Core {area::Bytes<0xf10> layer;std::vector<area::Object> objects;std::vector<void*> slots;} state;
    area::Object center;
    area::Bytes<0x40> group;
    area::Bytes<0x20> groupData;
    std::vector<void*> members;
    std::vector<Event> events;
    bool capture=true,observerWrites=false,observerClearsFlags=false,unknownObserver=false,overflow=false;
    unsigned callbacks=0;
    std::array<void*,0x600/8> objectTable{};
    std::array<void*,0x500/8> layerTable{};
    static inline thread_local World* active=nullptr;
    explicit World(const OwnedTransformOracle& oracle,unsigned count=4):child(oracle),parent(oracle) {
        if(count>8192)throw std::length_error("transform fixture budget");
        objectTable[0x4a8/8]=oracle.realPositionEntry();
        objectTable[0x520/8]=reinterpret_cast<void*>(&rotation);
        objectTable[0x508/8]=reinterpret_cast<void*>(&scale);
        layerTable[0x490/8]=reinterpret_cast<void*>(&section);
        layerTable[0x498/8]=reinterpret_cast<void*>(&disabled);
        std::vector<area::Object> seed(count);
        for(unsigned n=0;n<seed.size();++n) {
            seed[n].put(0,objectTable.data());seed[n].put(0x3a0,7);
            seed[n].put(0x3b8,100.125+31.5*n);seed[n].put(0x3c0,-12.75+3.125*n);
            seed[n].put(0x488,1.f);seed[n].put(0x48c,1.f);
            for(unsigned at:{0x4dcu,0x4e0u,0x4e4u})seed[n].put(at,-1);
        }
        state.objects=std::move(seed);state.slots.resize(count>4?count*2+64:64);members.resize(count);
        state.layer.put(0,layerTable.data());state.layer.put(0x3e0,1);
        state.layer.put(0xe28,state.slots.data());state.layer.put(0xe30,state.slots.data());state.layer.put(0xe38,state.slots.data()+state.slots.size());
        state.layer.put(0x8c8,parent.data.data());state.layer.put(0x8e8,child.data.data());
        if(!state.objects.empty())center=state.objects[0];
        else center.put(0,objectTable.data());
        center.put(0x4e0,std::numeric_limits<int>::max());
        center.put(0x3b8,1048576.25);center.put(0x3c0,65536.125);
        group.put(0x38,groupData.data.data());groupData.put<unsigned>(0,count);groupData.put(0x10,members.data());
        for(unsigned n=0;n<members.size();++n)members[n]=state.objects[n].data.data();
        events.reserve(64);
    }
    int identity(const void* p)const {
        if(!p)return -1;if(p==center.data.data())return static_cast<int>(state.objects.size());
        const auto base=reinterpret_cast<std::uintptr_t>(state.objects.data()),value=reinterpret_cast<std::uintptr_t>(p);
        if(value>=base&&value-base<state.objects.size()*sizeof(area::Object)&&(value-base)%sizeof(area::Object)==0)
            return static_cast<int>((value-base)/sizeof(area::Object));
        return -2;
    }
    bool ownedPointers(bool inspectObjects=true)const {
        const int length=state.layer.get<int>(0xef8);
        const int count=state.layer.get<int>(0xee4);
        bool valid=count>=0&&length>=count&&static_cast<unsigned>(length)<=state.slots.size()&&
            state.layer.get<void*>(0)==layerTable.data()&&state.layer.get<void*>(0x8c8)==parent.data.data()&&state.layer.get<void*>(0x8e8)==child.data.data()&&
            state.layer.get<void**>(0xe28)==state.slots.data()&&state.layer.get<void**>(0xe30)==state.slots.data()+length&&state.layer.get<void**>(0xe38)==state.slots.data()+state.slots.size();
        if(inspectObjects)for(const auto& o:state.objects)valid&=o.get<void*>(0)==objectTable.data();
        for(auto slot:state.slots)valid&=identity(slot)>=-1;
        valid&=center.get<void*>(0)==objectTable.data();
        const auto parentOfChild=child.get<void*>(0xd8);
        valid&=!parent.get<void*>(0xd8)&&(!parentOfChild||parentOfChild==parent.data.data());return valid;
    }
    void record(EventKind kind,unsigned index=0,float argument=0) {
        ++callbacks;if(!capture)return;
        if(events.size()==64){overflow=true;return;}
        if(state.objects.size()>4)std::abort();
        Event e;e.kind=kind;e.object=index;e.argument=std::bit_cast<std::uint32_t>(argument);e.flags=_mm_getcsr()&63;
        e.pointersValid=ownedPointers();
        std::copy(state.objects.begin(),state.objects.end(),e.objects.begin());e.center=center;e.layer=state.layer;e.parent=parent.math();e.child=child.math();
        for(auto& o:e.objects)o.put<std::uintptr_t>(0,0);e.center.put<std::uintptr_t>(0,0);
        for(unsigned n=0;n<64;++n)e.tracking[n]=identity(state.slots[n]);
        for(unsigned at:{0u,0x8c8u,0x8e8u,0xe28u,0xe30u,0xe38u})e.layer.put<std::uintptr_t>(at,0);
        events.push_back(e);
        if(observerWrites&&kind==EventKind::Section) {
            auto& o=state.objects[(index+1)%state.objects.size()];o.put(0x3b8,o.get<double>(0x3b8)+.125);
        }
        if(observerClearsFlags)_mm_setcsr(_mm_getcsr()&~63u);
    }
    static void section(void* layer,void* object) {
        if(!active||layer!=active->state.layer.data.data())std::abort();
        if(!active->capture){++active->callbacks;return;}
        const auto n=active->identity(object);if(n<0)std::abort();active->record(EventKind::Section,static_cast<unsigned>(n));
    }
    static void disabled(void* layer,void*) {
        if(!active||layer!=active->state.layer.data.data())std::abort();active->record(EventKind::Disabled);
    }
    static void rotation(void* object,float argument) {
        if(!active)std::abort();const auto n=active->identity(object);if(n<0)std::abort();active->record(EventKind::Rotation,static_cast<unsigned>(n),argument);
    }
    static void scale(void* object,float argument) {
        if(!active)std::abort();const auto n=active->identity(object);if(n<0)std::abort();active->record(EventKind::Scale,static_cast<unsigned>(n),argument);
    }
    void native(const OwnedTransformOracle& oracle,float sx=1,float sy=1,bool reset=true) {
        active=this;oracle.transform(state.layer.data.data(),center.data.data(),group.data.data(),sx,sy,reset);active=nullptr;
    }
};
}
