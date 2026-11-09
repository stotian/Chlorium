#pragma once
#include "SameCell.hpp"
#include "../../src/renderer/NativeMovementLoop.hpp"

namespace chlorium::research::samecell {
struct PacketStats {std::uint64_t visited=0,unchanged=0;};
using SectionCall=void(*)(void*,void*);
// Caller must prove the callback is the exact native section body; otherwise run native moveObjects.
template<class Object>
PacketStats advancePacket(Object* const* objects,unsigned count,int command,double dx,double dy,
                          const float* inverse,void* layer,SectionCall section){
    PacketStats stats;PacketClassifier classifier(inverse);
    for(unsigned i=0;i<count;++i){auto* object=objects[i];if(!object)break;
        chlorium::native::advanceNativeMove(*object,command,dx,dy);++stats.visited;
        if(classifier.unchanged(object))++stats.unchanged;else{section(layer,object);classifier.refresh();}
    }
    return stats;
}
}
