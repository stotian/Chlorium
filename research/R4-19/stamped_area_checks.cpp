#include "OwnedAreaOracle.hpp"
#include "StampedAreaShadow.hpp"
#include "../R4-18/ExactPhaseTranslation.hpp"
#include <iostream>
#include <memory>
#include <limits>
#include <cmath>
using namespace chlorium::research::area_shadow;
unsigned checks=0,nativeCalls=0,sequences=0,callbackEvents=0;
void require(bool condition,const char* why) {
    ++checks;if(!condition){std::cerr<<"FAIL "<<why<<" after "<<checks<<" checks\n";std::exit(1);}
}
std::uint64_t rng=0xe2f735d4ab1e093full;
std::uint64_t randomBits(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
std::array<Object,World::objectCount> seeds() {
    std::array<Object,World::objectCount> objects;
    for(unsigned n=0;n<objects.size();++n) {
        auto& o=objects[n];
        for(unsigned at=0x530;at<0x600;++at)o.data[at]=static_cast<unsigned char>(at+n);
        o.put(0x3b8,32767.5+static_cast<double>(n)*7.125);
        o.put(0x3c0,-100.25+static_cast<double>(n)*3.5);
        for(unsigned at:{0x4dcu,0x4e0u,0x4e4u})o.put(at,-1);
        o.put(0x488,1.f);o.put(0x48c,1.f);
    }
    return objects;
}
void nativeMove(const OwnedAreaOracle& oracle,World& w,const Move& m) {
    // This fixture never enters GD's allocator. The original grow call points
    // to a terminating sentinel, so unsupported growth cannot pass silently.
    require(w.layer.get<int>(0xee4)<static_cast<int>(World::capacity),"native fixture has a tracking slot");
    w.layer.put(0x3e0,m.stamp);activeWorld=&w;
    oracle.move(w.layer.data.data(),w.objects[m.object].data.data(),m.dx,m.dy);
    activeWorld=nullptr;++nativeCalls;
}
int slotId(const World& w,unsigned slot) {
    if(w.slots[slot]==nullptr)return -1;
    for(unsigned n=0;n<World::objectCount;++n)if(w.slots[slot]==w.objects[n].data.data())return static_cast<int>(n);
    return -2;
}
void equivalent(const World& native,const World& shadow) {
    for(unsigned n=0;n<World::objectCount;++n)
        require(native.objects[n].data==shadow.objects[n].data,"entire object bytes match owned native producer");
    auto a=native.layer,b=shadow.layer;
    // Only owned addresses differ; compare the rest of the layer byte-for-byte.
    a.put<std::uintptr_t>(0,0);b.put<std::uintptr_t>(0,0);
    for(unsigned at:{0xe28u,0xe30u,0xe38u}) {a.put<std::uintptr_t>(at,0);b.put<std::uintptr_t>(at,0);}
    require(a.data==b.data,"layer writes and command tracking match");
    const auto count=native.layer.get<int>(0xee4),length=native.layer.get<int>(0xef8);
    require(count>=0&&length>=count&&length<=static_cast<int>(World::capacity),"native tracking counts valid");
    for(auto w:{&native,&shadow}) {
        require(w->layer.get<void**>(0xe28)==w->slots.data(),"tracking begin retained");
        require(w->layer.get<void**>(0xe30)==w->slots.data()+length,"tracking end matches length");
        require(w->layer.get<void**>(0xe38)==w->slots.data()+World::capacity,"tracking capacity retained");
    }
    for(unsigned n=0;n<World::capacity;++n)
        require(slotId(native,n)>=-1&&slotId(native,n)==slotId(shadow,n),"tracking order, tail and object identity match");
    require(!native.overflow&&!shadow.overflow,"no observer log overflow");
    require(native.events==shadow.events,"every callback and its visible state match in order");
}
void compare(const OwnedAreaOracle& oracle,const std::array<Object,World::objectCount>& seed,
             std::span<const Move> commands,unsigned mode,bool writes=false,unsigned reuse=0) {
    auto native=std::make_unique<World>(),shadow=std::make_unique<World>();
    native->initialize(seed,sectionVtable().data(),reuse);shadow->initialize(seed,sectionVtable().data(),reuse);
    native->observerWrites=shadow->observerWrites=writes;
    Program program(commands);
    _mm_setcsr(mode);for(const auto& m:commands)nativeMove(oracle,*native,m);
    const auto flags=_mm_getcsr()&63;
    _mm_setcsr(mode);program.replay(*shadow);const auto shadowFlags=_mm_getcsr()&63;
    equivalent(*native,*shadow);
    require(flags==shadowFlags,"masked floating flags match native reset/move/callback sequence");
    ++sequences;callbackEvents+=static_cast<unsigned>(native->events.size());
}
int main() {
    const auto saved=_mm_getcsr();_mm_setcsr(0x1f80);
    OwnedAreaOracle oracle;auto seed=seeds();
    const std::array<Move,4> additive{{{1,0,2.f,0},{1,0,4.f,0},{1,1,1.f,0},{1,0,-1.f,0}}};
    compare(oracle,seed,additive,0x1f80,true);
    auto native=std::make_unique<World>();native->initialize(seed,sectionVtable().data());
    nativeMove(oracle,*native,additive[0]);nativeMove(oracle,*native,additive[1]);
    require(native->objects[0].get<double>(0x3b8)==seed[0].get<double>(0x3b8)+6.,"same-stamp native moves accumulate");
    require(native->objects[0].get<float>(0x2a0)==6.f,"float offset accumulates rather than last-writer replacement");
    require(native->layer.get<int>(0xee4)==1&&native->events.size()==2,"one reset registration but two visible section callbacks");
    const std::array<float,2> r418{2.f,4.f};
    const auto replacement=chlorium::research::phase::Program(r418).evaluate({seed[0].get<double>(0x3b8),0});
    require(replacement.state.position!=native->objects[0].get<double>(0x3b8),"R4-18 replacement domain cannot represent same-command native moves");
    auto immutableInput=additive;Program immutable(immutableInput);immutableInput[0].dx=100.f;
    auto immutableWorld=std::make_unique<World>();immutableWorld->initialize(seed,sectionVtable().data());
    immutable.replay(*immutableWorld);require(immutableWorld->objects[0].get<double>(0x3b8)==seed[0].get<double>(0x3b8)+5.,"shadow schedule owns immutable commands");
    bool rejected=false;try{std::vector<Move> oversized(65);Program p(oversized);}catch(const std::length_error&){rejected=true;}
    require(rejected,"oversized schedules rejected before allocation");
    rejected=false;try{const std::array<Move,1> invalid{{{1,4,0,0}}};Program p(invalid);}catch(const std::out_of_range&){rejected=true;}
    require(rejected,"unknown objects rejected before execution");
    for(float invalid:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),2097152.f}) {
        rejected=false;try{const std::array<Move,1> bad{{{1,0,invalid,0}}};Program p(bad);}catch(const std::domain_error&){rejected=true;}
        require(rejected,"unsupported operand cannot receive a shadow-domain proof");
    }
    {
        auto invalidWorld=std::make_unique<World>();invalidWorld->initialize(seed,sectionVtable().data());
        invalidWorld->objects[0].put(0x3b8,std::numeric_limits<double>::infinity());
        const auto before=invalidWorld->objects;
        rejected=false;try{Program(additive).replay(*invalidWorld);}catch(const std::domain_error&){rejected=true;}
        require(rejected&&invalidWorld->events.empty()&&invalidWorld->objects[0].data==before[0].data,
                "unsupported state rejected before publication");
        invalidWorld->initialize(seed,sectionVtable().data());invalidWorld->layer.put(0xee4,63);invalidWorld->layer.put(0xef8,63);
        rejected=false;try{Program(additive).replay(*invalidWorld);}catch(const std::length_error&){rejected=true;}
        require(rejected&&invalidWorld->events.empty(),"tracking capacity rejected before publication");
    }
    const std::array<float,12> values{0.f,-0.f,.125f,-.375f,1.1f,-17.25f,1024.125f,0x1p-126f,-0x1p-126f,0x1p-149f,-0x1p-149f,65536.125f};
    for(unsigned control=0;control<16;++control) {
        const auto mode=0x1f80u|((control&3)<<13)|((control&4)?0x40:0)|((control&8)?0x8000:0);
        for(unsigned trial=0;trial<128;++trial) {
            auto input=seeds();
            for(auto& o:input) {
                for(unsigned at:{0x2a0u,0x2a4u,0x2a8u,0x2acu,0x2b0u,0x2b4u,0x2b8u,0x2bcu,0x2c0u,0x2c4u})o.put(at,values[randomBits()%values.size()]);
                o.put<unsigned char>(0x512,static_cast<unsigned char>(randomBits()%2));
                o.put<unsigned char>(0x2c8,static_cast<unsigned char>(randomBits()%2));
                o.put(0x3b8,trial%2?std::nextafter(32768.,0.):32768.125);
                o.put(0x4e0,trial%4==0?std::numeric_limits<int>::max():-1);
            }
            std::array<Move,16> commands;
            for(unsigned n=0;n<commands.size();++n)commands[n]={static_cast<int>(n/4),static_cast<unsigned>(randomBits()%4),values[randomBits()%values.size()],values[randomBits()%values.size()]};
            const unsigned reuse=trial%3==0?24:trial%3==1?5:0;
            compare(oracle,input,commands,mode,trial%2!=0,reuse);
            const auto prefix=randomBits()%(commands.size()+1);
            compare(oracle,input,std::span(commands).first(prefix),mode,trial%2!=0,reuse);
        }
        auto special=seeds();special[0].put(0x4e0,std::numeric_limits<int>::min());
        const std::array<Move,8> edge{{{std::numeric_limits<int>::min(),0,0,0},{-1,0,0,0},{-1,0,-0.f,0},
            {0,0,2,0},{0,0,0,0},{1,0,0,0},{std::numeric_limits<int>::max(),0,0,0},{std::numeric_limits<int>::max(),0,1,0}}};
        compare(oracle,special,edge,mode);
        special=seeds();special[0].put<unsigned char>(0x2c8,1);
        compare(oracle,special,additive,mode);
        const std::array<Move,5> backwards{{{std::numeric_limits<int>::max(),0,1,1},
            {std::numeric_limits<int>::min(),0,2,-1},{-1,0,1,0},{0,0,0,0},{1,0,-1,0}}};
        compare(oracle,seeds(),backwards,mode,true,5);
    }
    _mm_setcsr(saved);
    std::cout<<"PASS "<<checks<<" owned native Area shadow checks; 3 complete native bodies; 16 SSE controls\n";
    std::cout<<"Command stamps, object bytes, tracking identity/order, callback views and masked FP flags compared.\n";
    std::cout<<"native_move_calls="<<nativeCalls<<" sequences="<<sequences<<" callback_events="<<callbackEvents<<'\n';
    std::cout<<"No native work suppressed; no GD launch; no FPS claim.\n";
}
