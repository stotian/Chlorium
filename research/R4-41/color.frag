#version 430 core
in vec4 renderColor;
in vec2 texCoord;
uniform sampler2D CC_Texture0;
out vec4 frag;
void main(){frag=renderColor*texture(CC_Texture0,texCoord);}
