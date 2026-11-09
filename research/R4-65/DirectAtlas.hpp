#pragma once
#include "DirectVertexPages.hpp"
#include "../../src/renderer/NativeVertexLease.hpp"

namespace chlorium::research::directvertices {
// The caller pins native atlas/getter/bind callbacks and excludes foreign writers.
class DirectAtlas {
    struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
    DirectVertexPages pages_;
    static void restoreNativeAttributes(GLuint canonical){
        glBindBuffer(GL_ARRAY_BUFFER,canonical);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);
        glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }
public:
    std::uint64_t fenceFailures=0;
    const auto& metrics()const{return pages_.metrics;}
    template<class Forward> bool draw(GLuint& canonical,bool& dirty,const void* quads,unsigned count,unsigned first,unsigned total,unsigned capacity,Forward&& native){
        GLuint owned=0;
        {
            HostState preserve;
            if(!wglGetCurrentContext() || !dirty || !quads || !count || first>total || count>total-first || total>capacity || first>16384 || count>16384-first ||
               !canonical || !glIsBuffer(canonical))return false;
            GLint old=0,mapped=0;GLint64 bytes=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);glBindBuffer(GL_COPY_READ_BUFFER,canonical);
            glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&bytes);glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_MAPPED,&mapped);
            glBindBuffer(GL_COPY_READ_BUFFER,old);
            if(mapped || bytes<0 || std::uint64_t(bytes)<std::size_t(first+count)*96)return false;
            owned=pages_.acquire({static_cast<const std::byte*>(quads),std::size_t(first+count)*96});if(!owned)return false;
        }
        const auto original=canonical;
        snapshot::NativeVertexLease lease(canonical,dirty,owned);
        struct Finish {
            DirectAtlas* adapter;GLuint canonical;
            ~Finish(){HostState preserve;restoreNativeAttributes(canonical);if(!adapter->pages_.finish())++adapter->fenceFailures;}
        }finish{this,original};
        native();return true;
    }
};
}
