#include "GroupProgram.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace chlorium::research::groupprogram;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()try{
    std::vector<Command> commands(80);for(auto& command:commands)command.delta[0]=.125;
    std::array<State,1> states{};states[0].position[0]=1;
    std::array<Range,1> ranges{{{0,3}}};std::array<unsigned,3> indices{64,70,79};
    require(seedEligible(states,commands,ranges,indices,0x1f80),"global table above64 commands is valid for a short object program");
    require(!chlorium::research::grouptape::arithmeticEligible(states,commands,ranges,indices,0x1f80),"old global bound discriminant");
    indices={64,79,79};require(seedEligible(states,commands,ranges,indices,0x1f80),"duplicate member executes the same command twice");
    indices={79,70,64};require(!seedEligible(states,commands,ranges,indices,0x1f80),"command reordering rejects");
    indices={64,70,80};require(!seedEligible(states,commands,ranges,indices,0x1f80),"outside global table rejects");
    std::vector<unsigned> longIndices(65,0);ranges[0]={0,65};
    require(!seedEligible(states,commands,ranges,longIndices,0x1f80),"per-object64-step proof is retained");
    ranges[0]={0,0};require(seedEligible(states,{},ranges,{},0x1f80),"empty frame retains an existing object without motion");
    states[0].flags=1u<<16;require(!seedEligible(states,{},ranges,{},0x1f80),"bad program class rejects");
    states[0].flags=64;require(!seedEligible(states,{},ranges,{},0x1f80),"unknown low state flags reject");
    states[0].flags=0;states[0].position[0]=std::bit_cast<double>(std::uint64_t{1});
    require(!seedEligible(states,{},ranges,{},0x1f80),"subnormal seed rejects");
    states[0].position[0]=1;ranges[0]={0,1};std::array<unsigned,1> one{0};
    require(!seedEligible(states,commands,ranges,one,0x3f80),"unknown rounding rejects");
    commands[0].control[2]=1;require(!seedEligible(states,commands,ranges,one,0x1f80),"unknown command flags reject");
    commands[0].control[2]=0;commands[0].delta[0]=std::numeric_limits<double>::infinity();
    require(!seedEligible(states,commands,ranges,one,0x1f80),"nonfinite delta rejects");
    std::cout<<"R64 shared program bounds and ordered duplicate controls passed; no native offload\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
