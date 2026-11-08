#include "NativeMovementCensus.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
using chlorium::research::movementcensus::Census;
struct Object {unsigned m_uID=0;unsigned char m_isDecoration2=0,m_tempOffsetXRelated=0;int m_outerSectionIndex=0;};
struct Data {unsigned num=0;Object** arr=nullptr;};
struct Array {Data* data=nullptr;unsigned m_uID=0;};
struct Layer {struct {int m_commandIndex=-7;}m_gameState;};
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main()try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);_putenv_s("CHLORIUM_MOVE_COMMAND_CENSUS","1");
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    Layer layer,other;Object a{17,1,0,-1},b{19,0,1,3};Object* entries[]{&a,&b,nullptr};Data data{3,entries};Array array{&data,23};
    auto census=std::make_unique<Census>();census->configure(&layer);
    census->before<Object>(0,&layer,&array,0,0);census->before<Object>(1,&other,&array,0,0);require(census->ownedRow(0).frame==0,"frame/owner gate before reading arrays");
    _mm_setcsr(0x9fe1);SetLastError(0x12345678);census->before<Object>(1,&layer,&array,-0.0,.25);require(_mm_getcsr()==0x9fe1&&GetLastError()==0x12345678,"FP and LastError preserved");
    const auto& row=census->ownedRow(0);require(row.complete&&row.nullTerminated&&row.count==3&&row.visited==2&&row.decorations==1&&row.negativeSections==1&&row.lockedX==1&&row.dx==0x8000000000000000ull&&row.stamp==std::bit_cast<unsigned>(-7),"exact bounded native metadata and null stop");
    std::swap(entries[0],entries[1]);census->before<Object>(2,&layer,&array,1,2);require(census->ownedRow(1).membersHash!=row.membersHash,"order-sensitive diagnostic membership fingerprint");
    entries[0]=reinterpret_cast<Object*>(1);census->before<Object>(1,&layer,&array,1,2);require(!census->ownedRow(2).complete,"bad member read marked incomplete without crash");
    data.num=1000001;census->before<Object>(1,&layer,&array,1,2);require(!census->ownedRow(3).complete,"array count budget rejected");
    data.num=131073;census->before<Object>(1,&layer,&array,1,2);require(!census->ownedRow(4).complete&&census->ownedRow(4).visited==0,"member budget rejects before reading");
    data.num=1;entries[0]=&a;for(unsigned i=0;i<2100;++i)census->before<Object>(1,&layer,&array,1,2);require(census->ownedRow(2047).complete,"bounded row storage fills without overflow write");
    _putenv_s("CHLORIUM_MOVE_COMMAND_CENSUS","");census->configure(&layer);census->before<Object>(1,&layer,reinterpret_cast<Array*>(1),1,2);
    std::cout<<"PASS movement census frame/owner gates, exact fields/null stop, FP/LastError, invalid reads, budgets and disabled invalid-pointer path\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
