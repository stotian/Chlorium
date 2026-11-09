#include "MovementTrace.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
using namespace chlorium::research::grouptrace;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Layer {struct {int m_commandIndex=7;} m_gameState;};
struct Data {unsigned num=0;void** arr=nullptr;};
struct Array {unsigned m_uID=42;Data* data=nullptr;};
static bool corrupt=false;
static void* selected=nullptr;
static unsigned selectedUid=0;
static bool select(void* pointer)noexcept{return pointer==selected && static_cast<Object*>(pointer)->uid==selectedUid;}
static void read(void* pointer,Object& out)noexcept{
    out=*static_cast<Object*>(pointer);
    if(corrupt){_mm_setcsr(0x9fc0);SetLastError(99);}
}
int main(int argc,char** argv)try{
    require(argc==2,"new owned trace path");require(!std::filesystem::exists(argv[1]),"refuse existing test output");
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    Layer layer,foreign;Object a,b;a.address=123;a.uid=9;a.section=2;a.state.position[0]=.25;
    {Capture invalid(1);require(!invalid.start(&layer,0,74) && !invalid.start(&layer,1,-1) &&
        !invalid.start(&layer,1,101) && !invalid.start(&layer,1,std::numeric_limits<double>::quiet_NaN()),"unknown owner or invalid progress cannot arm a trace");}
    b.address=456;b.uid=10;b.state.position[1]=-.5;
    void* pointers[]{&a,nullptr,&b};Data data{3,pointers};Array array{42,&data};
    Capture trace(8);trace.before(&layer,&array,.125,-0.0,&read);require(!trace.header.rows,"disabled trace reads nothing");
    require(trace.start(&layer,17,74) && !trace.start(&layer,17,74),"trace starts once");
    require(!trace.beginFrame(&foreign,17) && !trace.beginFrame(&layer,18),"wrong owner and UID refuse");
    require(trace.beginFrame(&layer,17),"begin first frame");require(!trace.write(argv[1]),"active frame cannot be serialized");
    _mm_setcsr(0x1f80);SetLastError(73);corrupt=true;
    trace.before(&layer,&array,.125,-0.0,&read);corrupt=false;
    require(_mm_getcsr()==0x1f80 && GetLastError()==73,"observer preserves FP and LastError");
    require(trace.header.objects==1 && trace.row(0).flags==3 && trace.row(0).count==3,"null member preserves native prefix");
    require(trace.row(0).mxcsr==0x1f80 && trace.row(0).stamp==7 && trace.row(0).dy==0x8000000000000000ull,"exact command controls and signed zero");
    require(trace.object(0).address==123 && trace.object(0).uid==9 && trace.object(0).state.position[0]==.25,"copied owned object state");
    pointers[1]=&b;data.num=2;trace.before(&layer,&array,.25,.5,&read);
    require(trace.header.objects==3 && trace.row(1).first==1 && trace.row(1).observed==2,"contiguous bounded row spans");
    trace.endFrame();trace.before(&layer,&array,0,0,&read);require(trace.header.rows==2,"closed frame reads nothing");
    require(trace.beginFrame(&layer,17),"begin second frame");
    trace.before(&layer,static_cast<Array*>(nullptr),0,0,&read);require(trace.row(2).flags==17,"native null array is a valid empty call");
    trace.before(&foreign,&array,0,0,&read);require(trace.header.rows==3,"foreign layer ignored");
    pointers[0]=reinterpret_cast<void*>(1);data.num=1;trace.before(&layer,&array,0,0,&read);
    require(trace.row(3).flags==8 && trace.header.objects==3 && (trace.header.reasons&2),"bad object marks incomplete without retaining a pointer");
    trace.endFrame();require(trace.header.frames==2 && !trace.beginFrame(&layer,17),"two-frame window is bounded");
    require(trace.write(argv[1]) && !trace.write(argv[1]),"exclusive new output only");
    const auto bytes=std::filesystem::file_size(argv[1]);require(bytes==64+4*56+3*64,"exact serialized lengths");
    Capture budget(1);budget.start(&layer,1,0);budget.beginFrame(&layer,1);data.num=2;
    budget.before(&layer,&array,0,0,&read);require(budget.row(0).flags==4 && !budget.header.objects,"budget rejects before object dereference");
    budget.abort();require(budget.header.reasons&4,"ownership loss disarms capture");
    Capture calls(1);calls.start(&layer,1,0);calls.beginFrame(&layer,1);
    for(unsigned i=0;i<8193;++i)calls.before(&layer,static_cast<Array*>(nullptr),0,0,&read);
    require(calls.header.rows==8192 && (calls.header.reasons&1),"call budget cannot grow the output unboundedly");
    Capture snapshots(2);snapshots.start(&layer,17,74);snapshots.beginFrame(&layer,17);
    pointers[0]=&a;pointers[1]=nullptr;pointers[2]=&b;data.num=3;selected=&b;selectedUid=b.uid;
    snapshots.snapshot(&layer,&array,&read,&select);snapshots.endFrame();
    require(snapshots.header.features==1 && snapshots.row(0).flags==33 && snapshots.row(0).observed==1 &&
        snapshots.row(0).count==3 && snapshots.object(0).address==456,"final snapshot filters current owners and skips holes");
    b.uid=11;b.state.position[0]=.75;snapshots.beginFrame(&layer,17);
    snapshots.snapshot(&layer,&array,&read,&select);snapshots.endFrame();
    require(snapshots.header.objects==1 && snapshots.row(1).observed==0 && snapshots.object(0).state.position[0]==0,
        "reused address with a new UID cannot refresh a retired object snapshot");
    require(snapshots.write(std::string(argv[1])+".snapshots"),"serialize filtered end-frame observations");
    std::cout<<"R63 bounded observer, lifetime, failure, FP and serialization controls passed; no native suppression\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
