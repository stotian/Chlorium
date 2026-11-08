#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include "RetainedScene.hpp"
#include "NativeShaderSources.hpp"
#ifdef CHLORIUM_RENDERER_TIMING_R60
#include "../R4-60/RendererTiming.hpp"
#endif
#include "../../src/renderer/ContextEpoch.hpp"
#include "../../src/renderer/GLState.hpp"
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include "../R4-49/NativeVertexStream.hpp"

namespace chlorium::research::resident {
// Owned offscreen submission endpoint. Neither fixture nor reviewed-source mode
// hooks native sprites, suppresses callbacks or replaces a live native program.
class Renderer {
    friend class NativeReplay;
    struct State {
        GLint vao=0, storage=0, program=0, active=0, texture=0;
        std::array<GLStateSnapshot::IndexedBufferBinding,4> indexed{};
        State() {
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
            glGetIntegerv(GLStateSnapshot::storageBinding,&storage);
            glGetIntegerv(GL_CURRENT_PROGRAM,&program);
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
            glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); glActiveTexture(active);
            for(unsigned i=0;i<4;++i)indexed[i]=GLStateSnapshot::captureIndexed(
                GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,i);
        }
        ~State() {
            glUseProgram(program); glBindVertexArray(vao);
            for(unsigned i=0;i<4;++i)GLStateSnapshot::restoreIndexed(GLStateSnapshot::storageTarget,i,indexed[i]);
            glBindBuffer(GLStateSnapshot::storageTarget,storage);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(active);
        }
    };
    std::shared_ptr<gl::ContextEpoch<>> epoch_;
    GLuint buffers_[4]{}, indices_=0, vao_=0, program_=0;
    GLint mvp_=-1, sampler_=-1, effect_=-1;
    std::size_t capacity_=0, orderCount_=0;
    std::uint64_t identity_=0, owner_=0, revision_=0;
    std::array<std::uint64_t,4> uploaded_{};
    NativeVertexStream nativeStream_;
    static constexpr std::array<std::size_t,4> strides{
        sizeof(Geometry),sizeof(Parameters),sizeof(std::uint32_t),sizeof(NativeQuadState)};
    bool current() const noexcept { return epoch_ && epoch_->isCurrent(); }
    static GLuint compile(GLenum stage,std::string_view text,std::string& error) {
        const auto shader=glCreateShader(stage);
        if(!shader){error="shader allocation";return 0;}
        const auto* data=text.data(); const auto length=static_cast<GLint>(text.size());
        glShaderSource(shader,1,&data,&length); glCompileShader(shader);
        GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);error=log;glDeleteShader(shader);return 0;}
        return shader;
    }
    bool readyToDraw(const Scene& scene,std::size_t first,std::size_t count) const noexcept {
        return scene.usable() && matches(scene.identity(),scene.revision()) &&
               first<=orderCount_ && count<=orderCount_-first;
    }
    static bool observationsIdle(){
        GLboolean feedback=GL_FALSE;glGetBooleanv(0x8e24,&feedback);if(feedback)return false;
        for(auto query:{GL_SAMPLES_PASSED,0x8c2f,0x8d6a,GL_PRIMITIVES_GENERATED,GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN}){
            GLint active=0;glGetQueryiv(query,GL_CURRENT_QUERY,&active);if(active)return false;
        }
        return true;
    }
    bool seedNativeStream(Scene& scene){
        if(!matches(scene.identity(),scene.revision()) || !scene.usable() ||
           glIsEnabled(GL_RASTERIZER_DISCARD) || !observationsIdle())return false;
        State saved;glBindVertexArray(vao_);
        for(unsigned lane=0;lane<4;++lane)glBindBufferBase(GLStateSnapshot::storageTarget,lane,buffers_[lane]);
        return glGetError()==GL_NO_ERROR && nativeStream_.materialize(scene.orderedCount());
    }
public:
    enum class Draw { nativeRequired, submitted, failedAfterSubmission };
    Renderer()=default;
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    ~Renderer(){close();}
    std::uint64_t identity() const noexcept { return identity_; }
    const std::array<std::uint64_t,4>& uploadedBytes() const noexcept{return uploaded_;}
    bool synchronize(Scene& scene){
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timing(residenttiming::Stage::upload);
#endif
        uploaded_={};return scene.synchronize(*this);
    }
    bool belongsTo(std::uint64_t owner,std::size_t capacity) const noexcept {
        return owner_==owner && owner_ && capacity_==capacity;
    }
    bool matches(std::uint64_t owner,std::uint64_t revision) const noexcept {
        return identity_ && current() && owner_==owner && revision_==revision;
    }
    void close() noexcept {
        nativeStream_.close();
        // Context generations prevent deleting recycled names in a new context.
        if(current()){
            glDeleteBuffers(4,buffers_); if(indices_)glDeleteBuffers(1,&indices_);
            if(vao_)glDeleteVertexArrays(1,&vao_); if(program_)glDeleteProgram(program_);
        }
        std::fill(std::begin(buffers_),std::end(buffers_),0);
        indices_=vao_=program_=0; identity_=owner_=revision_=0; capacity_=orderCount_=0;
        mvp_=sampler_=effect_=-1; epoch_.reset();
    }
    bool initialize(std::size_t capacity,std::string_view vertex,std::string_view fragment,std::string& error) {
        return initializeProgram(capacity,vertex,fragment,true,error);
    }
    bool initializeReviewed(std::size_t capacity,std::string_view residentVertex,
                            std::string_view nativeVertex,std::string_view nativeFragment,std::string& error) {
        auto sources=reviewedNativeSources(residentVertex,nativeVertex,nativeFragment,error);
        return sources && initializeProgram(capacity,sources->vertex,sources->fragment,false,error);
    }
private:
    bool initializeProgram(std::size_t capacity,std::string_view vertex,std::string_view fragment,
                           bool requireFixtureEffect,std::string& error) {
        error.clear();
        if(epoch_ || !capacity || capacity>65536 || vertex.empty() || fragment.empty() ||
           vertex.size()>65536 || fragment.size()>65536 || !wglGetCurrentContext())return false;
        GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        if(major<4 || (major==4 && minor<3))return false;
        std::vector<std::uint32_t> indices(capacity*6);
        constexpr std::uint32_t topology[]{0,1,2,3,2,1};
        for(std::size_t n=0;n<capacity;++n)for(unsigned c=0;c<6;++c)indices[n*6+c]=static_cast<std::uint32_t>(n*4+topology[c]);
        // shortcut: isolated owner supplies a clean GL error boundary; native integration
        // needs an explicit error/state contract before this renderer can replace GD.
        State saved;
        epoch_=gl::ContextEpoch<>::capture(); if(!epoch_)return false;
        const auto vs=compile(GL_VERTEX_SHADER,vertex,error);
        if(!vs){close();return false;}
        const auto fs=compile(GL_FRAGMENT_SHADER,fragment,error);
        if(!fs){glDeleteShader(vs);close();return false;}
        program_=glCreateProgram();
        if(program_){glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);}
        glDeleteShader(vs);glDeleteShader(fs);
        GLint linked=0;if(program_)glGetProgramiv(program_,GL_LINK_STATUS,&linked);
        if(!linked){char log[4096]{};if(program_)glGetProgramInfoLog(program_,sizeof(log),nullptr,log);error=log;close();return false;}
        mvp_=glGetUniformLocation(program_,"CC_MVPMatrix");
        sampler_=glGetUniformLocation(program_,"CC_Texture0"); effect_=glGetUniformLocation(program_,"native_effect");
        if(mvp_<0 || sampler_<0 || (requireFixtureEffect && effect_<0)){error="owned shader interface";close();return false;}
        glGenBuffers(4,buffers_);glGenBuffers(1,&indices_);glGenVertexArrays(1,&vao_);
        if(!indices_ || !vao_ || std::find(std::begin(buffers_),std::end(buffers_),GLuint{0})!=std::end(buffers_)){
            error="resident allocation";close();return false;
        }
        for(unsigned lane=0;lane<4;++lane){
            glBindBuffer(GLStateSnapshot::storageTarget,buffers_[lane]);
            glBufferData(GLStateSnapshot::storageTarget,static_cast<GLsizeiptr>(capacity*strides[lane]),nullptr,GL_DYNAMIC_DRAW);
        }
        glBindVertexArray(vao_);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(indices.size()*sizeof(indices[0])),indices.data(),GL_STATIC_DRAW);
        if(glGetError()!=GL_NO_ERROR){error="resident buffer initialization";close();return false;}
        identity_=nextIdentity();capacity_=capacity;
        if(!identity_){error="resident identity exhausted";close();return false;}
        return true;
    }

private:
    friend class Scene;
    // Scene calls these only while synchronize() owns the CPU snapshot.
    bool begin(std::uint64_t owner,std::size_t capacity) noexcept {
        if(!identity_ || !owner || capacity!=capacity_ || !current())return false;
        owner_=revision_=0; orderCount_=0; return true;
    }
    bool write(Lane lane,std::size_t first,std::span<const std::byte> bytes) noexcept {
        const auto index=static_cast<unsigned>(lane);
        if(!identity_ || !current() || index>=4 || first>capacity_ || bytes.size()%strides[index] ||
           bytes.size()/strides[index]>capacity_-first)return false;
        if(bytes.empty())return true;
        GLint previous=0;glGetIntegerv(GLStateSnapshot::storageBinding,&previous);
        glBindBuffer(GLStateSnapshot::storageTarget,buffers_[index]);
        glBufferSubData(GLStateSnapshot::storageTarget,static_cast<GLintptr>(first*strides[index]),
                        static_cast<GLsizeiptr>(bytes.size()),bytes.data());
        const auto error=glGetError();glBindBuffer(GLStateSnapshot::storageTarget,previous);
        if(error==GL_NO_ERROR)uploaded_[index]+=bytes.size();
        return error==GL_NO_ERROR;
    }
    bool commit(std::uint64_t owner,std::uint64_t revision,std::size_t count,std::span<const std::uint64_t> mask,bool seed,bool orderChanged) noexcept {
        if(!identity_ || !current() || !owner || !revision || count>capacity_)return false;
        if(!nativeStream_.publishChanges(mask,seed,orderChanged))return false;
        owner_=owner;revision_=revision;orderCount_=count;return true;
    }

public:
    Draw draw(Scene& scene,std::size_t first,std::size_t count,GLuint texture,GLuint ownedTarget,
              const std::array<float,16>& matrix,const std::array<float,4>& effect) {
        return drawTarget(scene,first,count,texture,ownedTarget,matrix,effect,false);
    }
private:
    Draw drawTarget(Scene& scene,std::size_t first,std::size_t count,GLuint texture,GLuint ownedTarget,
                    const std::array<float,16>& matrix,const std::array<float,4>& effect,bool checkedNativeReplay,GLuint nativeProgram=0,
                    bool(*beforeSubmit)(void*)=nullptr,void* checkpoint=nullptr) {
        if(!current() || !identity_ || !texture || !glIsTexture(texture) ||
           (_mm_getcsr()&0x7f80u)!=0x1f80u || !scene.usable() || first>scene.orderedCount() ||
           count>scene.orderedCount()-first)return Draw::nativeRequired;
        // The R4-33 fixture shaders do not implement native clip/depth/feedback
        // semantics. This endpoint must remain confined to its owned framebuffer.
        GLint fbo=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);
        if((!ownedTarget && !checkedNativeReplay) || static_cast<GLuint>(fbo)!=ownedTarget ||
           glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE ||
           glIsEnabled(GL_DEPTH_TEST) || glIsEnabled(GL_STENCIL_TEST) || glIsEnabled(GL_RASTERIZER_DISCARD))return Draw::nativeRequired;
        GLint drawBuffer=0,attachmentType=0,attachmentName=0;
        glGetIntegerv(GL_DRAW_BUFFER0,&drawBuffer);
        if(!checkedNativeReplay && drawBuffer!=GL_COLOR_ATTACHMENT0)return Draw::nativeRequired;
        GLint targets=0,samples=0;glGetIntegerv(GL_MAX_DRAW_BUFFERS,&targets);glGetIntegerv(GL_SAMPLES,&samples);
        if(targets<1 || samples>1)return Draw::nativeRequired;
        for(GLint i=1;i<targets;++i){GLint target=0;glGetIntegerv(GL_DRAW_BUFFER0+i,&target);if(target!=GL_NONE)return Draw::nativeRequired;}
        if(fbo){
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&attachmentType);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&attachmentName);
            if((!checkedNativeReplay && attachmentType!=GL_TEXTURE) ||
               (attachmentType==GL_TEXTURE && static_cast<GLuint>(attachmentName)==texture))return Draw::nativeRequired;
        }
        if(!observationsIdle())return Draw::nativeRequired;
        State saved;
        if(nativeProgram && (!checkedNativeReplay || first || !nativeStream_.vao() || GLuint(saved.program)!=nativeProgram))return Draw::nativeRequired;
        if(!synchronize(scene) || !readyToDraw(scene,first,count))return Draw::nativeRequired;
        if(!count)return Draw::submitted;
        glUseProgram(program_);glUniformMatrix4fv(mvp_,1,GL_FALSE,matrix.data());
        if(effect_>=0)glUniform4fv(effect_,1,effect.data());glUniform1i(sampler_,0);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
        glBindVertexArray(vao_);
        for(unsigned lane=0;lane<4;++lane)glBindBufferBase(GLStateSnapshot::storageTarget,lane,buffers_[lane]);
        if(glGetError()!=GL_NO_ERROR)return Draw::nativeRequired;
        if(nativeProgram){
            if(!nativeStream_.materialize(count))return Draw::nativeRequired;
            glUseProgram(nativeProgram);glBindVertexArray(nativeStream_.vao());
            if(glGetError()!=GL_NO_ERROR)return Draw::nativeRequired;
        }
        // Checkpoints must preserve GL state; copy only after all guards/uploads succeed.
        if(beforeSubmit && (!beforeSubmit(checkpoint) || glGetError()!=GL_NO_ERROR))return Draw::nativeRequired;
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timing(residenttiming::Stage::draw);
#endif
        glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(count*6),GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(first*6*sizeof(std::uint32_t)));
        // After submission, native redraw would duplicate blending. A future
        // game adapter must restore the target before retrying natively.
        return glGetError()==GL_NO_ERROR?Draw::submitted:Draw::failedAfterSubmission;
    }
};
}
