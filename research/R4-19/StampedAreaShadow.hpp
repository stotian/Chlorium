#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <vector>
#include <immintrin.h>

namespace chlorium::research::area_shadow {
template<std::size_t N> struct Bytes {
    alignas(16) std::array<unsigned char,N> data{};
    template<class T> T get(std::size_t at) const { T v;std::memcpy(&v,data.data()+at,sizeof(v));return v; }
    template<class T> void put(std::size_t at,T v) {std::memcpy(data.data()+at,&v,sizeof(v));}
};
using Object=Bytes<0x600>;
struct Event {
    unsigned object=0; int count=0,length=0;
    std::array<Object,4> objects;
    Bytes<0xf10> layer;
    std::array<int,64> tracking{};
    bool pointersValid=false;
    bool operator==(const Event& o) const {
        return object==o.object&&count==o.count&&length==o.length&&
               layer.data==o.layer.data&&tracking==o.tracking&&pointersValid==o.pointersValid&&
               std::equal(objects.begin(),objects.end(),o.objects.begin(),[](const auto& a,const auto& b){return a.data==b.data;});
    }
};
struct World {
    static constexpr unsigned objectCount=4,capacity=64;
    Bytes<0xf10> layer;
    std::array<Object,objectCount> objects;
    std::array<void*,capacity> slots{};
    std::vector<Event> events;
    bool observerWrites=false,overflow=false;
    void initialize(const std::array<Object,objectCount>& seed,void* vtable,unsigned reusedLength=0) {
        if(reusedLength>capacity) throw std::length_error("shadow tracking seed");
        layer={};objects=seed;slots.fill(nullptr);events.clear();events.reserve(capacity);
        overflow=false;observerWrites=false;
        for(unsigned n=0;n<reusedLength;++n)slots[n]=objects[n%objectCount].data.data();
        layer.put(0,vtable);
        layer.put(0xef8,static_cast<int>(reusedLength));
        layer.put(0xe28,slots.data());layer.put(0xe30,slots.data()+reusedLength);layer.put(0xe38,slots.data()+capacity);
    }
    void observe(unsigned index) {
        if(events.size()==capacity) {overflow=true;return;}
        Event e; e.object=index;e.count=layer.get<int>(0xee4);e.length=layer.get<int>(0xef8);
        e.objects=objects;e.layer=layer;
        e.pointersValid=e.length>=0&&e.length<=static_cast<int>(capacity)&&
            layer.get<void**>(0xe28)==slots.data()&&layer.get<void**>(0xe30)==slots.data()+e.length&&
            layer.get<void**>(0xe38)==slots.data()+capacity;
        for(unsigned n=0;n<capacity;++n) {
            e.tracking[n]=slots[n]?-2:-1;
            for(unsigned k=0;k<objectCount;++k) if(slots[n]==objects[k].data.data())e.tracking[n]=static_cast<int>(k);
        }
        for(unsigned at:{0u,0xe28u,0xe30u,0xe38u})e.layer.put<std::uintptr_t>(at,0);
        events.push_back(e);
        if(observerWrites) {
            auto& other=objects[(index+1)%objectCount];
            other.put(0x3b8,other.get<double>(0x3b8)+.25);
        }
    }
};
inline thread_local World* activeWorld=nullptr;
inline void sectionObserver(void* layer,void* object) {
    auto w=activeWorld;
    if(!w||layer!=w->layer.data.data()) std::terminate();
    for(unsigned n=0;n<World::objectCount;++n)
        if(object==w->objects[n].data.data()) {w->observe(n);return;}
    std::terminate();
}
inline auto& sectionVtable() {
    static std::array<void*,0x500/8> table{};
    table[0x490/8]=reinterpret_cast<void*>(&sectionObserver);return table;
}
inline float narrow(double v) {return _mm_cvtss_f32(_mm_cvtpd_ps(_mm_set_sd(v)));}
inline float add(float a,float b) {return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a),_mm_set_ss(b)));}
inline float sub(float a,float b) {return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a),_mm_set_ss(b)));}
inline double wide(float v) {return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(),_mm_set_ss(v)));}
inline double add(double a,double b) {return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a),_mm_set_sd(b)));}
inline double sub(double a,double b) {return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a),_mm_set_sd(b)));}
inline void lastValues(World& w,Object& o) {
    if(o.get<unsigned char>(0x512)) return;
    const auto command=w.layer.get<int>(0x3e0);
    if(o.get<int>(0x4dc)!=command) {
        o.put(0x4d0,narrow(o.get<double>(0x3b8)));o.put(0x4d4,narrow(o.get<double>(0x3c0)));
        o.put(0x4dc,command);o.put<std::uint16_t>(0x368,0x101);
    }
    if(o.get<int>(0x4e4)!=command) {
        o.put(0x3e4,o.get<std::uint32_t>(0x488));o.put(0x3e8,o.get<std::uint32_t>(0x48c));
        o.put(0x4e4,command);o.put<std::uint16_t>(0x368,0x101);
    }
}
// Only the update=true path used by moveAreaObject. No allocator, rotation or
// scale callback replacement is claimed for update=false or vector growth.
inline bool reset(World& w,Object& o) {
    const auto command=w.layer.get<int>(0x3e0);
    if(o.get<int>(0x4e0)>=command) return false;
    lastValues(w,o); bool moved=false;
    for(auto pair:std::array<std::array<unsigned,2>,2>{{{0x2a0,0x3b8},{0x2a4,0x3c0}}}) {
        const auto offset=o.get<float>(pair[0]);
        if(offset!=0.f) {o.put(pair[1],sub(o.get<double>(pair[1]),wide(offset)));o.put(pair[0],0.f);o.put<unsigned char>(0x352,1);moved=true;}
    }
    const auto sx=o.get<float>(0x2c0),sy=o.get<float>(0x2c4);
    if(sx!=0.f||sy!=0.f) {
        o.put(0x2b8,sub(o.get<float>(0x2b8),sx));o.put(0x488,sub(o.get<float>(0x488),sx));
        o.put(0x2bc,sub(o.get<float>(0x2bc),sy));o.put(0x48c,sub(o.get<float>(0x48c),sy));
        o.put(0x2c0,0.f);o.put(0x2c4,0.f);
    }
    const auto rx=o.get<float>(0x2ac),ry=o.get<float>(0x2b4);
    if(rx!=0.f||ry!=0.f) {
        o.put(0x2a8,sub(o.get<float>(0x2a8),rx));o.put(0x2ac,0.f);
        o.put(0x2b0,sub(o.get<float>(0x2b0),ry));o.put(0x2b4,0.f);
    }
    o.put(0x4e0,command);
    const auto count=w.layer.get<int>(0xee4),length=w.layer.get<int>(0xef8);
    if(count<0||count>=static_cast<int>(World::capacity)||length<count) throw std::length_error("shadow tracking capacity");
    w.slots[count]=o.data.data();w.layer.put(0xee4,count+1);
    if(count==length) {w.layer.put(0xef8,length+1);w.layer.put(0xe30,w.slots.data()+length+1);}
    return moved;
}
struct Move {int stamp; unsigned object;float dx,dy;};
inline void publish(World& w,unsigned object) {
#if defined(CHLORIUM_AREA_CALLBACK_SLOT_MUTANT)
    const auto saved=w.slots[0];w.slots[0]=nullptr;
    w.observe(object);w.slots[0]=saved;
#else
    w.observe(object);
#endif
}
inline void apply(World& w,const Move& m) {
    w.layer.put(0x3e0,m.stamp);auto& o=w.objects[m.object];
    const auto moved=reset(w,o);
    if(m.dx==0.f&&m.dy==0.f) {if(moved)publish(w,m.object);return;}
    if(m.dy!=0.f) {
        o.put(0x2a4,add(m.dy,o.get<float>(0x2a4)));o.put(0x3c0,add(wide(m.dy),o.get<double>(0x3c0)));
    }
    o.put<std::uint16_t>(0x351,0x101);o.put<std::uint16_t>(0x368,0x101);
    if(m.dx!=0.f&&!o.get<unsigned char>(0x2c8)) {
        o.put(0x2a0,add(m.dx,o.get<float>(0x2a0)));o.put(0x3b8,add(wide(m.dx),o.get<double>(0x3b8)));
    }
    publish(w,m.object);
}
// A shadow schedule owns its inputs and retains every native publication point.
// It is deliberately not a certificate to suppress GD calls or external hooks.
class Program {
    std::vector<Move> moves_;
public:
    explicit Program(std::span<const Move> moves) {
        if(moves.size()>World::capacity) throw std::length_error("shadow command budget");
        for(const auto& m:moves) {
            if(m.object>=World::objectCount) throw std::out_of_range("shadow object index");
            for(float f:{m.dx,m.dy}) if((std::bit_cast<std::uint32_t>(f)&0x7fffffffu)>0x49800000u)
                throw std::domain_error("shadow operand outside finite bounded domain");
        }
        moves_.assign(moves.begin(),moves.end());
    }
    void replay(World& world) const {
        const auto count=world.layer.get<int>(0xee4),length=world.layer.get<int>(0xef8);
        if(count<0||length<count||length>static_cast<int>(World::capacity)||
           moves_.size()>World::capacity-static_cast<unsigned>(count))
            throw std::length_error("shadow tracking budget before publication");
        for(const auto& o:world.objects) {
            for(unsigned at:{0x3b8u,0x3c0u})
                if((o.get<std::uint64_t>(at)&0x7fffffffffffffffull)>0x4270000000000000ull)
                    throw std::domain_error("shadow position outside finite bounded domain");
            for(unsigned at:{0x2a0u,0x2a4u,0x2a8u,0x2acu,0x2b0u,0x2b4u,0x2b8u,0x2bcu,0x2c0u,0x2c4u,0x488u,0x48cu})
                if((o.get<std::uint32_t>(at)&0x7fffffffu)>0x49800000u)
                    throw std::domain_error("shadow state outside finite bounded domain");
        }
#if defined(CHLORIUM_AREA_OBJECT_MAJOR_MUTANT)
        for(unsigned n=0;n<World::objectCount;++n) for(const auto& m:moves_) if(m.object==n)apply(world,m);
#else
        for(std::size_t n=0;n<moves_.size();++n) {
#if defined(CHLORIUM_AREA_LAST_WRITER_MUTANT)
            bool later=false;
            for(auto k=n+1;k<moves_.size();++k) later|=moves_[k].object==moves_[n].object&&moves_[k].stamp==moves_[n].stamp;
            if(later)continue;
#endif
            apply(world,moves_[n]);
        }
#endif
    }
};
}
