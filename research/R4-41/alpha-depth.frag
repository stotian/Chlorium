#version 430 core
in vec4 renderColor;
in vec2 texCoord;
uniform sampler2D CC_Texture0;
void main(){
    vec4 sampled=renderColor*texture(CC_Texture0,texCoord);
    if(sampled.a!=1.0)discard;
}
