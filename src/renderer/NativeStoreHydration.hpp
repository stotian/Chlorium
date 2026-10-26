#pragma once
// Include after current-context GL declarations. Only current atlas storage is
// touched; no cached native handle or sampled GPU-content authority is used.
inline bool hydrateNativeVertexStore(GLuint store,const void* data,std::size_t bytes) {
    if(!store||!data||!bytes||!wglGetCurrentContext()||!glGetBufferParameteri64v||!glIsBuffer(store)) return false;
    GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
    glBindBuffer(GL_ARRAY_BUFFER,store);GLint64 allocated=0;
    glGetBufferParameteri64v(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&allocated);
    const bool fits=allocated>=0&&static_cast<std::uint64_t>(allocated)>=bytes;
    if(fits) glBufferSubData(GL_ARRAY_BUFFER,0,static_cast<GLsizeiptr>(bytes),data);
    glBindBuffer(GL_ARRAY_BUFFER,static_cast<GLuint>(previous));return fits;
}
