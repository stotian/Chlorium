#pragma once
#include <cstddef>
#include <cstdint>

struct NativeGpuReplacement {std::size_t bytes=0,copied=0;};
// A dirty native draw only makes its requested CPU range authoritative. Keep
// every other byte from the actual native GPU store, including GPU-only data.
// Rebuild a distinct owned store before exchanging ownership with Cocos.
inline NativeGpuReplacement rebuildNativeGpuStore(GLuint source,GLuint target,std::size_t offset,
    std::size_t count,const void* cpu,std::size_t budget=32u*1024u*1024u) {
    if(!source||!target||source==target||!count||!cpu||!wglGetCurrentContext()||
        !glCopyBufferSubData||!glGetBufferParameteri64v||!glIsBuffer(source)||!glIsBuffer(target)) return {};
    // The bundled GLEW predates these queries. Require core 4.4 instead of
    // assuming an extension exists. Values: Khronos OpenGL-Registry glcorearb.h.
    GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
    if(major<4||(major==4&&minor<4)) return {};
    constexpr GLenum immutableStorage=0x821F,transformFeedbackActive=0x8E24;
    struct Bindings {
        GLint array=0,read=0,write=0;
        // COPY_*_BUFFER and COPY_*_BUFFER_BINDING share their core enum values.
        Bindings(){glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);glGetIntegerv(GL_COPY_READ_BUFFER,&read);glGetIntegerv(GL_COPY_WRITE_BUFFER,&write);}
        ~Bindings(){glBindBuffer(GL_ARRAY_BUFFER,array);glBindBuffer(GL_COPY_READ_BUFFER,read);glBindBuffer(GL_COPY_WRITE_BUFFER,write);}
    } saved;
    glBindBuffer(GL_ARRAY_BUFFER,source);GLint64 bytes=0;glGetBufferParameteri64v(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
    GLint mapped=0;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_MAPPED,&mapped);
    if(bytes<=0||static_cast<std::uint64_t>(bytes)>budget||offset>static_cast<std::size_t>(bytes)||
        count>static_cast<std::size_t>(bytes)-offset||mapped) return {};
    glBindBuffer(GL_ARRAY_BUFFER,target);glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_MAPPED,&mapped);
    if(mapped) return {};
    GLint immutable=0;glGetBufferParameteriv(GL_ARRAY_BUFFER,immutableStorage,&immutable);
    if(immutable) return {};
    GLboolean feedback=GL_FALSE;glGetBooleanv(transformFeedbackActive,&feedback);
    if(feedback) return {};
    glBufferData(GL_ARRAY_BUFFER,bytes,nullptr,GL_STREAM_DRAW);
    GLint64 allocated=0;glGetBufferParameteri64v(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&allocated);
    if(allocated!=bytes) return {};
    // Upload before queued GPU copies to avoid writing their destination from
    // the CPU afterwards. Orphaning never discards canonical native storage.
    glBufferSubData(GL_ARRAY_BUFFER,static_cast<GLintptr>(offset),static_cast<GLsizeiptr>(count),cpu);
    glBindBuffer(GL_COPY_READ_BUFFER,source);glBindBuffer(GL_COPY_WRITE_BUFFER,target);
    if(offset) glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,0,0,static_cast<GLsizeiptr>(offset));
    const auto tail=offset+count;
    if(tail<static_cast<std::size_t>(bytes)) glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,
        static_cast<GLintptr>(tail),static_cast<GLintptr>(tail),static_cast<GLsizeiptr>(bytes-tail));
    return {static_cast<std::size_t>(bytes),static_cast<std::size_t>(bytes)-count};
}
