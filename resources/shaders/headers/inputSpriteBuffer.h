#define SPRITE_TYPE_BASE   0
#define SPRITE_TYPE_DETAIL 1
#define SPRITE_TYPE_BLACK  2
#define SPRITE_TYPE_GLOW   3

/*
    This is the sprite info that is the input to the
    object culling compute shader.
*/
struct InputSprite {
    // Index into the object info buffer
    uint objectIndex;
    // The color type of the sprite, uses SPRITE_TYPE_* defines
    uint spriteType;
    // The place inside the index buffer where to find the first index belonging to this sprite
    uint indexStart;
    // Number of indicies belonging to this sprite
    uint indexCount;
};

STORAGE_BUFFER(INPUT_SPRITE_BUFFER_BINDING) InputSpriteBuffer {
    InputSprite u_inputSprites[CPP_ONLY(0)];
};

/*
    This contains the bare minimum info you
    need to check if a sprite is on screen or not.
    Keeping it small is important to make culling
    faster.
*/
struct PreCulledSprite {
    uint groupCombinationIndex;
    float radius;
    vec2 startPosition;
};

STORAGE_BUFFER(PRE_CULLED_SPRITE_BUFFER_BINDING) PreCulledSpriteBuffer {
    PreCulledSprite u_preCulledSprites[CPP_ONLY(0)];
};
