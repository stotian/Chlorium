#include "PackedMirror.hpp"
#include <cassert>
#include <bit>
#include <iostream>
using namespace chlorium::research::packedmirror;
int main(){
    const auto previous=_mm_getcsr();_mm_setcsr(0x1f80);
    std::array<State,3> states{};
    for(unsigned i=0;i<3;++i){states[i].position[0]=1;states[i].position[1]=2;states[i].stamp=17;states[i].flags=i;states[i].last[0]=0x7fc01234;}
    std::array<Command,1> commands{};commands[0].delta[0]=.5;commands[0].delta[1]=-.25;commands[0].control[0]=18;
    std::array<Range,1> ranges{{{0,1}}};std::array<unsigned,1> indices{0};PackedMirror mirror;
    assert(mirror.initialize(states,commands,ranges,indices));assert(!mirror.initialize(states,commands,ranges,indices));
    SetLastError(81);assert(mirror.advance(commands,ranges,indices));assert(GetLastError()==81);std::array<State,3> actual{};assert(mirror.copyOut(actual));
    assert(actual[0].position[0]==1.5 && actual[0].position[1]==1.75 && actual[0].last[0]==0x3f800000 && actual[0].last[1]==0x40000000 && actual[0].stamp==18 && actual[0].flags==60);
    assert(actual[1].position[0]==1.5 && actual[1].last[0]==0x7fc01234 && actual[1].stamp==17 && actual[1].flags==49);
    assert(actual[2].position[0]==1 && actual[2].position[1]==1.75 && actual[2].flags==62);
    auto before=actual;auto invalid=ranges;invalid[0][1]=65;const auto fp=_mm_getcsr();assert(!mirror.advance(commands,invalid,indices) && _mm_getcsr()==fp);
    assert(mirror.copyOut(actual) && std::memcmp(before.data(),actual.data(),sizeof(actual))==0);
    _mm_setcsr(0x3f80);assert(!mirror.advance(commands,ranges,indices) && _mm_getcsr()==0x3f80);_mm_setcsr(0x1f80);
    PackedMirror tiny;State seed{};seed.position[0]=0x1p-32+0x1p-84;Command cancel{};cancel.delta[0]=-0x1p-32;cancel.control[0]=1;
    assert(tiny.initialize({&seed,1},{&cancel,1},ranges,indices));assert(tiny.advance({&cancel,1},ranges,indices));State result{};assert(tiny.copyOut({&result,1}) && result.position[0]==0x1p-84);
    Command zero{};zero.control[0]=2;assert(tiny.advance({&zero,1},ranges,indices));assert(tiny.copyOut({&result,1}) && result.last[0]==0x15800000);
    const auto stores=tiny.metrics.actorStores;std::array<Range,1> empty{};assert(tiny.advance({},empty,{}) && tiny.metrics.actorStores==stores);
    PackedMirror overflow;seed.position[0]=0x1p32;cancel.delta[0]=0x1p32;assert(overflow.initialize({&seed,1},{&cancel,1},ranges,indices));assert(!overflow.advance({&cancel,1},ranges,indices));
    _mm_setcsr(previous);std::cout<<"R68 owned CPU state transitions, cancellation, empty/invalid programs and FP guards passed\n";
}
