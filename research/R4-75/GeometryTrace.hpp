#pragma once
#include "../R4-71/MixedProgram.hpp"
#include "../R4-32/PassCapture.hpp"
namespace chlorium::research::rootgeometry {
struct Stats {unsigned version=75,frames=0,records=0,missing=0,matched=0,unsupported=0,complete=0,reasons=0;};
struct alignas(16) Row {
    mixedprogram::State root;pass::SpriteRecord source;snapshot::NativeQuadState quad;
    unsigned frame=0,controls=0,roles=0,reserved=0;std::uint64_t atlas=0,batch=0;
};
struct Header {
    char magic[8]{'C','H','L','G','E','O','7','5'};unsigned version=75,rowBytes=sizeof(Row),frames=0,records=0,actors=0,reasons=0,matched=0,unsupported=0;
    std::uint64_t owner=0;unsigned ownerUid=0,thread=0;double percent=0;
};
static_assert(sizeof(pass::SpriteRecord)==192 && sizeof(Row)==368 && sizeof(Header)==64 && sizeof(Stats)==32);
}
