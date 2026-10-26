#pragma once
#include <cstdint>
#include <stdexcept>
namespace chlorium::snapshot {
// Native Cocos executes its complete draw contract using an owned vertex store.
// The original store remains dirty until an actual native fallback updates it.
class NativeVertexLease {
public:
    NativeVertexLease(std::uint32_t& store,bool& dirty,std::uint32_t owned):store_(store),dirty_(dirty),original_(store) {
        if(!owned) throw std::invalid_argument("empty leased native vertex store");
        store_=owned;dirty_=false;
    }
    ~NativeVertexLease() noexcept {store_=original_;dirty_=true;}
    NativeVertexLease(const NativeVertexLease&)=delete;
    NativeVertexLease& operator=(const NativeVertexLease&)=delete;
private:
    std::uint32_t& store_;bool& dirty_;std::uint32_t original_;
};
}
