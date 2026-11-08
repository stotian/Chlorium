#pragma once
#include "../R4-33/ResidentRecords.hpp"
#include "../R4-33/SpriteCorners.hpp"
#include "../../src/renderer/DirtyStateUploads.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace chlorium::research::resident {
using snapshot::NativeQuadState;
enum class Lane : unsigned { geometry, parameters, order, fallback };
struct Handle {
    std::uint64_t owner=0, generation=0;
    std::uint32_t slot=0;
    explicit operator bool() const noexcept { return owner && generation; }
};
inline std::uint64_t nextIdentity() noexcept {
    static std::atomic<std::uint64_t> value{0};
    auto old=value.load(std::memory_order_relaxed);
    while(old!=UINT64_MAX)
        if(value.compare_exchange_weak(old,old+1,std::memory_order_relaxed))return old+1;
    return 0;
}

// Single-threaded authoritative storage. The adapter must notify EVERY producer;
// an unknown producer invalidates the scene before any replacement draw.
class Scene {
    using Dirty=snapshot::DirtyStateUploads;
    struct Life { std::uint64_t generation=1; bool live=false; };
    const std::size_t capacity_;
    const std::uint64_t identity_=nextIdentity();
    std::uint64_t revision_=1, consumer_=0, orderRevision_=1;
    bool usable_=true, orderValid_=true, orderDirty_=true, uploading_=false;
    std::vector<Geometry> geometry_;
    std::vector<Parameters> parameters_;
    std::vector<NativeQuadState> fallback_;
    std::vector<Life> life_;
    std::vector<std::uint32_t> free_, order_;
    std::vector<std::uint64_t> orderedAt_;
    Dirty geometryDirty_, parameterDirty_, fallbackDirty_;
    std::vector<std::uint64_t> quadDirty_;
    void markQuad(std::uint32_t slot)noexcept{quadDirty_[slot/64]|=std::uint64_t{1}<<(slot%64);}

    static std::size_t boundedCapacity(std::size_t count) {
        if(!count || count>65536)throw std::invalid_argument("resident scene capacity");
        return count;
    }
    bool reject() noexcept { usable_=false; return false; }
    bool writable() noexcept {
        return usable_ && identity_ && !uploading_ && revision_!=UINT64_MAX;
    }
    bool live(Handle h) const noexcept {
        return h.owner==identity_ && h.slot<capacity_ && life_[h.slot].live &&
               h.generation==life_[h.slot].generation;
    }
    static bool nearestProducer() noexcept {return (_mm_getcsr()&0x7f80u)==0x1f80u;}
    static bool admitted(const Geometry& g,const Parameters& p) noexcept {
        if(g.control[0]>1 || g.control[1] || g.control[2] || g.control[3])return false;
        if(!g.control[0])return true;
        pass::SpriteRecord source{};
        std::copy_n(g.rect,4,source.localRect);
        std::copy_n(p.matrix,4,source.matrix);
        std::copy_n(p.translationZFlags,2,source.matrix+4);
        source.z=p.translationZFlags[2]; source.flags=p.translationZFlags[3];
        return corners::domain(source);
    }
    static void fallbackStyle(Geometry& g,Parameters& p,const NativeQuadState& native) {
        for(unsigned c=0;c<4;++c){
            p.colors[c]=native.corners[c].color;
            std::memcpy(g.uv+c*2,&native.corners[c].u,8);
        }
    }
    void store(std::uint32_t slot,const Geometry& g,const Parameters& p,const NativeQuadState* native) {
        geometry_[slot]=g; parameters_[slot]=p;
        if(!g.control[0]){
            fallback_[slot]=*native; fallbackStyle(geometry_[slot],parameters_[slot],*native);
            fallbackDirty_.mark(slot);
        }
        geometryDirty_.mark(slot); parameterDirty_.mark(slot);
        markQuad(slot);
    }
public:
    struct QuadSnapshot {Handle handle;std::uint64_t revision;NativeQuadState quad;};
    explicit Scene(std::size_t count)
        : capacity_(boundedCapacity(count)), geometry_(capacity_), parameters_(capacity_),
          fallback_(capacity_), life_(capacity_), orderedAt_(capacity_),
          geometryDirty_(capacity_), parameterDirty_(capacity_), fallbackDirty_(capacity_),quadDirty_((capacity_+63)/64) {
        free_.reserve(capacity_); order_.reserve(capacity_);
        for(std::size_t n=capacity_;n;--n)free_.push_back(static_cast<std::uint32_t>(n-1));
        if(!identity_)usable_=false;
    }
    Scene(const Scene&)=delete;
    Scene& operator=(const Scene&)=delete;
    std::uint64_t identity() const noexcept { return identity_; }
    std::uint64_t revision() const noexcept { return revision_; }
    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t orderedCount() const noexcept { return order_.size(); }
    bool usable() const noexcept { return usable_ && orderValid_; }
    void invalidate() noexcept { usable_=false; }
    bool current(const QuadSnapshot& snapshot) const noexcept {
        return usable_ && !uploading_ && live(snapshot.handle) && snapshot.revision==revision_;
    }
    // Owned bytes only: this does not write a sprite, atlas, dirty flag or native
    // FP status. The future native adapter must validate lifetime before copying.
    std::optional<QuadSnapshot> prepareNative(Handle h) const {
        if(!usable_ || uploading_ || !live(h))return std::nullopt;
        QuadSnapshot result{h,revision_,{}};
        const auto& g=geometry_[h.slot];const auto& p=parameters_[h.slot];
        if(!g.control[0]){result.quad=fallback_[h.slot];return result;}
        pass::SpriteRecord source{};
        std::copy_n(g.rect,4,source.localRect);std::copy_n(p.matrix,4,source.matrix);
        std::copy_n(p.translationZFlags,2,source.matrix+4);source.z=p.translationZFlags[2];source.flags=p.translationZFlags[3];
        for(unsigned c=0;c<4;++c){result.quad.corners[c].color=p.colors[c];std::memcpy(&result.quad.corners[c].u,g.uv+c*2,8);}
        const auto previous=_mm_getcsr();_mm_setcsr(0x1f80);
        const bool ok=corners::materialize(source,result.quad);_mm_setcsr(previous);
        if(!ok)return std::nullopt;
        return result;
    }

    Handle create(const Geometry& g,const Parameters& p,const NativeQuadState* native=nullptr) {
        if(!writable() || free_.empty() || !admitted(g,p) || (g.control[0] && !nearestProducer()) || (!g.control[0] && !native)) {
            reject(); return {};
        }
        const auto slot=free_.back(); free_.pop_back();
        store(slot,g,p,native);
        life_[slot].live=true; ++revision_;
        return {identity_,life_[slot].generation,slot};
    }
    bool replace(Handle h,const Geometry& g,const Parameters& p,const NativeQuadState* native=nullptr) {
        if(!writable() || !live(h) || !admitted(g,p) || (g.control[0] && !nearestProducer()) || (!g.control[0] && !native))return reject();
        store(h.slot,g,p,native);
        ++revision_; return true;
    }
    // Compare only the notified sprite, never the scene. A spurious notification
    // must not turn into a GPU upload or advance the acknowledged revision.
    bool update(Handle h,Geometry g,Parameters p,const NativeQuadState* native=nullptr) {
        if(!writable() || !live(h) || !admitted(g,p) || (!g.control[0] && !native))return reject();
        if(!g.control[0])fallbackStyle(g,p,*native);
        const bool geometryChanged=std::memcmp(&g,&geometry_[h.slot],sizeof(g))!=0;
        const bool parametersChanged=std::memcmp(&p,&parameters_[h.slot],sizeof(p))!=0;
        const bool fallbackChanged=!g.control[0] && std::memcmp(native,&fallback_[h.slot],sizeof(*native))!=0;
        const bool poseChanged=std::memcmp(g.rect,geometry_[h.slot].rect,sizeof(g.rect)) ||
            std::memcmp(g.control,geometry_[h.slot].control,sizeof(g.control)) ||
            std::memcmp(p.matrix,parameters_[h.slot].matrix,sizeof(p.matrix)) ||
            std::memcmp(p.translationZFlags,parameters_[h.slot].translationZFlags,sizeof(p.translationZFlags));
        if(g.control[0] && poseChanged && !nearestProducer())return reject();
        if(!geometryChanged && !parametersChanged && !fallbackChanged)return true;
        if(geometryChanged){geometry_[h.slot]=g;geometryDirty_.mark(h.slot);}
        if(parametersChanged){parameters_[h.slot]=p;parameterDirty_.mark(h.slot);}
        if(fallbackChanged){fallback_[h.slot]=*native;fallbackDirty_.mark(h.slot);}
        markQuad(h.slot);
        ++revision_;return true;
    }
    bool parameters(Handle h,const Parameters& value) {
        if(!writable() || !live(h) || !admitted(geometry_[h.slot],value) || !nearestProducer())return reject();
        // Fallback colors/UV/positions are a single authoritative 96-byte record.
        if(!geometry_[h.slot].control[0])return reject();
        parameters_[h.slot]=value; parameterDirty_.mark(h.slot);markQuad(h.slot); ++revision_; return true;
    }
    // Only for a classified style writer. Its copied CPU positions may predate
    // the owned GPU pose; flags/rectangle/matrix must remain from the pose event.
    bool style(Handle h,const NativeQuadState& value) {
        if(!writable() || !live(h))return reject();
        auto g=geometry_[h.slot];auto p=parameters_[h.slot];
        fallbackStyle(g,p,value);
        if(g.control[0])return update(h,g,p);
        // Unsupported geometry is still authoritative native data, never a
        // deferred mirror. A position change is not a style-only notification.
        for(unsigned c=0;c<4;++c)
            if(std::memcmp(&value.corners[c].x,&fallback_[h.slot].corners[c].x,12))return reject();
        return update(h,g,p,&value);
    }
    bool erase(Handle h) {
        if(!writable() || !live(h))return reject();
        if(orderedAt_[h.slot]==orderRevision_)orderValid_=false;
        auto& life=life_[h.slot]; life.live=false;
        if(life.generation!=UINT64_MAX){++life.generation; free_.push_back(h.slot);}
        ++revision_; return true;
    }
    bool painterOrder(std::span<const Handle> handles) {
        if(!writable() || handles.size()>capacity_)return reject();
        for(auto h:handles)if(!live(h))return reject();
        if(orderValid_ && handles.size()==order_.size() &&
           std::equal(handles.begin(),handles.end(),order_.begin(),[](Handle h,std::uint32_t slot){return h.slot==slot;}))return true;
        // Commit only after the complete order has passed handle validation.
        order_.resize(handles.size()); orderRevision_=++revision_;
        for(std::size_t i=0;i<handles.size();++i){order_[i]=handles[i].slot; orderedAt_[handles[i].slot]=orderRevision_;}
        orderValid_=true; orderDirty_=true; return true;
    }

    // Sink: identity(), matches(owner,revision), belongsTo(owner,capacity), begin(owner,capacity),
    // write(lane,first,bytes), commit(owner,revision,orderCount,quadMask,seed,orderChanged). Failed writes
    // retain ALL dirties; a different/recreated sink always gets a complete seed.
    template<class Sink> bool synchronize(Sink& sink) {
        if(!usable() || uploading_ || !sink.identity())return false;
        if(sink.matches(identity_,revision_))return true;
        struct Lock { bool& value; explicit Lock(bool& b):value(b){value=true;} ~Lock(){value=false;} } lock(uploading_);
        const auto sinkIdentity=sink.identity();
        const bool seed=consumer_!=sinkIdentity || !sink.belongsTo(identity_,capacity_);
        if(!sink.begin(identity_,capacity_))return false;
        auto lane=[&](Lane kind,const auto& data,Dirty& dirty){
            using Value=typename std::decay_t<decltype(data)>::value_type;
            if(seed)return sink.write(kind,0,std::as_bytes(std::span<const Value>(data)));
            for(const auto& range:dirty.finish())
                if(!sink.write(kind,range.first,std::as_bytes(std::span<const Value>(data).subspan(range.first,range.count))))return false;
            return true;
        };
        if(!lane(Lane::geometry,geometry_,geometryDirty_) ||
           !lane(Lane::parameters,parameters_,parameterDirty_) ||
           !lane(Lane::fallback,fallback_,fallbackDirty_))return false;
        if((seed || orderDirty_) && !sink.write(Lane::order,0,std::as_bytes(std::span<const std::uint32_t>(order_))))return false;
        if(!usable() || sink.identity()!=sinkIdentity || !sink.commit(identity_,revision_,order_.size(),std::span<const std::uint64_t>(quadDirty_),seed,seed||orderDirty_))return false;
        geometryDirty_=Dirty(capacity_); parameterDirty_=Dirty(capacity_); fallbackDirty_=Dirty(capacity_);
        std::fill(quadDirty_.begin(),quadDirty_.end(),0);orderDirty_=false; consumer_=sinkIdentity; return true;
    }
};
}
