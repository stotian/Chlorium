#pragma once
#include "CanonicalWrite.hpp"
#include "FrameProgram.hpp"
namespace chlorium::research::canonicaldraw {
class RootRuntime {
    std::shared_ptr<gl::ContextEpoch<>> context_;GLuint program_=0,buffers_[5]{};GLint count_=-1,prefix_=-1;using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);Dispatch dispatch_=nullptr;std::vector<State> initial_;
public:
    RootRuntime(const std::filesystem::path& shader,std::span<const State> initial):initial_(initial.begin(),initial.end()){
        HostState preserve;check(!initial.empty() && initial.size()<=128,"bounded live root seed");context_=gl::acquireContextEpoch();check(bool(context_),"live root context");ComputeLease lease(context_);
        try{program_=compile(shader);count_=glGetUniformLocation(program_,"objectCount");prefix_=glGetUniformLocation(program_,"prefixCount");dispatch_=gl::currentProc<Dispatch>("glDispatchCompute");check(count_>=0 && prefix_>=0 && dispatch_,"live root shader interface");glGenBuffers(5,buffers_);for(auto id:buffers_)check(id!=0,"live root buffer names");
            for(unsigned lane:{0u,2u}){glBindBuffer(storage,buffers_[lane]);glBufferData(storage,initial.size_bytes(),lane?nullptr:initial.data(),GL_DYNAMIC_DRAW);}}
        catch(...){if(context_->isCurrent()){glDeleteBuffers(5,buffers_);if(program_)glDeleteProgram(program_);}throw;}
    }
    ~RootRuntime(){HostState preserve;if(context_ && context_->isCurrent()){glDeleteBuffers(5,buffers_);if(program_)glDeleteProgram(program_);}}
    GLuint buffer()const{return buffers_[0];}
    std::vector<State> advance(const liveprogram::Frame& frame){
        HostState preserve;ComputeLease lease(context_);check(frame.ranges.size()==initial_.size(),"live root class count");unsigned steps=0;for(auto r:frame.ranges){check(r[0]<=frame.indices.size() && r[1]<=frame.indices.size()-r[0] && r[1]<=8192,"live root whole range");steps=std::max(steps,r[1]);for(unsigned k=1;k<r[1];++k)check(frame.indices[r[0]+k]>=frame.indices[r[0]+k-1],"live root chunk order");}
        auto upload=[&](unsigned lane,const auto values){glBindBuffer(storage,buffers_[lane]);glBufferData(storage,values.size_bytes(),values.data(),GL_DYNAMIC_DRAW);};
        upload(1,std::span<const mixedprogram::Command>(frame.commands));upload(4,std::span<const unsigned>(frame.indices));std::vector<mixedprogram::Range> ranges(frame.ranges.size());
        for(unsigned start=0;start<std::max(1u,steps);start+=64){for(unsigned i=0;i<ranges.size();++i){const auto r=frame.ranges[i];const auto first=std::min(start,r[1]);ranges[i]={r[0]+first,std::min(64u,r[1]-first)};}
            check(mixedprogram::fixtureInput(initial_,frame.commands,ranges,frame.indices,_mm_getcsr()),"live root bounded shader admission");upload(3,std::span<const mixedprogram::Range>(ranges));glUseProgram(program_);for(unsigned lane=0;lane<5;++lane)glBindBufferBase(storage,lane,buffers_[lane]);glUniform1ui(count_,unsigned(initial_.size()));glUniform1ui(prefix_,unsigned(frame.commands.size()));dispatch_((unsigned(initial_.size())+63)/64,1,1);glMemoryBarrier(0x2000|GL_BUFFER_UPDATE_BARRIER_BIT);std::swap(buffers_[0],buffers_[2]);}
        std::vector<State> actual(initial_.size());std::memset(actual.data(),0xa5,actual.size()*sizeof(State));glBindBuffer(storage,buffers_[0]);glGetBufferSubData(storage,0,actual.size()*sizeof(State),actual.data());return actual;
    }
};
}
