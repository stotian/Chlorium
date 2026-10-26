#pragma once
#include "renderer/NativeVertexLease.hpp"
#include "renderer/NativeStoreHydration.hpp"
#include "renderer/NativeGpuReplacement.hpp"
// Actual local Cocos draw oracle. A pixel-equal draw can still have wrong
// postconditions; this fixture records that known defect in the V5/V6 bridge.
struct AtlasTextureFixture {
    void** table;
    GLuint name;
    static GLuint getName(void* self) {return static_cast<AtlasTextureFixture*>(self)->name;}
};
struct AtlasMemoryFixture {
    unsigned char base[0x40]{};
    std::uint32_t vertex,index;
    bool dirty;
    unsigned char padding[15]{};
    AtlasTextureFixture* texture;
    chlorium::snapshot::NativeQuadState* quads;
};
static_assert(offsetof(AtlasMemoryFixture,texture)==0x58&&offsetof(AtlasMemoryFixture,quads)==0x60);
void nativeAtlasContractChecks() {
    const auto saved=GLStateSnapshot::capture();
    auto cocos=LoadLibraryExW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Geometry Dash\\libcocos2d.dll",nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!cocos) std::cerr<<"Cocos oracle load error "<<GetLastError()<<'\n';
    require(cocos!=nullptr,"local Cocos module available for actual atlas contract oracle");
    auto base=reinterpret_cast<const unsigned char*>(cocos);
    std::uint64_t hash=14695981039346656037ull;
    for(std::size_t n=32;n<0x15f;++n) hash=(hash^base[0xa6350+n])*1099511628211ull;
    require(hash==0xc9d0f08451f64908ull,"actual atlas body matches supported engine");
    using Invalidate=void(*)();
    auto invalidate=reinterpret_cast<Invalidate>(GetProcAddress(cocos,"?ccGLInvalidateStateCache@cocos2d@@YAXXZ"));
    require(invalidate!=nullptr,"native GL cache invalidation export");
    using Draw=void(*)(void*,unsigned,unsigned);
    auto native=reinterpret_cast<Draw>(const_cast<unsigned char*>(base+0xa6350));
    auto compile=[](GLenum type,const char* source) {
        auto shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);return shader;
    };
    const auto program=link({compile(GL_VERTEX_SHADER,R"(#version 330 core
layout(location=0) in vec3 position;layout(location=1) in vec4 color;out vec4 c;
void main(){gl_Position=vec4(position,1);c=color;})"),compile(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec4 c;out vec4 fragment;void main(){fragment=c;})")});
    chlorium::snapshot::NativeQuadState quad;
    quad.corners={{{-.8f,.8f,0,0xff4080ff,0,1},{-.8f,-.8f,0,0xff4080ff,0,0},
                   {.8f,.8f,0,0xff4080ff,1,1},{.8f,-.8f,0,0xff4080ff,1,0}}};
    std::array<chlorium::snapshot::NativeQuadState,2> quads{quad,quad};
    const std::uint16_t indices[]{0,1,2,3,2,1,4,5,6,7,6,5};
    GLuint buffers[3],vaos[2],texture;
    glGenBuffers(3,buffers);glGenVertexArrays(2,vaos);glGenTextures(1,&texture);
    for(unsigned n=0;n<2;++n) {glBindBuffer(GL_ARRAY_BUFFER,buffers[n]);glBufferData(GL_ARRAY_BUFFER,sizeof(quads),quads.data(),GL_DYNAMIC_DRAW);}
    glBindVertexArray(vaos[0]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[2]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    chlorium::snapshot::configureNativeVertexLayout(vaos[1],buffers[1]);
    std::array<void*,13> textureTable{};textureTable[12]=reinterpret_cast<void*>(&AtlasTextureFixture::getName);
    AtlasTextureFixture textureObject{textureTable.data(),texture};
    AtlasMemoryFixture atlas{};atlas.vertex=buffers[0];atlas.index=buffers[2];atlas.texture=&textureObject;atlas.quads=quads.data();atlas.dirty=true;
    auto prepare=[&] {
        invalidate();glUseProgram(program);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glViewport(0,0,64,64);
        glBindVertexArray(vaos[0]);glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[2]);
        glActiveTexture(GL_TEXTURE3);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    };
    auto pixel=[] {std::array<unsigned char,4> p{};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    prepare();native(&atlas,1,0);const auto expected=pixel();const auto nativeAfter=GLStateSnapshot::capture();
    require(expected[0]>0,"native atlas actually rendered visible geometry");
    require(nativeAfter.arrayBuffer==0&&nativeAfter.elementBuffer==0&&nativeAfter.activeTexture==GL_TEXTURE0,
        "actual native atlas unbinds both buffers and leaves texture unit zero");
    require(!atlas.dirty,"native atlas clears dirty after native storage update");
    prepare();
    {auto state=GLStateSnapshot::capture(GLStateScope::NativeVertex);
        chlorium::snapshot::submitNativeVertices(vaos[1],buffers[2],texture,1,0);state.restore();}
    const auto legacyAfter=GLStateSnapshot::capture();
    require(pixel()==expected,"legacy bridge current draw matches native pixel");
    require(legacyAfter.arrayBuffer!=nativeAfter.arrayBuffer&&legacyAfter.activeTexture!=nativeAfter.activeTexture,
        "known witness: legacy restore differs from actual native postconditions");
    prepare();
    {chlorium::snapshot::NativeVertexLease lease(atlas.vertex,atlas.dirty,buffers[1]);
        require(atlas.vertex==buffers[1]&&!atlas.dirty,"native lease points to populated owned storage");
        native(&atlas,1,0);}
    const auto handoffAfter=GLStateSnapshot::capture();
    require(pixel()==expected,"actual native handoff preserves current draw pixels");
    require(handoffAfter.vao==nativeAfter.vao&&handoffAfter.arrayBuffer==nativeAfter.arrayBuffer&&
        handoffAfter.elementBuffer==nativeAfter.elementBuffer&&handoffAfter.activeTexture==nativeAfter.activeTexture&&
        handoffAfter.textures==nativeAfter.textures&&handoffAfter.program==nativeAfter.program,
        "handoff leaves actual native GL postconditions, not pre-draw bindings");
    require(atlas.vertex==buffers[0]&&atlas.dirty,"lease restores atlas ownership and native-store dirty obligation");
    for(auto& v:quads[0].corners) v.color=0xff0000ffu;
    for(auto& v:quads[1].corners) v.color=0xffff0000u;
    require(hydrateNativeVertexStore(atlas.vertex,quads.data(),sizeof(quads)),"full native hydration fits current store");
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,0);
    require(pixel()[0]>250&&!atlas.dirty,"first native partial draw refreshes and clears atlas-wide dirty");
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,1);
    require(pixel()[2]>250,"later clean native subrange retains hydrated vertices");
    // A clean atlas reads its GPU store, not necessarily the CPU staging copy.
    // GPU producers can legitimately update that store without dirtying CPU
    // quads. Record the remaining V7 authority defect rather than hiding it.
    auto gpuOnly=quads[1];for(auto& v:gpuOnly.corners) v.color=0xff00ff00u;
    glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferSubData(GL_ARRAY_BUFFER,sizeof(quad),sizeof(quad),&gpuOnly);
    glBindBuffer(GL_ARRAY_BUFFER,0);atlas.dirty=false;
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,1);const auto canonical=pixel();
    require(canonical[1]>250,"clean native atlas preserves GPU-authoritative vertices");
    glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(quads),quads.data());glBindBuffer(GL_ARRAY_BUFFER,0);
    glClear(GL_COLOR_BUFFER_BIT);
    {chlorium::snapshot::NativeVertexLease lease(atlas.vertex,atlas.dirty,buffers[1]);native(&atlas,1,1);}
    require(pixel()!=canonical,"known witness: unconditional CPU streaming loses clean GPU-only data");
    // V8 protocol: preserve the canonical GPU store outside the actual dirty
    // CPU range, then permanently exchange native/private ownership.
    for(auto& v:quads[0].corners) v.color=0xff00ffffu;
    GLuint spare=buffers[1];
    const auto replacement=rebuildNativeGpuStore(atlas.vertex,spare,0,sizeof(quad),&quads[0]);
    require(replacement.bytes==sizeof(quads)&&replacement.copied==sizeof(quad),"replacement preserves GPU-authoritative tail");
    std::swap(atlas.vertex,spare);atlas.dirty=false;
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,0);const auto dirtyPixel=pixel();
    require(dirtyPixel[0]>250&&dirtyPixel[1]>250,"replacement has exact dirty CPU prefix");
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,1);
    require(pixel()==canonical&&!atlas.dirty,"clean draw retains canonical GPU-only tail after ownership exchange");
    const auto second=rebuildNativeGpuStore(atlas.vertex,spare,sizeof(quad),sizeof(quad),&quads[1]);
    require(second.bytes==sizeof(quads)&&second.copied==sizeof(quad),"second exchange preserves canonical GPU head");
    std::swap(atlas.vertex,spare);atlas.dirty=false;
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,0);require(pixel()==dirtyPixel,"GPU head remains exact after second exchange");
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,1);require(pixel()[2]>250,"second dirty CPU subrange is exact");
    require(!rebuildNativeGpuStore(atlas.vertex,atlas.vertex,0,sizeof(quad),&quads[0]).bytes,"aliased replacement rejected");
    require(!rebuildNativeGpuStore(atlas.vertex,spare,0,sizeof(quad),&quads[0],1).bytes,"replacement memory budget enforced");
    glClear(GL_COLOR_BUFFER_BIT);native(&atlas,1,1);require(pixel()[2]>250,"rejected replacement leaves native store intact");
    // Byte-level authority oracle over repeated partial replacements. Arbitrary
    // GPU-only bytes outside each dirty interval must survive every exchange.
    std::array<unsigned char,sizeof(quads)> bytes{},actual{},cpu{};
    std::uint32_t random=0x84a1762bu;
    auto next=[&] {random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(auto& x:bytes) x=static_cast<unsigned char>(next());
    glBindBuffer(GL_ARRAY_BUFFER,atlas.vertex);glBufferSubData(GL_ARRAY_BUFFER,0,bytes.size(),bytes.data());
    for(unsigned trial=0;trial<128;++trial) {
        for(auto& x:cpu) x=static_cast<unsigned char>(next());
        const auto offset=next()%bytes.size(),count=1+next()%(bytes.size()-offset);
        const auto previous=GLStateSnapshot::capture(GLStateScope::NativeVertex);
        const auto r=rebuildNativeGpuStore(atlas.vertex,spare,offset,count,cpu.data()+offset);
        require(r.bytes==bytes.size()&&r.copied==bytes.size()-count,"random GPU replacement preserves exact interval accounting");
        require(GLStateSnapshot::capture(GLStateScope::NativeVertex).arrayBuffer==previous.arrayBuffer,"GPU preparation restores caller array binding");
        std::copy_n(cpu.data()+offset,count,bytes.data()+offset);std::swap(atlas.vertex,spare);
        glBindBuffer(GL_ARRAY_BUFFER,atlas.vertex);glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size(),actual.data());
        require(actual==bytes,"random GPU authority matches full byte oracle");
    }
    glBindBuffer(GL_ARRAY_BUFFER,atlas.vertex);
    auto mapped=glMapBufferRange(GL_ARRAY_BUFFER,0,bytes.size(),GL_MAP_READ_BIT);
    require(mapped!=nullptr,"mapped authority witness available");
    require(!rebuildNativeGpuStore(atlas.vertex,spare,0,1,cpu.data()).bytes,"mapped canonical store rejected before replacement");
    glUnmapBuffer(GL_ARRAY_BUFFER);
    require(!hydrateNativeVertexStore(atlas.vertex,quads.data(),sizeof(quads)+1),"oversized hydration rejected");
    bool rejected=false;try {chlorium::snapshot::NativeVertexLease lease(atlas.vertex,atlas.dirty,0);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected&&atlas.vertex==buffers[0],"empty store cannot mutate native ownership");
    try {chlorium::snapshot::NativeVertexLease lease(atlas.vertex,atlas.dirty,buffers[1]);throw std::runtime_error("fixture");}catch(const std::runtime_error&){}
    require(atlas.vertex==buffers[0]&&atlas.dirty,"unwinding restores native ownership before fallback");
    saved.restore();glDeleteBuffers(3,buffers);glDeleteVertexArrays(2,vaos);glDeleteTextures(1,&texture);glDeleteProgram(program);
    FreeLibrary(cocos);
    require(glGetError()==GL_NO_ERROR,"native atlas contract witness has no GL errors");
    std::cout<<"REPRODUCED legacy GL/authority defects; canonical GPU replacement protocol verified\n";
}
