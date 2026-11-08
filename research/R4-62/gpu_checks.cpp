// Reuse the closed native movement world, not another model of GD movement.
#define wmain previousGroupTapeChecks
#include "../R4-42/movement_checks.cpp"
#undef wmain
#include "../R4-33/ResidentRecords.hpp"

using Geometry=chlorium::research::resident::Geometry;
using Parameters=chlorium::research::resident::Parameters;
static constexpr unsigned targetWidth=1280,targetHeight=720;
static PFNGLGETBUFFERSUBDATAPROC originalDownload=nullptr;
static unsigned downloads=0;
static void APIENTRY observeDownload(GLenum target,GLintptr offset,GLsizeiptr size,void* data){
    ++downloads;originalDownload(target,offset,size,data);
}
struct ObserveDownloads {
    PFNGLGETBUFFERSUBDATAPROC saved=glGetBufferSubData;
    ObserveDownloads(){originalDownload=saved;downloads=0;glGetBufferSubData=observeDownload;}
    ~ObserveDownloads(){glGetBufferSubData=saved;}
};
static bool rootPacketEligible(std::span<const State> states,std::span<const Geometry> geometry,std::span<const Parameters> parameters){
    if(states.empty() || geometry.size()!=states.size() || parameters.size()!=states.size())return false;
    for(unsigned i=0;i<states.size();++i){
        const auto& g=geometry[i];const auto& p=parameters[i];
        if(g.control[0]!=1 || g.control[1] || g.control[2] || g.control[3] ||
           p.translationZFlags[0] || p.translationZFlags[1] || (p.translationZFlags[3]&~(1u|512u|1024u)))return false;
        SpriteRecord source{};std::copy_n(g.rect,4,source.localRect);std::copy_n(p.matrix,4,source.matrix);
        source.matrix[4]=std::bit_cast<unsigned>(float(states[i].position[0]));
        source.matrix[5]=std::bit_cast<unsigned>(float(states[i].position[1]));
        source.z=p.translationZFlags[2];source.flags=p.translationZFlags[3];
        if(!chlorium::research::corners::domain(source))return false;
    }
    return true;
}
struct Fused {
    GLuint shader=0,buffers[7]{};GLint count=-1,prefix=-1;unsigned size=0,commandCount=0;
    Dispatch compute=nullptr;std::uint64_t frameUploads=0;
    Fused(const std::filesystem::path& path,std::span<const State> states,std::span<const Command> commands,
          std::span<const Range> ranges,std::span<const unsigned> indices,
          std::span<const Geometry> geometry,std::span<const Parameters> parameters):size(unsigned(states.size())),commandCount(unsigned(commands.size())){
        require(arithmeticEligible(states,commands,ranges,indices,_mm_getcsr()),"bounded closed movement packet");
        require(rootPacketEligible(states,geometry,parameters),"reviewed closed root geometry packet");
        shader=program(path);compute=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));
        require(compute!=nullptr,"fused dispatch available");
        count=glGetUniformLocation(shader,"objectCount");prefix=glGetUniformLocation(shader,"prefixCount");
        require(count>=0 && prefix>=0,"fused shader interface");glGenBuffers(7,buffers);
        auto upload=[&](unsigned lane,const void* data,std::size_t bytes){
            glBindBuffer(storage,buffers[lane]);glBufferData(storage,std::max<std::size_t>(bytes,4),data,GL_DYNAMIC_DRAW);
        };
        upload(0,states.data(),states.size_bytes());upload(1,commands.data(),commands.size_bytes());
        upload(2,geometry.data(),geometry.size_bytes());upload(3,ranges.data(),ranges.size_bytes());
        upload(4,indices.data(),indices.size_bytes());upload(5,parameters.data(),parameters.size_bytes());
        upload(6,nullptr,states.size()*sizeof(NativeQuadState));require(glGetError()==GL_NO_ERROR,"fused storage setup");
    }
    ~Fused(){glDeleteBuffers(7,buffers);glDeleteProgram(shader);}
    void dispatch(std::span<const Command> commands){
        require(commands.size()==commandCount,"frame command count cannot exceed owned storage or change the compiled ranges");
        for(const auto& command:commands)require(!command.control[2] && !command.control[3] &&
            bounded(command.delta[0]) && bounded(command.delta[1]),"frame movement commands remain bounded and known");
        glMemoryBarrier(0x2000);glUseProgram(shader);
        for(unsigned lane=0;lane<7;++lane)glBindBufferBase(storage,lane,buffers[lane]);
        glBindBuffer(storage,buffers[1]);glBufferSubData(storage,0,commands.size_bytes(),commands.data());
        frameUploads=commands.size_bytes();
        glUniform1ui(count,size);glUniform1ui(prefix,GLuint(commands.size()));compute((size+63)/64,1,1);
        glMemoryBarrier(0x2001);require(glGetError()==GL_NO_ERROR,"fused movement/quad dispatch");
    }
    template<class T>std::vector<T> read(unsigned lane){
        std::vector<T> out(size);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glBindBuffer(storage,buffers[lane]);glGetBufferSubData(storage,0,out.size()*sizeof(T),out.data());
        require(glGetError()==GL_NO_ERROR,"owned diagnostic download");return out;
    }
};
static std::vector<unsigned char> image(){
    std::vector<unsigned char> result(targetWidth*targetHeight*4);glReadPixels(0,0,targetWidth,targetHeight,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
    require(glGetError()==GL_NO_ERROR,"owned framebuffer download");return result;
}
static GLuint makeVao(GLuint buffer,GLuint indices){
    GLuint vao=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));
    for(unsigned n=0;n<3;++n)glEnableVertexAttribArray(n);return vao;
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==6,"GD-image Cocos-DLL fused.comp captured-native.vert captured-native.frag");
    const auto fp=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{fp};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);
    require(GLEW_ARB_gpu_shader_fp64,"GPU FP64 required; no precision downgrade");
    std::cout<<"owned GL renderer: "<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<" / "<<glGetString(GL_VERSION)<<'\n';
    const auto cocos=loadNative(argv[2],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};
    chlorium::research::movement::Oracle oracle(argv[1],reinterpret_cast<void*>(&countArray));
    std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    const auto nativeProgram=program(argv[4],GL_VERTEX_SHADER,false,argv[5]);
    glBindAttribLocation(nativeProgram,0,"a_position");glBindAttribLocation(nativeProgram,1,"a_color");
    glBindAttribLocation(nativeProgram,2,"a_texCoord");glLinkProgram(nativeProgram);
    GLint linked=0;glGetProgramiv(nativeProgram,GL_LINK_STATUS,&linked);require(linked!=0,"native attribute linkage");
    const std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUseProgram(nativeProgram);glUniformMatrix4fv(glGetUniformLocation(nativeProgram,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());
    glUniform1i(glGetUniformLocation(nativeProgram,"CC_Texture0"),0);
    GLuint framebuffer=0,textures[2]{};glGenTextures(2,textures);glActiveTexture(GL_TEXTURE0);
    for(unsigned n=0;n<2;++n){glBindTexture(GL_TEXTURE_2D,textures[n]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    const std::array<std::uint32_t,4> texels{0xa07f24ff,0xff287f90,0xc0ff60b0,0x80a0e040};
    glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels.data());
    glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,targetWidth,targetHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned target");
    glViewport(0,0,targetWidth,targetHeight);glDisable(GL_DITHER);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0,0,0,0);glBindTexture(GL_TEXTURE_2D,textures[0]);
    std::uint64_t checkedStates=0,checkedQuads=0;unsigned checkedImages=0,hostileCallbacks=0;
    GLuint timer=0;glGenQueries(1,&timer);
    std::map<std::pair<unsigned,unsigned>,std::vector<double>> intervals;
    for(unsigned control:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned objectCount:{65u,4096u})for(unsigned commandCount:{7u,10u}){
        _mm_setcsr(control);constexpr unsigned combinations=8;
        std::vector<Command> commands(commandCount);std::vector<Range> ranges(combinations);std::vector<unsigned> indices;
        for(unsigned c=0;c<combinations;++c){ranges[c][0]=unsigned(indices.size());for(unsigned k=0;k<commandCount;++k)if((k+c)%3){indices.push_back(k);++ranges[c][1];}}
        std::vector<State> initial(objectCount);std::vector<Geometry> geometry(objectCount);std::vector<Parameters> parameters(objectCount);
        for(unsigned k=0;k<commandCount;++k){commands[k].delta[0]=(k%2?-1:1)*.001;commands[k].delta[1]=(k%3?-1:1)*.0005;}
        commands[0].delta[0]=-0.0;
        for(unsigned i=0;i<objectCount;++i){
            initial[i].position[0]=-.85+double(i%23)*.073;initial[i].position[1]=-.85+double(i%19)*.087;
            if(i%31==0)initial[i].position[0]=-0.0;if(i%29==0)initial[i].position[1]=0.0;
            initial[i].flags=(i%combinations)<<16 | (i%5==0?2u:0u) | (i%7==0?1u:0u);
            auto& g=geometry[i];auto& p=parameters[i];
            const std::array<float,4> rect{-.013f,-.019f,.027f,.039f};std::memcpy(g.rect,rect.data(),16);g.control[0]=1;
            const std::array<float,8> uv{0,1,0,0,1,1,1,0};std::memcpy(g.uv,uv.data(),32);
            p.matrix[0]=std::bit_cast<unsigned>(.75f);p.matrix[1]=std::bit_cast<unsigned>(.125f);
            p.matrix[2]=std::bit_cast<unsigned>(-.25f);p.matrix[3]=std::bit_cast<unsigned>(1.f);
            p.translationZFlags[3]=512|1024|(i%11==0?1:0);
            if(i%13==0)p.translationZFlags[3]&=~512u;
            std::fill(std::begin(p.colors),std::end(p.colors),0x80ffffffu-(i%7)*0x000b1307u);
        }
        require(rootPacketEligible(initial,geometry,parameters),"admit owned root packet");
        {auto bad=geometry;bad[0].control[0]=0;require(!rootPacketEligible(initial,bad,parameters),"native fallback geometry is not silently reconstructed");}
        {auto bad=parameters;bad[0].translationZFlags[0]=0x3f800000;require(!rootPacketEligible(initial,geometry,bad),"unimplemented local translation rejects");}
        {auto bad=parameters;bad[0].translationZFlags[3]|=64;require(!rootPacketEligible(initial,geometry,bad),"vertex modifiers reject");}
        {auto bad=parameters;bad[1].matrix[0]=0x7fc00000;bad[1].translationZFlags[3]=512|1024;require(!rootPacketEligible(initial,geometry,bad),"nonfinite visible transform rejects");}
        require(!rootPacketEligible(initial,std::span<const Geometry>(geometry).subspan(1),parameters),"truncated geometry rejects");
        World world(initial,commands,ranges,indices,table.data());Fused gpu(argv[3],initial,commands,ranges,indices,geometry,parameters);
        {bool rejected=false;try{gpu.dispatch(std::span<const Command>(commands).subspan(1));}catch(const std::runtime_error&){rejected=true;}
         require(rejected && !gpu.frameUploads,"changed command count rejects before GPU mutation");}
        {auto bad=commands;bad[0].delta[0]=std::numeric_limits<double>::infinity();bool rejected=false;
         try{gpu.dispatch(bad);}catch(const std::runtime_error&){rejected=true;}
         require(rejected && !gpu.frameUploads,"nonfinite frame command rejects before GPU mutation");}
        GLuint drawBuffers[2]{};glGenBuffers(2,drawBuffers);glBindBuffer(GL_ARRAY_BUFFER,drawBuffers[0]);
        glBufferData(GL_ARRAY_BUFFER,objectCount*sizeof(NativeQuadState),nullptr,GL_DYNAMIC_DRAW);
        std::vector<unsigned> order(objectCount*6);constexpr unsigned topology[]{0,1,2,3,2,1};
        for(unsigned n=0;n<order.size();++n)order[n]=n/6*4+topology[n%6];
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,drawBuffers[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,order.size()*4,order.data(),GL_STATIC_DRAW);
        const auto nativeVao=makeVao(drawBuffers[0],drawBuffers[1]),gpuVao=makeVao(gpu.buffers[6],drawBuffers[1]);
        for(unsigned frame=0;frame<8;++frame){
            for(unsigned k=0;k<commandCount;++k)commands[k].control[0]=frame*commandCount+k+1;
            _mm_setcsr(control);world.run(oracle.native(),commands,commandCount);const auto expectedState=world.states(initial);
            std::vector<NativeQuadState> expected(objectCount);
            for(unsigned i=0;i<objectCount;++i){
                Captured record{};auto& source=record.sprite;const auto& g=geometry[i];const auto& p=parameters[i];
                std::copy_n(g.rect,4,source.localRect);std::copy_n(p.matrix,4,source.matrix);
                source.matrix[4]=std::bit_cast<unsigned>(float(expectedState[i].position[0]));
                source.matrix[5]=std::bit_cast<unsigned>(float(expectedState[i].position[1]));source.flags=p.translationZFlags[3];source.z=p.translationZFlags[2];
                for(unsigned c=0;c<4;++c){record.quad.corners[c].color=p.colors[c];std::memcpy(&record.quad.corners[c].u,g.uv+c*2,8);}
                require(chlorium::research::corners::domain(source),"composed root pose stays in the reviewed corner domain");
                expected[i]=nativeResult(record,control,reinterpret_cast<const unsigned char*>(cocos)).quad;
            }
            glUseProgram(nativeProgram);glBindVertexArray(nativeVao);glBindBuffer(GL_ARRAY_BUFFER,drawBuffers[0]);
            glBufferSubData(GL_ARRAY_BUFFER,0,expected.size()*sizeof(NativeQuadState),expected.data());glClear(GL_COLOR_BUFFER_BIT);
            glDrawElements(GL_TRIANGLES,objectCount*6,GL_UNSIGNED_INT,nullptr);const auto reference=image();
            require(std::any_of(reference.begin(),reference.end(),[](unsigned char value){return value!=0;}),"native reference actually produces pixels");
            {ObserveDownloads observer;glBeginQuery(GL_TIME_ELAPSED,timer);gpu.dispatch(commands);glUseProgram(nativeProgram);glBindVertexArray(gpuVao);glClear(GL_COLOR_BUFFER_BIT);
             glDrawElements(GL_TRIANGLES,objectCount*6,GL_UNSIGNED_INT,nullptr);glEndQuery(GL_TIME_ELAPSED);
             require(!downloads,"no object-state or quad download before drawing");}
            GLuint64 elapsed=0;glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&elapsed);
            if(frame)intervals[{objectCount,commandCount}].push_back(double(elapsed)*1e-6);
            require(image()==reference,"fused root group rendering matches the actual native programs");++checkedImages;
            require(gpu.frameUploads==commandCount*sizeof(Command),"frame upload contains group commands only");
            const auto actualState=gpu.read<State>(0);require(!differences(expectedState,actualState),"fused retained movement matches native state");
            const auto actual=gpu.read<NativeQuadState>(6);
            require(!std::memcmp(expected.data(),actual.data(),expected.size()*sizeof(NativeQuadState)),"fused native quad bytes match Cocos");
            checkedStates+=objectCount;checkedQuads+=objectCount;
        }
        // A native observer can modify future members: arithmetic equivalence grants no ownership.
        world.mutating=true;world.sections=0;
        world.run(oracle.native(),commands,commandCount);gpu.dispatch(commands);
        require(differences(world.states(initial),gpu.read<State>(0))>0,"hostile native callback disproves automatic GPU ownership");++hostileCallbacks;
        glDeleteVertexArrays(1,&nativeVao);glDeleteVertexArrays(1,&gpuVao);glDeleteBuffers(2,drawBuffers);
    }
    glDeleteProgram(nativeProgram);glDeleteTextures(2,textures);glDeleteFramebuffers(1,&framebuffer);
    glDeleteQueries(1,&timer);
    std::cout<<"R62 fused pipeline: states="<<checkedStates<<" quads="<<checkedQuads<<" images="<<checkedImages
             <<" hostile_callbacks_detected="<<hostileCallbacks
             <<"; group-command uploads only; no intermediate CPU readback; NOT GD FPS or native-reader closure\n";
    for(auto& [key,samples]:intervals){std::sort(samples.begin(),samples.end());
        std::cout<<"owned_GPU_interval target="<<targetWidth<<'x'<<targetHeight<<" objects="<<key.first<<" commands="<<key.second<<" samples="<<samples.size()
                 <<" median_ms="<<samples[samples.size()/2]<<" p95_ms="<<samples[(samples.size()-1)*95/100]
                 <<" worst_ms="<<samples.back()<<"; includes submission bubbles; NOT game frametime or GPU busy time\n";
    }
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
