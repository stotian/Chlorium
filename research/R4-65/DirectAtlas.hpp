#pragma once
#include "DirectVertexPages.hpp"
#include "../../src/renderer/NativeVertexLease.hpp"
#include "../R4-59/NativeUploadDeferral.hpp"

namespace chlorium::research::directvertices {
class PartialDirtyRecovery {
    bool* dirty_=nullptr;
public:
    PartialDirtyRecovery(bool mirrorMayBeStale,bool& dirty,unsigned first,unsigned count,unsigned total){
        if(mirrorMayBeStale && continuous::retainPartialDirty(dirty,first,count,total))dirty_=&dirty;
    }
    ~PartialDirtyRecovery(){if(dirty_)*dirty_=true;}
    PartialDirtyRecovery(const PartialDirtyRecovery&)=delete;PartialDirtyRecovery& operator=(const PartialDirtyRecovery&)=delete;
};
// The caller pins native atlas/getter/bind callbacks and excludes foreign writers.
class DirectAtlas {
    struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
    DirectVertexPages pages_;
    void finishPage(){if(!pages_.finish())++fenceFailures;}
    static void restoreNativeAttributes(GLuint canonical){
        glBindBuffer(GL_ARRAY_BUFFER,canonical);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);
        glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }
public:
    std::uint64_t fenceFailures=0,canonicalUploads=0,canonicalBytes=0;
    const auto& metrics()const{return pages_.metrics;}
    template<class Forward> bool draw(GLuint& canonical,bool& dirty,const void* quads,unsigned count,unsigned first,unsigned total,unsigned capacity,Forward&& native){
        GLuint owned=0;
        {
            HostState preserve;
            // A full dirty upload makes every logical quad CPU-authoritative; native handles partial stores.
            if(!wglGetCurrentContext() || !dirty || !quads || !count || first || count!=total || total>capacity || count>16384 ||
               !canonical)return false;
            // Pin the page's context before dispatching native GLEW buffer functions.
            owned=pages_.acquire({static_cast<const std::byte*>(quads),std::size_t(first+count)*96});if(!owned)return false;
            if(!glIsBuffer(canonical)){finishPage();return false;}
            GLint old=0,mapped=0,immutable=0;GLint64 bytes=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);glBindBuffer(GL_COPY_READ_BUFFER,canonical);
            glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&bytes);glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_MAPPED,&mapped);
            glGetBufferParameteriv(GL_COPY_READ_BUFFER,0x821f,&immutable);
            glBindBuffer(GL_COPY_READ_BUFFER,old);
            if(mapped || immutable || bytes<0 || std::uint64_t(bytes)<std::size_t(first+count)*96){finishPage();return false;}
            // Native consumers may read canonical storage without calling the atlas again.
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old);glBindBuffer(GL_ARRAY_BUFFER,canonical);
            glBufferSubData(GL_ARRAY_BUFFER,0,std::size_t(count)*96,quads);glBindBuffer(GL_ARRAY_BUFFER,old);
            ++canonicalUploads;canonicalBytes+=std::size_t(count)*96;
        }
        const auto original=canonical;
        {
        snapshot::NativeVertexLease lease(canonical,dirty,owned);
        struct Finish {
            DirectAtlas* adapter;GLuint canonical;
            ~Finish(){HostState preserve;restoreNativeAttributes(canonical);adapter->finishPage();}
        }finish{this,original};
        native();
        }
        dirty=false;return true;
    }
};
}
