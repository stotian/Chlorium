#include "MixedProgram.hpp"
#include <cassert>
#include <iostream>
namespace mp=chlorium::research::mixedprogram;
int main(){
    std::vector<mp::State> states(2);states[1].flags=1u<<16;std::vector<mp::Command> commands(2);commands[1].control[1]=1;commands[1].linear[0]=commands[1].linear[3]=1;
    std::vector<mp::Range> ranges{{0,0},{0,3}};std::vector<unsigned> indices{0,1,1};
    auto valid=[&](unsigned fp=0x1f80){return mp::fixtureInput(states,commands,ranges,indices,fp);};assert(valid());
    ranges[1][1]=65;assert(!valid());ranges[1][1]=3;indices[1]=9;assert(!valid());indices[1]=1;
    commands[1].control[1]=3;assert(!valid());commands[1].control[1]=2;commands[1].control[2]=3;assert(valid());commands[1].control[2]=4;assert(!valid());commands[1].control[1]=1;commands[1].control[2]=0;commands[1].control[3]=1;assert(!valid());commands[1].control[3]=0;
    commands[1].linear[0]=std::bit_cast<float>(0x7fc00001u);assert(!valid());commands[1].linear[0]=1;
    states[1].reserved[0]=1;assert(!valid());states[1].reserved[0]=0;assert(!valid(0x3f80));assert(valid(0x9fc0));
    std::cout<<"PASS mixed fixture structural/finite bounds; no production eligibility claim\n";
}
