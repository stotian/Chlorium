#pragma once
#include <string_view>
namespace chlorium::test {
inline bool nativeProducerObserverName(std::string_view name){
    return name=="cocos2d::CCTextureAtlas::updateQuad" || name=="cocos2d::CCSprite::updateTransform" || name=="cocos2d::CCSprite::updateColor";
}
inline bool nativePassObserverAllowed(bool compiledCaptureProfile, bool nativeMode,
                                     bool productionLoaded, std::string_view hookName,bool producerCapture=false) {
    return compiledCaptureProfile && nativeMode && !productionLoaded &&
        (hookName == "cocos2d::CCTextureAtlas::drawNumberOfQuads" ||
         (producerCapture && nativeProducerObserverName(hookName)));
}
}
