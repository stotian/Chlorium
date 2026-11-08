#version 430 core
struct Geometry {uvec4 rect;uvec4 uv01;uvec4 uv23;uvec4 control;};
struct Dynamic {uvec4 abcd;uvec4 translationZFlags;uvec4 colors;};
layout(std430,binding=0) readonly buffer Geometries {Geometry geometry[];};
layout(std430,binding=1) readonly buffer Parameters {Dynamic parameters[];};
layout(std430,binding=2) readonly buffer Order {uint slots[];};
layout(std430,binding=3) readonly buffer Fallback {uint words[];};
out vec3 nativePosition;
flat out uint nativeColor;
flat out uvec2 nativeUV;
out vec4 renderColor;
out vec2 texCoord;
uniform mat4 CC_MVPMatrix;
void main() {
    uint i=slots[uint(gl_VertexID)/4u],corner=uint(gl_VertexID)%4u;
    Dynamic p=parameters[i];Geometry g=geometry[i];
    nativeColor=p.colors[corner];
    uvec4 uv=corner<2u?g.uv01:g.uv23;
    nativeUV=(corner&1u)==0u?uv.xy:uv.zw;
    if(g.control.x==0u) {
        uint offset=i*24u+corner*6u;
        nativePosition=vec3(uintBitsToFloat(words[offset]),uintBitsToFloat(words[offset+1u]),uintBitsToFloat(words[offset+2u]));
    }else if((p.translationZFlags.w&1u)!=0u||(p.translationZFlags.w&1024u)==0u||(p.translationZFlags.w&512u)==0u){
        nativePosition=vec3(0);
    }else{
        precise float x1=uintBitsToFloat(g.rect.x),y1=uintBitsToFloat(g.rect.y);
        precise float x=corner<2u?x1:x1+uintBitsToFloat(g.rect.z);
        precise float y=(corner&1u)==0u?y1+uintBitsToFloat(g.rect.w):y1;
        precise float a=uintBitsToFloat(p.abcd.x),b=uintBitsToFloat(p.abcd.y);
        precise float nc=uintBitsToFloat(p.abcd.z^0x80000000u),d=uintBitsToFloat(p.abcd.w);
        precise float ax=a*x,bx=b*x,cy=nc*y,dy=d*y;
        precise float outX=(ax-cy)+uintBitsToFloat(p.translationZFlags.x);
        precise float outY=(corner&1u)==0u?(dy+bx)+uintBitsToFloat(p.translationZFlags.y):(bx+dy)+uintBitsToFloat(p.translationZFlags.y);
        nativePosition=vec3(outX,outY,uintBitsToFloat(p.translationZFlags.z));
    }
    renderColor=unpackUnorm4x8(nativeColor);texCoord=uintBitsToFloat(nativeUV);
    gl_Position=CC_MVPMatrix*vec4(nativePosition,1);
}
