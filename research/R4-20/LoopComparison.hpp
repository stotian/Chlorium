#pragma once
#include "TransformWorld.hpp"
#include <iostream>
namespace chlorium::research::transform_shadow {
inline unsigned checks=0;
inline void require(bool b,const char* why){++checks;if(!b){std::cerr<<"FAIL "<<why<<" after "<<checks<<'\n';std::exit(1);}}
inline void equivalent(const World& a,const World& b) {
    require(a.callbacks==b.callbacks&&a.events==b.events,"native callback order, state, helper caches and FP flags");
    for(const auto w:{&a,&b})for(const auto& e:w->events)require(e.pointersValid,"callback snapshots contain only valid owned pointers");
    require(a.ownedPointers()&&b.ownedPointers(),"all owned pointers remain valid");
    require(a.state.objects.size()==b.state.objects.size(),"object cardinality");
    for(unsigned n=0;n<a.state.objects.size();++n){auto x=a.state.objects[n],y=b.state.objects[n];x.put<std::uintptr_t>(0,0);y.put<std::uintptr_t>(0,0);require(x.data==y.data,"entire object bytes");}
    auto x=a.center,y=b.center;x.put<std::uintptr_t>(0,0);y.put<std::uintptr_t>(0,0);require(x.data==y.data,"center state");
    auto l=a.state.layer,r=b.state.layer;
    for(unsigned at:{0u,0x8c8u,0x8e8u,0xe28u,0xe30u,0xe38u}){l.put<std::uintptr_t>(at,0);r.put<std::uintptr_t>(at,0);}
    require(l.data==r.data,"layer mutations");
    require(a.state.slots.size()==b.state.slots.size(),"tracking cardinality");
    for(unsigned n=0;n<a.state.slots.size();++n)require(a.identity(a.state.slots[n])>=-1&&b.identity(b.state.slots[n])>=-1&&a.identity(a.state.slots[n])==b.identity(b.state.slots[n]),"tracking order and identity");
    require(a.parent.math()==b.parent.math()&&a.child.math()==b.child.math(),"final helper fields/caches");
    require(!a.overflow&&!b.overflow,"callback coverage complete");
}
}
