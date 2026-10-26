#pragma once
#include "TransformWorld.hpp"
#include <cerrno>
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
#include <chrono>
#endif

namespace chlorium::research::transform_shadow {
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
struct StageAudit {std::int64_t validation=0,compilation=0,preparation=0,objects=0,publication=0;unsigned phases=0;};
inline StageAudit stageAudit;
using AuditClock=std::chrono::steady_clock;
#endif
inline bool finitePosition(const area::Object& o) {
    for(unsigned at:{0x3b8u,0x3c0u})
        if((o.get<std::uint64_t>(at)&0x7fffffffffffffffull)>0x4270000000000000ull)return false;
    return true;
}
inline bool identityObject(const area::Object& o) {
    for(unsigned at:{0x3d4u,0x3d8u,0x2a8u,0x2acu,0x2b0u,0x2b4u,0x2b8u,0x2bcu,0x2c0u,0x2c4u})
        if(o.get<std::uint32_t>(at)!=0)return false;
    for(unsigned at:{0x488u,0x48cu})if(o.get<std::uint32_t>(at)!=0x3f800000u)return false;
    for(unsigned at:{0x2a0u,0x2a4u})if((o.get<std::uint32_t>(at)&0x7fffffffu)>0x49800000u)return false;
    return finitePosition(o)&&o.get<int>(0x3a0)==7;
}
inline pose::Matrix materializeIdentityHelper(Node& child,Point p) {
    const auto minusZero=std::bit_cast<float>(0x80000000u);
    pose::Matrix local{1,0,minusZero,1,p.x,p.y};
    child.put(0x38,0.f);child.put(0x3c,0.f);child.put(0x40,1.f);child.put(0x44,1.f);
    child.put(0x4c,p.x);child.put(0x50,p.y);child.put(0x114,p.x);child.put(0x118,p.y);
    child.put(0x8c,local);child.data[0x111]=child.data[0x112]=0;child.data[0x11c]=1;
    return local;
}
// Closed-fixture specialization. Object readers, resets, moves and publication
// order remain native. Private identity helpers are materialized directly and
// constant linear decomposition is compiled once. No GD hook uses this code.
inline bool identityLinearPhase(World& w,const OwnedTransformOracle& oracle,float sx=1,float sy=1,bool resetVisual=true) {
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit0=AuditClock::now();
#endif
    const auto control=_mm_getcsr();
    if((control&0x7f80u)!=0x1f80u||std::bit_cast<std::uint32_t>(sx)!=0x3f800000u||
       std::bit_cast<std::uint32_t>(sy)!=0x3f800000u||w.unknownObserver||!finitePosition(w.center))return false;
    if(!w.ownedPointers(false)||w.objectTable[0x4a8/8]!=oracle.realPositionEntry())return false;
    if(w.layerTable[0x490/8]!=reinterpret_cast<void*>(&World::section)||
       w.layerTable[0x498/8]!=reinterpret_cast<void*>(&World::disabled)||
       w.objectTable[0x508/8]!=reinterpret_cast<void*>(&World::scale)||
       w.objectTable[0x520/8]!=reinterpret_cast<void*>(&World::rotation))return false;
    if(w.state.layer.get<int>(0xee4)<0||static_cast<std::size_t>(w.state.layer.get<int>(0xee4))+w.members.size()+1>w.state.slots.size())return false;
    if(w.group.get<void*>(0x38)!=w.groupData.data.data()||w.groupData.get<unsigned>(0)!=w.members.size()||w.groupData.get<void*>(0x10)!=w.members.data())return false;
    for(const auto p:w.members) {
        if(!p)break;
        const int n=w.identity(p);if(n<0||static_cast<unsigned>(n)>=w.state.objects.size())return false;
        const auto& o=w.state.objects[n];if(o.get<unsigned char>(0x520))continue;
        if(o.get<void*>(0)!=w.objectTable.data()||!identityObject(o))return false;
    }
    // The fixture exclusively constructs native zero-anchor CCNodes. Reject
    // extra transforms/parents rather than treating any CCNode as this recipe.
    for(const auto node:{&w.parent,&w.child}) {
        if(!oracle.identityHelper(const_cast<unsigned char*>(node->data.data())))return false;
        for(unsigned at:{0x38u,0x3cu,0x54u,0x58u,0x5cu,0x60u,0x64u,0x68u})
            if(node->get<std::uint32_t>(at)!=0)return false;
        for(unsigned at:{0x40u,0x44u})if(node->get<std::uint32_t>(at)!=0x3f800000u)return false;
        if(node->data[0x11d]||node->data[0x11f])return false;
    }
    if(w.parent.get<void*>(0xd8))return false;
    const auto childParent=w.child.get<void*>(0xd8);
    if(childParent&&childParent!=w.parent.data.data())return false;
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit1=AuditClock::now();
#endif
    const auto trig=oracle.atan2();
    if(!trig)return false;
    const auto previousErrno=errno;
    _mm_setcsr(control&~63u);
    const auto minusZero=std::bit_cast<float>(0x80000000u);
    const auto linear=pose::decompose({1,0,minusZero,1,0,0},trig);
    const auto linearFlags=_mm_getcsr()&63;
    _mm_setcsr(control);errno=previousErrno;
    if(linear.scaleX!=1.f||linear.scaleY!=1.f||linear.rotationX!=0.f||linear.rotationY!=0.f)return false;
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit2=AuditClock::now();
#endif
    World::active=&w;
    void* layer=w.state.layer.data.data();void* center=w.center.data.data();
    const auto reset=oracle.reset();
    if(reset(layer,center,true))World::section(layer,center);
    Point centerPoint{};nodeCall<0x4a8,Point*>(center,&centerPoint);
    auto parent=w.parent.data.data(),child=w.child.data.data();
    nodeCall<0xb8>(parent,&centerPoint);nodeCall<0x78>(parent,sx);nodeCall<0x88>(parent,sy);
    nodeCall<0xf0>(parent,0.f);nodeCall<0x100>(parent,0.f);nodeCall<0x150>(parent,0.f);
    oracle.retain(child);nodeCall<0x1f8>(child,false);nodeCall<0x1c0>(parent,child);oracle.release(child);
    pose::Matrix parentMatrix{};nodeCall<0x320,pose::Matrix*>(parent,&parentMatrix);
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit3=AuditClock::now();
#endif
    bool flagsPublished=false;
    for(const auto p:w.members) {
        if(!p)break;
        auto o=static_cast<unsigned char*>(p);if(o[0x520])continue;
        Point point{};nodeCall<0x4a8,Point*>(p,&point);
        const auto moved=reset(layer,p,true);o[0x350]=1;
        nodeCall<0x4a8,Point*>(p,&point);
        Point relative{pose::sub(point.x,centerPoint.x),pose::sub(point.y,centerPoint.y)};
        const auto local=materializeIdentityHelper(w.child,relative);
        const float tx=pose::add(pose::add(pose::mul(local.tx,parentMatrix.a),pose::mul(local.ty,parentMatrix.c)),parentMatrix.tx);
        const float ty=pose::add(pose::add(pose::mul(local.tx,parentMatrix.b),pose::mul(local.ty,parentMatrix.d)),parentMatrix.ty);
        float dx=pose::sub(tx,point.x),dy=pose::sub(ty,point.y);
#ifdef CHLORIUM_TRANSFORM_ZERO_DELTA_MUTANT
        dx=dy=0;
#endif
        oracle.move(layer,p,dx,dy);
        // Native decomposition occurs AFTER moveAreaObject's section callback.
        // Reapply sticky flags here even if that observer cleared them.
#ifndef CHLORIUM_TRANSFORM_DROP_FLAGS_MUTANT
        if(!flagsPublished||w.observerClearsFlags){_mm_setcsr(_mm_getcsr()|linearFlags);flagsPublished=true;}
#endif
#ifndef CHLORIUM_TRANSFORM_DROP_CALLBACK_MUTANT
        if(moved)World::section(layer,p);
#endif
    }
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit4=AuditClock::now();
#endif
    World::disabled(layer,w.group.data.data());World::active=nullptr;
#ifdef CHLORIUM_TRANSFORM_STAGE_AUDIT
    const auto audit5=AuditClock::now();
    const auto ns=[](auto span){return std::chrono::duration_cast<std::chrono::nanoseconds>(span).count();};
    stageAudit.validation+=ns(audit1-audit0);stageAudit.compilation+=ns(audit2-audit1);
    stageAudit.preparation+=ns(audit3-audit2);stageAudit.objects+=ns(audit4-audit3);
    stageAudit.publication+=ns(audit5-audit4);++stageAudit.phases;
#endif
    (void)resetVisual;return true;
}
}
