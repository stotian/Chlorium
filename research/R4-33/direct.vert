#version 430 core
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
uniform mat4 CC_MVPMatrix;
out vec4 renderColor;
out vec2 texCoord;
out vec3 nativePosition;
flat out uint nativeColor;
flat out uvec2 nativeUV;
void main(){gl_Position=CC_MVPMatrix*vec4(position,1);renderColor=color;texCoord=uv;
nativePosition=position;nativeColor=packUnorm4x8(color);nativeUV=floatBitsToUint(uv);}
