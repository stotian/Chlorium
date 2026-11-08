#include "../R4-33/GpuFixture.hpp"
#include "NativeBatchReplay.hpp"
#include <numeric>
using chlorium::research::batchreplay::Replay;
using chlorium::research::batchreplay::differences;
using chlorium::research::batchreplay::pointQuad;
constexpr unsigned side=64;
GLuint makeProgram(){
    const char* sources[]{"#version 130\nin vec3 a_position;in vec4 a_color;out vec4 color;void main(){gl_Position=vec4(a_position,1);color=a_color;}","#version 130\nin vec4 color;out vec4 result;void main(){result=color;}"};
    const auto p=glCreateProgram();for(unsigned i=0;i<2;++i){auto s=glCreateShader(i?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER);glShaderSource(s,1,&sources[i],nullptr);glCompileShader(s);GLint good=0;glGetShaderiv(s,GL_COMPILE_STATUS,&good);require(good!=0,"owned replay shader compile");glAttachShader(p,s);glDeleteShader(s);}glBindAttribLocation(p,0,"a_position");glBindAttribLocation(p,1,"a_color");glLinkProgram(p);GLint good=0;glGetProgramiv(p,GL_LINK_STATUS,&good);require(good!=0,"owned replay link");return p;
}
std::array<GLint,13> state(){std::array<GLint,13> values{};unsigned i=0;for(auto parameter:{0x8ca6,0x8caa,int(GL_READ_BUFFER),int(GL_DRAW_BUFFER0),int(GL_ACTIVE_TEXTURE),int(GL_TEXTURE_BINDING_2D),int(GL_CURRENT_PROGRAM),int(GL_VERTEX_ARRAY_BINDING),int(GL_ARRAY_BUFFER_BINDING),int(GL_ELEMENT_ARRAY_BUFFER_BINDING),int(GL_COPY_WRITE_BUFFER),0x88ed,0x88ef})glGetIntegerv(parameter,&values[i++]);return values;}
std::vector<unsigned char> read(){std::vector<unsigned char> pixels(side*side*4);glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;}
int main()try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6,true);
    NativeQuadState classifier{};require(pointQuad(classifier),"finite identical positions form a point");classifier.corners[2].x=1;require(!pointQuad(classifier),"a visible corner cannot be removed");
    for(auto bits:{0x7fc00001u,0x7f800000u,0x50000000u}){for(auto& corner:classifier.corners)corner.x=std::bit_cast<float>(bits);require(!pointQuad(classifier),"unknown/overflow-prone point positions retained");}
    const auto program=makeProgram();glUseProgram(program);GLuint vao=0,vbo=0,ibo=0,target=0,framebuffer=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ibo);glGenTextures(1,&target);glGenFramebuffers(1,&framebuffer);
    glViewport(0,0,side,side);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(0x8db9);glDisable(GL_DITHER);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glActiveTexture(GL_TEXTURE0);
    unsigned cases=0,rejections=0,mutants=0,aliasGuards=0;
    for(int encoding:{int(GL_LINEAR),0x8c40})for(int alphaBits:{0,8})for(unsigned count:{2u,65u,256u}){
        glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,encoding==GL_LINEAR?(alphaBits?GL_RGBA8:GL_RGB8):(alphaBits?0x8c43:0x8c41),side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);glReadBuffer(GL_COLOR_ATTACHMENT0);glDrawBuffer(GL_COLOR_ATTACHMENT0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned native target");
        std::vector<unsigned char> background(side*side*4);for(unsigned i=0;i<background.size();++i)background[i]=i%4==3?255:static_cast<unsigned char>((i*17)%73);
        auto reset=[&]{glBindTexture(GL_TEXTURE_2D,target);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,background.data());};reset();
        std::vector<NativeQuadState> quads(count);std::vector<unsigned short> indices(count*6);constexpr unsigned topology[]{0,1,2,3,2,1};
        for(unsigned i=0;i<count;++i){for(unsigned c=0;c<4;++c){auto& v=quads[i].corners[c];v.x=i%2?((c<2?-.75f:.75f)+float(i%7)*.01f):0;v.y=i%2?((c&1)?-.75f:.75f):0;v.z=0;v.color=i%3?0x80204060u:0x80102030u;}for(unsigned k=0;k<6;++k)indices[i*6+k]=static_cast<unsigned short>(i*4+topology[k]);}
        glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,quads.size()*96,quads.data(),GL_STREAM_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);glBindBuffer(GL_ARRAY_BUFFER,0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*2,indices.data(),GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        unsigned calls=0;auto original=[&]{++calls;glBindBuffer(GL_ARRAY_BUFFER,vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glDrawElements(GL_TRIANGLES,count*6,GL_UNSIGNED_SHORT,nullptr);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);};
        auto bytes=std::span(reinterpret_cast<const unsigned char*>(quads.data()),quads.size()*96);auto indexBytes=std::span(reinterpret_cast<const unsigned char*>(indices.data()),indices.size()*2);
        Replay replay;const auto before=read();const auto saved=state();require(replay.prepare(side,side,encoding,alphaBits,before),"owned before copy must be exact");require(state()==saved,"prepare restores touched native bindings");original();const auto after=read();require(before!=after,"non-vacuous native draw");replay.run(before,after,bytes,indexBytes,count,true,original);const auto& r=replay.result;
        require(r.baselineRun&&r.candidateRun&&!r.copiedBeforeDiff&&!r.copiedAfterDiff&&!r.restoredBeforeDiff&&!r.baselineDiff&&!r.candidateDiff&&!r.restoredAfterDiff&&r.removedPoints==(count+1)/2,"same-target native and compacted pixels exact");require(calls==2&&state()==saved&&read()==after,"native callback count and final image/state restored");require(chlorium::research::batchreplay::comparisonPassed(r,false,false,0),"complete native replay accepted");++cases;
        if(count==2&&alphaBits==8&&encoding==GL_LINEAR){
            for(unsigned bad=0;bad<4;++bad){Replay rejected;reset();const auto image=read();if(bad==0)glPixelStorei(GL_PACK_SKIP_ROWS,1);if(bad==1)glColorMask(GL_FALSE,GL_TRUE,GL_TRUE,GL_TRUE);if(bad==2)glEnable(GL_DEPTH_TEST);if(bad==3)glEnable(GL_SCISSOR_TEST);require(!rejected.prepare(side,side,encoding,alphaBits,image),"unsafe native state rejected before reads or target writes");glPixelStorei(GL_PACK_SKIP_ROWS,0);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);require(read()==image,"rejected preflight leaves native image untouched");++rejections;}
            {reset();Replay queried;const auto pixels=read();GLuint query=0;glGenQueries(1,&query);glBeginQuery(GL_SAMPLES_PASSED,query);require(!queried.prepare(side,side,encoding,alphaBits,pixels),"occlusion-dependent drawing cannot be replayed");glEndQuery(GL_SAMPLES_PASSED);glDeleteQueries(1,&query);require(read()==pixels,"query rejection leaves native image untouched");++rejections;}
            reset();Replay poisoned;const auto b=read();require(poisoned.prepare(side,side,encoding,alphaBits,b),"mutant setup");original();const auto a=read();std::vector<unsigned char> falseVertices(bytes.size());poisoned.run(b,a,falseVertices,indexBytes,count,true,original);require(poisoned.result.candidateRun&&poisoned.result.candidateDiff&&!poisoned.result.restoredAfterDiff&&read()==a,"incorrect compaction detected and native pixels restored");require(!chlorium::research::batchreplay::comparisonPassed(poisoned.result,false,false,0),"poisoned compaction cannot be a successful diagnostic");++mutants;
            reset();Replay fallback;const auto b2=read();require(fallback.prepare(side,side,encoding,alphaBits,b2),"unsupported shader setup");original();const auto a2=read();fallback.run(b2,a2,bytes,indexBytes,count,false,original);require(fallback.result.baselineRun&&!fallback.result.candidateRun&&read()==a2,"unknown shader retains native draw");++rejections;
            for(bool execute:{false,true}){reset();Replay extra;const auto b3=read();require(extra.prepare(side,side,encoding,alphaBits,b3),"alternate setup");original();const auto a3=read();const auto bindings=state();extra.run(b3,a3,bytes,indexBytes,count,false,original,[&]{if(execute)original();return execute;});require(extra.result.baselineRun&&extra.result.alternateRun==execute&&!extra.result.alternateDiff&&!extra.result.restoredAfterDiff&&read()==a3&&state()==bindings,"alternate executed/declined paths restore native pixels and bindings");require(chlorium::research::batchreplay::comparisonPassed(extra.result,true,false,0)==execute,"requested shader must actually execute before validation");require(!chlorium::research::batchreplay::comparisonPassed(extra.result,true,true,2),"double native callback cannot validate a handoff");++cases;}
        }
        if(count==2&&alphaBits==8&&encoding==GL_LINEAR){
            reset();Replay error;const auto b4=read();require(error.prepare(side,side,encoding,alphaBits,b4),"GL error setup");original();const auto a4=read();
            error.run(b4,a4,bytes,indexBytes,count,false,original,[&]{original();glEnable(0xdead);return true;});
            require(error.result.glFailure&&!chlorium::research::batchreplay::comparisonPassed(error.result,true,false,0)&&read()==a4,"GL error cannot validate stale comparison bytes; native image restored");++rejections;
        }
        require(glGetError()==GL_NO_ERROR,"owned replay must not add GL errors");
    }
    for(GLint format:{0x881b,0x8c3a}){
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glBindTexture(GL_TEXTURE_2D,target);
        glTexImage2D(GL_TEXTURE_2D,0,format,side,side,0,GL_RGB,GL_FLOAT,nullptr);
        glClearColor(.12345f,.23456f,.34567f,1);glClear(GL_COLOR_BUFFER_BIT);
        std::vector<float> before(side*side*3),after(before.size());glReadPixels(0,0,side,side,GL_RGB,GL_FLOAT,before.data());
        Replay hdr;require(!hdr.prepare(side,side,GL_LINEAR,0,read()),"HDR/packed targets cannot be lossily restored through RGB8");
        glReadPixels(0,0,side,side,GL_RGB,GL_FLOAT,after.data());require(before==after,"HDR rejection preserves float target samples");++rejections;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDrawBuffer(GL_BACK_LEFT);glReadBuffer(GL_BACK);
    GLint alpha=0,encoding=0;glGetIntegerv(GL_ALPHA_BITS,&alpha);glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,0x8210,&encoding);
    glClear(GL_COLOR_BUFFER_BIT);const auto defaultBefore=read();const auto defaultState=state();
    Replay aliases;const auto aliasPrepared=aliases.prepare(side,side,encoding,alpha,defaultBefore);
    // Hidden default surfaces have no guaranteed pixel ownership; owned FBOs
    // above prove pixel parity. Here only attachment acceptance/state are tested.
    require(aliasPrepared || std::string_view(aliases.result.gate)=="before_copy_differs","mono BACK and BACK_LEFT pass attachment preflight");aliases.close();
    require(state()==defaultState,"default alias preflight preserves bindings");++aliasGuards;
    glReadBuffer(GL_FRONT_LEFT);Replay split;require(!split.prepare(side,side,encoding,alpha,read()) && std::string_view(split.result.gate)=="color_attachment_format","distinct front and back buffers reject before copies");++aliasGuards;
    glReadBuffer(GL_BACK);require(glGetError()==GL_NO_ERROR,"default attachment guards add no GL errors");
    glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&target);glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);
    std::cout<<"PASS "<<cases<<" exact same-target replay/compaction cases; "<<rejections<<" native fallbacks; "<<mutants<<" false-compaction mutant caught and restored; "<<aliasGuards<<" default attachment guards (no default pixel claim); bindings and owned RGBA checked\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
