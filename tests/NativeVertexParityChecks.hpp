#pragma once
#include "renderer/NativeVertexSubmission.hpp"

// Include after this fixture's require()/link() helpers and OpenGL headers.
void nativeVertexParityChecks() {
    const auto saved=GLStateSnapshot::capture();
    auto raw=[](GLenum type,const char* source) {
        const auto shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);require(ok!=0,"native vertex parity fixture shader compiles");
        return shader;
    };
    const auto program=link({
        raw(GL_VERTEX_SHADER,R"(#version 330 core
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec2 a_texCoord;
uniform mat4 CC_MVPMatrix;
out vec4 color;out vec2 uv;
void main(){gl_Position=CC_MVPMatrix*vec4(a_position,1);color=a_color;uv=a_texCoord;})"),
        raw(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec4 color;in vec2 uv;uniform sampler2D CC_Texture0;uniform vec4 native_effect;
out vec4 frag;void main(){frag=color*texture(CC_Texture0,uv)*native_effect;})")});
    std::array<chlorium::snapshot::NativeQuadState,2> quads{};
    quads[0].corners={{{-.9f,.8f,0,0xff0000ffu,0,1},{-.9f,-.8f,0,0xff00ff00u,0,0},
                       {-.1f,.8f,0,0xffff0000u,1,1},{-.1f,-.8f,0,0x80ffffffu,1,0}}};
    quads[1]=quads[0];for(auto& vertex:quads[1].corners) vertex.x+=1.f;
    const std::uint16_t elements[]{0,1,2,3,2,1,4,5,6,7,6,5};
    GLuint vaos[2]{},stores[3]{},texture=0;
    glGenVertexArrays(2,vaos);glGenBuffers(3,stores);glGenTextures(1,&texture);
    for(unsigned i=0;i<2;++i) {
        glBindBuffer(GL_ARRAY_BUFFER,stores[i]);glBufferData(GL_ARRAY_BUFFER,sizeof(quads),quads.data(),GL_STATIC_DRAW);
        chlorium::snapshot::configureNativeVertexLayout(vaos[i],stores[i]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,stores[2]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(elements),elements,GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned white=0xffffffffu;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glUseProgram(program);glUniform1i(glGetUniformLocation(program,"CC_Texture0"),0);
    glViewport(0,0,64,64);glClearColor(0,0,0,0);
    using Pixels=std::array<unsigned char,64*64*4>;
    auto read=[] {Pixels p{};glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    unsigned scenarios=0;
    for (unsigned first : {0u,1u}) for(bool blend : {false,true}) for(bool cull : {false,true})
        for(bool clipped : {false,true}) for(bool dark : {false,true}) {
        glm::mat4 matrix(1);matrix[3][0]=clipped?5.f:.025f;matrix[0][1]=.15f;
        glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"CC_MVPMatrix"),1,GL_FALSE,&matrix[0][0]);
        glUniform4f(glGetUniformLocation(program,"native_effect"),dark?0.f:.7f,.4f,.9f,1.f);
        if(blend) glEnable(GL_BLEND);else glDisable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ZERO);
        if(cull) glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
        glCullFace(GL_BACK);glFrontFace(GL_CCW);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(vaos[0]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,stores[2]);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_SHORT,reinterpret_cast<void*>(first*12));
        const auto expected=read();
        const auto before=GLStateSnapshot::capture();
        glClear(GL_COLOR_BUFFER_BIT);
        {
            const auto state=GLStateSnapshot::capture(GLStateScope::NativeVertex);
            chlorium::snapshot::submitNativeVertices(vaos[1],stores[2],texture,1,first);
            state.restore();
        }
        const auto actual=read();
        require(actual==expected,"native vertex helper preserves every pixel with native matrices/effects/blend/cull/subrange");
        const auto after=GLStateSnapshot::capture();
        require(before.program==after.program && before.vao==after.vao && before.arrayBuffer==after.arrayBuffer &&
            before.elementBuffer==after.elementBuffer && before.activeTexture==after.activeTexture && before.textures==after.textures,
            "native vertex helper restores touched bindings and keeps program");
        std::array<float,16> uniform{};glGetUniformfv(program,glGetUniformLocation(program,"CC_MVPMatrix"),uniform.data());
        require(!std::memcmp(uniform.data(),&matrix[0][0],sizeof(matrix)),"native MVP uniform is never rewritten");
        ++scenarios;
    }
    // Negative control: a guessed, zero MVP erases visible geometry. The
    // full-pixel comparator must reject it rather than accepting a black draw.
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glUseProgram(program);
    const glm::mat4 identity(1),wrong(0);
    glUniform4f(glGetUniformLocation(program,"native_effect"),1,1,1,1);
    glUniformMatrix4fv(glGetUniformLocation(program,"CC_MVPMatrix"),1,GL_FALSE,&identity[0][0]);
    glClear(GL_COLOR_BUFFER_BIT);chlorium::snapshot::submitNativeVertices(vaos[1],stores[2],texture,1,0);const auto visible=read();
    glUniformMatrix4fv(glGetUniformLocation(program,"CC_MVPMatrix"),1,GL_FALSE,&wrong[0][0]);
    glClear(GL_COLOR_BUFFER_BIT);chlorium::snapshot::submitNativeVertices(vaos[1],stores[2],texture,1,0);
    require(read()!=visible,"pixel oracle detects deliberate black output from an incorrect MVP");
    std::array<std::uint16_t,12> retained{};glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,stores[2]);
    glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,sizeof(retained),retained.data());
    require(!std::memcmp(retained.data(),elements,sizeof(elements)),"borrowed native index store never modified");
    glDisable(GL_CULL_FACE);glFrontFace(GL_CCW);
    saved.restore();
    glDeleteVertexArrays(2,vaos);glDeleteBuffers(3,stores);glDeleteTextures(1,&texture);glDeleteProgram(program);
    require(glGetError()==GL_NO_ERROR,"native vertex parity scenarios generate no GL errors");
    std::cout<<"Native vertex exact pixel scenarios: "<<scenarios<<"\n";
}
