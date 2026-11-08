#pragma once
#include "../R4-32/PassCapture.hpp"
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace chlorium::research::producers {
using pass::SpriteRecord;using snapshot::NativeQuadState;
struct Key {std::uint64_t owner=0;std::uint32_t id=0,index=0;bool operator==(const Key&)const=default;};
struct Hash {std::size_t operator()(Key k)const noexcept{return std::size_t(k.owner^(std::uint64_t(k.id)<<32)^(std::uint64_t(k.index)*0x9e3779b97f4a7c15ull));}};
struct Packet {SpriteRecord source;NativeQuadState quad;};
struct Coverage {
    unsigned frame=0,ordinal=0,count=0,rawMatched=0,rawMissing=0,rawDifferent=0,sourceMatched=0,sourceMissing=0,sourceDifferent=0,indexMatched=0,indexDifferent=0,linearOrderDifferent=0,incomplete=0;
};
// Opaque addresses identify copied packets only; never dereferenced after a callback.
// This is a two-snapshot observer, NOT proof of lifetime or CPU-reader closure.
class Shadow {
    static constexpr std::size_t maxEntries=pass::maxQuads;
    std::unordered_map<Key,NativeQuadState,Hash> quads_;
    std::unordered_map<Key,Packet,Hash> sprites_;
    template<class Map,class Value>bool put(Map& map,Key key,const Value& value){
        auto at=map.find(key);if(at!=map.end()){at->second=value;return true;}
        if(map.size()>=limit_){failed=true;return false;}map.emplace(key,value);return true;
    }
    const std::size_t limit_;
    static bool coreEqual(const SpriteRecord& a,const SpriteRecord& b)noexcept{
        return a.sprite==b.sprite && a.id==b.id &&
            (a.flags&~6u)==(b.flags&~6u) && a.z==b.z &&
            !std::memcmp(a.matrix,b.matrix,sizeof(a.matrix)) && !std::memcmp(a.localRect,b.localRect,sizeof(a.localRect)) &&
            !std::memcmp(a.anchor,b.anchor,sizeof(a.anchor)) && !std::memcmp(a.vertexMods,b.vertexMods,sizeof(a.vertexMods)) &&
            !std::memcmp(a.textureBounds,b.textureBounds,sizeof(a.textureBounds));
    }
public:
    bool failed=false;
    std::uint64_t poseCallbacks=0,colorCallbacks=0,atlasWrites=0;
    std::vector<Coverage> coverage;
    explicit Shadow(std::size_t limit=maxEntries):limit_(std::min(limit,maxEntries)){}
    // Oracle data validates the event packets; it never supplies their payload.
    // shortcut: two-frame diagnostic only, require lifetime/reader closure before live use.
    bool assemble(std::span<const SpriteRecord> oracle,std::span<const std::byte> bytes,
                  std::vector<SpriteRecord>& sources,std::vector<unsigned char>& vertices)const{
        sources.clear();vertices.clear();
        if(failed || oracle.empty() || oracle.size()>limit_ || bytes.size()!=oracle.size()*sizeof(NativeQuadState))return false;
        std::unordered_set<std::uint64_t> seen;seen.reserve(oracle.size());
        for(std::size_t n=0;n<oracle.size();++n){
            const auto& source=oracle[n];const auto at=sprites_.find({source.sprite,source.id,0});
            if(!source.sprite || !source.quadMatches || !seen.insert(source.sprite).second || at==sprites_.end())return false;
            const auto& packet=at->second;
            if(packet.source.atlasIndex!=source.atlasIndex || !coreEqual(packet.source,source) ||
               std::memcmp(&packet.quad,bytes.data()+n*sizeof(NativeQuadState),sizeof(NativeQuadState)))return false;
        }
        sources.reserve(oracle.size());vertices.resize(bytes.size());
        for(std::size_t n=0;n<oracle.size();++n){
            const auto& packet=sprites_.at({oracle[n].sprite,oracle[n].id,0});
            sources.push_back(packet.source);sources.back().quadMatches=1;
            std::memcpy(vertices.data()+n*sizeof(NativeQuadState),&packet.quad,sizeof(NativeQuadState));
        }
        return true;
    }
    void sprite(const SpriteRecord& source,const NativeQuadState& quad,bool color){
        if(color)++colorCallbacks;else ++poseCallbacks;
        if(!source.sprite){failed=true;return;}put(sprites_,{source.sprite,source.id,0},Packet{source,quad});
    }
    void atlas(std::uint64_t address,std::uint32_t id,unsigned index,const NativeQuadState& quad){
        ++atlasWrites;if(!address){failed=true;return;}put(quads_,{address,id,index},quad);
    }
    void observeRaw(unsigned frame,const pass::DrawRecord& draw,std::span<const SpriteRecord> sources,std::span<const std::byte> bytes,unsigned linearOrderDifferent=0){
        Coverage row;row.frame=frame;row.ordinal=draw.ordinal;row.count=draw.count;row.linearOrderDifferent=linearOrderDifferent;
        if((frame!=1 && frame!=2) || coverage.size()>=pass::maxDraws*2 || !draw.atlas || draw.count!=sources.size() || sources.size()>pass::maxQuads || sources.size()*sizeof(NativeQuadState)!=bytes.size()) {failed=true;return;}
        if(draw.reasons)row.incomplete=draw.count;
        for(unsigned n=0;n<draw.count;++n){const auto& source=sources[n];const Key slot{draw.atlas,draw.atlasId,draw.first+n};
            NativeQuadState quad;std::memcpy(&quad,bytes.data()+std::size_t(n)*sizeof(quad),sizeof(quad));
            if(frame==1){
                put(quads_,slot,quad);
                if(source.sprite && source.quadMatches)put(sprites_,{source.sprite,source.id,0},Packet{source,quad});
                else ++row.incomplete;
                continue;
            }
            auto q=quads_.find(slot);
            if(q==quads_.end())++row.rawMissing;
            else if(std::memcmp(&q->second,&quad,sizeof(quad)))++row.rawDifferent;else ++row.rawMatched;
            auto s=sprites_.find({source.sprite,source.id,0});
            if(s!=sprites_.end()){if(s->second.source.atlasIndex==source.atlasIndex)++row.indexMatched;else ++row.indexDifferent;}
            if(!source.sprite || s==sprites_.end())++row.sourceMissing;
            else if(!source.quadMatches || !coreEqual(s->second.source,source) || std::memcmp(&s->second.quad,&quad,sizeof(quad)))++row.sourceDifferent;
            else ++row.sourceMatched;
        }
        coverage.push_back(row);
    }
    void observe(unsigned frame,const pass::DrawRecord& draw,std::span<const SpriteRecord> sources,std::span<const NativeQuadState> quads){observeRaw(frame,draw,sources,std::as_bytes(quads));}
    bool sampledCoverageComplete()const noexcept{
        bool second=false;if(failed)return false;
        for(const auto& r:coverage){if(r.incomplete)return false;if(r.frame==2){second|=r.count!=0;if(r.rawMissing || r.rawDifferent || r.sourceMissing || r.sourceDifferent)return false;}}
        return second;
    }
};
}
