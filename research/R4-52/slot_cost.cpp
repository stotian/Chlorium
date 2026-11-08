#include "../R4-33/GpuFixture.hpp"
#include "../R4-47/ResidentRenderer.hpp"
#include "r51-stream-reference.local.hpp"
#include "r52-stream-reference.local.hpp"
#include <chrono>
#include <iomanip>
#include <numeric>
using namespace chlorium::research::resident;
static std::string readSource(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);require(bool(f),"benchmark source");return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char** argv)try{
    require(argc==5,"resident.vert native.vert native.frag NEW.csv");
    require(!std::filesystem::exists(argv[4]),"refuse overwrite of benchmark trace");
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);
    const auto resident=readSource(argv[1]);constexpr std::size_t count=8192;
    GLuint vao=0,sources[4]{},canonical=0,target=0,fbo=0,texture=0;
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(4,sources);glGenBuffers(1,&canonical);
    std::vector<Geometry> geometry(count);std::vector<Parameters> parameters(count);std::vector<NativeQuadState> fallback(count);
    std::vector<std::uint32_t> order(count),indices(count*6);std::iota(order.begin(),order.end(),0u);
    constexpr unsigned topology[]{0,1,2,3,2,1};
    for(std::size_t i=0;i<count;++i){
        auto& g=geometry[i];auto& p=parameters[i];g.control[0]=1;
        g.rect[0]=g.rect[1]=std::bit_cast<std::uint32_t>(-.5f);g.rect[2]=g.rect[3]=std::bit_cast<std::uint32_t>(1.f);
        const float uv[]{0,1,0,0,1,1,1,0};for(unsigned k=0;k<8;++k)g.uv[k]=std::bit_cast<std::uint32_t>(uv[k]);
        p.matrix[0]=p.matrix[3]=std::bit_cast<std::uint32_t>(.015625f);
        p.translationZFlags[0]=std::bit_cast<std::uint32_t>((float(i%128)-63)*.015625f);
        p.translationZFlags[1]=std::bit_cast<std::uint32_t>((float(i/128)-31)*.015625f);p.translationZFlags[3]=512|1024;
        for(auto& c:p.colors)c=0x80604020u+unsigned(i%16);
        for(unsigned k=0;k<6;++k)indices[i*6+k]=std::uint32_t(i*4+topology[k]);
    }
    const auto originalParameters=parameters;
    auto upload=[&](unsigned binding,const void* p,std::size_t bytes){glBindBuffer(GLStateSnapshot::storageTarget,sources[binding]);glBufferData(GLStateSnapshot::storageTarget,bytes,p,GL_DYNAMIC_DRAW);glBindBufferBase(GLStateSnapshot::storageTarget,binding,sources[binding]);};
    upload(0,geometry.data(),geometry.size()*sizeof(Geometry));upload(1,parameters.data(),parameters.size()*sizeof(Parameters));
    upload(2,order.data(),order.size()*sizeof(order[0]));upload(3,fallback.data(),fallback.size()*sizeof(fallback[0]));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,canonical);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*4,indices.data(),GL_STATIC_DRAW);
    NativeVertexStream cached;ReferenceR51Stream reference;ReferenceR52Stream coarse;
    require(cached.initialize(count,resident),"cached GPU stream initialize");require(reference.initialize(count,resident,canonical),"qualified reference initialize");
    require(coarse.initialize(count,resident),"qualified coarse cache initialize");
    require(coarse.changed(Lane::geometry,0,count)&&coarse.changed(Lane::parameters,0,count)&&coarse.changed(Lane::fallback,0,count)&&coarse.changed(Lane::order,0,count),"coarse GPU seed");
    std::vector<std::uint64_t> initialMask((count+63)/64);require(cached.publishChanges(initialMask,true,true),"exact GPU seed");
    const auto native=program(argv[2],GL_VERTEX_SHADER,false,argv[3]);
    glBindAttribLocation(native,0,"a_position");glBindAttribLocation(native,1,"a_color");glBindAttribLocation(native,2,"a_texCoord");glLinkProgram(native);
    GLint linked=0;glGetProgramiv(native,GL_LINK_STATUS,&linked);require(linked,"native benchmark program");
    const std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUseProgram(native);glUniformMatrix4fv(glGetUniformLocation(native,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());glUniform1i(glGetUniformLocation(native,"CC_Texture0"),0);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned benchmark target");
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);const std::uint32_t white=0xffffffff;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glViewport(0,0,256,256);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glDisable(0x8db9);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glClearColor(.07f,.11f,.13f,.17f);
    GLuint queries[3]{};glGenQueries(3,queries);
    std::ofstream csv(argv[4],std::ios::out|std::ios::noreplace);require(bool(csv),"new kernel trace");
    csv<<"workload,round,variant,gpu_ns,cpu_issue_ns,updated_slots,index_passes,cache_reuses,game_fps_evidence\n";
    std::vector<unsigned char> a(256*256*4),b(a.size()),c(a.size());
    using Clock=std::chrono::steady_clock;
    const std::array<const char*,6> workloads{"unchanged","one_pose","clustered_96","scattered_82","all_poses","order_only"};
    for(unsigned workload=0;workload<workloads.size();++workload){
        for(unsigned round=0;round<33;++round){
            chlorium::snapshot::DirtyStateUploads dirty(count);std::vector<std::uint64_t> exactMask((count+63)/64);
            const auto changes=workload==1?1u:workload==2?96u:workload==3?82u:workload==4?unsigned(count):0u;
            for(unsigned n=0;n<changes;++n){const auto index=workload==3?(std::size_t(n)*97)%count:workload==1?130u:std::size_t(n);
                const auto original=std::bit_cast<float>(originalParameters[index].translationZFlags[0]);
                parameters[index].translationZFlags[0]=std::bit_cast<std::uint32_t>(original+(round%2?.001f:.002f));dirty.mark(index);exactMask[index/64]|=std::uint64_t{1}<<(index%64);}
            glBindBuffer(GLStateSnapshot::storageTarget,sources[1]);
            for(const auto r:dirty.finish()){
                glBufferSubData(GLStateSnapshot::storageTarget,r.first*sizeof(Parameters),r.count*sizeof(Parameters),parameters.data()+r.first);
                require(coarse.changed(Lane::parameters,r.first,r.count),"coarse parameter notification");
            }
            if(workload==5){std::reverse(order.begin(),order.end());glBindBuffer(GLStateSnapshot::storageTarget,sources[2]);glBufferSubData(GLStateSnapshot::storageTarget,0,count*4,order.data());require(coarse.changed(Lane::order,0,count),"coarse order notification");}
            require(cached.publishChanges(exactMask,false,workload==5),"exact notifications independent of upload ranges");
            const auto prior=cached.metrics;const auto coarsePrior=coarse.metrics;
            std::array<GLuint64,3> elapsed{};std::array<long long,3> cpu{};
            for(unsigned k=0;k<3;++k){const unsigned variant=(round+k)%3;
                glClear(GL_COLOR_BUFFER_BIT);glBindVertexArray(vao);glUseProgram(native);
                glBeginQuery(0x88bf,queries[variant]);const auto start=Clock::now();
                const bool ok=variant==2?cached.materialize(count):variant==1?coarse.materialize(count):reference.materialize(count);require(ok,"GPU stream execution");
                glUseProgram(native);glBindVertexArray(variant==2?cached.vao():variant==1?coarse.vao():reference.vao());glDrawElements(GL_TRIANGLES,GLsizei(count*6),GL_UNSIGNED_INT,nullptr);
                cpu[variant]=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();glEndQuery(0x88bf);
                glGetQueryObjectui64v(queries[variant],GL_QUERY_RESULT,&elapsed[variant]);
                auto& pixels=variant==2?c:variant==1?b:a;glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                require(glGetError()==GL_NO_ERROR,"owned timing and image readback");
            }
            require(a==b && b==c,"timed paths must produce identical native-program pixels");
            if(round>=9){const auto after=cached.metrics;const auto coarseAfter=coarse.metrics;
                csv<<workloads[workload]<<','<<round<<",r51,"<<elapsed[0]<<','<<cpu[0]<<','<<count<<",0,0,false\n";
                csv<<workloads[workload]<<','<<round<<",r52,"<<elapsed[1]<<','<<cpu[1]<<','<<coarseAfter.lastSlots<<','<<coarseAfter.indexPasses-coarsePrior.indexPasses<<','<<coarseAfter.reuses-coarsePrior.reuses<<",false\n";
                csv<<workloads[workload]<<','<<round<<",r53,"<<elapsed[2]<<','<<cpu[2]<<','<<after.lastSlots<<','<<after.indexPasses-prior.indexPasses<<','<<after.reuses-prior.reuses<<",false\n";
            }
        }
    }
    require(bool(csv),"complete kernel trace");csv.close();reference.close();coarse.close();cached.close();
    glDeleteQueries(3,queries);glDeleteProgram(native);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);glDeleteTextures(1,&texture);
    glDeleteBuffers(4,sources);glDeleteBuffers(1,&canonical);glDeleteVertexArrays(1,&vao);
    std::cout<<"PASS 198 alternating owned image triples; kernel timings only, NO game FPS proof\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
