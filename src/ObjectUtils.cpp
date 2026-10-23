#include "ObjectUtils.hpp"
#include "Geode/cocos/cocoa/CCAffineTransform.h"

using namespace geode::prelude;

const char* zLayerAsString(ZLayer zLayer) {
    switch (zLayer) {
    case ZLayer::B5:      return "B5";
    case ZLayer::B4:      return "B4";
    case ZLayer::B3:      return "B3";
    case ZLayer::B2:      return "B2";
    case ZLayer::B1:      return "B1";
    default:
    case ZLayer::Default: return "Default";
    case ZLayer::T1:      return "T1";
    case ZLayer::T2:      return "T2";
    case ZLayer::T3:      return "T3";
    case ZLayer::T4:      return "T4";
    }
}

const char* spriteSheetAsString(SpriteSheet spriteSheet) {
    switch (spriteSheet) {
    case SpriteSheet::GLOW:     return "GLOW";
    case SpriteSheet::PARTICLE: return "PARTICLE";
    case SpriteSheet::PIXEL:    return "PIXEL";
    case SpriteSheet::GAME_2:   return "GAME_2";
    case SpriteSheet::FIRE:     return "FIRE";
    case SpriteSheet::TEXT:     return "TEXT";
    case SpriteSheet::SPECIAL:  return "SPECIAL";
    default:
    case SpriteSheet::_UNK:     return "_UNK"; 
    case SpriteSheet::GAME_1:   return "GAME_1";
    }
}

u8 LayerKey::asNumber() const {
    u8 zlayerKey = (u8)zlayer;
    switch (zlayer) {
    case ZLayer::B5:      zlayerKey = 0; break;
    case ZLayer::B4:      zlayerKey = 1; break;
    case ZLayer::B3:      zlayerKey = 2; break;
    case ZLayer::B2:      zlayerKey = 3; break;
    case ZLayer::B1:      zlayerKey = 4; break;
    default:
    case ZLayer::Default: zlayerKey = 5; break;
    case ZLayer::T1:      zlayerKey = 6; break;
    case ZLayer::T2:      zlayerKey = 7; break;
    case ZLayer::T3:      zlayerKey = 8; break;
    case ZLayer::T4:      zlayerKey = 9; break;
    }

    u8 spriteSheetKey = 0;
    switch (spriteSheet) {
    case SpriteSheet::GLOW:     spriteSheetKey = 0; break;
    case SpriteSheet::PARTICLE: spriteSheetKey = 1; break;
    case SpriteSheet::PIXEL:    spriteSheetKey = 2; break;
    case SpriteSheet::GAME_2:   spriteSheetKey = 3; break;
    case SpriteSheet::FIRE:     spriteSheetKey = 4; break;
    case SpriteSheet::TEXT:     spriteSheetKey = 5; break;
    case SpriteSheet::SPECIAL:  spriteSheetKey = 6; break;
    default:
    case SpriteSheet::_UNK:
    case SpriteSheet::GAME_1:   spriteSheetKey = 7; break;
    }
    
    return (zlayerKey << 4) | (spriteSheetKey << 1) | (blending ? 1 : 0);
}

LayerKey LayerKey::fromNumber(u8 num) {
    u8 zlayerKey = num >> 4;
    u8 spriteSheetKey = (num >> 1) & 0x7;
    bool blending = num & 1;

    ZLayer zlayer;
    switch (zlayerKey) {
    case 0: zlayer = ZLayer::B5; break;
    case 1: zlayer = ZLayer::B4; break;
    case 2: zlayer = ZLayer::B3; break;
    case 3: zlayer = ZLayer::B2; break;
    case 4: zlayer = ZLayer::B1; break;
    default:
    case 5: zlayer = ZLayer::Default; break;
    case 6: zlayer = ZLayer::T1; break;
    case 7: zlayer = ZLayer::T2; break;
    case 8: zlayer = ZLayer::T3; break;
    case 9: zlayer = ZLayer::T4; break;
    }

    SpriteSheet spriteSheet;
    switch (spriteSheetKey) {
    case 0: spriteSheet = SpriteSheet::GLOW; break;
    case 1: spriteSheet = SpriteSheet::PARTICLE; break;
    case 2: spriteSheet = SpriteSheet::PIXEL; break;
    case 3: spriteSheet = SpriteSheet::GAME_2; break;
    case 4: spriteSheet = SpriteSheet::FIRE; break;
    case 5: spriteSheet = SpriteSheet::TEXT; break;
    case 6: spriteSheet = SpriteSheet::SPECIAL; break;
    default:
    case 7: spriteSheet = SpriteSheet::GAME_1; break;
    }

    return { zlayer, spriteSheet, blending };
}

static bool isInSubtree(CCNode* node, CCNode* root) {
    for (auto ancestor = node; ancestor; ancestor = ancestor->getParent())
        if (ancestor == root) return true;
    return false;
}

static CCAffineTransform parentToObjectSpace(CCNode* node, GameObject* object) {
    auto parent = node->getParent();
    auto objectParent = object->getParent();
    if (!parent || parent == objectParent) return CCAffineTransformMakeIdentity();

    // Stop at the common ancestor: shared camera/object-layer transforms
    // cancel out, and must not be inverted (a transition may scale them to 0).
    auto commonAncestor = parent;
    while (commonAncestor && !isInSubtree(objectParent, commonAncestor))
        commonAncestor = commonAncestor->getParent();

    auto toCommonAncestor = [commonAncestor](CCNode* ancestor) {
        auto transform = CCAffineTransformMakeIdentity();
        for (; ancestor && ancestor != commonAncestor; ancestor = ancestor->getParent())
            transform = CCAffineTransformConcat(transform, ancestor->nodeToParentTransform());
        return transform;
    };
    return CCAffineTransformConcat(toCommonAncestor(parent),
        CCAffineTransformInvert(toCommonAncestor(objectParent)));
}

void ObjectUtils::unpackObjectIntoSprites(GameObject* object, ReceiveUnpackedSpriteFunc func) {
    if (!object) return;
    CCSprite* colorSprite = object->m_colorSprite;
    CCSprite* glowSprite = object->m_glowSprite;

    // Each root owns its whole subtree. A glow/detail child may be nested
    // below a container, or below the other sprite, rather than directly
    // below the object. Walking it separately duplicates pixels and loses
    // every ancestor transform.
    bool shouldUnpackGlowSprite = glowSprite && !isInSubtree(glowSprite, object) &&
        (!colorSprite || colorSprite == glowSprite || !isInSubtree(glowSprite, colorSprite));
    bool shouldUnpackColorSprite = colorSprite && colorSprite != glowSprite &&
        !isInSubtree(colorSprite, object) && (!glowSprite || !isInSubtree(colorSprite, glowSprite));
    bool isColorSpriteInFront    = object->m_colorZLayerRelated;

    auto transform = CCAffineTransformMakeIdentity();

    if (shouldUnpackGlowSprite)
        unpackSpriteRecursively(object, glowSprite, parentToObjectSpace(glowSprite, object), func);

    if (shouldUnpackColorSprite && !isColorSpriteInFront)
        unpackSpriteRecursively(object, colorSprite, parentToObjectSpace(colorSprite, object), func);

    unpackSpriteRecursively(object, object, transform, func);

    if (shouldUnpackColorSprite && isColorSpriteInFront)
        unpackSpriteRecursively(object, colorSprite, parentToObjectSpace(colorSprite, object), func);
}

void ObjectUtils::unpackSpriteRecursively(
    GameObject* object,
    cocos2d::CCNode* node,
    cocos2d::CCAffineTransform transform,
    const ReceiveUnpackedSpriteFunc& func,
    SpriteType type
) {
    if (!node) return;
    transform = CCAffineTransformConcat(node->nodeToParentTransform(), transform);
    auto sprite = typeinfo_cast<CCSprite*>(node);

    if (node == object->m_colorSprite) type = SpriteType::DETAIL;
    if (node == object->m_glowSprite)  type = SpriteType::GLOW;
    // Black detail coloring must not turn a glow sprite into a base-sheet
    // sprite. A sprite's atlas and its color classification are distinct.
    bool isSpriteBlack = type != SpriteType::GLOW &&
        (type == SpriteType::DETAIL ? object->m_isColorSpriteBlack : object->m_isObjectBlack);
    if (node->getTag() == 997) isSpriteBlack = false;

    CCArrayExt<CCNode*> spriteChildren = node->getChildren();

    for (auto child : spriteChildren) {
        if (child->getZOrder() < 0)
            unpackSpriteRecursively(object, child, transform, func, type);
    }
    
    if (sprite && !sprite->getDontDraw())
        func({sprite, object, isSpriteBlack ? SpriteType::BLACK : type, transform});

    for (auto child : spriteChildren) {
        if (child->getZOrder() >= 0)
            unpackSpriteRecursively(object, child, transform, func, type);
    }
}
