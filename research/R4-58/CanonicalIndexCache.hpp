#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace chlorium::research::continuous {
// The caller must qualify the native index writer and reset on context/lifetime changes.
class CanonicalIndexCache {
public:
    struct Owner {
        std::uintptr_t atlas=0,cpuIndices=0,context=0;
        unsigned uid=0,buffer=0,capacity=0;
        bool operator==(const Owner&)const=default;
    };
private:
    Owner owner_{};
    std::vector<unsigned char> verified_;
public:
    void reset()noexcept{owner_={};verified_.clear();}
    template<class Read>
    std::span<const unsigned char> acquire(Owner owner,std::span<const std::uint16_t> cpu,unsigned count,Read&& read){
        const auto capacity=std::min(owner.capacity,16384u);
        if(!owner.atlas || owner.cpuIndices!=reinterpret_cast<std::uintptr_t>(cpu.data()) || !owner.context ||
           !owner.buffer || !count || count>capacity || cpu.size()!=std::size_t(capacity)*6){
            reset();return {};
        }
        if(owner!=owner_){reset();owner_=owner;}
        const auto bytes=std::size_t(count)*12;
        if(!verified_.empty()){
            if(std::memcmp(cpu.data(),verified_.data(),bytes)){reset();return {};}
            return {verified_.data(),bytes};
        }
        std::vector<unsigned char> downloaded(std::size_t(capacity)*12);
        if(!read(std::span<unsigned char>(downloaded)) || std::memcmp(downloaded.data(),cpu.data(),downloaded.size()))return {};
        constexpr unsigned topology[]{0,1,2,3,2,1};
        for(std::size_t n=0;n<cpu.size();++n)if(cpu[n]!=(n/6)*4+topology[n%6])return {};
        verified_=std::move(downloaded);
        return {verified_.data(),bytes};
    }
};
}
