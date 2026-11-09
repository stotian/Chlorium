#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include "GroupProgram.hpp"
#include "../R4-62/RootPacket.hpp"
#include "../R4-47/NativeShaderSources.hpp"
#include "../../src/renderer/ContextEpoch.hpp"
#include "../../src/renderer/GLState.hpp"
#include <cmath>
#include <limits>
#include <type_traits>

namespace chlorium::research::groupprogram {
// Owned compute output only; the native adapter must retain CPU recovery and reader ownership.
class GroupQuadStream {
    struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
    struct Bindings {
        GLint program=0,vao=0,array=0,storage=0;
        std::array<GLStateSnapshot::IndexedBufferBinding,7> indexed{};
        Bindings(){
            glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);glGetIntegerv(GLStateSnapshot::storageBinding,&storage);
            for(unsigned i=0;i<7;++i)indexed[i]=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,i);
        }
        ~Bindings(){
            glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,array);
            for(unsigned i=0;i<7;++i)GLStateSnapshot::restoreIndexed(GLStateSnapshot::storageTarget,i,indexed[i]);
            glBindBuffer(GLStateSnapshot::storageTarget,storage);
        }
    };
    std::shared_ptr<gl::ContextEpoch<>> epoch_;
    GLuint program_=0,buffers_[7]{},indices_=0,vao_=0;
    using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);
    Dispatch dispatch_=nullptr;
    GLint countUniform_=-1,prefixUniform_=-1;
    unsigned count_=0,ranges_=0;
    bool usable_=false,ready_=false;
    double poseBound_=0;
    static GLuint link(std::string_view source){
        const auto shader=glCreateShader(0x91b9);if(!shader)return 0;
        const char* text=source.data();const auto length=GLint(source.size());glShaderSource(shader,1,&text,&length);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(shader);return 0;}
        const auto program=glCreateProgram();if(program){glAttachShader(program,shader);glLinkProgram(program);glGetProgramiv(program,GL_LINK_STATUS,&ok);}else ok=0;
        glDeleteShader(shader);if(!ok){if(program)glDeleteProgram(program);return 0;}return program;
    }
    static double advanceBound(double previous,std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices){
        double maximum=0;
        for(const auto& range:ranges){double x=0,y=0;
            for(unsigned i=0;i<range[1];++i){const auto& command=commands[indices[range[0]+i]];
                if(command.delta[0]!=0)x=std::nextafter(x+std::abs(command.delta[0]),std::numeric_limits<double>::infinity());
                if(command.delta[1]!=0)y=std::nextafter(y+std::abs(command.delta[1]),std::numeric_limits<double>::infinity());}
            maximum=std::max(maximum,std::max(x,y));
        }
        return maximum?std::nextafter(previous+maximum,std::numeric_limits<double>::infinity()):previous;
    }
public:
    struct Metrics {std::uint64_t stateSeedBytes=0,programBytes=0,lastProgramBytes=0,dispatches=0;} metrics;
    GroupQuadStream()=default;
    GroupQuadStream(const GroupQuadStream&)=delete;GroupQuadStream& operator=(const GroupQuadStream&)=delete;
    ~GroupQuadStream(){close();}
    void close()noexcept{
        HostState preserve;
        if(epoch_ && epoch_->isCurrent()){
            glDeleteBuffers(7,buffers_);if(indices_)glDeleteBuffers(1,&indices_);
            if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);
        }
        std::fill(std::begin(buffers_),std::end(buffers_),0);indices_=vao_=program_=0;
        count_=ranges_=0;usable_=ready_=false;poseBound_=0;dispatch_=nullptr;epoch_.reset();
    }
    bool initialize(std::span<const State> states,std::span<const resident::Geometry> geometry,std::span<const resident::Parameters> parameters,
                    std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices,std::string_view source){
        HostState preserve;
        if(epoch_ || !wglGetCurrentContext() || !seedEligible(states,commands,ranges,indices,preserve.fp) ||
           !resident::rootPacketEligible(states,geometry,parameters) || source.size()!=3017 || resident::sourceHash(source)!=0xa52c8752a3e2b74aull)return false;
        // shortcut: owned research context supplies a clean GL error boundary; qualify native errors before live use.
        if(glGetError()!=GL_NO_ERROR)return false;
        dispatch_=gl::currentProc<Dispatch>("glDispatchCompute");if(!dispatch_)return false;
        Bindings saved;
        struct Rollback {GroupQuadStream* owner;bool armed=true;~Rollback(){if(armed)owner->close();}}rollback{this};
        epoch_=gl::ContextEpoch<>::capture();if(!epoch_)return false;
        program_=link(source);if(!program_)return false;
        countUniform_=glGetUniformLocation(program_,"objectCount");prefixUniform_=glGetUniformLocation(program_,"prefixCount");
        if(countUniform_<0 || prefixUniform_<0)return false;
        glGenBuffers(7,buffers_);glGenBuffers(1,&indices_);glGenVertexArrays(1,&vao_);
        if(!indices_ || !vao_ || std::find(std::begin(buffers_),std::end(buffers_),GLuint{0})!=std::end(buffers_))return false;
        auto allocate=[&](unsigned lane,std::size_t bytes,const void* data){glBindBuffer(GLStateSnapshot::storageTarget,buffers_[lane]);glBufferData(GLStateSnapshot::storageTarget,bytes,data,GL_DYNAMIC_DRAW);};
        allocate(0,states.size_bytes(),states.data());allocate(1,maxCommands*sizeof(Command),nullptr);
        allocate(2,geometry.size_bytes(),geometry.data());allocate(3,ranges.size_bytes(),nullptr);
        allocate(4,ranges.size()*maxSteps*sizeof(unsigned),nullptr);allocate(5,parameters.size_bytes(),parameters.data());
        allocate(6,states.size()*sizeof(snapshot::NativeQuadState),nullptr);
        std::vector<unsigned> order(states.size()*6);constexpr unsigned topology[]{0,1,2,3,2,1};
        for(std::size_t i=0;i<order.size();++i)order[i]=unsigned(i/6*4+topology[i%6]);
        glBindVertexArray(vao_);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,order.size()*sizeof(unsigned),order.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,buffers_[6]);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));for(unsigned i=0;i<3;++i)glEnableVertexAttribArray(i);
        if(glGetError()!=GL_NO_ERROR)return false;
        for(const auto& state:states)poseBound_=std::max(poseBound_,std::max(std::abs(state.position[0]),std::abs(state.position[1])));
        count_=unsigned(states.size());ranges_=unsigned(ranges.size());usable_=true;metrics={states.size_bytes(),0,0,0};rollback.armed=false;return true;
    }
    bool produce(std::span<const Command> commands,std::span<const Range> ranges,std::span<const unsigned> indices){
        HostState preserve;ready_=false;
        if(!usable_ || !epoch_ || !epoch_->isCurrent() || ranges.size()!=ranges_ ||
           !programEligible(commands,ranges,indices,preserve.fp) || indices.size()>ranges_*maxSteps)return false;
        const auto nextBound=advanceBound(poseBound_,commands,ranges,indices);if(nextBound>0x1p32)return false;
        Bindings saved;
        glMemoryBarrier(0x2000);glUseProgram(program_);
        for(unsigned lane=0;lane<7;++lane)glBindBufferBase(GLStateSnapshot::storageTarget,lane,buffers_[lane]);
        auto upload=[&](unsigned lane,const auto data){if(data.empty())return;glBindBuffer(GLStateSnapshot::storageTarget,buffers_[lane]);glBufferSubData(GLStateSnapshot::storageTarget,0,data.size_bytes(),data.data());};
        upload(1,commands);upload(3,ranges);upload(4,indices);
        glUniform1ui(countUniform_,count_);glUniform1ui(prefixUniform_,unsigned(commands.size()));
        if(glGetError()!=GL_NO_ERROR){usable_=false;return false;}
        dispatch_((count_+63)/64,1,1);glMemoryBarrier(0x2001);
        if(glGetError()!=GL_NO_ERROR){usable_=false;return false;}
        poseBound_=nextBound;ready_=true;metrics.lastProgramBytes=commands.size_bytes()+ranges.size_bytes()+indices.size_bytes();
        metrics.programBytes+=metrics.lastProgramBytes;++metrics.dispatches;return true;
    }
    GLuint vao()const noexcept{return ready_ && epoch_ && epoch_->isCurrent()?vao_:0;}
    template<class T>bool copyOwned(unsigned lane,std::span<T> destination){
        static_assert(std::is_same_v<T,State> || std::is_same_v<T,snapshot::NativeQuadState>);
        HostState preserve;
        if(!ready_ || !epoch_ || !epoch_->isCurrent() || (lane!=0 && lane!=6) || destination.size()!=count_ ||
           sizeof(T)!=(lane==0?sizeof(State):sizeof(snapshot::NativeQuadState)))return false;
        Bindings saved;glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);glBindBuffer(GLStateSnapshot::storageTarget,buffers_[lane]);
        glGetBufferSubData(GLStateSnapshot::storageTarget,0,destination.size_bytes(),destination.data());return glGetError()==GL_NO_ERROR;
    }
};
}
