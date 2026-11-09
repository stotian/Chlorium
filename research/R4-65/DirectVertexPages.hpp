#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include "../../src/renderer/ContextEpoch.hpp"
#include <cstring>
#include <span>
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
#include "../R4-66/PageBudget.hpp"
#endif

namespace chlorium::research::directvertices {
// Native draws read coherent pages directly; no intermediate GPU copy.
class DirectVertexPages {
    using Storage=void(APIENTRY*)(GLenum,GLsizeiptr,const void*,GLbitfield);
    struct Page {GLuint id=0;unsigned char* mapped=nullptr;GLsync fence=nullptr;std::size_t bytes=0;};
    struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
    struct Binding {GLint old=0;Binding(){glGetIntegerv(GL_COPY_READ_BUFFER,&old);}~Binding(){glBindBuffer(GL_COPY_READ_BUFFER,old);}};
    std::shared_ptr<gl::ContextEpoch<>> context_;
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
    std::array<Page,adaptivePageCount> pages_{};
#else
    std::array<Page,16> pages_{};
#endif
    std::size_t cursor_=0,active_=pages_.size();
    Storage storage_=nullptr;
    bool failed_=false;
public:
    static constexpr std::size_t pageBytes=16384u*96u,maxBytes=pageBytes*16;
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
    static constexpr std::size_t pageCount=adaptivePageCount;
#else
    static constexpr std::size_t pageCount=16;
#endif
    struct Metrics {std::uint64_t uploads=0,bytes=0,busy=0,fences=0,allocated=0;} metrics;
#ifdef CHLORIUM_PRIVATE_DIRECT_PAGE_CHECKS
    bool denyReuse=false;
#endif
    DirectVertexPages()=default;
    DirectVertexPages(const DirectVertexPages&)=delete;DirectVertexPages& operator=(const DirectVertexPages&)=delete;
    ~DirectVertexPages(){
        HostState preserve;
        if(!context_ || !context_->isCurrent())return;
        for(auto& page:pages_){if(page.fence)glDeleteSync(page.fence);if(page.id)glDeleteBuffers(1,&page.id);}
    }
    GLuint acquire(std::span<const std::byte> source){
        HostState preserve;
        if(failed_ || active_!=pages_.size() || source.empty() || source.size()>pageBytes || !wglGetCurrentContext())return 0;
        if(!context_){
            GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
            if(major<4 || (major==4 && minor<4) || !glMapBufferRange || !glFenceSync || !glClientWaitSync || !glDeleteSync)return 0;
            storage_=gl::currentProc<Storage>("glBufferStorage");if(!storage_)return 0;
            context_=gl::acquireContextEpoch();if(!context_)return 0;
        }
        if(!context_->isCurrent())return 0;
        for(std::size_t offset=0;offset<pages_.size();++offset){
            const auto index=(cursor_+offset)%pages_.size();auto& page=pages_[index];
            if(page.fence){
#ifdef CHLORIUM_PRIVATE_DIRECT_PAGE_CHECKS
                if(denyReuse)continue;
#endif
                const auto ready=glClientWaitSync(page.fence,0,0);
                if(ready==GL_WAIT_FAILED){failed_=true;return 0;}
                if(ready!=GL_ALREADY_SIGNALED && ready!=GL_CONDITION_SATISFIED)continue;
                glDeleteSync(page.fence);page.fence=nullptr;
            }
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
            const auto needed=pageStorage(source.size());
            if(page.bytes<needed && !canReplacePage(metrics.allocated,page.bytes,needed))continue;
            if(page.bytes<needed){
                // Only the pinned native adapter owns these ids; its VAO pointers are restored before finish.
                Binding saved;
                if(page.id){glDeleteBuffers(1,&page.id);metrics.allocated-=page.bytes;page={};}
#else
            constexpr auto needed=pageBytes;
            if(!page.id){
                Binding saved;glGenBuffers(1,&page.id);if(!page.id){failed_=true;return 0;}
#endif
#ifdef CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66
                glGenBuffers(1,&page.id);if(!page.id){failed_=true;return 0;}
#endif
                glBindBuffer(GL_COPY_READ_BUFFER,page.id);
                constexpr GLbitfield flags=GL_MAP_WRITE_BIT|0x0040|0x0080;
                storage_(GL_COPY_READ_BUFFER,needed,nullptr,flags);
                GLint64 allocated=0;glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&allocated);
                if(allocated!=static_cast<GLint64>(needed)){failed_=true;return 0;}
                page.mapped=static_cast<unsigned char*>(glMapBufferRange(GL_COPY_READ_BUFFER,0,needed,flags));
                if(!page.mapped){failed_=true;return 0;}page.bytes=needed;metrics.allocated+=needed;
            }
            std::memcpy(page.mapped,source.data(),source.size());
            // Coherent host writes precede the subsequent native vertex fetch.
            _mm_sfence();active_=index;cursor_=(index+1)%pages_.size();
            ++metrics.uploads;metrics.bytes+=source.size();return page.id;
        }
        ++metrics.busy;return 0;
    }
    bool finish(){
        HostState preserve;
        if(active_==pages_.size())return false;
        auto& page=pages_[active_];active_=pages_.size();
        if(!context_ || !context_->isCurrent()){failed_=true;return false;}
        page.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);++metrics.fences;
        if(!page.fence)failed_=true;return !failed_;
    }
};
}
