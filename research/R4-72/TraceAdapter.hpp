#pragma once
#include "../R4-71/MixedProgram.hpp"
class PlayLayer;
namespace chlorium::research::rotationtrace {
#if defined(CHLORIUM_FOLLOW_TRACE_R76) || defined(CHLORIUM_FOLLOW_TRACE_AUDIT_R76)
inline constexpr unsigned traceVersion=76;
#elif defined(CHLORIUM_MIXED_TRACE_R74) || defined(CHLORIUM_MIXED_TRACE_AUDIT_R74)
inline constexpr unsigned traceVersion=74;
#else
inline constexpr unsigned traceVersion=72;
#endif
struct Stats {unsigned version=traceVersion,active=0,complete=0,reasons=0,frames=0,rows=0,closed=0,actors=0;};
struct alignas(16) Row {
    mixedprogram::State before;mixedprogram::Command command;mixedprogram::State after;
    std::uint64_t address=0;unsigned uid=0,frame=0,closed=0;float localRotation=0;unsigned controls=0,reserved=0;
};
struct Header {
#if defined(CHLORIUM_FOLLOW_TRACE_R76) || defined(CHLORIUM_FOLLOW_TRACE_AUDIT_R76)
    char magic[8]{'C','H','L','M','I','X','7','6'};
#elif defined(CHLORIUM_MIXED_TRACE_R74) || defined(CHLORIUM_MIXED_TRACE_AUDIT_R74)
    char magic[8]{'C','H','L','M','I','X','7','4'};
#else
    char magic[8]{'C','H','L','R','O','T','7','2'};
#endif
    unsigned version=traceVersion,rowBytes=sizeof(Row),frames=0,rows=0,actors=0,reasons=0,ownerUid=0,thread=0;
    std::uint64_t owner=0;double percent=0;std::uint64_t calls=0;
};
static_assert(sizeof(Row)==208 && sizeof(Header)==64);
void begin(PlayLayer*,bool isolated)noexcept;
void end()noexcept;
}
