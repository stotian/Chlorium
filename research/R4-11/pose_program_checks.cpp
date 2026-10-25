#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "PoseProgram.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <vector>
namespace pose=chlorium::research::pose;
std::size_t checks=0;
void require(bool ok,const char* reason) { ++checks; if(!ok) throw std::runtime_error(reason); }
std::uint64_t hash(std::span<const unsigned char> data) {
    std::uint64_t h=14695981039346656037ull; for(auto b:data) h=(h^b)*1099511628211ull; return h;
}
struct Image {
    std::vector<unsigned char> bytes;
    IMAGE_NT_HEADERS64 header{};
    std::vector<IMAGE_SECTION_HEADER> sections;
    explicit Image(const char* path,unsigned timestamp,unsigned size) {
        std::ifstream file(path,std::ios::binary); bytes={std::istreambuf_iterator<char>(file),{}};
        require(bytes.size()>sizeof(IMAGE_DOS_HEADER),"read image");
        IMAGE_DOS_HEADER dos; std::memcpy(&dos,bytes.data(),sizeof(dos));
        require(dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>0 &&
            std::size_t(dos.e_lfanew)+sizeof(header)<=bytes.size(),"bounded PE header");
        std::memcpy(&header,bytes.data()+dos.e_lfanew,sizeof(header));
        require(header.Signature==IMAGE_NT_SIGNATURE && header.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
            header.FileHeader.TimeDateStamp==timestamp && header.OptionalHeader.SizeOfImage==size,"known image");
        const auto offset=std::size_t(dos.e_lfanew)+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+header.FileHeader.SizeOfOptionalHeader;
        require(offset<=bytes.size() && header.FileHeader.NumberOfSections<=(bytes.size()-offset)/sizeof(IMAGE_SECTION_HEADER),"bounded sections");
        sections.resize(header.FileHeader.NumberOfSections);
        std::memcpy(sections.data(),bytes.data()+offset,sections.size()*sizeof(IMAGE_SECTION_HEADER));
    }
    std::span<const unsigned char> at(std::size_t rva,std::size_t count) const {
        for(auto s:sections) if(rva>=s.VirtualAddress) {
            const auto offset=rva-s.VirtualAddress,raw=std::size_t(s.PointerToRawData);
            if(offset<=s.SizeOfRawData && count<=s.SizeOfRawData-offset && raw<=bytes.size() &&
               offset<=bytes.size()-raw && count<=bytes.size()-raw-offset) return {bytes.data()+raw+offset,count};
        }
        throw std::runtime_error("RVA outside raw image");
    }
};
extern "C" pose::LinearPose* ChloriumPoseOracle(pose::LinearPose*,const pose::Matrix*,void*);
struct Oracle {
    unsigned char* code=nullptr;
    HMODULE crt=nullptr;
    pose::Concat concat=nullptr;
    pose::Atan2 atan2=nullptr;
    void* block=nullptr;
    Oracle(const char* game,const char* cocos) {
        const Image gd(game,0x69702da0,0xa3a000),cc(cocos,0x69702d99,0x1c5000);
        const auto affine=cc.at(0x6040,0xbd),decomp=gd.at(0x229a9f,0xc8);
        require(hash(affine)==0x0ebad0c4e794e24bull,"native affine body hash");
        require(hash(decomp)==0xa51fe5f26fff5ea7ull,"native Area decomposition block hash");
        crt=LoadLibraryExW(L"ucrtbase.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(crt!=nullptr,"system CRT loaded for isolated oracle");
        atan2=reinterpret_cast<pose::Atan2>(GetProcAddress(crt,"atan2f"));
        auto sqrt=GetProcAddress(crt,"sqrtf"); require(atan2&&sqrt,"native CRT math exports");
        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        require(code!=nullptr,"private oracle allocation");
        std::memcpy(code,affine.data(),affine.size());
        block=code+1024; auto p=code+1028;
        // The copied mid-function block calls CRT functions. Give it its own
        // shadow space so their parameter homes cannot overwrite our return.
        const unsigned char enter[]{0x48,0x83,0xec,0x20},leave[]{0x48,0x83,0xc4,0x20,0xc3};
        std::memcpy(code+1024,enter,sizeof(enter));
        std::memcpy(p,decomp.data(),decomp.size());std::memcpy(p+decomp.size(),leave,sizeof(leave));
        const auto thunk=[&](std::size_t offset,std::uintptr_t function) {
            code[offset]=0xff;code[offset+1]=0x25;
            std::memset(code+offset+2,0,4);std::memcpy(code+offset+6,&function,8);
        };
        thunk(512,reinterpret_cast<std::uintptr_t>(atan2));thunk(528,reinterpret_cast<std::uintptr_t>(sqrt));
        for(auto call:std::array<std::pair<unsigned,unsigned>,4>{{{0x35,528},{0x78,528},{0x87,512},{0xad,512}}}) {
            require(p[call.first]==0xe8,"native direct call opcode");
            auto displacement=static_cast<std::int32_t>(call.second)-(1028+call.first+5);
            std::memcpy(p+call.first+1,&displacement,4);
        }
        const unsigned char sub90[]{0xf3,0x0f,0x5c,0x35};
        require(!std::memcmp(p+0x9c,sub90,4),"native 90 degree RIP load");
        const auto constant=gd.at(0x623294,4);std::uint32_t bits;
        std::memcpy(&bits,constant.data(),4); require(bits==0x42b40000u,"native 90 constant");
        std::memcpy(code+768,constant.data(),4);
        const auto displacement=std::int32_t(768-(1028+0x9c+8));std::memcpy(p+0xa0,&displacement,4);
        DWORD previous;require(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&previous)!=0,"oracle W then RX");
        require(FlushInstructionCache(GetCurrentProcess(),code,4096)!=0,"instruction visibility");
        concat=reinterpret_cast<pose::Concat>(code);
    }
    ~Oracle() { if(code) VirtualFree(code,0,MEM_RELEASE); if(crt) FreeLibrary(crt); }
    pose::LinearPose linear(const pose::Matrix& matrix) const {
        pose::LinearPose output;ChloriumPoseOracle(&output,&matrix,block);return output;
    }
};
const Oracle* oracle=nullptr;
pose::LinearPose nativeLinear(const pose::Matrix& m) { return oracle->linear(m); }
volatile float sink=0;
void checkFused(const char* path,const Oracle& actual,const char* timingPath) {
    const auto module=LoadLibraryExA(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(module!=nullptr,"Cocos oracle loaded in isolated fixture");
    const auto base=reinterpret_cast<unsigned char*>(module);
    require(hash({base+0x37f0,0x468})==0x7abc292bc9db4855ull,"actual node matrix routine");
    const auto cosine=*reinterpret_cast<pose::Trig*>(base+0xe4ac8),sine=*reinterpret_cast<pose::Trig*>(base+0xe4ad0);
    using Transform=pose::Matrix* (*)(void*,pose::Matrix*);
    const auto transform=reinterpret_cast<Transform>(base+0x37f0);
    alignas(16) std::array<unsigned char,0x200> node{};
    const auto put=[&](unsigned offset,auto value) {std::memcpy(node.data()+offset,&value,sizeof(value));};
    const auto native=[&](const pose::RawPose& raw,const pose::Matrix& parent) {
        put(0x38,raw.rx);put(0x3c,raw.ry);put(0x40,raw.sx);put(0x44,raw.sy);put(0x4c,raw.x);put(0x50,raw.y);
        node[0x111]=node[0x112]=1;
        pose::Matrix local;transform(node.data(),&local);pose::Pose result;
        actual.concat(&result.matrix,&local,&parent);result.linear=actual.linear(result.matrix);return result;
    };
    const auto saved=_mm_getcsr();std::mt19937 random(0x229930);
    const pose::Matrix parent{.75f,.25f,-.125f,1.5f,-200,140};
    for(unsigned mode=0;mode<16;++mode) {
        const auto csr=(saved&~0xe07fu)|0x1f80u|((mode&3)<<13)|((mode&4)?0x8000u:0)|((mode&8)?0x40u:0);
        for(unsigned n=0;n<4096;++n) {
            const float angle=float(int(random()%72001)-36000)/100.f;
            pose::RawPose raw{angle,n%3?angle:-angle,float(int(random()%800)-400)/100.f,
                float(int(random()%800)-400)/100.f,float(n)*.3f,-float(n)*.7f};
            if(n<16) {raw.rx=(n&1)?-0.f:0.f;raw.ry=(n&2)?-0.f:0.f;raw.sx=(n&4)?-1.f:1.f;raw.sy=(n&8)?-0.f:0.f;}
            _mm_setcsr(csr);const auto reference=native(raw,parent);const auto flags=_mm_getcsr()&63;
            _mm_setcsr(csr);const auto candidate=pose::fused(raw,parent,cosine,sine,actual.atan2);
            require(!std::memcmp(&reference,&candidate,sizeof(reference)),"fused raw pose equals actual Cocos plus GD block");
            require((_mm_getcsr()&63)==flags,"fused pose FP flags");
        }
    }
    _mm_setcsr(saved);
    std::vector<pose::RawPose> inputs(16384);std::vector<pose::Pose> output(inputs.size());
    for(std::size_t n=0;n<inputs.size();++n) {
        const auto x=float(n+1);
        inputs[n]={x*.017f,x*-.013f,1.f+x*.0001f,1.f-x*.00001f,x*.3f,-x*.5f};
    }
    std::array<double,9> nativeTime{},fusedTime{};
    for(unsigned batch=0;batch<9;++batch) for(unsigned order=0;order<2;++order) {
        const bool optimized=((batch+order)&1)!=0;const auto begin=std::chrono::steady_clock::now();
        for(std::size_t n=0;n<inputs.size();++n) {
            if(optimized) output[n]=pose::fused(inputs[n],parent,cosine,sine,actual.atan2);
            else {
                const auto& raw=inputs[n];
                put(0x38,raw.rx);put(0x3c,raw.ry);put(0x40,raw.sx);put(0x44,raw.sy);put(0x4c,raw.x);put(0x50,raw.y);
                node[0x111]=node[0x112]=1;
                pose::Matrix local;transform(node.data(),&local);actual.concat(&output[n].matrix,&local,&parent);
                // Both timing paths use the validated C++ decomposition. The
                // native-block oracle's register-save wrapper is not timed.
                output[n].linear=pose::decompose(output[n].matrix,actual.atan2);
            }
        }
        sink=output.back().matrix.a;
        const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
        (optimized?fusedTime:nativeTime)[batch]=us;
    }
    std::sort(nativeTime.begin(),nativeTime.end());std::sort(fusedTime.begin(),fusedTime.end());
    std::ofstream timings(std::string(timingPath)+".fused.csv");
    timings<<"inputs,native_helper_fixture_us,fused_fixture_us\n"<<inputs.size()<<','<<nativeTime[4]<<','<<fusedTime[4]<<'\n';
    timings.flush();require(bool(timings),"fused timing output complete");
    std::cout<<"Fused helper-node pipeline: 65536 native comparisons, no recipe cache\n";
    FreeLibrary(module);
}
int main(int argc,char** argv) {
    const auto original=_mm_getcsr();
    try {
        require(argc==4,"game, Cocos and timing output arguments"); Oracle actual(argv[1],argv[2]);oracle=&actual;
        checkFused(argv[2],actual,argv[3]);
        std::mt19937 random(0xafe2ca05);
        for(unsigned control=0;control<16;++control) {
            const auto csr=(original&~0xe07fu)|0x1f80u|((control&3)<<13)|((control&4)?0x8000u:0)|((control&8)?0x40u:0);
            for(unsigned n=0;n<8192;++n) {
                pose::Matrix l{},r{};
                for(auto m:{&l,&r}) for(float* f=&m->a;f!=&m->a+6;++f)
                    *f=std::bit_cast<float>((random()&0x807fffffu)|((80+random()%80)<<23));
                if(n%8==0) l.b=std::bit_cast<float>(n%16?0u:0x80000000u);
                _mm_setcsr(csr);pose::Matrix reference;actual.concat(&reference,&l,&r);const auto nativeFlags=_mm_getcsr()&63;
                _mm_setcsr(csr);const auto scalar=pose::scalar(l,r);
                require(!std::memcmp(&reference,&scalar,sizeof(reference)),"scalar composition exact native pixels/values");
                require((_mm_getcsr()&63)==nativeFlags,"scalar composition FP flags");
                _mm_setcsr(csr);const auto packed=pose::packedLinear(l,r);
                require(!std::memcmp(&reference,packed.data(),16),"packed composition exact native values");
                _mm_setcsr(csr);const auto expected=actual.linear(reference);const auto flags=_mm_getcsr()&63;
                _mm_setcsr(csr);const auto candidate=pose::decompose(reference,actual.atan2);
                require(!std::memcmp(&expected,&candidate,sizeof(expected)),"decomposition exact native block");
                require((_mm_getcsr()&63)==flags,"decomposition FP flags");
            }
        }
        _mm_setcsr(original);
        pose::Recipes<2,8> table;require(!table.intern({1,2}).stored,"phase required");table.begin();
        require(!table.intern({1,2}).hit&&table.intern({1,2}).hit,"exact recipe reuse");
        require(!table.intern({0,0}).hit&&!table.intern({0x80000000,0}).hit,"signed zero distinct recipe");
        table.begin();require(!table.intern({1,2}).hit,"phase invalidation");
        for(unsigned n=0;n<32;++n) table.intern({n,7});require(table.overflow()>0,"bounded overflow reported");
        auto program=std::make_unique<pose::Program<>>();
        std::vector<pose::Matrix> inputs(65536);std::vector<pose::Pose> result(inputs.size()),reference(inputs.size());
        const pose::Matrix parent{0.75f,-0.4f,0.2f,1.1f,150.f,-70.f};
        std::ofstream timings(argv[3]);timings<<"recipes,native_fixture_us,program_fixture_us,recipe_compilations,reused,fallback,stopped_at\n";
        for(unsigned recipes:{1u,16u,256u,65536u}) {
            for(std::size_t n=0;n<inputs.size();++n) {
                const auto p=float(n%recipes+1);
                inputs[n]={p*.001f,p*.002f,-p*.004f,p*.003f,float(n)*.3f,float(n)*-.2f};
                actual.concat(&reference[n].matrix,&inputs[n],&parent);reference[n].linear=actual.linear(reference[n].matrix);
            }
            auto work=program->evaluate(inputs,parent,result,actual.concat,nativeLinear,actual.atan2);
            for(std::size_t n=0;n<inputs.size();++n) require(!std::memcmp(&result[n],&reference[n],sizeof(pose::Pose)),"program equals native sequence");
            std::array<double,7> nativeTime{},programTime{};
            for(unsigned trial=0;trial<7;++trial) for(unsigned order=0;order<2;++order) {
                const bool optimized=((trial+order)&1)!=0;const auto begin=std::chrono::steady_clock::now();
                if(optimized) work=program->evaluate(inputs,parent,result,actual.concat,nativeLinear,actual.atan2);
                else for(std::size_t n=0;n<inputs.size();++n) {
                    actual.concat(&result[n].matrix,&inputs[n],&parent);result[n].linear=actual.linear(result[n].matrix);
                }
                sink=result.back().linear.rotationX;
                auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
                (optimized?programTime:nativeTime)[trial]=us;
            }
            std::sort(nativeTime.begin(),nativeTime.end());std::sort(programTime.begin(),programTime.end());
            if(recipes==65536) require(work.stoppedAt==1024,"unique phase compiler stops before dense overhead");
            else require(work.reused==inputs.size()-recipes && !work.stoppedAt,"shared recipes compile once");
            timings<<recipes<<','<<nativeTime[3]<<','<<programTime[3]<<','<<work.recipes<<','<<work.reused<<','<<work.fallback<<','<<work.stoppedAt<<'\n';
        }
        timings.flush();require(bool(timings),"timing output complete");
        std::cout<<"PASS "<<checks<<" native pose checks; snapshots only, no FPS claim\n";
        _mm_setcsr(original);return 0;
    } catch(const std::exception& e) { _mm_setcsr(original);std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1; }
}
