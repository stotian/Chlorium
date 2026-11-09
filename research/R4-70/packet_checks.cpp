#include "MovePacket.hpp"
#include "SectionOracle.hpp"
#include "../R4-38/NativeMovement.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
using namespace chlorium::research::samecell;
using chlorium::research::opacityhash::require;
template<class T> void store(void* object,unsigned offset,T value){std::memcpy(static_cast<unsigned char*>(object)+offset,&value,sizeof(value));}
struct alignas(64) Object {
    unsigned char prefix[0x278]{};int m_outerSectionIndex=0,inner=0;unsigned char pad280[0x48]{};bool m_tempOffsetXRelated=false;
    unsigned char pad2c9[0x88]{};bool m_isObjectPosDirty=false,m_isUnmodifiedPosDirty=false;unsigned char pad353[0x15]{};
    bool m_isObjectRectDirty=false,m_isOrientedBoxDirty=false;unsigned char pad36a[0x4e]{};double m_positionX=0,m_positionY=0;
    unsigned char pad3c8[0x108]{};struct Point{float x=0,y=0;}m_lastPosition;unsigned char pad4d8[4]{};int m_unk4C4=0;
    unsigned char pad4e0[0x32]{};bool m_isDecoration2=false;unsigned char tail[0xed]{};
};
static_assert(sizeof(Object)==0x600 && offsetof(Object,m_positionX)==0x3b8 && offsetof(Object,m_isDecoration2)==0x512);
static_assert(offsetof(Object,m_lastPosition)==0x4d0 && offsetof(Object,m_tempOffsetXRelated)==0x2c8 && offsetof(Object,m_isObjectPosDirty)==0x351);
struct ArrayData {unsigned count=0,capacity=0;void* padding=nullptr;Object** pointers=nullptr;};
struct Array {unsigned char prefix[0x38]{};ArrayData* data=nullptr;};
__declspec(noinline) unsigned countArray(void* array){return static_cast<Array*>(array)->data->count;}
__declspec(noinline) void end(void* layer,void*){auto n=read<unsigned>(layer,0x3f10);store(layer,0x3f10,n+1);}
struct World {
    std::array<unsigned char,0x4000> layer{};std::vector<Object> objects;std::vector<Object*> pointers;ArrayData data;Array array;
    World(unsigned count,void* table):objects(count),pointers(count){store(layer.data(),0,table);store(layer.data(),0x36a0,.01f);store(layer.data(),0x36a4,.01f);
        for(unsigned i=0;i<count;++i){auto& o=objects[i];pointers[i]=&o;o.m_positionX=double(i%2000)*.125+25.;o.m_positionY=double(i%1777)*.25+75.;
            o.m_outerSectionIndex=int(double(.01f)*o.m_positionX);o.inner=int(double(.01f)*o.m_positionY);o.m_isDecoration2=i%11==0;o.m_tempOffsetXRelated=i%13==0;o.m_unk4C4=i%4;
            o.m_lastPosition={std::bit_cast<float>(0x7fc12345u),-0.f};}
        data={count,count,nullptr,pointers.data()};array.data=&data;
    }
    PacketStats fast(int command,double dx,double dy,SectionCall section){
        store(layer.data(),0x3724,read<unsigned>(layer.data(),0x3724)+data.count);
        auto stats=advancePacket(pointers.data(),data.count,command,dx,dy,reinterpret_cast<const float*>(layer.data()+0x36a0),layer.data(),section);
        end(layer.data(),&array);return stats;
    }
};
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==2,"pinned GD image required");SectionOracle section(argv[1]);
    chlorium::research::movement::Oracle movement(argv[1],reinterpret_cast<void*>(&countArray));std::array<void*,148> table{};
    table[0x490/8]=reinterpret_cast<void*>(section.native());table[0x498/8]=reinterpret_cast<void*>(&end);
    const auto saved=_mm_getcsr();std::uint64_t checked=0,skipped=0;
    for(unsigned fp:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u,0x3f80u,0x5f80u,0x7f80u})for(unsigned count:{0u,1u,2u,31u,257u})for(bool hole:{false,true}){
        World native(count,table.data()),fast(count,table.data());if(hole && count>1)native.pointers[count/2]=fast.pointers[count/2]=nullptr;
        for(unsigned step=0;step<65;++step){const int command=step/3;const double dx=step%3?double(int(step%17)-8)*.125:-0.,dy=step%4?double(int(step%13)-6)*.1:0;
            store(native.layer.data(),0x3e0,command);store(fast.layer.data(),0x3e0,command);
            _mm_setcsr(fp);movement.native()(native.layer.data(),&native.array,dx,dy);const auto expected=_mm_getcsr();
            _mm_setcsr(fp);SetLastError(49);const auto stats=fast.fast(command,dx,dy,section.native());
            require(_mm_getcsr()==expected && GetLastError()==49,"packet FP and LastError match original native movement");
            require(!std::memcmp(native.objects.data(),fast.objects.data(),count*sizeof(Object)),"all packet object bytes equal native");
            require(read<unsigned>(native.layer.data(),0x3724)==read<unsigned>(fast.layer.data(),0x3724) && read<unsigned>(native.layer.data(),0x3f10)==read<unsigned>(fast.layer.data(),0x3f10) && read<unsigned>(native.layer.data(),0x3f00)==read<unsigned>(fast.layer.data(),0x3f00),"native visit count, changed-section witness and terminal callback preserved");
            checked+=stats.visited;skipped+=stats.unchanged;
        }
    }
    World native(32768,table.data()),fast(32768,table.data());std::vector<double> times[2];
    for(unsigned batch=0;batch<10;++batch)for(unsigned order=0;order<2;++order){const unsigned variant=order^(batch&1);auto& world=variant?fast:native;_mm_setcsr(0x1f80);
        const auto start=std::chrono::steady_clock::now();for(unsigned step=0;step<32;++step){store(world.layer.data(),0x3e0,step/4);if(variant)world.fast(step/4,1./1024,-1./1024,section.native());else movement.native()(world.layer.data(),&world.array,1./1024,-1./1024);}
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/32.;if(batch)times[variant].push_back(ms);
    }
    for(unsigned variant=0;variant<2;++variant){std::sort(times[variant].begin(),times[variant].end());std::cout<<(variant?"fused":"native")<<" owned movement ms median="<<times[variant][4]<<" worst="<<times[variant].back()<<'\n';}
    _mm_setcsr(saved);std::cout<<"PASS "<<checked<<" complete owned movement visits, "<<skipped<<" verified native section no-ops; no GD FPS claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
