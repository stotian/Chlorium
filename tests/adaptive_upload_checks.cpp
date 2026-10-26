#include "renderer/AdaptiveUpload.hpp"
#include <iostream>
#include <random>
namespace s=chlorium::snapshot;
unsigned checks=0;
void require(bool ok,const char* why) {++checks;if(!ok) throw std::runtime_error(why);}
int main() try {
    s::AdaptiveUpload transport;std::vector<s::NativeQuadState> source(128),gpu(512);
    for(unsigned n=0;n<source.size();++n) source[n].corners[0].color=0xff000000u+n;
    auto write=[&](std::size_t offset,std::size_t count,const void* data) {
        require(offset<=gpu.size()&&count<=gpu.size()-offset,"write inside owned capacity");
        std::memcpy(gpu.data()+offset,data,count*sizeof(s::NativeQuadState));
    };
    auto run=[&] {
        auto work=transport.update(std::as_bytes(std::span<const s::NativeQuadState>(source)),gpu.size(),write);
        require(!std::memcmp(source.data(),gpu.data(),source.size()*sizeof(s::NativeQuadState)),"all drawn source bytes reach GPU, including unsampled records");
        return work;
    };
    require(run().mode==s::UploadMode::Direct,"first complete direct prefix");
    require(run().mode==s::UploadMode::Seed,"stable transition creates complete shadow");
    require(run().mode==s::UploadMode::Sparse,"subsequent stable comparison");
    source[1].corners[3].u=.75f;require(run().mode==s::UploadMode::Sparse,"unsampled change still compared and uploaded");
    source.back().corners[1].z=5;run();
    source.resize(300);run();source.resize(63);run();run();run();
    std::mt19937 random(0xa1c6e);
    for(unsigned frame=0;frame<1200;++frame) {
        if(frame%83==0) {source.resize(1+random()%511);transport.invalidate();std::memset(gpu.data(),0,gpu.size()*sizeof(s::NativeQuadState));}
        for(unsigned n=0;n<(frame%3==0?source.size():3u);++n) {
            auto bytes=reinterpret_cast<unsigned char*>(source.data());
            bytes[random()%(source.size()*sizeof(s::NativeQuadState))]^=static_cast<unsigned char>(1+random()%255);
        }
        run();
    }
    source.resize(128);run();run();run();source[1].corners[0].color^=0xffu;
    bool thrown=false;
    try {transport.update(std::as_bytes(std::span<const s::NativeQuadState>(source)),gpu.size(),[&](auto offset,auto count,const void* data){write(offset,count,data);throw std::runtime_error("injected writer failure");});}
    catch(const std::runtime_error&) {thrown=true;}
    require(thrown,"writer failure injected");require(run().mode==s::UploadMode::Direct,"failure invalidates shadow before retry");
    transport.invalidate();source.resize(512);
    auto work=run();require(work.copied==8&&work.uploaded==512,"dense mode avoids full CPU shadow copy");
    for(auto& q:source) q.corners[0].x+=1;
    work=run();require(work.mode==s::UploadMode::Direct&&work.copied==8,"dense animation stays direct");
    bool rejected=false;
    try {transport.update(std::as_bytes(std::span<const s::NativeQuadState>(source)),1,write);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"bounds rejected before any GPU write");
    std::cout<<"PASS "<<checks<<" adaptive upload checks\n";return 0;
} catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
