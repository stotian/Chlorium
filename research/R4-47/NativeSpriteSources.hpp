#pragma once
#include "RetainedScene.hpp"
#include "../R4-33/SpriteEncoding.hpp"
#include <unordered_map>
#include <unordered_set>
#ifdef CHLORIUM_RESIDENT_FAST_R58
#include "../R4-58/VerifiedEncoding.hpp"
#include "../R4-58/AddressSet.hpp"
#endif

namespace chlorium::research::resident {
struct SpriteKey {
    std::uint64_t address=0;
    std::uint32_t nativeId=0;
    std::uint64_t lifetime=0;
    explicit operator bool() const noexcept {return address && lifetime;}
};

// Shadow adapter for complete, synchronous source notifications. Addresses are
// identity values only; no sprite is dereferenced and no native work is skipped.
class SpriteSources {
    struct Entry {
        std::uint32_t nativeId; Handle handle; std::uint64_t lifetime; bool reconstructed;
#ifdef CHLORIUM_RESIDENT_FAST_R58
        VerifiedEncoding encoding;
#endif
    };
    Scene scene_;
    std::unordered_map<std::uint64_t,Entry> entries_;
    std::vector<Handle> order_;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    AddressSet active_;
#endif
    std::size_t reconstructed_=0;
    bool reject() noexcept {scene_.invalidate();return false;}
    static bool sameLifetime(const Entry& entry,SpriteKey key) noexcept {
        return key.lifetime && entry.nativeId==key.nativeId && entry.lifetime==key.lifetime;
    }
    static bool known(const pass::SpriteRecord& source) noexcept {
        return source.sprite && !(source.flags&~0x7ffu) && source.quadMatches<=1;
    }
    static void shadow(const pass::SpriteRecord& source,const NativeQuadState& native,Geometry& g,Parameters& p) {
        encodeSprite(source,native,g,p);
        if(!g.control[0])return;
        if(!source.quadMatches){g.control[0]=0;return;}
        const auto previous=_mm_getcsr();
        if((previous&0x7f80u)!=0x1f80u){g.control[0]=0;return;}
        auto predicted=native;
        const bool matches=corners::materialize(source,predicted) && std::memcmp(&predicted,&native,sizeof(native))==0;
        _mm_setcsr(previous);
        if(!matches)g.control[0]=0;
    }
public:
    explicit SpriteSources(std::size_t capacity):scene_(capacity){entries_.reserve(capacity);order_.reserve(capacity);}
    SpriteSources(const SpriteSources&)=delete;
    SpriteSources& operator=(const SpriteSources&)=delete;
    Scene& scene() noexcept {return scene_;}
    std::size_t reconstructedCount() const noexcept {return reconstructed_;}
    std::optional<SpriteKey> keyFor(std::uint64_t address) const {
        auto entry=entries_.find(address);if(entry==entries_.end())return std::nullopt;
        return SpriteKey{address,entry->second.nativeId,entry->second.lifetime};
    }
    bool reconstructs(SpriteKey key) const noexcept {
        auto entry=entries_.find(key.address);
        return entry!=entries_.end() && sameLifetime(entry->second,key) && entry->second.reconstructed;
    }
    SpriteKey create(const pass::SpriteRecord& source,const NativeQuadState& style) {
        if(!known(source) || entries_.contains(source.sprite)){reject();return {};}
        Geometry g{};Parameters p{};shadow(source,style,g,p);
        const auto h=scene_.create(g,p,g.control[0]?nullptr:&style);if(!h)return {};
        const auto lifetime=nextIdentity();if(!lifetime){reject();return {};}
        try{entries_.emplace(source.sprite,Entry{source.id,h,lifetime,g.control[0]!=0});}
        catch(...){scene_.invalidate();throw;}
        reconstructed_+=g.control[0]!=0;
        return {source.sprite,source.id,lifetime};
    }
    bool publish(SpriteKey key,const pass::SpriteRecord& source,const NativeQuadState& style) {
        if(!known(source) || key.address!=source.sprite || key.nativeId!=source.id)return reject();
        auto entry=entries_.find(source.sprite);
        if(entry==entries_.end() || !sameLifetime(entry->second,key))return reject();
        Geometry g{};Parameters p{};
#ifdef CHLORIUM_RESIDENT_FAST_R58
        entry->second.encoding.get(source,style,_mm_getcsr()&~0x3fu,g,p,shadow);
#else
        shadow(source,style,g,p);
#endif
        if(!scene_.update(entry->second.handle,g,p,g.control[0]?nullptr:&style))return false;
        reconstructed_-=entry->second.reconstructed;entry->second.reconstructed=g.control[0]!=0;
        reconstructed_+=entry->second.reconstructed;return true;
    }
    // The hook adapter must establish the writer kind; an unknown full-quad
    // writer must use publish(), not discard its positions through this API.
    bool publishStyle(SpriteKey key,const NativeQuadState& style) {
        auto entry=entries_.find(key.address);
        if(entry==entries_.end() || !sameLifetime(entry->second,key))return reject();
        return scene_.style(entry->second.handle,style);
    }
    std::optional<Scene::QuadSnapshot> prepareNative(SpriteKey key) const {
        auto entry=entries_.find(key.address);
        if(entry==entries_.end() || !sameLifetime(entry->second,key))return std::nullopt;
        return scene_.prepareNative(entry->second.handle);
    }
    bool erase(SpriteKey key) {
        auto entry=entries_.find(key.address);
        if(entry==entries_.end() || !sameLifetime(entry->second,key))return reject();
        if(!scene_.erase(entry->second.handle))return false;
        reconstructed_-=entry->second.reconstructed;
        entries_.erase(entry);return true;
    }
    // Cache eviction only; absence here says nothing about native destruction.
    bool discardAbsent(std::span<const pass::SpriteRecord> active) {
#ifdef CHLORIUM_RESIDENT_FAST_R58
        if(!active_.reset(active.size()))return reject();
        auto& keep=active_;
        for(const auto& source:active)if(!source.sprite || !keep.insert(source.sprite))return reject();
#else
        std::unordered_set<std::uint64_t> keep;keep.reserve(active.size());
        for(const auto& source:active)if(!source.sprite || !keep.insert(source.sprite).second)return reject();
#endif
        for(auto at=entries_.begin();at!=entries_.end();){
            if(keep.contains(at->first)){++at;continue;}
            const SpriteKey key{at->first,at->second.nativeId,at->second.lifetime};++at;
            if(!erase(key))return false;
        }
        return true;
    }
    bool painterOrder(std::span<const SpriteKey> keys) {
        if(keys.size()>scene_.capacity())return reject();
        order_.clear();
        for(auto key:keys){
            auto entry=entries_.find(key.address);
            if(entry==entries_.end() || !sameLifetime(entry->second,key))return reject();
            order_.push_back(entry->second.handle);
        }
        return scene_.painterOrder(order_);
    }
};
}
