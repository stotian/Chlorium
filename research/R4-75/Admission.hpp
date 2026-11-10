#pragma once
#include "GeometryTrace.hpp"
#include "../R4-33/SpriteEncoding.hpp"
namespace chlorium::research::rootgeometry {
struct Admission {bool accepted=false,poseDeclined=false,rootChanged=false;unsigned biasDeclines=0;};
inline Admission encodeNativeRow(const Row& row,std::span<const mixedprogram::State> states,unsigned poseSlot,resident::Geometry& g,resident::Parameters& p){
    resident::encodeSprite(row.source,row.quad,g,p);Admission result;const bool poseValid=poseSlot<states.size();g.control[1]=poseValid?poseSlot:0;result.poseDeclined=!poseValid;
    bool accept=poseValid && (row.controls&0x7f80u)==0x1f80u && row.atlas && row.batch && row.source.quadMatches && !(row.source.flags&(1u|2u|64u|128u)) && (row.source.flags&1536u)==1536u && g.control[0];
    if(poseValid && std::memcmp(states[poseSlot].position,row.root.position,16)){accept=false;result.rootChanged=true;}
    auto bias=[&](unsigned axis){const auto pose=_mm_cvtsd_ss(_mm_setzero_ps(),_mm_set_sd(row.root.position[axis]));const auto target=corners::bits(row.source.matrix[4+axis]);const auto value=_mm_sub_ss(target,pose);const auto roundtrip=_mm_add_ss(pose,value);
        const auto bits=std::bit_cast<unsigned>(_mm_cvtss_f32(roundtrip));if(bits!=row.source.matrix[4+axis]){accept=false;++result.biasDeclines;}p.translationZFlags[axis]=std::bit_cast<unsigned>(_mm_cvtss_f32(value));};
    if(accept){bias(0);bias(1);}g.control[0]=accept;result.accepted=accept;return result;
}
}
