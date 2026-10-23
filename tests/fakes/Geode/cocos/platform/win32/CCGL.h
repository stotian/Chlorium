#pragma once
#include <cstddef>
#include <cstdint>
using GLenum = unsigned;
using GLuint = unsigned;
using GLint = int;
using GLsizei = int;
using GLint64 = std::int64_t;
enum : unsigned {
    GL_ARRAY_BUFFER_BINDING = 1, GL_ARRAY_BUFFER, GL_UNIFORM_BUFFER,
    GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE, GL_STATIC_DRAW, GL_DYNAMIC_DRAW,
    GL_DYNAMIC_COPY, GL_BUFFER, GL_BUFFER_SIZE, GL_MAP_WRITE_BIT = 0x10,
    GL_MAP_INVALIDATE_RANGE_BIT = 0x20
};
void glGetIntegerv(GLenum, GLint*);
void glBindBuffer(GLenum, GLuint);
void glGetBufferSubData(GLenum, std::size_t, std::size_t, void*);
void glBufferSubData(GLenum, std::size_t, std::size_t, const void*);
void* glMapBufferRange(GLenum, std::size_t, std::size_t, unsigned);
bool glUnmapBuffer(GLenum);
void glGenBuffers(GLsizei, GLuint*);
void glDeleteBuffers(GLsizei, const GLuint*);
void glObjectLabel(GLenum, GLuint, GLsizei, const char*);
void glBufferData(GLenum, std::size_t, const void*, GLenum);
void glGetBufferParameteri64v(GLenum, GLenum, GLint64*);
void glBindBufferBase(GLenum, GLuint, GLuint);
