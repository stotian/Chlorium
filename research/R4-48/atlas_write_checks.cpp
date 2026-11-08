#include "../R4-33/NativeCornerOracleFixture.hpp"
#include "NativeAtlasWrite.hpp"
#include <iostream>

namespace {
struct Atlas {
    std::array<unsigned char,0x48> prefix{};
    std::uint8_t dirty=0,padding[3]{};
    std::uint32_t total=0;
    std::array<unsigned char,0x10> middle{};
    void* quads=nullptr;
    std::array<unsigned char,0x18> suffix{};
};
static_assert(offsetof(Atlas,dirty)==0x48 && offsetof(Atlas,total)==0x4c && offsetof(Atlas,quads)==0x60 && sizeof(Atlas)==0x80);
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: atlas_write_checks.exe owned-libcocos2d.dll");
    chlorium::test::SystemInputGuard system;
    const auto module=loadNative(argv[1],system);
    const auto entry=GetProcAddress(module,"?updateQuad@CCTextureAtlas@cocos2d@@QEAAXPEAU_ccV3F_C4B_T2F_Quad@2@I@Z");
    const auto base=reinterpret_cast<const unsigned char*>(module);
    require(reinterpret_cast<const unsigned char*>(entry)==base+0xa6fa0,"native updateQuad export");
    // Hash is populated from static input inspection, not from a test result.
    std::uint64_t digest=14695981039346656037ull;
    for(unsigned n=0;n<95;++n)digest=(digest^base[0xa6fa0+n])*1099511628211ull;
    require(digest==0xc41d2ee16bc7515full,"reviewed complete native updateQuad body");
    using Native=void(*)(void*,const void*,unsigned);
    const auto native=reinterpret_cast<Native>(entry);
    std::array<std::byte,384> memory{},external{};Atlas atlas;atlas.quads=memory.data();
    std::size_t cases=0;
    for(unsigned index=0;index<4;++index)for(unsigned total:{0u,1u,4u,0xffffffffu})
    for(unsigned initialDirty:{0u,1u,255u})for(int sourceOffset:{-1,0,1,8,16,32,64,96,160,256}){
        if(sourceOffset>=0 && sourceOffset+96>int(memory.size()))continue;
        for(unsigned n=0;n<memory.size();++n){memory[n]=std::byte((n*37+index*11+total)&255);external[n]=std::byte((n*71+initialDirty)&255);}
        atlas.total=total;atlas.dirty=static_cast<std::uint8_t>(initialDirty);
        const auto beforeMemory=memory;const auto beforeAtlas=atlas;
        const auto* input=sourceOffset<0?external.data():memory.data()+sourceOffset;
        const auto fp=_mm_getcsr();SetLastError(0x5522);
        native(&atlas,input,index);
        const auto expectedMemory=memory;const auto expectedAtlas=atlas;
        require(_mm_getcsr()==fp && GetLastError()==0x5522,"native integer/copy boundary");
        memory=beforeMemory;atlas=beforeAtlas;
        require(chlorium::research::atlaswrite::apply(memory,index,{input,96},atlas.total,atlas.dirty),"owned atlas write model");
        require(memory==expectedMemory && !std::memcmp(&atlas,&expectedAtlas,sizeof(atlas)),"full native atlas metadata/copy postcondition");
        require(_mm_getcsr()==fp && GetLastError()==0x5522,"model integer/copy boundary");++cases;
    }
    const auto before=memory;const auto oldTotal=atlas.total;const auto oldDirty=atlas.dirty;
    require(!chlorium::research::atlaswrite::apply(memory,4,{external.data(),96},atlas.total,atlas.dirty) &&
            memory==before && atlas.total==oldTotal && atlas.dirty==oldDirty,"out-of-range rejection is atomic");
    require(!chlorium::research::atlaswrite::apply(memory,0,{external.data(),95},atlas.total,atlas.dirty) && memory==before,"short input rejects atomically");
    std::cout<<"native atlas model cases="<<cases<<"; no native write suppression or FPS claim\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
