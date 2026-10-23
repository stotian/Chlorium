#pragma once

#include <common.hpp>

#include "ObjectBatch.hpp"
#include "ObjectUtils.hpp"

class Renderer;

class ObjectBatchNode : public cocos2d::CCNode {
public:
    inline ObjectBatchNode(Renderer* owner, const LayerKey& id)
        : owner(owner), layerId(id) {}

    bool init() override;

    void draw() override;

public:
    static inline Ref<ObjectBatchNode> create(Renderer* owner, const LayerKey& id) {
        auto ret = new ObjectBatchNode(owner, id);
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

private:
    // Compare with the live renderer before dereferencing; strong ownership
    // here would cycle with Renderer::batchNodes.
    Renderer* owner;
    LayerKey layerId;
    cocos2d::CCTexture2D* spriteSheetTexture;
};
