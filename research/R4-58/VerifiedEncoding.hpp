#pragma once
#include "../R4-33/SpriteEncoding.hpp"

namespace chlorium::research::resident {
// Cache the pure encoding only; the caller still performs Scene::update and its guards.
class VerifiedEncoding {
    pass::SpriteRecord source_{};
    snapshot::NativeQuadState native_{};
    Geometry geometry_{};
    Parameters parameters_{};
    unsigned control_=0;
    bool valid_=false;
public:
    template<class Encode>
    bool get(const pass::SpriteRecord& source,const snapshot::NativeQuadState& native,unsigned control,
             Geometry& g,Parameters& p,Encode&& encode){
        const bool reuse=valid_ && control_==control && !std::memcmp(&source_,&source,sizeof(source)) &&
            !std::memcmp(&native_,&native,sizeof(native));
        if(!reuse){
            Geometry nextG{};Parameters nextP{};encode(source,native,nextG,nextP);
            std::memcpy(&source_,&source,sizeof(source));std::memcpy(&native_,&native,sizeof(native));
            geometry_=nextG;parameters_=nextP;control_=control;valid_=true;
        }
        g=geometry_;p=parameters_;return reuse;
    }
};
}
