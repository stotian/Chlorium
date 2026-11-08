#pragma once
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace chlorium::research::continuous {
// Native dirty remains true until an unsuppressed native upload covers its data.
inline bool retainPartialDirty(bool dirty,unsigned first,unsigned count,unsigned total)noexcept{
    return dirty && (first || count!=total);
}
class NativeUploadDeferral {
public:
    struct Stats {std::uint64_t version=59,skippedCalls=0,skippedBytes=0,declined=0;};
    inline static Stats stats{};
    class Scope {
        bool* dirty_=nullptr;
    public:
        // Caller qualifies the native body/driver, CPU range and zero-render predicate.
        Scope(GLuint buffer,bool& dirty,std::size_t bytes,bool allowed){
            if(!dirty)return;
            if(!allowed || !bytes || bytes>16384u*96u || !buffer || !wglGetCurrentContext() || !glIsBuffer(buffer)){
                ++stats.declined;return;
            }
            GLint old=0,size=0,mapped=0;glGetIntegerv(GL_COPY_READ_BUFFER,&old);
            glBindBuffer(GL_COPY_READ_BUFFER,buffer);
            glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);
            glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_MAPPED,&mapped);
            glBindBuffer(GL_COPY_READ_BUFFER,old);
            if(size<0 || bytes>std::size_t(size) || mapped){++stats.declined;return;}
            dirty_=&dirty;dirty=false;++stats.skippedCalls;stats.skippedBytes+=bytes;
        }
        Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
        ~Scope(){if(dirty_)*dirty_=true;}
        bool skipped()const noexcept{return dirty_!=nullptr;}
    };
};
}
