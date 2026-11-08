#include "NativeOpacityHash.hpp"
#include <xmmintrin.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cerrno>
#include <random>
#include "../R4-35/RegisterProbe.hpp"
using namespace chlorium::research::opacityhash;
template<std::size_t Size>struct Bytes {
    std::array<unsigned char,Size> bytes{};
    template<class T>void put(unsigned offset,const T& value){require(offset<=Size&&sizeof(T)<=Size-offset,"fixture byte field");std::memcpy(bytes.data()+offset,&value,sizeof(value));}
};
struct Map {
    Bytes<0x300> manager;Bytes<0x40> sentinel;
    std::vector<std::unique_ptr<Bytes<0x40>>> nodes;
    std::vector<void*> buckets;unsigned mask=0;
    void rehash(unsigned value){require(value<16384&&!(value&(value+1)),"power-of-two bucket mask");mask=value;buckets.assign((mask+1)*2,sentinel.bytes.data());
        for(auto& node:nodes){int key=0;std::memcpy(&key,node->bytes.data()+16,4);const auto bucket=hashGroup(key)&mask;
            auto& first=buckets[bucket*2];auto& last=buckets[bucket*2+1];node->put(8,last);
            if(first==sentinel.bytes.data())first=node->bytes.data();last=node->bytes.data();}
        manager.put(0x1b8,sentinel.bytes.data());manager.put(0x1c8,buckets.data());manager.put(0x1e0,std::uint64_t(mask));}
    Map(){rehash(7);}
    void set(int key,std::uint32_t bits){
        for(auto& node:nodes){int k=0;std::memcpy(&k,node->bytes.data()+16,4);if(k==key){node->put(0x2c,bits);return;}}
        auto node=std::make_unique<Bytes<0x40>>();node->put(16,key);node->put(0x2c,bits);nodes.push_back(std::move(node));rehash(mask);
    }
    void erase(int key){std::erase_if(nodes,[&](const auto& node){int k=0;std::memcpy(&k,node->bytes.data()+16,4);return k==key;});rehash(mask);}
};
std::uint64_t checks=0;
void compare(const Oracle& oracle,Map& map,int id,unsigned controls){
    std::uint32_t result=0;unsigned flags=0;
    for(unsigned mode=0;mode<4;++mode){_mm_setcsr(controls);errno=1177;const auto value=oracle.getter(mode)(map.manager.bytes.data(),id);const auto after=_mm_getcsr();
        const auto bits=std::bit_cast<std::uint32_t>(value);require(errno==1177,"getter errno unchanged");
        if(mode)require(bits==result&&after==flags,"getter result and complete FP state");else{result=bits;flags=after;}++checks;}
}
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3,"owned GD image and new kernel CSV required");
    const auto saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};Oracle oracle(argv[1]);
    Map map;chlorium::research::registers::Probe caller;
    for(unsigned id=0;id<10000;++id)require((lowHashes[id&255]^(id>>8))*prime*prime*prime==hashGroup(id),"all clamped hashes are exact");
    for(unsigned rounding=0;rounding<4;++rounding)for(unsigned flush=0;flush<4;++flush){
        const auto controls=0x1f80u|(rounding<<13)|((flush&1)?0x40u:0u)|((flush&2)?0x8000u:0u);
        for(auto bits:{0u,0x80000000u,1u,0x007fffffu,0x00800000u,0x3f000000u,0x3f800000u,0xbf800000u,0x7f800000u,0x7fc12345u,0x7f812345u}){
            map.set(0,bits);map.set(127,bits);map.set(9999,bits);
            for(int key:{-2147483647,-1,0,1,127,128,255,256,9999,10000,2147483647})for(unsigned sticky:{0u,63u})compare(oracle,map,key,controls|sticky);
            for(unsigned mode=0;mode<4;++mode){_mm_setcsr(controls);const auto result=caller.external(reinterpret_cast<void*>(oracle.getter(mode)),map.manager.bytes.data(),127);
                require(chlorium::research::registers::Probe::preserved(result)&&result.result==bits&&_mm_getcsr()==controls,"native XMM1-3, value bits and FP controls preserved");++checks;}
        }
    }
    for(int n=0;n<10000;++n)map.set(n,0x3f000000u+unsigned(n));
    for(int key=-1;key<=10000;++key)compare(oracle,map,key,0x1f80);
    for(unsigned mask:{0u,1u,31u,16383u}){map.rehash(mask);for(int key:{0,1,127,128,255,256,9999})compare(oracle,map,key,0x1f80);}
    map.erase(127);compare(oracle,map,127,0x1f80);map.set(127,0x3e800000);compare(oracle,map,127,0x1f80);map.set(127,0x3f400000);compare(oracle,map,127,0x1f80);
    Bytes<0x520> object;std::array<short,10> groups{127,128,255,256,127,9999,0,4,5,6};object.put(0x4b0,groups.data());object.put(0x508,map.manager.bytes.data());
    for(short count=-2;count<=10;++count)for(unsigned modeControl:{0x1f80u,0x9fc0u,0x3f80u,0x5f80u,0x7f80u}){
        object.put(0x4b8,count);std::uint32_t wanted=0;unsigned flags=0;
        for(unsigned mode=0;mode<4;++mode){_mm_setcsr(modeControl);const auto bits=std::bit_cast<std::uint32_t>(oracle.product(mode)(object.bytes.data()));const auto after=_mm_getcsr();
            if(mode)require(bits==wanted&&after==flags,"native complete group product parity");else{wanted=bits;flags=after;}++checks;}
    }
    std::ofstream csv(std::filesystem::path(argv[2]),std::ios::out|std::ios::noreplace);require(bool(csv),"refuse existing timings");csv<<"shape,batch,mode,calls,wall_ms,digest,game_fps_evidence\n";
    std::mt19937 random(0x436u);std::vector<int> queries(262144);for(auto& key:queries)key=int(random()%12000)-1000;
    for(unsigned shape=0;shape<2;++shape){map.rehash(shape?16383:255);if(shape==0)for(auto& key:queries)key=int(random()%128);
        else for(auto& key:queries)key=int(random()%12000)-1000;
        for(unsigned batch=0;batch<9;++batch)for(unsigned order=0;order<4;++order){const auto mode=(order+batch)%4;const auto getter=oracle.getter(mode);_mm_setcsr(0x1f80);
            std::uint64_t digest=0;const auto start=std::chrono::steady_clock::now();
            for(auto key:queries)digest+=std::bit_cast<std::uint32_t>(getter(map.manager.bytes.data(),key));
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(batch)csv<<shape<<','<<batch<<','<<mode<<','<<queries.size()<<','<<ms<<','<<digest<<",false\n";
        }
    }
    require(bool(csv),"write kernel timings");std::cout<<"PASS "<<checks<<" native getter/product comparisons; no GD launch and no game FPS verdict\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
