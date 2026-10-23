#include "ObjectBatchNode.hpp"
#include "Geode/cocos/platform/win32/CCGL.h"
#include "Renderer.hpp"
#include <profiler.hpp>

using namespace geode::prelude;

bool ObjectBatchNode::init() {
    if (!CCNode::init() || !owner) return false;
    spriteSheetTexture = owner->getSpriteSheetTexture(layerId.spriteSheet);
    return spriteSheetTexture != nullptr;
}

void ObjectBatchNode::draw() {
    if (!spriteSheetTexture)
        return;

    auto renderer = Renderer::get();
    if (!renderer || renderer.operator->() != owner || !renderer->isEnabled()) return;
    PROFILER_PUSH("ObjectBatchNode::draw");

    auto shader = renderer->prepareDraw();
    shader->setTexture("u_spriteSheetTexture", 0, spriteSheetTexture);

    glBlendEquation(GL_FUNC_ADD);
    glEnable(GL_BLEND);
    if (layerId.blending)
        glBlendFunc(GL_ONE, GL_ONE);
    else
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    renderer->getObjectBatch().draw(layerId);

    renderer->finishDraw();
    PROFILER_POP();
}
