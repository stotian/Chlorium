#define CHLORIUM_R62_FIXTURE_ONLY 1
#include "../R4-62/gpu_checks.cpp"
#define CHLORIUM_PRIVATE_DIRECT_PAGE_CHECKS 1
#include "DirectAtlas.hpp"
#include "../../src/renderer/GLState.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
namespace dv=chlorium::research::directvertices;
struct TextureFixture {
    void** table;GLuint name;unsigned calls=0;
    static GLuint getName(void* self){auto& t=*static_cast<TextureFixture*>(self);++t.calls;return t.name;}
};
struct AtlasFixture {
    unsigned char base[0x40]{};GLuint vertex=0,index=0;bool dirty=true;unsigned char padding[15]{};
    TextureFixture* texture=nullptr;NativeQuadState* quads=nullptr;
};
static_assert(offsetof(AtlasFixture,texture)==0x58 && offsetof(AtlasFixture,quads)==0x60);
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4,"Cocos DLL native.vert native.frag");
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);
    const auto cocos=loadNative(argv[1],input);struct Module{HMODULE h;~Module(){FreeLibrary(h);}}module{cocos};
    require(chlorium::research::handoff::nativeAtlasBody(cocos),"pinned actual native atlas body");
    auto native=reinterpret_cast<void(*)(void*,unsigned,unsigned)>(GetProcAddress(cocos,"?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
    auto invalidate=reinterpret_cast<void(*)()>(GetProcAddress(cocos,"?ccGLInvalidateStateCache@cocos2d@@YAXXZ"));require(native && invalidate,"native draw/cache exports");
    const auto shader=program(argv[2],GL_VERTEX_SHADER,false,argv[3]);
    glBindAttribLocation(shader,0,"a_position");glBindAttribLocation(shader,1,"a_color");glBindAttribLocation(shader,2,"a_texCoord");glLinkProgram(shader);
    GLint linked=0;glGetProgramiv(shader,GL_LINK_STATUS,&linked);require(linked!=0,"native graphics program");glUseProgram(shader);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(shader,"CC_MVPMatrix"),1,GL_FALSE,identity);
    glUniform1i(glGetUniformLocation(shader,"CC_Texture0"),0);
    GLuint textures[2]{},fbo=0,buffers[2]{},vao=0;glGenTextures(2,textures);glActiveTexture(GL_TEXTURE0);
    for(auto texture:textures){glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    unsigned white=0xffffffff;glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);
    glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned framebuffer");
    std::array<NativeQuadState,2> quads{};
    for(auto& q:quads)q.corners={{{-.8f,.8f,0,0x804080ff,0,1},{-.8f,-.8f,0,0x804080ff,0,0},{.8f,.8f,0,0x804080ff,1,1},{.8f,-.8f,0,0x804080ff,1,0}}};
    const unsigned short indices[]{0,1,2,3,2,1,4,5,6,7,6,5};glGenBuffers(2,buffers);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferData(GL_ARRAY_BUFFER,sizeof(quads),quads.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    std::array<void*,13> textureTable{};textureTable[12]=reinterpret_cast<void*>(&TextureFixture::getName);TextureFixture texture{textureTable.data(),textures[0]};
    AtlasFixture atlas;atlas.vertex=buffers[0];atlas.index=buffers[1];atlas.texture=&texture;atlas.quads=quads.data();
    auto prepare=[&]{invalidate();glUseProgram(shader);glBindVertexArray(vao);glViewport(0,0,64,64);glDisable(GL_DITHER);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);};
    auto pixels=[] {std::vector<unsigned char> result(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,result.data());return result;};
    dv::DirectAtlas adapter;unsigned checked=0,random=0xa37046c1;
    for(unsigned trial=0;trial<256;++trial){
        random^=random<<13;random^=random>>17;random^=random<<5;
        for(unsigned q=0;q<2;++q)for(auto& v:quads[q].corners)v.color=(random^(q*0x345678))|0x40000000u;
        const auto first=trial%2,count=trial%3?1u:2u-first;
        atlas.dirty=true;prepare();native(&atlas,count,first);const auto expected=pixels();const auto state=GLStateSnapshot::capture();
        require(std::any_of(expected.begin(),expected.end(),[](unsigned char x){return x!=0;}),"reference draws nonempty alpha pixels");
        atlas.dirty=true;prepare();const auto getterCalls=texture.calls;unsigned nativeCalls=0;SetLastError(71);const auto fp=_mm_getcsr();
        require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),count,first,2,2,[&]{++nativeCalls;native(&atlas,count,first);SetLastError(93);}),"native draw accepts direct coherent page");
        require(GetLastError()==93 && _mm_getcsr()==fp,"added work preserves native post-call host state");
        require(nativeCalls==1 && texture.calls==getterCalls+1,"actual native body/getter execute exactly once");
        require(atlas.vertex==buffers[0] && atlas.dirty,"canonical ownership and recovery dirt restored");
        const auto after=GLStateSnapshot::capture();require(after.program==state.program && after.vao==state.vao && after.arrayBuffer==state.arrayBuffer && after.elementBuffer==state.elementBuffer && after.activeTexture==state.activeTexture,"actual native post-draw GL bindings preserved");
        for(unsigned i=0;i<3;++i){GLint buffer=0;glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);require(GLuint(buffer)==buffers[0],"native VAO pointer ownership restored after page draw");}
        require(pixels()==expected,"full image equals actual native atlas including alpha and subranges");++checked;
    }
    unsigned unexpected=0;atlas.dirty=false;require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,0,2,2,[&]{++unexpected;}),"clean GPU-authoritative draw declines before callback");
    atlas.dirty=true;require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,1,2,2,[&]{++unexpected;}),"invalid subrange declines");require(!unexpected,"declined native callback remains caller-owned");
    prepare();atlas.dirty=true;glEnable(0xffffffffu);
    require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,0,2,2,[&]{native(&atlas,1,0);}),"pending error does not prevent safe native draw");
    require(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR,"added path preserves preexisting native GL error");
    glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);const auto mapped=glMapBufferRange(GL_ARRAY_BUFFER,0,sizeof(quads),GL_MAP_READ_BIT);
    require(mapped && !adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,0,2,2,[&]{++unexpected;}),"mapped native store declines before leasing");glUnmapBuffer(GL_ARRAY_BUFFER);
    bool unwound=false;try{adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,0,2,2,[&]{throw std::runtime_error("owned unwind witness");});}catch(const std::runtime_error&){unwound=true;}
    require(unwound && atlas.vertex==buffers[0] && atlas.dirty,"unwinding fences owned memory and restores native ownership");
    dv::DirectVertexPages pages;const auto source=std::as_bytes(std::span(quads));
    for(unsigned i=0;i<16;++i){const auto id=pages.acquire(source);require(id!=0,"bounded distinct pages acquired");require(!pages.acquire(source),"nested acquire cannot overwrite leased page");require(pages.finish(),"draw completion fences page");}
    pages.denyReuse=true;const auto uploads=pages.metrics.uploads;require(!pages.acquire(source) && pages.metrics.busy==1 && pages.metrics.uploads==uploads,"all busy pages decline without host wait or mutation");
    pages.denyReuse=false;glFinish();require(pages.acquire(source)!=0 && pages.finish(),"completed pages wrap safely");
    require(pages.metrics.allocated==dv::DirectVertexPages::maxBytes,"memory bounded to 24 MiB per renderer");
    const auto dc=wglGetCurrentDC();const auto context=wglGetCurrentContext();require(wglMakeCurrent(nullptr,nullptr)!=0,"detach fixture context");require(!pages.acquire(source),"absent context rejects before GL");require(wglMakeCurrent(dc,context)!=0,"restore fixture context");
    require(!adapter.fenceFailures && glGetError()==GL_NO_ERROR,"native direct stream has no fence or GL errors");
    std::cout<<"R65 actual native atlas: images="<<checked<<" native_callbacks="<<adapter.metrics().uploads<<" allocated="<<adapter.metrics().allocated<<" no duplicate draw, compute shader, framebuffer backup or GPU copy; NOT GD frametime\n";
    glDeleteBuffers(2,buffers);glDeleteVertexArrays(1,&vao);glDeleteTextures(2,textures);glDeleteFramebuffers(1,&fbo);glDeleteProgram(shader);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
