#version 430 core
in vec4 a_position;
in vec4 a_color;
in vec2 a_texCoord;
uniform mat4 CC_MVPMatrix;
out vec4 v_fragmentColor;
out vec2 v_texCoord;
void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
}
