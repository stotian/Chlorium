#pragma once
#include "renderer/ContextEpoch.hpp"
#include <algorithm>
#include <cstring>

namespace chlorium::research {
// A transfer source, never native vertex storage. Canonical VBO identity and
// every byte outside the exact dirty CPU range remain unchanged.
class PersistentTransfer {
    using Storage=void(APIENTRY*)(GLenum,GLsizeiptr,const void*,GLbitfield);
    struct Page {std::size_t used=0;GLsync fence=nullptr;};
    struct Copies {
        GLint read=0,write=0;
        Copies(){glGetIntegerv(GL_COPY_READ_BUFFER,&read);glGetIntegerv(GL_COPY_WRITE_BUFFER,&write);}
        ~Copies(){glBindBuffer(GL_COPY_READ_BUFFER,read);glBindBuffer(GL_COPY_WRITE_BUFFER,write);}
    };
    std::shared_ptr<gl::ContextEpoch<>> context_;
    GLuint id_=0;
    unsigned char* mapped_=nullptr;
    std::array<Page,3> pages_{};
    std::size_t pageBytes_=0,current_=0;
    bool failed_=false;
public:
    struct Work {std::uint64_t uploads=0,bytes=0,fences=0,busy=0;} work;
#ifdef CHLORIUM_PRIVATE_RING_CHECKS
    bool denyReuse=false;
#endif
    PersistentTransfer(const PersistentTransfer&)=delete;
    PersistentTransfer& operator=(const PersistentTransfer&)=delete;
    PersistentTransfer()=default;
    bool current() const {return context_&&context_->isCurrent();}
    ~PersistentTransfer() {
        if(!context_||!context_->isCurrent()) return;
        for(auto& p:pages_) if(p.fence) glDeleteSync(p.fence);
        // Deletion unmaps; GL retains storage needed by queued copy commands.
        if(id_) glDeleteBuffers(1,&id_);
    }
    static std::unique_ptr<PersistentTransfer> create(std::size_t pageBytes=1024u*1024u) {
        if(!pageBytes||pageBytes>8u*1024u*1024u||!wglGetCurrentContext()||
            !glMapBufferRange||!glCopyBufferSubData||!glGetBufferParameteri64v||
            !glFenceSync||!glClientWaitSync||!glDeleteSync) return {};
        GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        if(major<4||(major==4&&minor<4)) return {};
        const auto storage=gl::currentProc<Storage>("glBufferStorage");
        if(!storage) return {};
        auto result=std::make_unique<PersistentTransfer>();
        result->context_=gl::acquireContextEpoch();if(!result->context_) return {};
        result->pageBytes_=pageBytes;
        Copies saved;glGenBuffers(1,&result->id_);if(!result->id_) return {};
        glBindBuffer(GL_COPY_READ_BUFFER,result->id_);
        // Core 4.4 flags; bundled GLEW predates buffer_storage declarations.
        constexpr GLbitfield flags=GL_MAP_WRITE_BIT|0x0040|0x0080;
        storage(GL_COPY_READ_BUFFER,static_cast<GLsizeiptr>(pageBytes*3),nullptr,flags);
        GLint64 allocated=0;glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&allocated);
        if(allocated!=static_cast<GLint64>(pageBytes*3)) return {};
        result->mapped_=static_cast<unsigned char*>(glMapBufferRange(GL_COPY_READ_BUFFER,0,
            static_cast<GLsizeiptr>(pageBytes*3),flags));
        if(!result->mapped_) return {};
        return result;
    }
    bool seal() {
        if(failed_||!context_||!context_->isCurrent()) return false;
        auto& p=pages_[current_];
        if(p.used&&!p.fence) {
            p.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);++work.fences;
            if(!p.fence) {failed_=true;return false;}
        }
        return true;
    }
    bool upload(GLuint target,std::size_t offset,std::size_t count,const void* cpu) {
        if(failed_||!mapped_||!target||target==id_||!cpu||!count||count>pageBytes_||
            !context_->isCurrent()||!glIsBuffer(target)) return false;
        Copies saved;glBindBuffer(GL_COPY_WRITE_BUFFER,target);
        GLint64 bytes=0;GLint mapped=0;
        glGetBufferParameteri64v(GL_COPY_WRITE_BUFFER,GL_BUFFER_SIZE,&bytes);
        glGetBufferParameteriv(GL_COPY_WRITE_BUFFER,GL_BUFFER_MAPPED,&mapped);
        if(bytes<0||offset>static_cast<std::uint64_t>(bytes)||
            count>static_cast<std::uint64_t>(bytes)-offset||mapped) return false;
        GLboolean feedback=GL_FALSE;glGetBooleanv(0x8E24,&feedback);
        if(feedback) return false;
        auto* p=&pages_[current_];
        if(p->fence||count>pageBytes_-p->used) {
            if(!seal()) return false;
            const auto next=(current_+1)%pages_.size();auto& candidate=pages_[next];
            if(candidate.fence) {
#ifdef CHLORIUM_PRIVATE_RING_CHECKS
#ifndef CHLORIUM_PRIVATE_RING_REUSE_MUTANT
                if(denyReuse) {++work.busy;return false;}
#endif
#endif
                const auto ready=glClientWaitSync(candidate.fence,0,0);
                if(ready!=GL_ALREADY_SIGNALED&&ready!=GL_CONDITION_SATISFIED) {
                    if(ready==GL_WAIT_FAILED) failed_=true;
                    ++work.busy;return false;
                }
                glDeleteSync(candidate.fence);candidate.fence=nullptr;
            }
            current_=next;candidate.used=0;p=&candidate;
        }
        const auto source=current_*pageBytes_+p->used;
        std::memcpy(mapped_+source,cpu,count);
        // Coherent persistent CPU writes precede this server read. The page
        // cannot be overwritten until the fence after its final copy signals.
        glBindBuffer(GL_COPY_READ_BUFFER,id_);
        glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,static_cast<GLintptr>(source),
            static_cast<GLintptr>(offset),static_cast<GLsizeiptr>(count));
        p->used+=count;++work.uploads;work.bytes+=count;
        return true;
    }
};
}
