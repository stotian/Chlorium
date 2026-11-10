#pragma once
#include <Geode/binding/GameObject.hpp>
#include "GeometryTrace.hpp"
#include "../R4-33/SpriteCorners.hpp"
namespace chlorium::research::rootgeometry {
struct Capture {Row row;bool supported=false;};
inline Capture capture(GameObject* object,const mixedprogram::State& root,unsigned frame,unsigned roleMask){
        Row row;row.root=root;row.frame=frame;row.controls=_mm_getcsr();row.roles=roleMask;row.atlas=reinterpret_cast<std::uintptr_t>(object->m_pobTextureAtlas);row.batch=reinterpret_cast<std::uintptr_t>(object->m_pobBatchNode);
        auto& source=row.source;source.sprite=source.root=reinterpret_cast<std::uintptr_t>(object);source.id=source.rootId=object->m_uID;source.rootFlags=1;source.atlasIndex=object->m_uAtlasIndex;
        source.parent=reinterpret_cast<std::uintptr_t>(object->m_pParent);source.objectId=object->m_objectID;source.objectType=unsigned(object->m_objectType);source.groupCount=unsigned(object->m_groupCount);
        using pass::nativeFlag;
        source.flags=nativeFlag(&object->m_bShouldBeHidden)|nativeFlag(&object->m_bDirty)<<1|nativeFlag(&object->m_bRecursiveDirty)<<2|nativeFlag(&object->m_bFlipX)<<3|nativeFlag(&object->m_bFlipY)<<4|nativeFlag(&object->m_bRectRotated)<<5|nativeFlag(&object->m_bUseVertexMod)<<6|nativeFlag(&object->m_bDontDraw)<<7|nativeFlag(&object->m_bOpacityModifyRGB)<<8|nativeFlag(&object->_realOpacity)<<9|nativeFlag(&object->m_bVisible)<<10;
        std::memcpy(source.matrix,&object->m_transformToBatch,24);const void* rect[]{&object->m_obOffsetPosition.x,&object->m_obOffsetPosition.y,&object->m_obRect.size.width,&object->m_obRect.size.height};
        for(unsigned k=0;k<4;++k)std::memcpy(source.localRect+k,rect[k],4);std::memcpy(source.anchor,&object->m_obAnchorPointInPoints,8);std::memcpy(&source.z,&object->m_fVertexZ,4);
        const void* mods[]{&object->m_fTlVertexMod,&object->m_fTrVertexMod,&object->m_fBlVertexMod,&object->m_fBrVertexMod};const void* bounds[]{&object->m_fTextureLeft,&object->m_fTextureRight,&object->m_fTextureBottom,&object->m_fTextureTop};
        for(unsigned k=0;k<4;++k){std::memcpy(source.vertexMods+k,mods[k],4);std::memcpy(source.textureBounds+k,bounds[k],4);}std::memcpy(&row.quad,&object->m_sQuad,96);
        auto predicted=row.quad;const bool supported=corners::materialize(source,predicted);if(supported)source.quadMatches=!std::memcmp(&predicted,&row.quad,96);return {row,supported};
}
}
