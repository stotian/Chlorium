#pragma once
// Include after the current context's OpenGL API declarations.
#include "NativeQuadState.hpp"

namespace chlorium::snapshot {
// Same fixed attribute ABI as desktop Cocos' native atlas. Only our VAO is
// modified; the native index store is bound read-only and never rewritten.
inline void configureNativeVertexLayout(GLuint vao, GLuint vertexBuffer) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(NativeQuadVertex),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(NativeQuadVertex),reinterpret_cast<void*>(offsetof(NativeQuadVertex,color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(NativeQuadVertex),reinterpret_cast<void*>(offsetof(NativeQuadVertex,u)));
}
inline void submitNativeVertices(GLuint vao, GLuint nativeIndices, GLuint texture,
                                 GLsizei count, std::size_t first) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texture);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,nativeIndices);
    // Verified GD 2.2081 Cocos routine: triangles, unsigned-short indices,
    // six indices per quad and a byte offset of first * 12.
    glDrawElements(GL_TRIANGLES,count*6,GL_UNSIGNED_SHORT,
        reinterpret_cast<void*>(first*6*sizeof(std::uint16_t)));
}
}
