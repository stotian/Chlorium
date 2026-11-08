#include "NativeMovement.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <xmmintrin.h>
using chlorium::research::movement::Oracle;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class T> T get(const void* bytes,unsigned offset){T value;std::memcpy(&value,static_cast<const unsigned char*>(bytes)+offset,sizeof(T));return value;}
template<class T> void put(void* bytes,unsigned offset,T value){std::memcpy(static_cast<unsigned char*>(bytes)+offset,&value,sizeof(T));}
struct alignas(64) Node{std::array<unsigned char,4096> bytes{};};
struct Fixture;
struct Data{unsigned count=0,capacity=0;void* padding=nullptr;void** entries=nullptr;};
struct Array{Fixture* fixture=nullptr;std::array<unsigned char,0x30> padding{};Data* data=nullptr;};
static_assert(offsetof(Data,entries)==0x10&&offsetof(Array,data)==0x38);
struct Fixture{
    std::array<unsigned char,0x4000> layer{};
    std::vector<Node> objects;
    std::vector<void*> slots;
    Data data;Array array;
    std::vector<std::array<std::uint64_t,6>> log;
    unsigned sectionCalls=0,endCalls=0,countCalls=0,mode=0;bool record=true;
    Fixture(unsigned size,void* table):objects(size),slots(size){
        put(layer.data(),0,table);put(layer.data(),0x3f00,this);put(layer.data(),0x3e0,13u);
        data={size,size,nullptr,slots.data()};array.fixture=this;array.data=&data;log.reserve(size+1);
        for(unsigned i=0;i<size;++i){auto p=objects[i].bytes.data();slots[i]=p;
            put(p,0x10,i);put(p,0x278,static_cast<int>(i%11));put(p,0x27c,static_cast<int>(i%17));
            put(p,0x3b8,static_cast<double>(i*31));put(p,0x3c0,static_cast<double>(i*17));
            put(p,0x4dc,i%3?12u:13u);p[0x512]=i%5==0;p[0x2c8]=i%7==0;
        }
    }
    void scramble(){std::uint32_t seed=0x4c38;for(std::size_t i=slots.size();i>1;--i){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;std::swap(slots[i-1],slots[seed%i]);}}
    void trace(void* p){if(record)log.push_back({get<unsigned>(p,0x10),get<std::uint64_t>(p,0x3b8),get<std::uint64_t>(p,0x3c0),get<std::uint64_t>(p,0x4d0),get<unsigned>(p,0x4dc),get<unsigned short>(p,0x351)});}
};
__declspec(noinline) unsigned countArray(void* pointer){auto& a=*static_cast<Array*>(pointer);++a.fixture->countCalls;return a.data->count;}
__declspec(noinline) void section(void* layer,void* object){
    auto& f=*get<Fixture*>(layer,0x3f00);++f.sectionCalls;f.trace(object);
    put(object,0x278,get<int>(object,0x278)+1);
    put(layer,0x700,get<std::uint64_t>(layer,0x700)^get<std::uint64_t>(object,0x3b8)^get<std::uint64_t>(object,0x3c0));
    if(f.mode==1&&f.sectionCalls==1&&f.objects.size()>20){
        // A callback can alter future members, array ordering, stamps and native loop inputs.
        std::swap(f.slots[16],f.slots[17]);auto future=f.objects.back().bytes.data();
        put(future,0x3b8,std::bit_cast<double>(0x40123456789abcdeull));future[0x2c8]=0;
        put(layer,0x3e0,14u);f.data.count=1;
    }
    if(f.mode==2&&f.sectionCalls%7==0)_mm_setcsr((_mm_getcsr()&~0x6000u)|((f.sectionCalls%4)<<13)|0x21);
}
__declspec(noinline) void end(void* layer,void* array){
    auto& f=*get<Fixture*>(layer,0x3f00);++f.endCalls;
    require(array==&f.array,"original array callback argument");
    if(f.record)f.log.push_back({~0ull,f.sectionCalls,f.data.count,0,0,0});
}
void same(const Fixture& a,const Fixture& b){
    require(a.log==b.log&&a.sectionCalls==b.sectionCalls&&a.endCalls==b.endCalls&&a.countCalls==b.countCalls,"native callback count/order/intermediate states");
    require(std::memcmp(a.layer.data(),b.layer.data(),0x3f00)==0,"all native layer bytes");
    require(a.objects.size()==b.objects.size(),"fixture sizes");
    for(unsigned i=0;i<a.objects.size();++i)require(a.objects[i].bytes==b.objects[i].bytes,"all object bytes including unvisited members");
}
int main(int argc,char** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==2||argc==4,"usage: movement_checks GD-image [--timings NEW.csv]");
    if(argc==4)require(std::string_view(argv[2])=="--timings","explicit kernel timings switch");
    Oracle oracle(std::filesystem::path(argv[1]),reinterpret_cast<void*>(&countArray));
    std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    const auto savedFP=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{savedFP};
    const std::array<std::pair<double,double>,7> deltas{{{0.,-0.},{1.25,-2.5},{-4.,8.},{std::bit_cast<double>(1ull),std::bit_cast<double>(0x8000000000000001ull)},{std::bit_cast<double>(0x7ff0000000000000ull),0.},{std::bit_cast<double>(0x7ff0000000000001ull),1.},{0.,std::bit_cast<double>(0xfff8000000001234ull)}}};
    unsigned cases=0;
    for(unsigned variant=0;variant<2;++variant)for(unsigned fp=0;fp<32;++fp)for(unsigned mode=0;mode<3;++mode)for(unsigned size:{0u,1u,17u,33u,257u})for(auto [dx,dy]:deltas){
        Fixture a(size,table.data()),b(size,table.data());a.mode=b.mode=mode;
        if(size>32){a.scramble();b.scramble();a.slots[31]=b.slots[31]=nullptr;}
        if(size){const auto raw=0x7ff0000000000042ull;put(a.objects[0].bytes.data(),0x3b8,raw);put(b.objects[0].bytes.data(),0x3b8,raw);}
        const unsigned controls=(savedFP&~0xe07fu)|((fp&3)<<13)|((fp&4)?0x40:0)|((fp&8)?0x8000:0)|((fp&16)?0x21:0)|0x1f80;
        _mm_setcsr(controls);oracle.native()(a.layer.data(),&a.array,dx,dy);const auto nativeFP=_mm_getcsr();
        _mm_setcsr(controls);(variant?oracle.prefetch():oracle.trampoline())(b.layer.data(),&b.array,dx,dy);const auto actualFP=_mm_getcsr();
        require(nativeFP==actualFP,"native MXCSR flags and control parity");same(a,b);
        require(a.countCalls==1&&a.endCalls==1,"native count and final callback retained");++cases;
    }
    Fixture absent(1,table.data());oracle.prefetch()(absent.layer.data(),nullptr,1.,1.);
    require(!absent.countCalls&&!absent.sectionCalls&&!absent.endCalls,"native null-array early return");
    _mm_setcsr(savedFP);
    if(argc==4){
        std::ofstream csv(argv[3],std::ios::out|std::ios::noreplace);require(static_cast<bool>(csv),"new kernel receipt only");
        csv<<"layout,objects,round,variant,milliseconds,calls\n";
        for(unsigned size:{256u,4096u})for(unsigned layout=0;layout<2;++layout){
            Fixture a(size,table.data()),b(size,table.data());a.record=b.record=false;
            if(layout){a.scramble();b.scramble();}
            for(unsigned round=0;round<10;++round)for(unsigned order=0;order<2;++order){
                const unsigned variant=order^(round&1);auto& f=variant?b:a;auto move=variant?oracle.prefetch():oracle.native();
                const auto start=std::chrono::steady_clock::now();
                for(unsigned repeat=0;repeat<128;++repeat)move(f.layer.data(),&f.array,0.125,-0.25);
                const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(round)csv<<(layout?"permuted":"contiguous")<<','<<size<<','<<round<<','<<variant<<','<<elapsed<<','<<128ull*size<<'\n';
            }
            same(a,b);
        }
        require(static_cast<bool>(csv),"complete kernel receipt");
    }
    std::cout<<"PASS "<<cases<<" owned native movement comparisons; all bytes, callback barriers/order and MXCSR match. Kernel only; no GD process\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
