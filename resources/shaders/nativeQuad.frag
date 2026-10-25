uniform sampler2D u_spriteSheetTexture;
smooth in vec2 t_texCoord;
smooth in vec4 t_color;
layout(location=0) out vec4 FragColor;
// Match native PositionTextureColor: no alpha discard, RGB premultiplication,
// gamma conversion or opacity threshold. Blend factors belong to the batch.
void main() { FragColor = t_color * texture(u_spriteSheetTexture,t_texCoord); }
