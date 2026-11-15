#version 430 core
uniform sampler2D CC_Texture0;
in vec4 v_fragmentColor;
in vec2 v_texCoord;
out vec4 FragColor;
void main() {
    FragColor = v_fragmentColor * texture(CC_Texture0, v_texCoord);
}
