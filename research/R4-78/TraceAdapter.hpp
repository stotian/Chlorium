#pragma once
#include "../R4-72/TraceAdapter.hpp"
namespace cocos2d {class CCTextureAtlas;class CCSpriteBatchNode;}
namespace chlorium::research::liveroot {
enum class Status {inactive,native,gpu};
struct Stats {std::uint64_t version=78,frames=0,drawnMask=0,draws=0,quads=0,verifiedBytes=0,failures=0,declines=0;};
Status prepare(cocos2d::CCTextureAtlas*,cocos2d::CCSpriteBatchNode*,std::span<const rotationtrace::Row>,unsigned frame)noexcept;
void submitted()noexcept;
}
namespace chlorium::research::rotationtrace {
liveroot::Status livePrepare(cocos2d::CCTextureAtlas*,cocos2d::CCSpriteBatchNode*)noexcept;
}
