#pragma once
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace chlorium::research::phase {
struct State {double position;float offset;};
inline State eager(State s,std::span<const float> offsets) {
    for(const float offset:offsets) {
        if(s.offset!=0) s.position-=static_cast<double>(s.offset);
        s.offset=offset;
        if(offset!=0) s.position+=static_cast<double>(offset);
    }
    return s;
}
inline bool normalDouble(double value) {
    const auto exponent=(std::bit_cast<std::uint64_t>(value)>>52)&2047;
    return exponent&&exponent!=2047;
}
inline bool normalOrZero(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value)&0x7fffffffu;
    const auto exponent=bits>>23;
    return !bits||(exponent&&exponent!=255);
}
struct Result {State state;bool coalesced=false;};
// Immutable command ownership prevents stale proof metadata from certifying a
// different stream. This closed model has no intermediate native observers.
class Program {
    std::vector<float> offsets_;
    int unit_=std::numeric_limits<int>::max(),maxExponent_=std::numeric_limits<int>::min();
    bool valid_=true;
public:
    explicit Program(std::span<const float> offsets) {
        if(offsets.size()>4096) throw std::length_error("phase program budget exceeded");
        offsets_.assign(offsets.begin(),offsets.end());
        for(const auto value:offsets_) {
            if(!normalOrZero(value)) {valid_=false;continue;}
            const auto bits=std::bit_cast<std::uint32_t>(value)&0x7fffffffu;
            if(!bits) continue;
            const int exponent=static_cast<int>(bits>>23)-127;
            const auto mantissa=(bits&0x7fffffu)|0x800000u;
            unit_=std::min(unit_,exponent-23+static_cast<int>(std::countr_zero(mantissa)));
            maxExponent_=std::max(maxExponent_,exponent);
        }
    }
    Result evaluate(State input) const {
        if(offsets_.empty()) return {input,false};
        if(!valid_||!normalDouble(input.position)||!normalOrZero(input.offset))
            return {eager(input,offsets_),false};
        auto base=input.position;
        if(input.offset!=0) base-=static_cast<double>(input.offset);
        if(!normalDouble(base)) return {eager(input,offsets_),false};
        const auto bits=std::bit_cast<std::uint64_t>(base)&0x7fffffffffffffffull;
        const int exponent=static_cast<int>(bits>>52)-1023;
        const auto mantissa=(bits&0xfffffffffffffull)|0x10000000000000ull;
        const int unit=std::min(unit_,exponent-52+static_cast<int>(std::countr_zero(mantissa)));
        // Both magnitudes are < 2^(unit+52). Their sum is an integer multiple
        // of 2^unit with magnitude < 2^(unit+53), hence exactly representable.
        // Every add/reset pair therefore restores this exact canonical base.
#ifndef CHLORIUM_PRIVATE_PHASE_UNSAFE_MUTANT
        if(exponent>unit+51||maxExponent_>unit+51) return {eager(input,offsets_),false};
#endif
        const auto last=offsets_.back();
        if(last!=0) base+=static_cast<double>(last);
        return {{base,last},true};
    }
};
}
