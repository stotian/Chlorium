#define CHLORIUM_R62_FIXTURE_ONLY 1
#include "../R4-62/gpu_checks.cpp"
#include "GroupQuadStream.hpp"
#include <iterator>
namespace gp=chlorium::research::groupprogram;
#include "PacketReader.hpp"
using gp::readPacket;
static std::string readText(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"open shader");return {std::istreambuf_iterator<char>(input),{}};}
static void verifyForeign(GLuint buffer,GLint alignment){
    for(unsigned i=0;i<7;++i){const auto value=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,i);
        require(GLuint(value.buffer)==buffer && value.offset==GLint64((i+1)*alignment) && value.size==128,"owned stream restores every foreign SSBO range");}
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==7,"GD-image Cocos-DLL group_quads.comp native.vert native.frag compiled-packet");
    const auto previousFp=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{previousFp};_mm_setcsr(0x1f80);
    auto packet=readPacket(argv[6],_mm_getcsr());
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);
    require(GLEW_ARB_gpu_shader_fp64,"FP64 required");std::cout<<"R64 owned GL: "<<glGetString(GL_RENDERER)<<'\n';
    const auto cocos=loadNative(argv[2],input);struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};
    chlorium::research::movement::Oracle oracle(argv[1],reinterpret_cast<void*>(&countArray));
    std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    double maxX=1,maxY=1;for(const auto& state:packet.states){maxX=std::max(maxX,std::abs(state.position[0]));maxY=std::max(maxY,std::abs(state.position[1]));}
    for(const auto& frame:packet.frames)for(const auto& p:frame.expected){maxX=std::max(maxX,std::abs(p[0]));maxY=std::max(maxY,std::abs(p[1]));}
    const float width=float(maxX*.008),height=float(maxY*.008);
    std::vector<Geometry> geometry(packet.states.size());std::vector<Parameters> parameters(packet.states.size());
    for(unsigned i=0;i<geometry.size();++i){auto& g=geometry[i];auto& p=parameters[i];
        const std::array<float,4> rectangle{-width*.5f,-height*.5f,width,height};std::memcpy(g.rect,rectangle.data(),16);g.control[0]=1;
        if(std::abs(packet.states[i].position[0])>0 && std::abs(packet.states[i].position[0])<0x1p-31)std::fill(std::begin(g.rect),std::end(g.rect),0);
        const std::array<float,8> uv{0,1,0,0,1,1,1,0};std::memcpy(g.uv,uv.data(),32);
        p.matrix[0]=p.matrix[3]=0x3f800000;p.translationZFlags[3]=512|1024;
        std::fill(std::begin(p.colors),std::end(p.colors),0x60ffffffu-(i%7)*0x000b1307u);
    }
    const auto nativeProgram=program(argv[4],GL_VERTEX_SHADER,false,argv[5]);
    glBindAttribLocation(nativeProgram,0,"a_position");glBindAttribLocation(nativeProgram,1,"a_color");glBindAttribLocation(nativeProgram,2,"a_texCoord");glLinkProgram(nativeProgram);
    GLint linked=0;glGetProgramiv(nativeProgram,GL_LINK_STATUS,&linked);require(linked!=0,"native program linkage");
    const std::array<float,16> matrix{float(.9/maxX),0,0,0,0,float(.9/maxY),0,0,0,0,1,0,0,0,0,1};
    glUseProgram(nativeProgram);glUniformMatrix4fv(glGetUniformLocation(nativeProgram,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());glUniform1i(glGetUniformLocation(nativeProgram,"CC_Texture0"),0);
    GLuint fbo=0,textures[2]{};glGenTextures(2,textures);glActiveTexture(GL_TEXTURE0);
    for(auto texture:textures){glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    const std::array<unsigned,4> texels{0xa07f24ff,0xff287f90,0xc0ff60b0,0x80a0e040};
    glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels.data());
    glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,targetWidth,targetHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned framebuffer");
    glViewport(0,0,targetWidth,targetHeight);glDisable(GL_DITHER);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,0);glBindTexture(GL_TEXTURE_2D,textures[0]);
    GLuint foreign=0;glGenBuffers(1,&foreign);GLint alignment=0;glGetIntegerv(0x90df,&alignment);require(alignment>0,"SSBO alignment");
    glBindBuffer(storage,foreign);glBufferData(storage,alignment*8+128,nullptr,GL_DYNAMIC_DRAW);
    for(unsigned i=0;i<7;++i)glBindBufferRange(storage,i,foreign,(i+1)*alignment,128);
    gp::GroupQuadStream stream;const auto source=readText(argv[3]);const auto& first=packet.frames[0];
    require(!stream.initialize(packet.states,geometry,parameters,first.commands,first.ranges,first.indices,source+"\n"),"changed shader rejects before allocation");
    SetLastError(71);const auto fp=_mm_getcsr();
    require(stream.initialize(packet.states,geometry,parameters,first.commands,first.ranges,first.indices,source),"initialize shared GPU stream");
    require(GetLastError()==71 && _mm_getcsr()==fp,"initialization preserves host state");verifyForeign(foreign,alignment);
    require(!stream.vao(),"unmaterialized output cannot be drawn");
    GLuint referenceBuffers[2]{};glGenBuffers(2,referenceBuffers);glBindBuffer(GL_ARRAY_BUFFER,referenceBuffers[0]);glBufferData(GL_ARRAY_BUFFER,packet.states.size()*96,nullptr,GL_DYNAMIC_DRAW);
    std::vector<unsigned> order(packet.states.size()*6);constexpr unsigned topology[]{0,1,2,3,2,1};for(unsigned i=0;i<order.size();++i)order[i]=i/6*4+topology[i%6];
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,referenceBuffers[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,order.size()*4,order.data(),GL_STATIC_DRAW);const auto referenceVao=makeVao(referenceBuffers[0],referenceBuffers[1]);
    auto privateReference=packet.states;std::uint64_t statesCompared=0,quadsCompared=0;unsigned images=0;
    for(unsigned frameIndex=0;frameIndex<2;++frameIndex){const auto& frame=packet.frames[frameIndex];
        World world(privateReference,frame.commands,frame.ranges,frame.indices,table.data());world.run(oracle.native(),frame.commands,unsigned(frame.commands.size()));privateReference=world.states(privateReference);
        for(unsigned i=0;i<privateReference.size();++i)require(!std::memcmp(privateReference[i].position,frame.expected[i].data(),16),"compiled replay matches native end-frame pose witnesses");
        std::vector<NativeQuadState> expected(privateReference.size());
        for(unsigned i=0;i<expected.size();++i){Captured record{};auto& s=record.sprite;const auto& g=geometry[i];const auto& p=parameters[i];
            std::copy_n(g.rect,4,s.localRect);std::copy_n(p.matrix,4,s.matrix);s.matrix[4]=std::bit_cast<unsigned>(float(privateReference[i].position[0]));s.matrix[5]=std::bit_cast<unsigned>(float(privateReference[i].position[1]));s.flags=512|1024;
            auto local=s;local.matrix[4]=local.matrix[5]=0;
            auto translation=[](unsigned bits){bits&=0x7fffffff;return !bits || (bits>=0x15800000u && bits<=0x4f800000u);};
            require(chlorium::research::corners::domain(local) && translation(s.matrix[4]) && translation(s.matrix[5]),"normal translated corner domain on the 2^-84 group lattice");
            for(unsigned c=0;c<4;++c){record.quad.corners[c].color=p.colors[c];std::memcpy(&record.quad.corners[c].u,g.uv+c*2,8);}expected[i]=nativeResult(record,0x1f80,reinterpret_cast<const unsigned char*>(cocos)).quad;
        }
        glUseProgram(nativeProgram);glBindVertexArray(referenceVao);glBindBuffer(GL_ARRAY_BUFFER,referenceBuffers[0]);glBufferSubData(GL_ARRAY_BUFFER,0,expected.size()*96,expected.data());glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,GLsizei(expected.size()*6),GL_UNSIGNED_INT,nullptr);const auto reference=image();require(std::any_of(reference.begin(),reference.end(),[](unsigned char value){return value!=0;}),"native reference is nonempty");
        auto bad=frame.ranges;bad[0][1]=65;require(!stream.produce(frame.commands,bad,frame.indices) && !stream.vao(),"invalid object program disarms drawing before native fallback");
        _mm_setcsr(0x3f80);require(!stream.produce(frame.commands,frame.ranges,frame.indices) && !stream.vao(),"unsupported rounding declines before changing GPU poses");_mm_setcsr(0x1f80);
        if(frameIndex==1){
            const auto context=wglGetCurrentContext();const auto dc=wglGetCurrentDC();require(wglMakeCurrent(nullptr,nullptr)!=0,"detach owned context");
            require(!stream.produce(frame.commands,frame.ranges,frame.indices) && !stream.vao(),"missing context declines without stale drawing");
            require(wglMakeCurrent(dc,context)!=0,"restore owned context");
        }
        {ObserveDownloads observer;SetLastError(73);const auto beforeFp=_mm_getcsr();
         require(stream.produce(frame.commands,frame.ranges,frame.indices),"produce from shared command/range tables");
         require(GetLastError()==73 && _mm_getcsr()==beforeFp,"production preserves host FP and LastError");verifyForeign(foreign,alignment);
         GLint restoredProgram=0,restoredVao=0;glGetIntegerv(GL_CURRENT_PROGRAM,&restoredProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&restoredVao);
         require(GLuint(restoredProgram)==nativeProgram && GLuint(restoredVao)==referenceVao,"native program and VAO restored");
         glBindVertexArray(stream.vao());glClear(GL_COLOR_BUFFER_BIT);glDrawElements(GL_TRIANGLES,GLsizei(expected.size()*6),GL_UNSIGNED_INT,nullptr);require(!downloads,"no CPU pose download before native-program drawing");}
        require(image()==reference,"native image equivalence for the compiled real movement workload");++images;
        std::vector<State> actual(privateReference.size());require(stream.copyOwned(0,std::span<State>(actual)),"owned state readback");
        require(!differences(privateReference,actual),"native closed-loop state equals shared GPU program");statesCompared+=actual.size();
        std::vector<NativeQuadState> quads(expected.size());require(stream.copyOwned(6,std::span<NativeQuadState>(quads)),"owned quad readback");
        require(!std::memcmp(quads.data(),expected.data(),expected.size()*96),"native quad bytes equal grouped GPU output");quadsCompared+=quads.size();
        require(stream.metrics.lastProgramBytes==frame.commands.size()*32+frame.ranges.size()*8+frame.indices.size()*4,"only shared program data uploaded after residency");
    }
    require(stream.metrics.stateSeedBytes==packet.states.size()*32 && stream.metrics.dispatches==2,"positions seeded once for both native frames");
    const auto uploaded=stream.metrics.programBytes;
    std::vector<Range> empty(first.ranges.size());require(stream.produce({},empty,{}),"empty command frame produces current geometry");
    std::vector<State> unchanged(privateReference.size());require(stream.copyOwned(0,std::span<State>(unchanged)) && !differences(privateReference,unchanged),"empty frame retains every resident state byte");
    require(stream.metrics.lastProgramBytes==empty.size()*sizeof(Range),"empty frame uploads only shared ranges");
    stream.close();require(!stream.vao() && !stream.produce(first.commands,first.ranges,first.indices),"closed stream declines without stale output");
    glDeleteVertexArrays(1,&referenceVao);glDeleteBuffers(2,referenceBuffers);for(unsigned i=0;i<7;++i)glBindBufferBase(storage,i,0);glDeleteBuffers(1,&foreign);
    glDeleteProgram(nativeProgram);glDeleteTextures(2,textures);glDeleteFramebuffers(1,&fbo);
    require(glGetError()==GL_NO_ERROR,"owned pipeline cleanup leaves no GL errors");
    std::cout<<"R64 shared stream states="<<statesCompared<<" quads="<<quadsCompared<<" images="<<images<<" program_bytes="<<uploaded<<" position_uploads_after_seed=0; native commands/witnesses, synthetic geometry; NOT GD FPS or CPU reader closure\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
