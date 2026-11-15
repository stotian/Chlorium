#include "../../src/renderer/NativeVertexLease.hpp"
#include <array>
#include <cstdint>
#include <iostream>

struct Atlas {
    std::array<std::uint32_t,4> native{1,2,3,4},vbo{};
    std::uint32_t buffer=1;
    bool dirty=false;
    unsigned uploads=0,draws=0;
    void draw(){if(dirty){vbo=native;dirty=false;++uploads;}++draws;}
};

int main(){
    auto check=[](bool valid,const char* message){if(!valid)std::cerr<<"FAIL "<<message<<'\n';return valid;};
    for(bool initiallyDirty:{false,true}){
        Atlas atlas;atlas.dirty=initiallyDirty;
        atlas.vbo=atlas.native; // A successful prepare writes the exact canonical bytes itself.
        {
            chlorium::snapshot::NativeVertexLease lease(atlas.buffer,atlas.dirty,atlas.buffer);
            atlas.draw();
        }
        atlas.dirty=false;
        if(!check(atlas.vbo==atlas.native && atlas.draws==1 && atlas.uploads==0 && !atlas.dirty,"successful canonical draw suppresses stale native upload"))return 1;

        for(bool modified:{false,true}){
            const auto uploads=atlas.uploads;
            const auto draws=atlas.draws;
            atlas.dirty=initiallyDirty;atlas.vbo=atlas.native;if(modified)atlas.vbo[2]=0xbad;
            chlorium::snapshot::markNativeVboDirtyForFallback(atlas.dirty);
            atlas.draw();
            if(!check(atlas.vbo==atlas.native && atlas.draws==draws+1 && atlas.uploads==uploads+1 && !atlas.dirty,"declined write forces exactly one native refresh"))return 1;
        }
    }

    Atlas repeated;repeated.vbo={8,8,8,8};
    chlorium::snapshot::markNativeVboDirtyForFallback(repeated.dirty);repeated.draw();
    repeated.dirty=false;repeated.vbo={7,7,7,7};
    repeated.vbo=repeated.native; // A following successful canonical write replaces the repaired buffer.
    {chlorium::snapshot::NativeVertexLease lease(repeated.buffer,repeated.dirty,repeated.buffer);repeated.draw();}
    repeated.dirty=false;
    if(!check(repeated.vbo==repeated.native && repeated.draws==2 && repeated.uploads==1 && !repeated.dirty,"failure followed by success remains clean"))return 1;
    std::cout<<"PASS clean/dirty success, pre/post-write fallback, repeated failure then success\n";
}
