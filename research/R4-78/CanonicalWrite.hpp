#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include "ComputeLease.hpp"
#include "../R4-75/Admission.hpp"
#include "../R4-77/ParameterClasses.hpp"
#include <fstream>
#include <filesystem>
#include <set>
namespace chlorium::research::canonicaldraw {
using State=mixedprogram::State;using Row=rootgeometry::Row;using Quad=snapshot::NativeQuadState;
inline GLuint compile(const std::filesystem::path& file){
    std::ifstream input(file);check(bool(input),"read canonical compute shader");std::string text((std::istreambuf_iterator<char>(input)),{});check(!text.empty() && text.size()<65536,"bounded canonical compute shader");
    const GLuint shader=glCreateShader(0x91b9);check(shader!=0,"canonical shader allocation");const char* source=text.c_str();glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){glDeleteShader(shader);throw std::runtime_error("canonical shader compile");}const auto program=glCreateProgram();if(!program){glDeleteShader(shader);throw std::runtime_error("canonical program allocation");}
    glAttachShader(program,shader);glLinkProgram(program);glDeleteShader(shader);glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(program);throw std::runtime_error("canonical program link");}return program;
}
class CanonicalWrite {
    std::shared_ptr<gl::ContextEpoch<>> context_;GLuint program_=0,buffers_[3]{};GLint count_=-1;using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);Dispatch dispatch_=nullptr;
    parameterclasses::Cache classes_;
public:
    std::uint64_t committed=0,quads=0,verifiedBytes=0,failures=0,declines=0;
#ifdef CHLORIUM_PRIVATE_CANONICAL_DRAW_CHECKS
    bool denyDispatch=false;
#endif
    explicit CanonicalWrite(const std::filesystem::path& shader){
        HostState preserve;check(wglGetCurrentContext()!=nullptr,"canonical context available");GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);check(major>4 || (major==4 && minor>=4),"canonical compute requires GL4.4");
        context_=gl::acquireContextEpoch();check(bool(context_),"canonical context epoch");ComputeLease lease(context_);dispatch_=gl::currentProc<Dispatch>("glDispatchCompute");check(dispatch_!=nullptr,"canonical dispatch available");
        try{program_=compile(shader);count_=glGetUniformLocation(program_,"objectCount");check(count_>=0,"canonical shader interface");glGenBuffers(3,buffers_);check(buffers_[0] && buffers_[1] && buffers_[2],"canonical input buffers");}
        catch(...){if(context_->isCurrent()){glDeleteBuffers(3,buffers_);if(program_)glDeleteProgram(program_);}throw;}
    }
    ~CanonicalWrite(){HostState preserve;if(context_ && context_->isCurrent()){glDeleteBuffers(3,buffers_);if(program_)glDeleteProgram(program_);}}
    // Diagnostic only: poison/verify before the caller forwards the native draw exactly once.
    // On false, the caller must leave dirty=true and execute the complete native upload/draw.
    bool prepare(GLuint canonical,GLuint residentState,std::span<const Quad> native,std::span<const State> states,std::span<const Row* const> rows,std::span<const unsigned> poseSlots)noexcept{
        HostState preserve;try{
            if(!context_ || !context_->isCurrent() || !canonical || !residentState || native.empty() || native.size()>16384 || states.empty() || states.size()>128 || rows.empty() || rows.size()>128 || rows.size()!=poseSlots.size()){++declines;return false;}
            ComputeLease lease(context_);if(!glIsBuffer(canonical) || !glIsBuffer(residentState)){++declines;return false;}
            glBindBuffer(GL_ARRAY_BUFFER,canonical);GLint mapped=0,immutable=0;GLint64 bytes=0;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_MAPPED,&mapped);glGetBufferParameteriv(GL_ARRAY_BUFFER,0x821f,&immutable);glGetBufferParameteri64v(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
            if(mapped || immutable || bytes<0 || std::uint64_t(bytes)<native.size_bytes()){++declines;return false;}
            GLint64 rootBytes=0;GLint rootMapped=0;glBindBuffer(storage,residentState);glGetBufferParameteri64v(storage,GL_BUFFER_SIZE,&rootBytes);glGetBufferParameteriv(storage,GL_BUFFER_MAPPED,&rootMapped);
            if(rootMapped || rootBytes<0 || std::uint64_t(rootBytes)<states.size_bytes()){++declines;return false;}
            std::vector<resident::Geometry> geometry;std::vector<resident::Parameters> parameters;std::set<unsigned> targets;auto poison=std::vector<Quad>(native.begin(),native.end());
            for(unsigned i=0;i<rows.size();++i){check(rows[i]!=nullptr,"canonical source row");const auto& row=*rows[i];const auto target=row.source.atlasIndex;resident::Geometry g;resident::Parameters p;
                const auto admission=rootgeometry::encodeNativeRow(row,states,poseSlots[i],g,p);if(!admission.accepted)continue;
                if(target>=native.size() || std::memcmp(&row.quad,&native[target],96) || !targets.insert(target).second){++declines;return false;}g.control[3]=target;geometry.push_back(g);parameters.push_back(p);
                auto word=std::bit_cast<unsigned>(poison[target].corners[0].x)^1u;std::memcpy(&poison[target].corners[0].x,&word,4);
            }
            if(geometry.empty()){++declines;return false;}std::vector<unsigned char> admitted(geometry.size(),1);const auto& classes=classes_.update(parameters,admitted);
            auto upload=[&](unsigned buffer,unsigned binding,const auto values){glBindBuffer(storage,buffers_[buffer]);glBufferData(storage,values.size_bytes(),values.data(),GL_DYNAMIC_DRAW);GLint64 allocated=0;glGetBufferParameteri64v(storage,GL_BUFFER_SIZE,&allocated);check(allocated==GLint64(values.size_bytes()),"canonical input storage");glBindBufferBase(storage,binding,buffers_[buffer]);};
            upload(0,1,std::span<const resident::Geometry>(geometry));upload(1,2,std::span<const resident::Parameters>(classes.values));upload(2,5,std::span<const unsigned>(classes.slots));
            glBindBuffer(GL_ARRAY_BUFFER,canonical);glBufferSubData(GL_ARRAY_BUFFER,0,poison.size()*96,poison.data());
            std::vector<Quad> poisoned(poison.size());std::memset(poisoned.data(),0xa5,poisoned.size()*96);glGetBufferSubData(GL_ARRAY_BUFFER,0,poisoned.size()*96,poisoned.data());
            if(std::memcmp(poisoned.data(),poison.data(),poison.size()*96)){++failures;return false;}
            glUseProgram(program_);glBindBufferBase(storage,0,residentState);glBindBufferBase(storage,3,canonical);glUniform1ui(count_,unsigned(geometry.size()));glMemoryBarrier(0x2000);
#ifdef CHLORIUM_PRIVATE_CANONICAL_DRAW_CHECKS
            if(!denyDispatch)
#endif
            dispatch_((unsigned(geometry.size())+63)/64,1,1);
            glMemoryBarrier(0x2001|GL_BUFFER_UPDATE_BARRIER_BIT);std::vector<Quad> actual(native.size());std::memset(actual.data(),0xa5,actual.size()*96);glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size()*96,actual.data());
            if(std::memcmp(actual.data(),native.data(),native.size_bytes())){++failures;return false;}
            ++committed;quads+=geometry.size();verifiedBytes+=native.size_bytes();return true;
        }catch(...){++failures;return false;}
    }
};
}
