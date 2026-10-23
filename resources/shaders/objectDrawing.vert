#extension GL_ARB_shading_language_include:require

#define SHADER_SPRITE_DRAW
#include "shared.h"

//// VERTEX ATTRIBUTES ////
layout (location = 0) in vec2 a_positionOffset;
layout (location = 1) in vec2 a_texCoord;
layout (location = 2) in uint a_spriteIndex;

//// VARIABLES TO BE TRANSFERED TO THE FRAGMENT SHADER ////
out vec2 t_texCoord;
out vec4 t_color;

// Raw sprites are from the raw sprite buffer (headers/rawSpriteBuffer.h)
#define RAW_SPRITE (u_rawSprites[a_spriteIndex])

//// MAIN FUNCTION ////
void main() {
    vec2 vertexPos = RAW_SPRITE.position + RAW_SPRITE.localTransform * a_positionOffset;

    gl_Position = u_mvp * vec4(vertexPos, 0.0, 1.0);

    t_color    = RAW_SPRITE.color;
    t_texCoord = a_texCoord;
}
