#pragma once
#include "../../src/renderer/ContextEpoch.hpp"
#include <stdexcept>
#include <emmintrin.h>
namespace chlorium::research::canonicaldraw {
inline constexpr GLenum storage=0x90d2;
inline void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
class ComputeLease {
    struct Binding {GLint name=0;GLint64 offset=0,size=0;};
    std::shared_ptr<gl::ContextEpoch<>> context_;std::array<Binding,6> bindings_{};GLint program_=0,generic_=0,array_=0;
public:
    explicit ComputeLease(std::shared_ptr<gl::ContextEpoch<>> context):context_(std::move(context)){
        check(context_ && context_->isCurrent(),"current compute lease context");GLint count=0;glGetIntegerv(0x90dd,&count);check(count>=6,"six compute binding slots");
        GLboolean active=GL_FALSE,paused=GL_FALSE;glGetBooleanv(0x8e24,&active);glGetBooleanv(0x8e23,&paused);check(!active || paused,"unpaused transform feedback retains native ownership");
        glGetIntegerv(GL_CURRENT_PROGRAM,&program_);glGetIntegerv(0x90d3,&generic_);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array_);
        for(unsigned i=0;i<bindings_.size();++i){auto& b=bindings_[i];glGetIntegeri_v(0x90d3,i,&b.name);if(b.name){glGetInteger64i_v(0x90d4,i,&b.offset);glGetInteger64i_v(0x90d5,i,&b.size);}}
    }
    ~ComputeLease(){HostState preserve;if(!context_ || !context_->isCurrent())return;
        for(unsigned i=0;i<bindings_.size();++i){const auto& b=bindings_[i];if(b.name && b.size>0)glBindBufferRange(storage,i,b.name,b.offset,b.size);else glBindBufferBase(storage,i,b.name);}
        glBindBuffer(storage,generic_);glBindBuffer(GL_ARRAY_BUFFER,array_);glUseProgram(program_);
    }
    ComputeLease(const ComputeLease&)=delete;ComputeLease& operator=(const ComputeLease&)=delete;
};
}
