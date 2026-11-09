#define CHLORIUM_R62_FIXTURE_ONLY 1
#include "../R4-62/gpu_checks.cpp"
#define CHLORIUM_PRIVATE_DIRECT_PAGE_CHECKS 1
#include "DirectAtlas.hpp"
#include "../../src/renderer/GLState.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
namespace dv=chlorium::research::directvertices;
static PFNGLISBUFFERPROC nativeIsBuffer=nullptr;
static unsigned bufferQueries=0;
static GLboolean APIENTRY observeIsBuffer(GLuint name){++bufferQueries;return nativeIsBuffer(name);}
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
static PFNGLBUFFERSUBDATAPROC uploadNative=nullptr;
static PFNGLCOPYBUFFERSUBDATAPROC copyNative=nullptr;
static unsigned cpuUploads=0,gpuCopies=0;
static void APIENTRY observeUpload(GLenum target,GLintptr offset,GLsizeiptr size,const void* data){++cpuUploads;uploadNative(target,offset,size,data);}
static void APIENTRY observeCopy(GLenum read,GLenum write,GLintptr from,GLintptr to,GLsizeiptr size){++gpuCopies;copyNative(read,write,from,to,size);}
#endif
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
        const auto first=0u,count=2u;
        atlas.dirty=true;prepare();native(&atlas,count,first);const auto expected=pixels();const auto state=GLStateSnapshot::capture();
        require(std::any_of(expected.begin(),expected.end(),[](unsigned char x){return x!=0;}),"reference draws nonempty alpha pixels");
        atlas.dirty=true;prepare();const auto getterCalls=texture.calls;unsigned nativeCalls=0;SetLastError(71);const auto fp=_mm_getcsr();
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
        uploadNative=glBufferSubData;copyNative=glCopyBufferSubData;cpuUploads=gpuCopies=0;glBufferSubData=observeUpload;glCopyBufferSubData=observeCopy;
        glBindBuffer(GL_COPY_READ_BUFFER,buffers[1]);glBindBuffer(GL_COPY_WRITE_BUFFER,buffers[1]);
#endif
        require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),count,first,2,2,[&]{
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
            require(atlas.vertex==buffers[0],"R73 native draw retains canonical buffer identity");
#endif
            ++nativeCalls;native(&atlas,count,first);SetLastError(93);}),"native draw accepts direct coherent page");
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
        glBufferSubData=uploadNative;glCopyBufferSubData=copyNative;require(!cpuUploads && gpuCopies==1,"one GPU copy and no duplicate CPU upload");
        GLint read=0,write=0;glGetIntegerv(GL_COPY_READ_BUFFER,&read);glGetIntegerv(GL_COPY_WRITE_BUFFER,&write);require(GLuint(read)==buffers[1] && GLuint(write)==buffers[1],"foreign copy-buffer bindings restored");
#endif
        require(GetLastError()==93 && _mm_getcsr()==fp,"added work preserves native post-call host state");
        require(nativeCalls==1 && texture.calls==getterCalls+1,"actual native body/getter execute exactly once");
        require(atlas.vertex==buffers[0] && !atlas.dirty,"canonical ownership and native full-upload dirty state restored");
        const auto after=GLStateSnapshot::capture();require(after.program==state.program && after.vao==state.vao && after.arrayBuffer==state.arrayBuffer && after.elementBuffer==state.elementBuffer && after.activeTexture==state.activeTexture,"actual native post-draw GL bindings preserved");
        for(unsigned i=0;i<3;++i){GLint buffer=0;glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);require(GLuint(buffer)==buffers[0],"native VAO pointer ownership restored after page draw");}
        require(pixels()==expected,"full image equals actual native atlas including alpha");++checked;
    }
    unsigned unexpected=0;atlas.dirty=false;require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,0,2,2,[&]{++unexpected;}),"clean GPU-authoritative draw declines before callback");
    atlas.dirty=true;require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,1,2,2,[&]{++unexpected;}),"invalid subrange declines");require(!unexpected,"declined native callback remains caller-owned");
    require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),1,1,2,2,[&]{++unexpected;}),"valid partial range stays native to preserve authority outside its upload");
    prepare();atlas.dirty=true;glEnable(0xffffffffu);
    require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{native(&atlas,2,0);}),"pending error does not prevent safe native draw");
    require(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR,"added path preserves preexisting native GL error");
    atlas.dirty=true;glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);const auto mapped=glMapBufferRange(GL_ARRAY_BUFFER,0,sizeof(quads),GL_MAP_READ_BIT);
    require(mapped && !adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{++unexpected;}),"mapped native store declines before leasing");glUnmapBuffer(GL_ARRAY_BUFFER);
    bool unwound=false;try{adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{throw std::runtime_error("owned unwind witness");});}catch(const std::runtime_error&){unwound=true;}
    require(unwound && atlas.vertex==buffers[0] && atlas.dirty,"unwinding fences owned memory and restores native ownership");
    const auto nativeContext=wglGetCurrentContext();const auto nativeDC=wglGetCurrentDC();
    const auto foreign=wglCreateContext(nativeDC);require(foreign && wglMakeCurrent(nativeDC,foreign),"create unrelated context");
    nativeIsBuffer=glIsBuffer;glIsBuffer=observeIsBuffer;bufferQueries=0;
    const auto foreignAccepted=adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{++unexpected;});glIsBuffer=nativeIsBuffer;
    require(wglMakeCurrent(nativeDC,nativeContext) && wglDeleteContext(foreign),"restore native context after foreign probe");
    require(!foreignAccepted && bufferQueries==0,"foreign context declines before native GLEW buffer dispatch");
    for(auto& q:quads)for(auto& vertex:q.corners)vertex.color=0xff21f4d3u;
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
    {const auto copy=glCopyBufferSubData;glCopyBufferSubData=nullptr;atlas.dirty=true;
     require(!adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{++unexpected;}) && atlas.dirty,"missing GPU copy declines before suppressing native upload");glCopyBufferSubData=copy;}
    {std::array<std::byte,96*3> storage;storage.fill(std::byte{0xa7});glBindBuffer(GL_ARRAY_BUFFER,atlas.vertex);glBufferData(GL_ARRAY_BUFFER,storage.size(),storage.data(),GL_DYNAMIC_DRAW);
     atlas.dirty=true;prepare();require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,3,[&]{native(&atlas,2,0);}),"canonical GPU copy accepts active prefix with an unused tail");
     std::array<std::byte,96> tail;glBindBuffer(GL_COPY_READ_BUFFER,atlas.vertex);glGetBufferSubData(GL_COPY_READ_BUFFER,96*2,96,tail.data());require(std::all_of(tail.begin(),tail.end(),[](auto b){return b==std::byte{0xa7};}),"unwritten native canonical tail preserved");}
#endif
    atlas.dirty=true;prepare();require(adapter.draw(atlas.vertex,atlas.dirty,quads.data(),2,0,2,2,[&]{native(&atlas,2,0);}),"accelerated full draw before native GPU consumer");
    const auto accelerated=pixels();prepare();glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,atlas.index);glDrawElements(GL_TRIANGLES,12,GL_UNSIGNED_SHORT,nullptr);
    require(pixels()==accelerated,"subsequent native GPU consumer sees the same canonical vertex bytes");
    // A native partial draw outside an accelerated batch can clear atlas-wide recovery dirt.
    auto partialRecovery=[&](bool guard){
        for(auto& vertex:quads[0].corners)vertex.color=0xffff0000u;
        atlas.dirty=true;prepare();native(&atlas,2,0);
        for(auto& vertex:quads[0].corners)vertex.color=0xff00ff00u;
        atlas.dirty=true;prepare();GLuint deferred=0;glGenBuffers(1,&deferred);glBindBuffer(GL_ARRAY_BUFFER,deferred);glBufferData(GL_ARRAY_BUFFER,sizeof(quads),quads.data(),GL_DYNAMIC_DRAW);
        {chlorium::snapshot::NativeVertexLease legacy(atlas.vertex,atlas.dirty,deferred);native(&atlas,2,0);}
        for(auto& vertex:quads[1].corners)vertex.color=0x808000ffu;
        prepare();
        if(guard){dv::PartialDirtyRecovery recovery(true,atlas.dirty,1,1,2);native(&atlas,1,1);}
        else native(&atlas,1,1);
        const auto retained=atlas.dirty;prepare();native(&atlas,2,0);const auto observed=pixels();
        atlas.dirty=true;prepare();native(&atlas,2,0);const auto expected=pixels();
        require((observed==expected)==guard && retained==guard,"outside-batch partial recovery preserves the next full native image");
        glDeleteBuffers(1,&deferred);
    };
    partialRecovery(false);partialRecovery(true);
    dv::DirectVertexPages pages;const auto source=std::as_bytes(std::span(quads));
    for(unsigned i=0;i<dv::DirectVertexPages::pageCount;++i){const auto id=pages.acquire(source);require(id!=0,"bounded distinct pages acquired");require(!pages.acquire(source),"nested acquire cannot overwrite leased page");require(pages.finish(),"draw completion fences page");}
    pages.denyReuse=true;const auto uploads=pages.metrics.uploads;require(!pages.acquire(source) && pages.metrics.busy==1 && pages.metrics.uploads==uploads,"all busy pages decline without host wait or mutation");
    pages.denyReuse=false;glFinish();require(pages.acquire(source)!=0 && pages.finish(),"completed pages wrap safely");
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
    require(pages.metrics.allocated==dv::pageStorage(source.size())*dv::DirectVertexPages::pageCount,"small pages allocate their bounded rounded capacity");
    std::vector<std::byte> bigger(16385,std::byte{0xa7});glFinish();const auto larger=pages.acquire(bigger);require(larger!=0,"completed small page grows for a later large atlas");
    require(pages.finish(),"grown page is fenced");GLint old=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);glBindBuffer(GL_COPY_READ_BUFFER,larger);
    std::vector<std::byte> actual(bigger.size());glGetBufferSubData(GL_COPY_READ_BUFFER,0,actual.size(),actual.data());glBindBuffer(GL_COPY_READ_BUFFER,old);require(actual==bigger,"grown coherent buffer holds exact native input bytes");
#else
    require(pages.metrics.allocated==dv::DirectVertexPages::maxBytes,"fixed pages allocate exactly 24 MiB");
#endif
    require(pages.metrics.allocated<=dv::DirectVertexPages::maxBytes,"memory bounded to 24 MiB per renderer");
    const auto dc=wglGetCurrentDC();const auto context=wglGetCurrentContext();require(wglMakeCurrent(nullptr,nullptr)!=0,"detach fixture context");require(!pages.acquire(source),"absent context rejects before GL");require(wglMakeCurrent(dc,context)!=0,"restore fixture context");
    require(!adapter.fenceFailures && glGetError()==GL_NO_ERROR,"native direct stream has no fence or GL errors");
    std::cout<<"Direct actual native atlas: images="<<checked<<" leases="<<adapter.metrics().uploads<<" allocated="<<adapter.metrics().allocated
#ifdef CHLORIUM_CANONICAL_GPU_COPY_R73
        <<" one canonical GPU copy, zero duplicate CPU uploads, unchanged native VBO/VAO"
#else
        <<" no duplicate draw, compute shader, framebuffer backup or GPU copy"
#endif
        <<"; NOT GD frametime\n";
    glDeleteBuffers(2,buffers);glDeleteVertexArrays(1,&vao);glDeleteTextures(2,textures);glDeleteFramebuffers(1,&fbo);glDeleteProgram(shader);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
