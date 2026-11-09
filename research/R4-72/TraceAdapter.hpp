#pragma once
#include "../R4-71/MixedProgram.hpp"
class PlayLayer;
namespace chlorium::research::rotationtrace {
struct Stats {unsigned version=72,active=0,complete=0,reasons=0,frames=0,rows=0,closed=0,actors=0;};
struct alignas(16) Row {
    mixedprogram::State before;mixedprogram::Command command;mixedprogram::State after;
    std::uint64_t address=0;unsigned uid=0,frame=0,closed=0;float localRotation=0;unsigned controls=0,reserved=0;
};
struct Header {
    char magic[8]{'C','H','L','R','O','T','7','2'};unsigned version=72,rowBytes=sizeof(Row),frames=0,rows=0,actors=0,reasons=0,ownerUid=0,thread=0;
    std::uint64_t owner=0;double percent=0;std::uint64_t calls=0;
};
static_assert(sizeof(Row)==208 && sizeof(Header)==64);
void begin(PlayLayer*,bool isolated)noexcept;
void end()noexcept;
}
