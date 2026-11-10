#include "ParameterClasses.hpp"
#include <cassert>
#include <iostream>
namespace pc=chlorium::research::parameterclasses;using Parameters=chlorium::research::resident::Parameters;
int main(){
    std::vector<Parameters> p(4);std::vector<unsigned char> enabled(4,1);p[2].matrix[0]=0x80000000;p[3].colors[0]=0x7fc12345;
    auto a=pc::build(p,enabled);assert(a.values.size()==3 && (a.slots==std::vector<unsigned>{0,0,1,2}));
    p[0].colors[1]=p[1].colors[1]=17;auto b=pc::build(p,enabled);assert(a.slots==b.slots && b.values.size()==3);assert(pc::changedRanges<Parameters>(a.values,b.values)==std::vector<pc::Range>({{0,1}}));
    p[1].colors[1]=18;auto split=pc::build(p,enabled);assert(split.values.size()==4 && split.slots[0]!=split.slots[1]);
    enabled[1]=0;auto inactive=pc::build(p,enabled);assert(inactive.slots[1]==~0u && inactive.values.size()==3);
    std::vector<unsigned> before{0,1,2,3,4},after{0,8,9,3,7};assert(pc::changedRanges<unsigned>(before,after)==std::vector<pc::Range>({{1,2},{4,1}}));
    assert(pc::changedRanges<unsigned>(after,after).empty());after.push_back(5);assert(pc::changedRanges<unsigned>(before,after)==std::vector<pc::Range>({{1,2},{4,2}}));
    after=before;after.pop_back();assert(pc::changedRanges<unsigned>(before,after).empty());
    const std::array<float,2> fa{0.f,0.f},fb{0.f,-0.f};assert(pc::changedRanges<float>(fa,fb)==std::vector<pc::Range>({{1,1}}));
    bool rejected=false;try{pc::build(p,std::span(enabled).first(3));}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    enabled[0]=2;rejected=false;try{pc::build(p,enabled);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    pc::Cache cache;enabled[0]=1;enabled[1]=1;p[1]=p[0];auto first=cache.update(p,enabled);p[0].colors[2]=p[1].colors[2]=77;auto sameGroups=cache.update(p,enabled);assert(first.slots==sameGroups.slots && sameGroups.values[sameGroups.slots[0]].colors[2]==77);
    p[1].colors[2]=78;auto diverged=cache.update(p,enabled);assert(diverged.slots[0]!=diverged.slots[1]);enabled[0]=0;assert(cache.update(p,enabled).slots[0]==~0u);
    std::vector<Parameters> unique(320);std::vector<unsigned char> all(320,1);for(unsigned i=0;i<unique.size();++i)unique[i].colors[0]=i;
    const auto& direct=cache.update(unique,all);assert(!direct.grouped && direct.values.size()==320 && direct.slots.back()==319);unique.back().matrix[0]=123;assert(cache.update(unique,all).values.back().matrix[0]==123);
    all.back()=2;rejected=false;try{cache.update(unique,all);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);all.back()=1;assert(cache.update(unique,all).values.back().matrix[0]==123);
    std::vector<Parameters> oversized(65537);std::vector<unsigned char> tooMany(65537,1);rejected=false;try{pc::build(oversized,tooMany);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    std::cout<<"PASS exact parameter classes, uniform updates, divergence, fallback slots and sparse range boundaries\n";
}
