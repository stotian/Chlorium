#pragma once
#include "../R4-33/ResidentRecords.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>
namespace chlorium::research::parameterclasses {
struct Frame {std::vector<resident::Parameters> values;std::vector<unsigned> slots;bool grouped=true;};
inline Frame build(std::span<const resident::Parameters> parameters,std::span<const unsigned char> admitted){
    if(parameters.size()>65536 || parameters.size()!=admitted.size())throw std::invalid_argument("bounded parameter/admission association");
    Frame frame;frame.slots.assign(parameters.size(),~0u);std::map<std::array<std::uint32_t,12>,unsigned> classes;
    for(unsigned i=0;i<parameters.size();++i){if(admitted[i]>1)throw std::invalid_argument("known parameter admission flag");if(!admitted[i])continue;
        const auto key=std::bit_cast<std::array<std::uint32_t,12>>(parameters[i]);auto [at,inserted]=classes.emplace(key,unsigned(frame.values.size()));
        if(inserted && classes.size()>256){
            // Bound classification work when sharing is poor; identity slots preserve every byte.
            frame.values.clear();frame.grouped=false;
            for(unsigned k=0;k<parameters.size();++k){if(admitted[k]>1)throw std::invalid_argument("known parameter admission flag");if(!admitted[k]){frame.slots[k]=~0u;continue;}frame.slots[k]=unsigned(frame.values.size());frame.values.push_back(parameters[k]);}
            return frame;
        }
        if(inserted)frame.values.push_back(parameters[i]);frame.slots[i]=at->second;}
    return frame;
}
class Cache {
    Frame frame_;std::vector<resident::Parameters> scratch_;std::vector<unsigned char> seen_;
public:
    const Frame& update(std::span<const resident::Parameters> parameters,std::span<const unsigned char> admitted){
        if(parameters.size()>65536 || parameters.size()!=admitted.size())throw std::invalid_argument("bounded retained parameter association");
        for(auto flag:admitted)if(flag>1)throw std::invalid_argument("known retained parameter admission flag");
        if(frame_.slots.size()==parameters.size()){
            scratch_.resize(frame_.values.size());seen_.assign(frame_.values.size(),0);bool uniform=true;
            for(unsigned i=0;i<parameters.size();++i){const auto slot=frame_.slots[i];if(!admitted[i]){if(slot!=~0u){uniform=false;break;}continue;}
                if(slot>=scratch_.size()){uniform=false;break;}if(!seen_[slot]){scratch_[slot]=parameters[i];seen_[slot]=1;}
                else if(std::memcmp(&scratch_[slot],&parameters[i],sizeof(parameters[i]))){uniform=false;break;}}
            if(uniform){frame_.values.swap(scratch_);return frame_;}
        }
        auto rebuilt=build(parameters,admitted);frame_=std::move(rebuilt);return frame_;
    }
};
using Range=std::array<unsigned,2>;
template<class T>std::vector<Range> changedRanges(std::span<const T> previous,std::span<const T> current){
    // Fully initialised wire records compare by bits, including float signed zeros/NaN payloads.
    static_assert(std::is_trivially_copyable_v<T>);
    if(previous.size()>65536 || current.size()>65536)throw std::invalid_argument("bounded retained record update");
    auto changed=[&](unsigned i){return i>=previous.size() || std::memcmp(&previous[i],&current[i],sizeof(T));};std::vector<Range> ranges;
    for(unsigned i=0;i<current.size();){if(!changed(i)){++i;continue;}const auto first=i++;while(i<current.size() && changed(i))++i;ranges.push_back({first,i-first});}
    return ranges;
}
}
