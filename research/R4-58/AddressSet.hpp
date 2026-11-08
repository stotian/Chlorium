#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

namespace chlorium::research::resident {
// Bounded scratch membership; one flat allocation replaces per-key hash nodes.
class AddressSet {
    std::vector<std::uint64_t> slots_;
    std::size_t count_=0,limit_=0;
    std::size_t bucket(std::uint64_t key)const noexcept{
        key^=key>>33;key*=0xff51afd7ed558ccdull;key^=key>>33;
        key*=0xc4ceb9fe1a85ec53ull;key^=key>>33;
        return std::size_t(key)&(slots_.size()-1);
    }
public:
    bool reset(std::size_t limit){
        if(limit>65536)return false;
        const auto size=std::bit_ceil(std::max<std::size_t>(8,limit*2));
        if(slots_.size()<size)slots_.resize(size);
        std::fill(slots_.begin(),slots_.end(),0);count_=0;limit_=limit;return true;
    }
    bool insert(std::uint64_t key)noexcept{
        if(!key || count_>=limit_ || slots_.empty())return false;
        auto at=bucket(key);
        while(slots_[at]){
            if(slots_[at]==key)return false;
            at=(at+1)&(slots_.size()-1);
        }
        slots_[at]=key;++count_;return true;
    }
    bool contains(std::uint64_t key)const noexcept{
        if(!key || slots_.empty())return false;
        auto at=bucket(key);
        while(slots_[at]){
            if(slots_[at]==key)return true;
            at=(at+1)&(slots_.size()-1);
        }
        return false;
    }
};
}
