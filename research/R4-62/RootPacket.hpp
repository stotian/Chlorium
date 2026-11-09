#pragma once
#include "../R4-33/ResidentRecords.hpp"
#include "../R4-33/SpriteCorners.hpp"
#include "../R4-42/GroupTape.hpp"
#include <algorithm>
#include <cstring>

namespace chlorium::research::resident {
inline bool rootPacketEligible(std::span<const grouptape::State> states,std::span<const Geometry> geometry,std::span<const Parameters> parameters){
    if(states.empty() || geometry.size()!=states.size() || parameters.size()!=states.size())return false;
    for(std::size_t i=0;i<states.size();++i){
        const auto& g=geometry[i];const auto& p=parameters[i];
        if(g.control[0]!=1 || g.control[1] || g.control[2] || g.control[3] ||
           p.translationZFlags[0] || p.translationZFlags[1] || (p.translationZFlags[3]&~(1u|512u|1024u)))return false;
        pass::SpriteRecord source{};std::copy_n(g.rect,4,source.localRect);std::copy_n(p.matrix,4,source.matrix);
        source.matrix[4]=std::bit_cast<unsigned>(float(states[i].position[0]));
        source.matrix[5]=std::bit_cast<unsigned>(float(states[i].position[1]));
        source.z=p.translationZFlags[2];source.flags=p.translationZFlags[3];
        if(!corners::domain(source))return false;
    }
    return true;
}
}
