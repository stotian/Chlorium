#include "../R4-33/GpuFixture.hpp"
#include "AlphaMask.hpp"
#include "DepthContract.hpp"
#include <chrono>
#include <cmath>
#include <numeric>
using chlorium::research::occlusion::AlphaMask;
constexpr unsigned side=128;
constexpr unsigned triangle[]{0,1,2,3,2,1};
struct Harness {
    GLuint color=0,depth=0,alphaDepth=0,earlyMutant=0,vao=0,vbo=0,allIndices=0,opaqueIndices=0,framebuffer=0,colorTarget=0,depthTarget=0,texture=0;
    unsigned capacity=0;
    std::vector<unsigned char> pixels;
    AlphaMask mask;
    static std::vector<unsigned char> image(){std::vector<unsigned char> out(64*64*4);
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){const auto i=(y*64+x)*4;const unsigned a=x<32?255:((x%13==0||y%17==0)?0:((x%11==0&&y%5==0)?128:255));
            out[i]=static_cast<unsigned char>((x*3%256)*a/255);out[i+1]=static_cast<unsigned char>((y*5%256)*a/255);out[i+2]=static_cast<unsigned char>(((x+y)*7%256)*a/255);out[i+3]=static_cast<unsigned char>(a);
        }return out;
    }
    Harness(const std::filesystem::path& shaders):pixels(image()),mask(64,64,pixels){
        color=program(shaders/"ordered.vert",GL_VERTEX_SHADER,false,shaders/"color.frag");depth=program(shaders/"ordered.vert",GL_VERTEX_SHADER,false,shaders/"depth.frag");
        alphaDepth=program(shaders/"ordered.vert",GL_VERTEX_SHADER,false,shaders/"alpha-depth.frag");earlyMutant=program(shaders/"ordered.vert",GL_VERTEX_SHADER,false,shaders/"early-depth-mutant.frag");
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));glEnableVertexAttribArray(2);
        glGenBuffers(1,&allIndices);glGenBuffers(1,&opaqueIndices);
        glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenTextures(1,&colorTarget);glBindTexture(GL_TEXTURE_2D,colorTarget);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glGenRenderbuffers(1,&depthTarget);glBindRenderbuffer(GL_RENDERBUFFER,depthTarget);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,side,side);
        glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colorTarget,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthTarget);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned color/depth framebuffer");
        glBindTexture(GL_TEXTURE_2D,texture);glDisable(GL_DITHER);glDisable(GL_STENCIL_TEST);glDisable(GL_POLYGON_OFFSET_FILL);glViewport(0,0,side,side);glDepthRange(0,1);
    }
    ~Harness(){glDeleteProgram(color);glDeleteProgram(depth);glDeleteProgram(alphaDepth);glDeleteProgram(earlyMutant);glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&allIndices);glDeleteBuffers(1,&opaqueIndices);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&colorTarget);glDeleteTextures(1,&texture);glDeleteRenderbuffers(1,&depthTarget);}
    static bool supported(std::span<const NativeQuadState> quads,const std::array<float,16>& m){
        return chlorium::research::occlusion::depthContract(quads,m);
    }
    void uniforms(GLuint p,const std::array<float,16>& matrix,unsigned count,bool ordered){
        glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());glUniform1f(glGetUniformLocation(p,"quadCount"),float(count));glUniform1i(glGetUniformLocation(p,"orderedDepth"),ordered);glUniform1i(glGetUniformLocation(p,"CC_Texture0"),0);
    }
    bool submit(std::span<const NativeQuadState> quads,const std::array<float,16>& matrix,unsigned first,unsigned amount,unsigned variant,bool additive=false,GLuint samples=0){
        require(first<=quads.size()&&amount<=quads.size()-first,"bounded native-order draw range");
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        if(capacity<quads.size()){capacity=static_cast<unsigned>(quads.size());glBufferData(GL_ARRAY_BUFFER,capacity*sizeof(NativeQuadState),nullptr,GL_STREAM_DRAW);
            std::vector<unsigned> indices(capacity*6);for(unsigned i=0;i<capacity;++i)for(unsigned j=0;j<6;++j)indices[i*6+j]=i*4+triangle[j];glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,allIndices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned),indices.data(),GL_STATIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER,0,quads.size_bytes(),quads.data());glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
        std::vector<unsigned> opaque;opaque.reserve(amount*6);
        const bool eligible=variant&&!additive&&supported(quads,matrix);
        if(eligible)for(unsigned i=first+amount;i-->first;)if(variant==2||(variant>=3?mask.maybeOpaqueFootprint(quads[i]):mask.opaqueFootprint(quads[i])))for(unsigned j:triangle)opaque.push_back(i*4+j);
        const bool optimized=eligible&&!opaque.empty();
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.12f,.23f,.34f,.45f);glClearDepth(1);
        glClear(GL_COLOR_BUFFER_BIT|(optimized?GL_DEPTH_BUFFER_BIT:0));
        if(optimized){
            glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDisable(GL_BLEND);
            uniforms(variant==4?earlyMutant:variant==3?alphaDepth:depth,matrix,static_cast<unsigned>(quads.size()),true);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,opaqueIndices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,opaque.size()*sizeof(unsigned),opaque.data(),GL_STREAM_DRAW);glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(opaque.size()),GL_UNSIGNED_INT,nullptr);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);
        }else glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,additive?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
        uniforms(color,matrix,static_cast<unsigned>(quads.size()),optimized);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,allIndices);
        if(samples)glBeginQuery(GL_SAMPLES_PASSED,samples);
        glDrawElements(GL_TRIANGLES,amount*6,GL_UNSIGNED_INT,reinterpret_cast<void*>(std::uintptr_t(first)*6*sizeof(unsigned)));
        if(samples)glEndQuery(GL_SAMPLES_PASSED);
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);require(glGetError()==GL_NO_ERROR,"owned occlusion draw");return optimized;
    }
    std::vector<unsigned char> read(){std::vector<unsigned char> result(side*side*4);glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,result.data());require(glGetError()==GL_NO_ERROR,"owned pixel readback");return result;}
};
std::vector<NativeQuadState> scene(const unsigned char* native,unsigned count,bool transparentOnly,bool heavy){
    std::vector<NativeQuadState> result;result.reserve(count);
    for(unsigned i=0;i<count;++i){Captured record{};record.sprite.flags=512|1024;const float angle=heavy?0.f:float(i%5)*.07f,c=std::cos(angle),s=std::sin(angle);
        const float matrix[]{c,s,-s,c,heavy?0.f:float(i%3)*.035f,heavy?0.f:float(i%4)*.025f};std::memcpy(record.sprite.matrix,matrix,sizeof(matrix));
        const float rect[]{heavy?-1.f:-.85f,heavy?-1.f:-.85f,heavy?2.f:1.7f,heavy?2.f:1.7f};std::memcpy(record.sprite.localRect,rect,sizeof(rect));
        const unsigned alpha=transparentOnly?128u:(i%7==6?128u:255u);const bool holes=!heavy&&i%4==3;
        for(unsigned j=0;j<4;++j){auto& v=record.quad.corners[j];const unsigned r=(i*37+j*11)%256,g=(i*17+j*31)%256,b=(i*53+j*7)%256;
            v.color=(r*alpha/255)|((g*alpha/255)<<8)|((b*alpha/255)<<16)|(alpha<<24);v.u=j>=2?(holes?.93f:.35f):(holes?.55f:.08f);v.v=(j&1)?.1f:.9f;
        }
        record.quad=nativeResult(record,0x1f80,native).quad;result.push_back(record.quad);
    }return result;
}
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4||argc==5,"Cocos DLL, R4-41 shader directory, NEW receipt and optional --timings");
    if(argc==5)require(std::wstring_view(argv[4])==L"--timings","explicit isolated GPU timings");
    chlorium::test::SystemInputGuard input;const auto native=loadNative(std::filesystem::path(argv[1]),input);Graphics graphics;graphics.create();Harness gpu{std::filesystem::path(argv[2])};
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1};
    const std::array<float,16> camera{.92f,.13f,0,0,-.07f,.89f,0,0,0,0,0,0,.04f,-.03f,0,1};
    auto quads=scene(reinterpret_cast<const unsigned char*>(native),16,false,false);unsigned scenarios=0,optimized=0,fallbacks=0;
    for(bool linear:{false,true})for(bool cull:{false,true})for(bool scissor:{false,true})for(bool additive:{false,true})for(unsigned first:{0u,3u})for(const auto& matrix:{identity,camera}){
        glBindTexture(GL_TEXTURE_2D,gpu.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,linear?GL_LINEAR:GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,linear?GL_LINEAR:GL_NEAREST);
        if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
        // Initialize outside a later scissor too, so untouched pixels have defined identical contents.
        glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(.12f,.23f,.34f,.45f);glClear(GL_COLOR_BUFFER_BIT);
        if(scissor){glEnable(GL_SCISSOR_TEST);glScissor(17,11,side-35,side-27);}
        gpu.submit(quads,matrix,first,static_cast<unsigned>(quads.size())-first,0,additive);const auto expected=gpu.read();
        for(unsigned variant:{1u,3u}){const auto used=gpu.submit(quads,matrix,first,static_cast<unsigned>(quads.size())-first,variant,additive);const auto actual=gpu.read();require(actual==expected,"opaque depth path changes RGBA8 pixels");
            require(used!=additive,"additive blend must use direct fallback");optimized+=used;fallbacks+=!used;++scenarios;}
    }
    glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glBindTexture(GL_TEXTURE_2D,gpu.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    gpu.submit(quads,identity,0,quads.size(),0);const auto visible=gpu.read();gpu.submit(quads,identity,0,quads.size(),2);require(visible!=gpu.read(),"forced opaque classification mutant must be rejected");
    gpu.submit(quads,identity,0,quads.size(),4);require(visible!=gpu.read(),"forced early tests must not write depth through discarded transparent fragments");
    const std::array<float,16> zero{};gpu.submit(quads,zero,0,quads.size(),1);require(visible!=gpu.read(),"black/zero-MVP mutant must be rejected");
    for(unsigned unsupported=0;unsupported<2;++unsupported){auto matrix=identity;matrix[unsupported?11:14]=unsupported?.05f:2.f;
        gpu.submit(quads,matrix,0,quads.size(),0);const auto expected=gpu.read();require(!gpu.submit(quads,matrix,0,quads.size(),1)&&gpu.read()==expected,"Z/perspective requires direct full-pass fallback");++fallbacks;
    }
    {auto shifted=quads;for(auto& quad:shifted)for(auto& v:quad.corners)v.z=1.25f;auto matrix=identity;matrix[10]=.125f;
        gpu.submit(shifted,matrix,0,shifted.size(),0);const auto expected=gpu.read();require(gpu.submit(shifted,matrix,0,shifted.size(),1)&&gpu.read()==expected,"bounded original nonzero clip depth");++scenarios;++optimized;
        for(auto& quad:shifted)for(auto& v:quad.corners)v.z=20.f;gpu.submit(shifted,matrix,0,shifted.size(),0);const auto clipped=gpu.read();require(!gpu.submit(shifted,matrix,0,shifted.size(),1)&&gpu.read()==clipped,"original Z clipping retained by fallback");++fallbacks;
    }
    auto transparent=scene(reinterpret_cast<const unsigned char*>(native),16,true,false);gpu.submit(transparent,identity,0,transparent.size(),0);const auto expectedTransparent=gpu.read();require(!gpu.submit(transparent,identity,0,transparent.size(),1)&&gpu.read()==expectedTransparent,"no occluder requires direct fallback");++fallbacks;
    auto heavy=scene(reinterpret_cast<const unsigned char*>(native),128,false,true);GLuint queries[2]{};glGenQueries(2,queries);
    gpu.submit(heavy,identity,0,heavy.size(),0,false,queries[0]);const auto expectedHeavy=gpu.read();gpu.submit(heavy,identity,0,heavy.size(),1,false,queries[1]);require(expectedHeavy==gpu.read(),"dense opaque parity");
    GLuint64 samples[2]{};for(unsigned i=0;i<2;++i)glGetQueryObjectui64v(queries[i],GL_QUERY_RESULT,&samples[i]);glDeleteQueries(2,queries);require(samples[1]<samples[0],"actual main-pass sample work must decrease");
    if(argc==5){
        std::ofstream csv(std::filesystem::path(argv[3]).wstring()+L".kernel.csv",std::ios::out|std::ios::noreplace);require(bool(csv),"new GPU kernel timings");csv<<"scene,round,variant,cpu_issue_ms,gpu_ms,wall_ms,game_fps_evidence\n";
        for(unsigned scenario=0;scenario<3;++scenario){const auto& objects=scenario==0?heavy:scenario==1?quads:transparent;
            for(unsigned round=0;round<10;++round)for(unsigned order=0;order<3;++order){const unsigned choice=(order+round)%3;const unsigned variant=choice==2?3:choice;glFinish();GLuint timer=0;glGenQueries(1,&timer);const auto start=std::chrono::steady_clock::now();
                glBeginQuery(0x88bf,timer);for(unsigned repeat=0;repeat<8;++repeat)gpu.submit(objects,identity,0,objects.size(),variant);glEndQuery(0x88bf);const auto issued=std::chrono::steady_clock::now();glFinish();const auto finished=std::chrono::steady_clock::now();
                GLuint64 nanos=0;glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&nanos);glDeleteQueries(1,&timer);
                if(round)csv<<scenario<<','<<round<<','<<variant<<','<<std::chrono::duration<double,std::milli>(issued-start).count()/8<<','<<double(nanos)/8e6<<','<<std::chrono::duration<double,std::milli>(finished-start).count()/8<<",false\n";
            }
        }require(bool(csv),"complete GPU kernel timings");
    }
    std::ofstream output(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(output),"new GPU occlusion receipt");
    output<<"{\"diagnostic_only\":true,\"pixel_scenarios\":"<<scenarios<<",\"optimized_cases\":"<<optimized<<",\"direct_fallbacks\":"<<fallbacks<<",\"forced_opaque_mutant_rejected\":true,\"early_transparent_depth_mutant_rejected\":true,\"zero_mvp_mutant_rejected\":true,\"dense_main_samples_native\":"<<samples[0]<<",\"dense_main_samples_occluded\":"<<samples[1]<<",\"game_fps_evidence\":false}\n";require(bool(output),"complete occlusion receipt");
    std::cout<<"PASS "<<scenarios<<" exact RGBA8 cases, "<<fallbacks<<" direct fallbacks, classification/black mutants rejected; dense main samples "<<samples[0]<<" -> "<<samples[1]<<"; owned GPU fixture only\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
