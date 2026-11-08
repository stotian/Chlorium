#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../tests/SystemInputGuard.hpp"
#include "SpriteCorners.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <map>
#include <stdexcept>

using chlorium::research::pass::SpriteRecord;
using chlorium::snapshot::NativeQuadState;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct alignas(16) Memory {
    std::array<std::byte,0x300> data{};
    template<class T> void put(unsigned offset,const T& value){std::memcpy(data.data()+offset,&value,sizeof(value));}
};
struct Matrix {std::uint32_t values[6];};
unsigned calls[3]{};
bool dirty(void* object){++calls[0];return std::to_integer<unsigned>(static_cast<Memory*>(object)->data[0x178])!=0;}
void setDirty(void* object,bool value){++calls[2];static_cast<Memory*>(object)->data[0x178]=std::byte(value);}
Matrix* transform(void* object,Matrix* out){++calls[1];std::memcpy(out,static_cast<Memory*>(object)->data.data()+0x280,sizeof(Matrix));return out;}
bool invoke(void* method,void* object,unsigned long& fault) noexcept {
    __try {reinterpret_cast<void(*)(void*)>(method)(object);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();return false;}
}
std::uint64_t hash(const unsigned char* bytes,std::size_t size){std::uint64_t value=14695981039346656037ull;for(std::size_t i=0;i<size;++i)value=(value^bytes[i])*1099511628211ull;return value;}
struct Captured {
    SpriteRecord sprite;NativeQuadState quad;
    std::array<std::uint64_t,12> resource{};
    unsigned frame=2; // Synthetic arithmetic corpus is separate from captured frames.
};
void appendArithmeticCases(std::vector<Captured>& records) {
    std::mt19937 random(0x43c0c05u);
    constexpr std::array<std::uint32_t,24> values{0,0x80000000u,0x3f800000u,0xbf800000u,
        0x33800000u,0xb3800000u,0x4f800000u,0xcf800000u,0x3f800001u,0x3f7fffffu,
        0x3dccccccu,0xbdccccccu,0x40000000u,0xc0000000u,0x47800000u,0xc7800000u,
        0x3e800000u,0xbe800000u,0x2b800000u,0xab800000u,0x2b800001u,0xab800001u,0x33000000u,0xb3000000u};
    for(unsigned i=0;i<4096;++i){
        Captured record{};auto& source=record.sprite;
        source.flags=512u|1024u;
        if(i%17==0)source.flags&=~512u;
        if(i%19==0)source.flags&=~1024u;
        if(i%23==0)source.flags|=1u;
        if(i%29==0)source.flags|=64u;
        if(i%31==0)source.flags|=128u;
        for(auto& value:source.matrix)value=values[random()%values.size()];
        for(auto& value:source.localRect)value=values[random()%values.size()];
        source.z=i%3==0?0x80000000u:i%3==1?0x00000001u:values[random()%values.size()];
        auto* bytes=reinterpret_cast<unsigned char*>(&record.quad);
        for(std::size_t j=0;j<sizeof(record.quad);++j)bytes[j]=static_cast<unsigned char>(random());
        records.push_back(record);
    }
    Captured boundary{};boundary.sprite.flags=512|1024;
    boundary.sprite.matrix[0]=0x2b800001u;boundary.sprite.matrix[2]=0xab800000u;
    boundary.sprite.localRect[0]=boundary.sprite.localRect[1]=0x2b800001u;
    boundary.sprite.localRect[2]=boundary.sprite.localRect[3]=0xab800000u;
    records.push_back(boundary); // TR.x is exactly the smallest positive normal float.
}
std::vector<Captured> readCapture(const std::filesystem::path& path) {
    require(std::filesystem::file_size(path)<=64*1024*1024,"capture file budget");
    std::ifstream input(path,std::ios::binary);
    auto read=[&](void* data,std::size_t bytes){input.read(static_cast<char*>(data),bytes);require(bool(input),"truncated capture");};
    std::array<char,8> magic;std::array<std::uint32_t,4> layout;
    read(magic.data(),magic.size());read(layout.data(),sizeof(layout));
    require(magic==std::array<char,8>{'C','H','L','P','A','S','S','3'}&&layout[0]==2&&layout[1]==96&&layout[2]==192&&layout[3]==96,"expected complete V3 capture layout");
    std::vector<Captured> output;
    std::uint64_t context=0,scene=0,lastFrame=0;
    for(unsigned f=0;f<layout[0];++f){
        chlorium::research::pass::FrameHeader frame;read(&frame,sizeof(frame));
        require(!frame.reasons&&frame.draws<=2048&&frame.quads<=65536&&frame.sprites==frame.quads,"complete bounded frame");
        require(frame.context&&frame.scene&&(!f||(frame.context==context&&frame.scene==scene&&frame.frame==lastFrame+1)),"continuous native capture");
        context=frame.context;scene=frame.scene;lastFrame=frame.frame;
        std::vector<chlorium::research::pass::DrawRecord> draws(frame.draws);read(draws.data(),draws.size()*sizeof(draws[0]));
        std::vector<NativeQuadState> quads(frame.quads);read(quads.data(),quads.size()*sizeof(quads[0]));
        std::vector<SpriteRecord> sprites(frame.sprites);read(sprites.data(),sprites.size()*sizeof(sprites[0]));
        using Base=std::array<std::uint64_t,9>;
        std::map<std::pair<Base,unsigned>,unsigned> scopes;std::map<Base,unsigned> occurrences;
        std::size_t consumed=0;
        for(const auto& draw:draws){
            require(!draw.reasons&&draw.quadOffset==consumed&&draw.spriteOffset==consumed&&draw.count<=quads.size()-consumed&&draw.mapped==draw.count,"complete native draw span");
            Base key{draw.batch,draw.batchId,draw.atlas,draw.atlasId,draw.texture,draw.textureId,draw.shader,draw.blendSrc,draw.blendDst};
            auto [scope,fresh]=scopes.try_emplace({key,draw.scope},0);
            if(fresh)scope->second=occurrences[key]++;
            for(unsigned j=0;j<draw.count;++j){
                const auto index=consumed+j;const auto& sprite=sprites[index];
                require(sprite.sprite&&sprite.quadMatches==1&&!(sprite.flags&~2047u)&&sprite.atlasIndex==draw.first+j,"mapped native owner");
                Captured record{sprite,quads[index]};std::copy(key.begin(),key.end(),record.resource.begin());
                record.resource[9]=scope->second;record.resource[10]=sprite.sprite;record.resource[11]=sprite.id;record.frame=f;
                output.push_back(record);
            }
            consumed+=draw.count;
        }
        require(consumed==quads.size(),"full native draw coverage");
    }
    require(input.peek()==std::char_traits<char>::eof(),"trailing capture bytes");
    return output;
}
HMODULE loadNative(const std::filesystem::path& source,const chlorium::test::SystemInputGuard&) {
    auto cocosPath=std::filesystem::absolute(source).lexically_normal();cocosPath.make_preferred();
    const auto module=LoadLibraryExW(cocosPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module)std::cerr<<"Cocos load error "<<GetLastError()<<'\n';
    require(module!=nullptr,"load owned Cocos DLL");chlorium::test::SystemInputGuard::assertNoGeode();
    const auto base=reinterpret_cast<const unsigned char*>(module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    require(pe->FileHeader.TimeDateStamp==0x69702d99&&pe->OptionalHeader.SizeOfImage==0x1c5000,"supported Cocos image");
    require(hash(base+0x8d570,0x719)==0x302554714c2943d2ull,"exact native sprite arithmetic body");
    return module;
}
struct NativeResult {NativeQuadState quad;unsigned fpState;};
NativeResult nativeResult(const Captured& record,unsigned controls,const unsigned char* base) {
    require((controls&0x1f80u)==0x1f80u&&!(controls&~0xffffu),"masked bounded native FP controls");
    Memory object,parent;
    std::array<void*,160> table{}; table[0x400/8]=reinterpret_cast<void*>(&dirty);
    table[0x408/8]=reinterpret_cast<void*>(&setDirty); table[0x308/8]=reinterpret_cast<void*>(&transform);
    unsigned long fault=0;
    object.put(0,table.data());object.put(0x170,std::uintptr_t{1});
    object.put(0x11e,bool(record.sprite.flags&1024));object.put(0x149,bool(record.sprite.flags&512));
    object.put(0x178,true);object.put(0x17b,bool(record.sprite.flags&1));
    if(record.sprite.flags&1){parent.put(0x17b,true);object.put(0xd8,parent.data.data());}
    object.put(0x280,record.sprite.matrix);object.put(0x17c,record.sprite.matrix);
    object.put(0x1bc,record.sprite.localRect[0]);object.put(0x1c0,record.sprite.localRect[1]);
    object.put(0x1b0,record.sprite.localRect[2]);object.put(0x1b4,record.sprite.localRect[3]);
    object.put(0x48,record.sprite.z);object.put(0x1cc,record.quad);
    std::fill(std::begin(calls),std::end(calls),0);_mm_setcsr(controls);
    require(invoke(const_cast<unsigned char*>(base+0x8d570),&object,fault),"native arithmetic oracle fault");
    const auto nativeFlags=_mm_getcsr();NativeQuadState actual;std::memcpy(&actual,object.data.data()+0x1cc,sizeof(actual));
    const bool hidden=(record.sprite.flags&1)||!(record.sprite.flags&1024);
    require(calls[0]==1&&calls[2]==1&&calls[1]==unsigned(!hidden),"native oracle callback shape changed");
    return {actual,nativeFlags};
}
}
