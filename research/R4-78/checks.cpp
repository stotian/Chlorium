#include "../R4-33/GpuFixture.hpp"
#define CHLORIUM_PRIVATE_CANONICAL_DRAW_CHECKS 1
#include "CanonicalWrite.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
#include "../../src/renderer/NativeVertexLease.hpp"
namespace cd=chlorium::research::canonicaldraw;
struct Texture {void** table;GLuint name;unsigned calls=0;static GLuint get(void* self){auto& t=*static_cast<Texture*>(self);++t.calls;SetLastError(93);return t.name;}};
struct Atlas {unsigned char base[0x40]{};GLuint vertex=0,index=0;bool dirty=true;unsigned char padding[15]{};Texture* texture=nullptr;NativeQuadState* quads=nullptr;};
static_assert(offsetof(Atlas,quads)==0x60 && offsetof(Atlas,texture)==0x58);
struct Bindings {
    GLint program=0,generic=0,array=0;std::array<GLint,6> names{};std::array<GLint64,6> offsets{},sizes{};
    Bindings(){glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(0x90d3,&generic);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);for(unsigned i=0;i<6;++i){glGetIntegeri_v(0x90d3,i,&names[i]);if(names[i]){glGetInteger64i_v(0x90d4,i,&offsets[i]);glGetInteger64i_v(0x90d5,i,&sizes[i]);}}}
    bool operator==(const Bindings&)const=default;
};
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==5,"Cocos native.vert native.frag canonical_scatter.comp required");
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);const auto cocos=loadNative(argv[1],input);struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};
    require(chlorium::research::handoff::nativeAtlasBody(cocos),"pinned actual native atlas body");auto native=reinterpret_cast<void(*)(void*,unsigned,unsigned)>(GetProcAddress(cocos,"?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));auto invalidate=reinterpret_cast<void(*)()>(GetProcAddress(cocos,"?ccGLInvalidateStateCache@cocos2d@@YAXXZ"));require(native && invalidate,"native draw/cache exports");
    const auto shader=program(argv[2],GL_VERTEX_SHADER,false,argv[3]);glBindAttribLocation(shader,0,"a_position");glBindAttribLocation(shader,1,"a_color");glBindAttribLocation(shader,2,"a_texCoord");glLinkProgram(shader);GLint linked=0;glGetProgramiv(shader,GL_LINK_STATUS,&linked);require(linked,"native shader link");glUseProgram(shader);
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(shader,"CC_MVPMatrix"),1,GL_FALSE,identity);glUniform1i(glGetUniformLocation(shader,"CC_Texture0"),0);glUniform4f(glGetUniformLocation(shader,"native_effect"),1,1,1,1);
    GLuint textures[2]{},fbo=0,buffers[4]{},vao=0;glGenTextures(2,textures);glActiveTexture(GL_TEXTURE0);for(auto t:textures){glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    unsigned white=~0u;glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"native comparison FBO");
    std::array<cd::State,2> states{};std::array<cd::Row,2> records{};std::array<NativeQuadState,2> quads{};std::array<const cd::Row*,2> rows{&records[0],&records[1]};const std::array<unsigned,2> slots{0,1};
    for(unsigned i=0;i<2;++i){auto& r=records[i];r.root=states[i];r.atlas=r.batch=1;r.controls=0x1f80;r.source.flags=1536;r.source.atlasIndex=i;r.source.matrix[0]=r.source.matrix[3]=0x3f800000;r.source.matrix[4]=std::bit_cast<unsigned>(float(i)*.125f);
        r.source.localRect[0]=r.source.localRect[1]=std::bit_cast<unsigned>(-.8f);r.source.localRect[2]=r.source.localRect[3]=std::bit_cast<unsigned>(1.6f);r.quad.corners={{{0,0,0,0x804080ff,0,1},{0,0,0,0x804080ff,0,0},{0,0,0,0x804080ff,1,1},{0,0,0,0x804080ff,1,0}}};require(chlorium::research::corners::materialize(r.source,r.quad),"qualified native geometry");r.source.quadMatches=1;quads[i]=r.quad;}
    glGenBuffers(4,buffers);glGenVertexArrays(1,&vao);glBindVertexArray(vao);const unsigned short indices[]{0,1,2,3,2,1,4,5,6,7,6,5};glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    std::vector<unsigned char> initial(sizeof(quads)+128,0x37);std::memcpy(initial.data(),quads.data(),sizeof(quads));glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferData(GL_ARRAY_BUFFER,initial.size(),initial.data(),GL_DYNAMIC_DRAW);glBindBuffer(cd::storage,buffers[2]);glBufferData(cd::storage,sizeof(states),states.data(),GL_DYNAMIC_DRAW);
    GLint alignment=0;glGetIntegerv(0x90df,&alignment);require(alignment>0,"SSBO alignment");glBindBuffer(cd::storage,buffers[3]);glBufferData(cd::storage,std::size_t(alignment)*6+128,nullptr,GL_DYNAMIC_DRAW);for(unsigned i=0;i<6;++i)glBindBufferRange(cd::storage,i,buffers[3],std::size_t(alignment)*i,128);
    std::array<void*,13> table{};table[12]=reinterpret_cast<void*>(&Texture::get);Texture texture{table.data(),textures[0]};Atlas atlas;atlas.vertex=buffers[0];atlas.index=buffers[1];atlas.texture=&texture;atlas.quads=quads.data();cd::CanonicalWrite writer(argv[4]);
    auto prepare=[&]{invalidate();glUseProgram(shader);glBindVertexArray(vao);glViewport(0,0,64,64);glDisable(GL_DITHER);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);atlas.dirty=true;};
    auto pixels=[] {std::vector<unsigned char> result(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,result.data());return result;};
    unsigned tested=0,fallBacks=0;for(unsigned trial=0;trial<128;++trial){for(unsigned i=0;i<2;++i){for(auto& v:records[i].quad.corners)v.color=(trial*0x152b41u+i*0x91717u)|0x80000000u;quads[i]=records[i].quad;}
        prepare();native(&atlas,2,0);const auto expectedError=GetLastError();const auto expectedFp=_mm_getcsr();const auto expected=pixels();require(std::any_of(expected.begin(),expected.end(),[](auto x){return x!=0;}),"nonempty native image");
        prepare();const Bindings before;const auto getterCalls=texture.calls;SetLastError(71);const auto fp=_mm_getcsr();writer.denyDispatch=trial%16==0;
        const bool committed=writer.prepare(atlas.vertex,buffers[2],quads,states,rows,slots);require(Bindings()==before && GetLastError()==71 && _mm_getcsr()==fp,"program and all six SSBO ranges, including3, generic binding and host state restored");
        require(committed!=writer.denyDispatch,"poison rejects missing dispatch before native draw");
        if(committed){chlorium::snapshot::NativeVertexLease lease(atlas.vertex,atlas.dirty,atlas.vertex);native(&atlas,2,0);}else{native(&atlas,2,0);++fallBacks;}
        require(texture.calls==getterCalls+1 && GetLastError()==expectedError && _mm_getcsr()==expectedFp,"native getter/draw exactly once and native host state retained");require(pixels()==expected,"whole canonical/native image including native fallback");++tested;
    }
    writer.denyDispatch=false;prepare();glEnable(0xffffffffu);require(writer.prepare(atlas.vertex,buffers[2],quads,states,rows,slots),"pending native GL error does not fake a failed GPU write");require(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR,"pending native error retained without extra errors");
    std::vector<unsigned char> tail(128);glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glGetBufferSubData(GL_ARRAY_BUFFER,sizeof(quads),tail.size(),tail.data());require(std::all_of(tail.begin(),tail.end(),[](auto x){return x==0x37;}),"canonical bytes outside logical quad range preserved");
    const auto dc=wglGetCurrentDC();const auto context=wglGetCurrentContext();require(wglMakeCurrent(nullptr,nullptr),"detach owned context");require(!writer.prepare(atlas.vertex,buffers[2],quads,states,rows,slots),"contextless path declines before GL dispatch");require(wglMakeCurrent(dc,context),"restore owned context");
    require(writer.committed==121 && writer.quads==242 && writer.failures==8 && fallBacks==8,"positive GPU writes and forced-failure witnesses");
    std::cout<<"PASS "<<tested<<" complete native Cocos images, 8 poisoned-dispatch native fallbacks, six SSBO range bindings including3, pending GL errors, canonical tails and no-context guard; no GD launch\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
