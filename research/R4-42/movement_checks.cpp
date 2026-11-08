#include "../R4-33/GpuFixture.hpp"
#include "../R4-38/NativeMovement.hpp"
#include "GroupTape.hpp"
#include <chrono>
#include <iomanip>
using namespace chlorium::research::grouptape;
constexpr GLenum storage=0x90d2;
using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);
template<class T> T load(const void* object,unsigned offset){T value;std::memcpy(&value,static_cast<const unsigned char*>(object)+offset,sizeof(value));return value;}
template<class T> void store(void* object,unsigned offset,T value){std::memcpy(static_cast<unsigned char*>(object)+offset,&value,sizeof(value));}
struct alignas(64) Object {std::array<unsigned char,0x600> bytes{};};
struct ArrayData {unsigned count=0,capacity=0;void* padding=nullptr;void** pointers=nullptr;};
struct Array {std::array<unsigned char,0x38> padding{};ArrayData* data=nullptr;};
static_assert(offsetof(Array,data)==0x38&&offsetof(ArrayData,pointers)==0x10);
struct World {
    std::array<unsigned char,0x4000> layer{};
    std::vector<Object> objects;
    std::vector<std::vector<void*>> members;
    std::vector<ArrayData> data;
    std::vector<Array> arrays;
    unsigned sections=0,ends=0;bool mutating=false;
    World(std::span<const State> states,std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices,void* table):objects(states.size()),members(commands.size()),data(commands.size()),arrays(commands.size()){
        store(layer.data(),0,table);store(layer.data(),0x3f00,this);
        for(unsigned i=0;i<states.size();++i){const auto range=ranges[states[i].flags>>16];for(unsigned k=0;k<range[1];++k)members[indices[range[0]+k]].push_back(objects[i].bytes.data());}
        for(unsigned i=0;i<commands.size();++i){data[i]={static_cast<unsigned>(members[i].size()),static_cast<unsigned>(members[i].size()),nullptr,members[i].data()};arrays[i].data=&data[i];}
        for(auto& object:objects)store(object.bytes.data(),0x278,-1);
        reset(states);
    }
    void reset(std::span<const State> states){
        sections=ends=0;store(layer.data(),0x3724,0u);
        scatter(states);
    }
    void scatter(std::span<const State> states){
        for(unsigned i=0;i<states.size();++i){auto* object=objects[i].bytes.data();const auto& state=states[i];
            store(object,0x3b8,state.position[0]);store(object,0x3c0,state.position[1]);std::memcpy(object+0x4d0,state.last,8);store(object,0x4dc,state.stamp);
            for(auto [bit,offset]:{std::pair{0u,0x512u},std::pair{1u,0x2c8u},std::pair{2u,0x368u},std::pair{3u,0x369u},std::pair{4u,0x351u},std::pair{5u,0x352u}})object[offset]=(state.flags>>bit)&1;
        }
    }
    std::vector<State> states(std::span<const State> original)const{
        auto result=std::vector<State>(original.begin(),original.end());
        for(unsigned i=0;i<result.size();++i){const auto* object=objects[i].bytes.data();auto& state=result[i];
            state.position[0]=load<double>(object,0x3b8);state.position[1]=load<double>(object,0x3c0);std::memcpy(state.last,object+0x4d0,8);state.stamp=load<unsigned>(object,0x4dc);state.flags&=0xffff0000u;
            for(auto [bit,offset]:{std::pair{0u,0x512u},std::pair{1u,0x2c8u},std::pair{2u,0x368u},std::pair{3u,0x369u},std::pair{4u,0x351u},std::pair{5u,0x352u}})state.flags|=unsigned(object[offset]!=0)<<bit;
        }return result;
    }
    void run(chlorium::research::movement::Oracle::Move native,std::span<const Command> commands,unsigned prefix){for(unsigned i=0;i<prefix;++i){store(layer.data(),0x3e0,commands[i].control[0]);native(layer.data(),&arrays[i],commands[i].delta[0],commands[i].delta[1]);}}
};
__declspec(noinline) unsigned countArray(void* pointer){return static_cast<Array*>(pointer)->data->count;}
__declspec(noinline) void section(void* layer,void*){auto& world=*load<World*>(layer,0x3f00);if(world.mutating&&!world.sections&&world.objects.size()>1){auto* future=world.objects.back().bytes.data();store(future,0x3b8,load<double>(future,0x3b8)+17.0);}++world.sections;}
__declspec(noinline) void end(void* layer,void*){++load<World*>(layer,0x3f00)->ends;}
std::size_t differences(std::span<const State> a,std::span<const State> b){require(a.size()==b.size(),"same GPU/native state count");std::size_t changed=0;for(unsigned i=0;i<a.size();++i)changed+=std::memcmp(&a[i],&b[i],sizeof(State))!=0;return changed;}
struct Gpu {
    GLuint shader=0,buffers[5]{};GLint count=-1,prefix=-1;unsigned size=0;Dispatch compute=nullptr;
    Gpu(const std::filesystem::path& path,std::span<const State> states,std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices):size(static_cast<unsigned>(states.size())){
        compute=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));require(compute!=nullptr,"group tape compute dispatch");
        shader=program(path);glUseProgram(shader);count=glGetUniformLocation(shader,"objectCount");prefix=glGetUniformLocation(shader,"prefixCount");require(count>=0&&prefix>=0,"GPU group tape uniforms");glGenBuffers(5,buffers);
        auto upload=[&](unsigned binding,const void* data,std::size_t bytes){glBindBuffer(storage,buffers[binding]);glBufferData(storage,std::max<std::size_t>(bytes,4),data,GL_DYNAMIC_DRAW);glBindBufferBase(storage,binding,buffers[binding]);};
        upload(0,states.data(),states.size_bytes());upload(1,commands.data(),commands.size_bytes());upload(2,nullptr,states.size_bytes());upload(3,ranges.data(),ranges.size_bytes());upload(4,indices.data(),indices.size_bytes());require(glGetError()==GL_NO_ERROR,"GPU group tape storage setup");
    }
    ~Gpu(){glDeleteBuffers(5,buffers);glDeleteProgram(shader);}
    void dispatch(std::span<const Command> commands,unsigned amount){glUseProgram(shader);glBindBuffer(storage,buffers[1]);if(!commands.empty())glBufferSubData(storage,0,commands.size_bytes(),commands.data());glUniform1ui(count,size);glUniform1ui(prefix,amount);compute((size+63)/64,1,1);glMemoryBarrier(0x2000|GL_BUFFER_UPDATE_BARRIER_BIT);}
    void readInto(std::span<State> result){require(result.size()==size,"exact movement materialization size");glBindBuffer(storage,buffers[2]);glGetBufferSubData(storage,0,result.size_bytes(),result.data());require(glGetError()==GL_NO_ERROR,"GPU group tape result read");}
    std::vector<State> read(){std::vector<State> result(size);readInto(result);return result;}
};
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4||argc==5,"GD-image shader NEW-receipt [--timings]");if(argc==5)require(std::wstring_view(argv[4])==L"--timings","explicit isolated timings");
    require(!std::filesystem::exists(argv[3]),"refusing an existing group tape receipt");
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create();require(GLEW_ARB_gpu_shader_fp64,"exact double movement requires GPU FP64");
    chlorium::research::movement::Oracle oracle(std::filesystem::path(argv[1]),reinterpret_cast<void*>(&countArray));std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    std::uint64_t comparisons=0;unsigned rejected=0,mutants=0;
    std::ofstream timing;if(argc==5){timing.open(std::filesystem::path(argv[3]).wstring()+L".kernel.csv",std::ios::out|std::ios::noreplace);require(bool(timing),"new group tape kernel timings");timing<<"objects,combinations,commands,round,variant,wall_ms,gpu_ms,game_fps_evidence\n";}
    for(unsigned objectCount:{65u,4096u})for(unsigned combinations:{1u,16u}){
        std::vector<Command> commands(64);for(unsigned k=0;k<commands.size();++k){auto& command=commands[k];command.delta[0]=k%7==0?-0.0:k%3==0?-4294967296.0:k%3==1?4294967296.0:0x1p-32;command.delta[1]=k%5==0?0.0:double(int(k%13)-6)*.1;command.control[0]=k%9==0?0x80000000u:k/2;command.control[1]=k%8;}
        std::vector<Range> ranges;std::vector<unsigned> indices;
        for(unsigned group=0;group<combinations;++group){const auto start=static_cast<unsigned>(indices.size());for(unsigned k=0;k<commands.size();++k)if(combinations==1||commands[k].control[1]==group%8||commands[k].control[1]==(group+3)%8)indices.push_back(k);ranges.push_back({start,static_cast<unsigned>(indices.size())-start});}
        std::vector<State> initial(objectCount);for(unsigned i=0;i<objectCount;++i){auto& state=initial[i];state.position[0]=i%5==0?-0.0:i%7==0?4294967296.0:double(i)*.1;state.position[1]=i%3==0?-double(i):0x1p-32;state.last[0]=0x7fc12345u;state.last[1]=0x80000000u;state.stamp=i%4;state.flags=((i%combinations)<<16)|(i%11==0?1u:0u)|(i%13==0?2u:0u)|(i%17==0?12u:0u);}
        require(arithmeticEligible(initial,commands,ranges,indices,0x1f80),"supported group tape arithmetic");
        auto reject=[&](bool condition){require(!condition,"unsafe group tape input accepted");++rejected;};
        for(unsigned controls:{0u,0x1f00u,0x3f80u,0x5f80u,0x7f80u})reject(arithmeticEligible(initial,commands,ranges,indices,controls));
        auto bad=initial;bad[0].position[0]=std::bit_cast<double>(0x7ff8000000000001ull);reject(arithmeticEligible(bad,commands,ranges,indices,0x1f80));bad=initial;bad[0].flags|=0x40;reject(arithmeticEligible(bad,commands,ranges,indices,0x1f80));bad=initial;bad[0].flags=64u<<16;reject(arithmeticEligible(bad,commands,ranges,indices,0x1f80));
        auto wrong=indices;wrong[1]=wrong[0];reject(arithmeticEligible(initial,commands,ranges,wrong,0x1f80));
        World world(initial,commands,ranges,indices,table.data());Gpu gpu(std::filesystem::path(argv[2]),initial,commands,ranges,indices);
        for(unsigned controls:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned prefix=0;prefix<=commands.size();++prefix){
            _mm_setcsr(controls);world.reset(initial);world.run(oracle.native(),commands,prefix);const auto expected=world.states(initial);require(world.ends==prefix,"native per-command terminal callbacks preserved in reference");gpu.dispatch(commands,prefix);const auto actual=gpu.read();if(differences(actual,expected)){std::cerr<<"mismatch objects="<<objectCount<<" combinations="<<combinations<<" prefix="<<prefix<<" controls="<<std::hex<<controls<<std::dec<<'\n';for(unsigned i=0;i<actual.size();++i)if(std::memcmp(&actual[i],&expected[i],sizeof(State))){std::cerr<<"state "<<i<<" expected xy "<<std::hexfloat<<expected[i].position[0]<<','<<expected[i].position[1]<<" actual "<<actual[i].position[0]<<','<<actual[i].position[1]<<std::defaultfloat<<'\n';break;}throw std::runtime_error("GPU group tape differs from native prefix");}comparisons+=initial.size();
        }
        world.reset(initial);world.mutating=true;world.run(oracle.native(),commands,64);gpu.dispatch(commands,64);require(differences(gpu.read(),world.states(initial))>0,"mutating native observer must invalidate GPU-only movement");world.mutating=false;++mutants;
        if(argc==5)for(unsigned round=0;round<10;++round)for(unsigned order=0;order<2;++order){const auto variant=order^(round&1);_mm_setcsr(0x1f80);world.reset(initial);glFinish();GLuint query=0;glGenQueries(1,&query);const auto start=std::chrono::steady_clock::now();GLuint64 ns=0;
            if(variant){glBeginQuery(0x88bf,query);gpu.dispatch(commands,64);glEndQuery(0x88bf);glFinish();glGetQueryObjectui64v(query,GL_QUERY_RESULT,&ns);}else world.run(oracle.native(),commands,64);
            const auto end=std::chrono::steady_clock::now();glDeleteQueries(1,&query);require(glGetError()==GL_NO_ERROR,"GPU group tape timing errors");if(round)timing<<objectCount<<','<<combinations<<",64,"<<round<<','<<variant<<','<<std::chrono::duration<double,std::milli>(end-start).count()<<','<<double(ns)/1e6<<",false\n";
        }
    }
    std::ofstream receipt(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(receipt),"new GPU group tape receipt");receipt<<"{\"diagnostic_only\":true,\"native_prefix_states_compared\":"<<comparisons<<",\"arithmetic_rejections\":"<<rejected<<",\"observer_mutants_detected\":"<<mutants<<",\"native_callbacks_reproduced_on_gpu\":false,\"cpu_dependency_closure_proven\":false,\"game_fps_evidence\":false,\"production_eligible\":false}\n";require(bool(receipt),"complete group tape receipt");
    std::cout<<"PASS "<<comparisons<<" native/GPU movement prefix states; "<<rejected<<" arithmetic guards; "<<mutants<<" mutating-observer mutants detected. GPU arithmetic only, NOT a replacement of native callbacks or game state\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
