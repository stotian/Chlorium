#include "../R4-33/GpuFixture.hpp"
#include "AlphaMask.hpp"
#include "DepthContract.hpp"
#include "../R4-43/NativeBatchReplay.hpp"
#include "../R4-44/ZeroFragmentProgram.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
#include <chrono>
#include <numeric>
using chlorium::research::occlusion::AlphaMask;
struct Header {
    unsigned version,width,height,alphaBits,flags,cullMode,frontFace,sourceFramebuffer;
    unsigned alphaFunc,alphaRef,colorEncoding,textureFormat,colorMask,alphaTest,srgb;
    int scissor[4];unsigned count,tw,th,vertexLength,fragmentLength,matrix[16];
};
static_assert(sizeof(Header)==160);
struct Data {
    Header header{};std::string vertex,fragment;unsigned binaryFormat=0,binaryLength=0;
    std::vector<unsigned char> binary;
    std::vector<unsigned char> texture,before,after;
    std::vector<NativeQuadState> quads;
    explicit Data(const std::filesystem::path& file){
        require(std::filesystem::file_size(file)<=64*1024*1024,"native replay file budget");std::ifstream stream(file,std::ios::binary);
        auto read=[&](void* to,std::size_t size){stream.read(static_cast<char*>(to),size);require(bool(stream),"complete native replay bytes");};
        std::array<char,8> magic{};read(magic.data(),8);require(magic==std::array<char,8>{'C','H','L','P','I','X','4','1'},"native replay magic");read(&header,sizeof(header));const auto& h=header;
        require((h.version==1||h.version==2)&&h.width&&h.width<=1920&&h.height&&h.height<=1080&&h.tw&&h.tw<=4096&&h.th&&h.th<=4096&&h.count&&h.count<=16384,"native replay dimensions/count");
        if(h.version==2){read(&binaryFormat,4);read(&binaryLength,4);require(binaryLength<=4*1024*1024&&bool(binaryFormat)==bool(binaryLength),"bounded native linked binary");}
        require((h.alphaBits==0||h.alphaBits==8)&&!(h.flags&~7u)&&h.colorMask<=15&&h.alphaTest<=1&&h.srgb<=1&&h.alphaFunc>=GL_NEVER&&h.alphaFunc<=GL_ALWAYS,"native replay raster contract");
        require((h.colorEncoding==GL_LINEAR||h.colorEncoding==0x8c40)&&h.vertexLength&&h.vertexLength<=65536&&h.fragmentLength&&h.fragmentLength<=65536,"native replay encoding/shaders");
        const auto expected=8ull+sizeof(Header)+(h.version==2?8:0)+h.vertexLength+h.fragmentLength+binaryLength+std::uint64_t(h.tw)*h.th*4+std::uint64_t(h.count)*96+std::uint64_t(h.width)*h.height*8;
        require(expected==std::filesystem::file_size(file)&&expected<=64*1024*1024,"exact complete replay length before allocating images");
        require(h.scissor[2]>=0&&h.scissor[3]>=0&&(h.alphaRef&0x7fffffffu)<=0x3f800000u&&(!(h.alphaRef>>31)||!(h.alphaRef&0x7fffffffu)),"native scissor/alpha-reference bounds");
        require(h.cullMode==GL_FRONT||h.cullMode==GL_BACK||h.cullMode==GL_FRONT_AND_BACK,"native cull mode");require(h.frontFace==GL_CW||h.frontFace==GL_CCW,"native face winding");
        vertex.resize(h.vertexLength);fragment.resize(h.fragmentLength);read(vertex.data(),vertex.size());read(fragment.data(),fragment.size());
        require(hash(reinterpret_cast<const unsigned char*>(vertex.data()),vertex.size())==0xa2318967aee60148ull&&hash(reinterpret_cast<const unsigned char*>(fragment.data()),fragment.size())==0xa9c8e0b64e142972ull,"reviewed complete native shader sources");
        binary.resize(binaryLength);if(binaryLength)read(binary.data(),binary.size());
        texture.resize(std::size_t(h.tw)*h.th*4);quads.resize(h.count);before.resize(std::size_t(h.width)*h.height*4);after.resize(before.size());
        read(texture.data(),texture.size());read(quads.data(),quads.size()*sizeof(NativeQuadState));read(before.data(),before.size());read(after.data(),after.size());require(stream.peek()==std::char_traits<char>::eof(),"no trailing replay inputs");require(before!=after,"non-vacuous native batch pixels");
    }
};
GLuint textProgram(const std::string& vertex,const std::string& fragment){
    auto compile=[&](const std::string& source,GLenum stage){const auto shader=glCreateShader(stage);const char* text=source.c_str();const GLint size=static_cast<GLint>(source.size());glShaderSource(shader,1,&text,&size);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::cerr<<log<<'\n';glDeleteShader(shader);throw std::runtime_error("compile exact native replay shader");}return shader;};
    const auto vs=compile(vertex,GL_VERTEX_SHADER),fs=compile(fragment,GL_FRAGMENT_SHADER),p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);
    glBindAttribLocation(p,0,"a_position");glBindAttribLocation(p,1,"a_color");glBindAttribLocation(p,2,"a_texCoord");glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);
    if(!linked){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::cerr<<log<<'\n';glDeleteProgram(p);throw std::runtime_error("link exact native replay shader");}return p;
}
std::string replaceOne(std::string source,const std::string& before,const std::string& after){const auto position=source.find(before);require(position!=std::string::npos&&source.find(before,position+before.size())==std::string::npos,"one exact native shader expression");source.replace(position,before.size(),after);return source;}
struct Replay {
    const Data& data;AlphaMask mask;GLuint native=0,color=0,depth=0,vao=0,vbo=0,indices=0,occluders=0,pointIndices=0,sourceTexture=0;
    std::vector<unsigned short> compactPoints;unsigned removedPoints=0;
    chlorium::research::zerofragment::Program zeroProgram;
    using NativeAtlasDraw=void(*)(void*,unsigned,unsigned);
    NativeAtlasDraw nativeAtlas=nullptr;std::array<unsigned char,0x80> atlasMemory{},textureMemory{};std::array<void*,13> textureTable{};
    GLuint framebuffers[2]{},targets[2]{},depthBuffer=0;std::vector<unsigned> opaque;bool eligible=false;
    int diagnosticDither=-1;
    void bindNativeAtlas(HMODULE cocos){
        require(chlorium::research::handoff::nativeAtlasBody(cocos),"exact Cocos native atlas prefix/body");
        textureTable[12]=reinterpret_cast<void*>(GetProcAddress(cocos,"?getName@CCTexture2D@cocos2d@@UEAAIXZ"));const auto table=textureTable.data();std::memcpy(textureMemory.data(),&table,8);std::memcpy(textureMemory.data()+0x48,&sourceTexture,4);
        require(chlorium::research::handoff::standardTexture(cocos,textureMemory.data()),"native integer-only texture getter");
        const auto texture=textureMemory.data();const auto quads=data.quads.data();std::memcpy(atlasMemory.data()+0x58,&texture,8);std::memcpy(atlasMemory.data()+0x60,&quads,8);std::memcpy(atlasMemory.data()+0x40,&vbo,4);std::memcpy(atlasMemory.data()+0x44,&indices,4);
        nativeAtlas=reinterpret_cast<NativeAtlasDraw>(GetProcAddress(cocos,"?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
    }
    Replay(const Data& input,bool zeroVariant=false):data(input),mask(input.header.tw,input.header.th,input.texture){const auto& h=data.header;
        native=textProgram(data.vertex,data.fragment);
        if(!data.binary.empty()){require(GLEW_ARB_get_program_binary&&glProgramBinary,"native linked binary supported");glProgramBinary(native,data.binaryFormat,data.binary.data(),static_cast<GLsizei>(data.binary.size()));GLint linked=0;glGetProgramiv(native,GL_LINK_STATUS,&linked);require(linked==GL_TRUE,"actual native program binary reload");}
        if(zeroVariant){require(!h.alphaTest&&h.colorMask==15,"zero fragment native raster contract");require(zeroProgram.prepare(native),zeroProgram.gate);}
        const auto vertex="#version 130\ninvariant gl_Position;\nuniform float chloriumQuadCount;\n"+replaceOne(data.vertex,"gl_Position = CC_MVPMatrix * a_position;","gl_Position = CC_MVPMatrix * a_position;\nfloat chloriumOrdinal=float(gl_VertexID/4);\nfloat chloriumDepth=1.0-(chloriumOrdinal+1.0)/(chloriumQuadCount+1.0);\ngl_Position.z=(chloriumDepth*2.0-1.0)*gl_Position.w;");
        color=textProgram(vertex,"#version 130\n"+data.fragment);
        const auto fragment="#version 130\n"+replaceOne(data.fragment,"gl_FragColor = v_fragmentColor * texture2D(CC_Texture0, v_texCoord);","vec4 chloriumSample=v_fragmentColor * texture2D(CC_Texture0, v_texCoord);\nif(chloriumSample.a!=1.0)discard;\ngl_FragColor=chloriumSample;");depth=textProgram(vertex,fragment);
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,data.quads.size()*96,data.quads.data(),GL_STREAM_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));glEnableVertexAttribArray(2);
        constexpr unsigned triangle[]{0,1,2,3,2,1};std::vector<unsigned short> order(h.count*6);for(unsigned i=0;i<h.count;++i)for(unsigned j=0;j<6;++j)order[i*6+j]=static_cast<unsigned short>(i*4+triangle[j]);
        glGenBuffers(1,&indices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,order.size()*2,order.data(),GL_STATIC_DRAW);glGenBuffers(1,&occluders);
        compactPoints.reserve(order.size());glGenBuffers(1,&pointIndices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pointIndices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,order.size()*2,nullptr,GL_STREAM_DRAW);
        for(unsigned i=h.count;i-->0;)if(mask.maybeOpaqueFootprint(data.quads[i]))for(unsigned corner:triangle)opaque.push_back(i*4+corner);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,occluders);glBufferData(GL_ELEMENT_ARRAY_BUFFER,opaque.size()*4,opaque.data(),GL_STATIC_DRAW);
        std::array<float,16> matrix{};std::memcpy(matrix.data(),h.matrix,sizeof(matrix));eligible=!opaque.empty()&&chlorium::research::occlusion::depthContract(data.quads,matrix);
        glGenTextures(1,&sourceTexture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sourceTexture);
        require(h.textureFormat==GL_RGBA||h.textureFormat==GL_RGB||h.textureFormat==GL_RGBA8||h.textureFormat==GL_RGB8||h.textureFormat==GL_RGBA4||h.textureFormat==GL_RGB5_A1||h.textureFormat==0x8c41||h.textureFormat==0x8c43,"uncompressed exact native texture format");
        glTexImage2D(GL_TEXTURE_2D,0,h.textureFormat,h.tw,h.th,0,GL_RGBA,GL_UNSIGNED_BYTE,data.texture.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(2,framebuffers);glGenTextures(2,targets);glGenRenderbuffers(1,&depthBuffer);glBindRenderbuffer(GL_RENDERBUFFER,depthBuffer);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,h.width,h.height);
        const GLenum format=h.colorEncoding==0x8c40?(h.alphaBits?0x8c43:0x8c41):(h.alphaBits?GL_RGBA8:GL_RGB8);
        for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,targets[i]);glTexImage2D(GL_TEXTURE_2D,0,format,h.width,h.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,targets[i],0);if(i)glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthBuffer);glReadBuffer(GL_COLOR_ATTACHMENT0);glDrawBuffer(GL_COLOR_ATTACHMENT0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"exact native-format replay framebuffer");}
        glViewport(0,0,h.width,h.height);glDepthRange(0,1);glDisable(GL_STENCIL_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);glDisable(GL_POLYGON_OFFSET_FILL);
        require(glGetError()==GL_NO_ERROR,"native replay setup");
    }
    ~Replay(){glDeleteProgram(native);glDeleteProgram(color);glDeleteProgram(depth);glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&indices);glDeleteBuffers(1,&occluders);glDeleteBuffers(1,&pointIndices);glDeleteTextures(1,&sourceTexture);glDeleteTextures(2,targets);glDeleteFramebuffers(2,framebuffers);glDeleteRenderbuffers(1,&depthBuffer);}
    void diagnosticFormat(bool linear){const auto& h=data.header;const GLenum format=linear?(h.alphaBits?GL_RGBA8:GL_RGB8):(h.alphaBits?0x8c43:0x8c41);
        for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,targets[i]);glTexImage2D(GL_TEXTURE_2D,0,format,h.width,h.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[i]);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"diagnostic target format");}glBindTexture(GL_TEXTURE_2D,sourceTexture);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[0]);}
    void raster(){const auto& h=data.header;
        if(diagnosticDither>=0?diagnosticDither:bool(h.flags&1))glEnable(GL_DITHER);else glDisable(GL_DITHER);if(h.flags&2)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glCullFace(h.cullMode);glFrontFace(h.frontFace);
        if(h.flags&4){glEnable(GL_SCISSOR_TEST);glScissor(h.scissor[0],h.scissor[1],h.scissor[2],h.scissor[3]);}else glDisable(GL_SCISSOR_TEST);
        if(h.alphaTest)glEnable(GL_ALPHA_TEST);else glDisable(GL_ALPHA_TEST);glAlphaFunc(h.alphaFunc,std::bit_cast<float>(h.alphaRef));
        if(h.srgb)glEnable(0x8db9);else glDisable(0x8db9);
    }
    void colorMask(){const auto channels=data.header.colorMask;glColorMask(channels&1,(channels>>1)&1,(channels>>2)&1,(channels>>3)&1);}
    void uniforms(GLuint p){glUseProgram(p);std::array<float,16> matrix{};std::memcpy(matrix.data(),data.header.matrix,sizeof(matrix));const auto m=glGetUniformLocation(p,"CC_MVPMatrix"),texture=glGetUniformLocation(p,"CC_Texture0");require(m>=0&&texture>=0,"exact native MVP/sampler uniforms");glUniformMatrix4fv(m,1,GL_FALSE,matrix.data());glUniform1i(texture,0);if(p!=native){const auto count=glGetUniformLocation(p,"chloriumQuadCount");require(count>=0,"native-order depth uniform");glUniform1f(count,float(data.header.count));}}
    void reset(){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,targets[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,data.header.width,data.header.height,GL_RGBA,GL_UNSIGNED_BYTE,data.before.data());glBindTexture(GL_TEXTURE_2D,sourceTexture);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[0]);glBindVertexArray(vao);raster();glDepthMask(GL_TRUE);glDisable(GL_DEPTH_TEST);colorMask();require(glGetError()==GL_NO_ERROR,"native background reset");}
    void draw(bool custom,bool points=false,bool discardZero=false){const auto& h=data.header;const bool optimized=custom&&eligible;
        if(points){compactPoints.clear();removedPoints=0;constexpr unsigned topology[]{0,1,2,3,2,1};for(unsigned i=0;i<h.count;++i){if(chlorium::research::batchreplay::pointQuad(data.quads[i])){++removedPoints;continue;}for(unsigned corner:topology)compactPoints.push_back(static_cast<unsigned short>(i*4+corner));}glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pointIndices);if(!compactPoints.empty())glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,compactPoints.size()*2,compactPoints.data());}
        if(optimized){
            glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffers[0]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffers[1]);glBlitFramebuffer(0,0,h.width,h.height,0,0,h.width,h.height,GL_COLOR_BUFFER_BIT,GL_NEAREST);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[1]);
            glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDisable(GL_BLEND);uniforms(depth);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,occluders);glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(opaque.size()),GL_UNSIGNED_INT,nullptr);glDepthMask(GL_FALSE);
        }
        colorMask();glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);uniforms(optimized?color:native);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,points?pointIndices:indices);
        if(nativeAtlas){atlasMemory[0x48]=1;auto call=[&]{nativeAtlas(atlasMemory.data(),h.count,0);};if(discardZero)require(zeroProgram.withNativeDraw(call),zeroProgram.gate);else call();require(atlasMemory[0x48]==0,"native dirty upload retained");}
        else if(discardZero)require(zeroProgram.draw(indices,h.count,0),zeroProgram.gate);else glDrawElements(GL_TRIANGLES,points?static_cast<GLsizei>(compactPoints.size()):h.count*6,GL_UNSIGNED_SHORT,nullptr);
        if(optimized){glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffers[1]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffers[0]);glBlitFramebuffer(0,0,h.width,h.height,0,0,h.width,h.height,GL_COLOR_BUFFER_BIT,GL_NEAREST);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[0]);}
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);require(glGetError()==GL_NO_ERROR,"native replay/color-copy draw");
    }
    std::vector<unsigned char> read(){std::vector<unsigned char> result(data.before.size());glReadPixels(0,0,data.header.width,data.header.height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());require(glGetError()==GL_NO_ERROR,"native replay RGBA8 output");return result;}
};
std::size_t different(std::span<const unsigned char> a,std::span<const unsigned char> b){require(a.size()==b.size(),"equal pixel output sizes");std::size_t n=0;for(unsigned i=0;i<a.size();++i)n+=a[i]!=b[i];return n;}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc>=3&&argc<=5,"native pair dataset, NEW receipt, timing variant and optional Cocos");const bool handoff=argc==5&&std::wstring_view(argv[3])==L"--handoff-timings";require(argc!=5||handoff,"Cocos argument requires the explicit native handoff mode");const bool points=argc==4&&std::wstring_view(argv[3])==L"--point-timings",zeroVariant=handoff||(argc==4&&std::wstring_view(argv[3])==L"--zero-timings");if(argc>=4)require(handoff||points||zeroVariant||std::wstring_view(argv[3])==L"--timings","explicit native-input replay timings");
    Data data{std::filesystem::path(argv[1])};chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);Replay replay(data,zeroVariant);
    if(handoff){const auto cocos=loadNative(argv[4],input);replay.bindNativeAtlas(cocos);replay.reset();replay.uniforms(replay.native);unsigned calls=0;require(replay.zeroProgram.withNativeDraw([&]{++calls;}),"native callback entered");require(calls==1,"native callback called exactly once");bool caught=false;try{replay.zeroProgram.withNativeDraw([]{throw std::runtime_error("owned callback failure");});}catch(const std::runtime_error&){caught=true;}GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);require(caught&&unsigned(current)==replay.native,"native program restored after callback exception");}
    if(handoff){
        const auto savedFP=_mm_getcsr();const auto savedError=GetLastError();
        _mm_setcsr(0x9fe1);SetLastError(0x4c460001);unsigned invoked=0;
        require(replay.zeroProgram.withNativeDraw([&]{require(_mm_getcsr()==0x9fe1&&GetLastError()==0x4c460001,"introduced preparation preserves incoming CPU state");++invoked;_mm_setcsr(0x3fa2);SetLastError(0x4c460002);}),"owned native state callback");
        require(invoked==1&&_mm_getcsr()==0x3fa2&&GetLastError()==0x4c460002,"native CPU side effects survive shader restoration");
        struct OwnedFailure{};auto throwing=[](){_mm_setcsr(0x1fa1);SetLastError(0x4c460003);throw OwnedFailure{};};
        try{throwing();}catch(const OwnedFailure&){}const auto exceptionFP=_mm_getcsr();const auto exceptionError=GetLastError();
        bool caught=false;try{replay.zeroProgram.withNativeDraw(throwing);}catch(const OwnedFailure&){caught=true;}require(caught&&_mm_getcsr()==exceptionFP&&GetLastError()==exceptionError,"throwing callback CPU state matches direct native throw");
        _mm_setcsr(savedFP);SetLastError(savedError);const auto sampler=glGetUniformLocation(replay.native,"CC_Texture0");glUniform1i(sampler,1);
        invoked=0;_mm_setcsr(0x9fe1);SetLastError(0x4c460004);require(!replay.zeroProgram.withNativeDraw([&]{++invoked;}),"foreign sampler rejected");
        require(!invoked&&_mm_getcsr()==0x9fe1&&GetLastError()==0x4c460004,"native fallback leaves CPU state and callback untouched");_mm_setcsr(savedFP);SetLastError(savedError);glUniform1i(sampler,0);
        {chlorium::research::zerofragment::Program empty;invoked=0;_mm_setcsr(0x9fe1);SetLastError(0x4c460005);require(!empty.withNativeDraw([&]{++invoked;})&&!invoked&&_mm_getcsr()==0x9fe1&&GetLastError()==0x4c460005,"unprepared native fallback preserves CPU state");}_mm_setcsr(savedFP);SetLastError(savedError);
        const auto getter=replay.textureTable[12];replay.textureTable[12]=nullptr;require(!chlorium::research::handoff::standardTexture(GetModuleHandleW(L"libcocos2d.dll"),replay.textureMemory.data()),"foreign texture virtual callback rejects before handoff");replay.textureTable[12]=getter;
        require(glGetError()==GL_NO_ERROR,"owned native handoff state controls");
    }
    replay.reset();const auto backgroundMismatch=different(replay.read(),data.before);replay.draw(false);const auto baseline=replay.read();const auto nativeMismatch=different(baseline,data.after);
    replay.reset();replay.draw(!points&&!zeroVariant,points,zeroVariant);const auto candidate=replay.read();const auto replayMismatch=different(candidate,baseline),originalMismatch=different(candidate,data.after);
    std::ofstream receipt(std::filesystem::path(argv[2]),std::ios::out|std::ios::noreplace);require(bool(receipt),"new native replay receipt");
    receipt<<"{\"native_atlas_handoff\":"<<(handoff?"true":"false")<<",\"zero_fragment_discard\":"<<(zeroVariant?"true":"false")<<",\"point_compaction\":"<<(points?"true":"false")<<",\"removed_point_quads\":"<<replay.removedPoints<<",\"diagnostic_only\":true,\"width\":"<<data.header.width<<",\"height\":"<<data.header.height<<",\"quads\":"<<data.header.count<<",\"alpha_prepass_candidates\":"<<replay.opaque.size()/6<<",\"optimized\":"<<(!points&&!zeroVariant&&replay.eligible?"true":"false")<<",\"native_baseline_different_bytes\":"<<nativeMismatch<<",\"optimized_vs_baseline_different_bytes\":"<<replayMismatch<<",\"optimized_vs_native_different_bytes\":"<<originalMismatch<<",\"game_fps_evidence\":false}\n";require(bool(receipt),"complete native replay receipt");receipt.close();
    std::cout<<"background mismatch "<<backgroundMismatch<<", native baseline mismatch "<<nativeMismatch<<", optimized/baseline "<<replayMismatch<<", optimized/native "<<originalMismatch<<" bytes; "<<replay.opaque.size()/6<<" alpha candidates of "<<data.header.count<<'\n';
    if(nativeMismatch){std::ofstream pixels(std::filesystem::path(argv[2]).wstring()+L".baseline.rgba",std::ios::binary|std::ios::out|std::ios::noreplace);require(bool(pixels),"new mismatching native replay pixels");pixels.write(reinterpret_cast<const char*>(baseline.data()),baseline.size());require(bool(pixels),"complete mismatch pixels");}
    require(!backgroundMismatch&&!replayMismatch,"owned replay background/candidate parity not established");
    if(argc>=4){std::ofstream csv(std::filesystem::path(argv[2]).wstring()+L".kernel.csv",std::ios::out|std::ios::noreplace);require(bool(csv),"new actual-input GPU timing receipt");csv<<"round,variant,cpu_issue_ms,gpu_ms,wall_ms,game_fps_evidence\n";
        for(unsigned round=0;round<10;++round)for(unsigned order=0;order<2;++order){const unsigned variant=order^(round&1);replay.reset();glFinish();GLuint timer=0;glGenQueries(1,&timer);const auto start=std::chrono::steady_clock::now();glBeginQuery(0x88bf,timer);replay.draw(variant!=0&&!points&&!zeroVariant,variant!=0&&points,variant!=0&&zeroVariant);glEndQuery(0x88bf);const auto issued=std::chrono::steady_clock::now();glFinish();const auto finished=std::chrono::steady_clock::now();GLuint64 ns=0;glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&ns);glDeleteQueries(1,&timer);if(round)csv<<round<<','<<variant<<','<<std::chrono::duration<double,std::milli>(issued-start).count()<<','<<double(ns)/1e6<<','<<std::chrono::duration<double,std::milli>(finished-start).count()<<",false\n";
        }require(bool(csv),"complete actual-input GPU timings");
    }
    require(!nativeMismatch&&!originalMismatch,"actual native pixel parity not established; any timings are an isolated unvalidated replay only");
    std::cout<<"PASS actual native batch pixels and selected GPU variant; no whole-game FPS claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
