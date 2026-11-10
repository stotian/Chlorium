#include "ParameterClasses.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <windows.h>
namespace pc=chlorium::research::parameterclasses;using Parameters=chlorium::research::resident::Parameters;
int main(){
    SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);std::size_t sink=0;
    for(unsigned unique:{16u,65536u}){std::vector<Parameters> p(65536);std::vector<unsigned char> enabled(p.size(),1);for(unsigned i=0;i<p.size();++i){p[i].colors[0]=i%unique;p[i].matrix[0]=0x3f800000;}
        pc::Cache cache;cache.update(p,enabled);std::vector<double> retained;
        std::vector<double> samples;for(unsigned sample=0;sample<9;++sample){const auto begin=std::chrono::steady_clock::now();for(unsigned repeat=0;repeat<8;++repeat){auto frame=pc::build(p,enabled);sink+=frame.values.size()+frame.slots.back();}
            samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/8);}
        for(unsigned sample=0;sample<9;++sample){for(auto& value:p)value.colors[1]=sample;const auto begin=std::chrono::steady_clock::now();for(unsigned repeat=0;repeat<8;++repeat){const auto& frame=cache.update(p,enabled);sink+=frame.values.size()+frame.slots.back();}retained.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/8);}
        std::sort(samples.begin(),samples.end());std::sort(retained.begin(),retained.end());std::cout<<"classes="<<unique<<" actors="<<p.size()<<" build_median_cpu_ms="<<samples[4]<<" build_worst_batch_ms="<<samples.back()<<" retained_median_cpu_ms="<<retained[4]<<" retained_worst_batch_ms="<<retained.back()<<'\n';}
    std::cout<<"fixture_only=true game_fps_evidence=false sink="<<sink<<'\n';
}
