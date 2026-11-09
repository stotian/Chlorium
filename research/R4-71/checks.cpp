#define wmain previousMovementChecks
#include "../R4-42/movement_checks.cpp"
#undef wmain
#include "MixedProgram.hpp"
namespace mp=chlorium::research::mixedprogram;
extern "C" {void* r71PointCtor=nullptr;void* r71PointApply=nullptr;void R71Rotation(void*,const mp::Command*);}
struct MixedWorld {
    std::vector<mp::State> seed;
    World world;
    static std::vector<State> oldStates(std::span<const mp::State> states){std::vector<State> out(states.size());for(unsigned i=0;i<states.size();++i){const auto& s=states[i];auto& d=out[i];std::memcpy(d.position,s.position,16);std::memcpy(d.last,s.last,8);d.stamp=s.stamp;d.flags=s.flags&0xffff003fu;}return out;}
    static std::vector<Command> oldCommands(std::span<const mp::Command> commands){std::vector<Command> out(commands.size());for(unsigned i=0;i<commands.size();++i){std::memcpy(out[i].delta,commands[i].delta,16);out[i].control[0]=commands[i].control[0];}return out;}
    MixedWorld(std::span<const mp::State> states,std::span<const mp::Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices,void* table):seed(states.begin(),states.end()),world(oldStates(states),oldCommands(commands),ranges,indices,table){
        for(unsigned i=0;i<states.size();++i){auto* object=world.objects[i].bytes.data();std::memcpy(object+0x2a0,states[i].offset,8);store(object,0x350,unsigned char((states[i].flags>>6)&1));store(object,0x513,unsigned char((states[i].flags>>7)&1));}
    }
    void advance(chlorium::research::movement::Oracle::Move movement,std::span<const mp::Command> commands,unsigned prefix){
        for(unsigned i=0;i<prefix;++i){const auto& c=commands[i];store(world.layer.data(),0x3e0,c.control[0]);
            if(c.control[1])for(auto* object:world.members[i])R71Rotation(object,&c);
            else movement(world.layer.data(),&world.arrays[i],c.delta[0],c.delta[1]);}
    }
    std::vector<mp::State> states()const{
        auto out=seed;const auto old=world.states(oldStates(seed));for(unsigned i=0;i<out.size();++i){auto& s=out[i];const auto& p=old[i];std::memcpy(s.position,p.position,16);std::memcpy(s.last,p.last,8);s.stamp=p.stamp;s.flags=p.flags;
            s.flags|=unsigned(world.objects[i].bytes[0x350]!=0)<<6;s.flags|=unsigned(world.objects[i].bytes[0x513]!=0)<<7;}return out;
    }
};
struct MixedGpu {
    GLuint shader=0,buffers[5]{};GLint count=-1,prefix=-1;Dispatch dispatch=nullptr;unsigned size=0;
    MixedGpu(const std::filesystem::path& source,std::span<const mp::State> seed):size(unsigned(seed.size())){
        shader=program(source);count=glGetUniformLocation(shader,"objectCount");prefix=glGetUniformLocation(shader,"prefixCount");require(count>=0 && prefix>=0,"mixed shader interface");
        dispatch=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));require(dispatch!=nullptr,"mixed compute available");glGenBuffers(5,buffers);
        for(unsigned lane:{0u,2u}){glBindBuffer(storage,buffers[lane]);glBufferData(storage,seed.size_bytes(),lane?nullptr:seed.data(),GL_DYNAMIC_DRAW);}require(glGetError()==GL_NO_ERROR,"mixed owned state storage");
    }
    ~MixedGpu(){glDeleteBuffers(5,buffers);if(shader)glDeleteProgram(shader);}
    std::vector<mp::State> run(std::span<const mp::State> seed,std::span<const mp::Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices,unsigned end,bool reseed){
        require(mp::fixtureInput(seed,commands,ranges,indices,_mm_getcsr()),"finite bounded mixed fixture inputs");glUseProgram(shader);
        auto upload=[&](unsigned lane,const auto values){glBindBuffer(storage,buffers[lane]);glBufferData(storage,values.size_bytes(),values.data(),GL_DYNAMIC_DRAW);};
        if(reseed)upload(0,seed);upload(1,commands);upload(3,ranges);upload(4,indices);
        for(unsigned lane=0;lane<5;++lane)glBindBufferBase(storage,lane,buffers[lane]);glUniform1ui(count,size);glUniform1ui(prefix,end);dispatch((size+63)/64,1,1);
        glMemoryBarrier(0x2000|GL_BUFFER_UPDATE_BARRIER_BIT);std::vector<mp::State> out(size);glBindBuffer(storage,buffers[2]);glGetBufferSubData(storage,0,out.size()*sizeof(mp::State),out.data());
        require(glGetError()==GL_NO_ERROR,"mixed owned dispatch/readback");std::swap(buffers[0],buffers[2]);return out;
    }
};
#ifndef CHLORIUM_R71_FIXTURE_ONLY
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4,"GD image, Cocos DLL and mixed.comp required");
    const auto saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);const auto cocos=loadNative(argv[2],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};r71PointCtor=reinterpret_cast<void*>(GetProcAddress(cocos,"??0CCPoint@cocos2d@@QEAA@MM@Z"));
    r71PointApply=reinterpret_cast<void*>(GetProcAddress(cocos,"?__CCPointApplyAffineTransform@cocos2d@@YA?AVCCPoint@1@AEBV21@AEBUCCAffineTransform@1@@Z"));
    require(r71PointCtor && r71PointApply && hash(static_cast<unsigned char*>(r71PointApply),0xae)==0x1e32bc1900bee292ull,"pinned native Cocos affine helper");
    chlorium::research::movement::Oracle movement(argv[1],reinterpret_cast<void*>(&countArray));std::array<void*,148> table{};
    table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    std::vector<mp::State> seed(1027);std::vector<mp::Command> commands(64);std::vector<Range> ranges(17);std::vector<unsigned> indices;
    for(unsigned i=0;i<commands.size();++i){auto& c=commands[i];c.control[0]=i/5;c.control[1]=i%3==0;c.control[2]=i%2;
        if(c.control[1]){const float angle=i%2?1.f:-1.f;c.linear[0]=c.linear[3]=i%4?0.f:1.f;c.linear[1]=angle;c.linear[2]=-angle;
            if(c.linear[0]){c.linear[1]=.125f;c.linear[2]=-.25f;}c.affineCenter[0]=float(int(i%7)-3)*.125f;c.affineCenter[1]=float(int(i%5)-2)*.25f;
            c.affineCenter[2]=float(i%11)*3.25f;c.affineCenter[3]=-float(i%13)*1.5f;c.move[0]=i%2?.1f:0.f;c.move[1]=i%3?-.125f:.5f;
        }else{c.delta[0]=i%4?double(int(i%17)-8)*.1:-0.;c.delta[1]=i%5?double(int(i%13)-6)*.125:0.;}}
    commands[0].linear[0]=commands[0].linear[1]=commands[0].linear[2]=commands[0].linear[3]=0;
    commands[0].affineCenter[0]=.1f;commands[0].affineCenter[1]=-.1f;
    for(unsigned group=0;group<ranges.size();++group){ranges[group]={unsigned(indices.size()),group*4};for(unsigned k=0;k<ranges[group][1];++k)indices.push_back(k && k%7==group%7?k-1:k);}
    for(unsigned i=0;i<seed.size();++i){auto& s=seed[i];s.position[0]=i%7==0?-0.:double(i)*.1;s.position[1]=i%9==0?0x1p-32:-double(i)*.125;
        s.offset[0]=float(i%5)*.125f;s.offset[1]=-float(i%7)*.25f;s.stamp=i%4;s.flags=((i%ranges.size())<<16)|(i%11==0?1u:0u)|(i%13==0?2u:0u)|(i%17==0?192u:0u);
        s.last[0]=0x7fc12345u;s.last[1]=0x80000000u;}
    seed[19].position[0]=0x1p32;seed[31].position[0]=0x1p-32;
    require(mp::fixtureInput(seed,commands,ranges,indices,0x1f80),"mixed admitted corpus");MixedGpu gpu(argv[3],seed);std::uint64_t comparisons=0;
    for(unsigned mode:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned prefix=0;prefix<=commands.size();++prefix){
        MixedWorld native(seed,commands,ranges,indices,table.data());_mm_setcsr(mode);native.advance(movement.native(),commands,prefix);const auto expected=native.states();_mm_setcsr(mode);
        if(mode==0x1f80 && prefix==1){const double simplified=double(.1f)+double(seed[19].offset[0]);require(expected[19].position[0]!=simplified,"native cancellation differs from simplified affine assignment");}
        const auto actual=gpu.run(seed,commands,ranges,indices,prefix,true);
        for(unsigned i=0;i<actual.size();++i)if(std::memcmp(&actual[i],&expected[i],sizeof(mp::State))){std::cerr<<"prefix="<<prefix<<" actor="<<i<<" mode="<<std::hex<<mode<<std::dec<<" expected="<<std::hexfloat<<expected[i].position[0]<<','<<expected[i].position[1]<<" actual="<<actual[i].position[0]<<','<<actual[i].position[1]<<'\n';throw std::runtime_error("mixed native/GPU root-state mismatch");}
        comparisons+=actual.size();
    }
    MixedWorld native(seed,commands,ranges,indices,table.data());
    for(unsigned frame=0;frame<4;++frame){for(unsigned i=0;i<commands.size();++i)commands[i].control[0]=frame*100+i/5;_mm_setcsr(0x1f80);native.advance(movement.native(),commands,unsigned(commands.size()));const auto expected=native.states();_mm_setcsr(0x1f80);
        const auto actual=gpu.run(seed,commands,ranges,indices,unsigned(commands.size()),frame==0);require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(mp::State)),"four mixed resident recurrences without position upload");comparisons+=actual.size();}
    std::cout<<"PASS "<<comparisons<<" native/GPU mixed translation/root-rotation states; 4 resident recurrences; orientation, callbacks, reader closure and GD FPS remain unqualified\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#endif
