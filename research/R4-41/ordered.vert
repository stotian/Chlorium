#version 430 core
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
uniform mat4 CC_MVPMatrix;
uniform float quadCount;
uniform bool orderedDepth;
invariant gl_Position;
out vec4 renderColor;
out vec2 texCoord;
void main(){
    gl_Position=CC_MVPMatrix*vec4(position,1);
    if(orderedDepth){
        float ordinal=float(gl_VertexID/4);
        float depth=1.0-(ordinal+1.0)/(quadCount+1.0);
        gl_Position.z=(depth*2.0-1.0)*gl_Position.w;
    }
    renderColor=color;texCoord=uv;
}
