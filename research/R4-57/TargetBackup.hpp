#pragma once
#include "../R4-43/NativeBatchReplay.hpp"
#include "../../src/renderer/ContextEpoch.hpp"

namespace chlorium::research::batchreplay {
// GPU-only rollback; the normal path never reads pixels back to the CPU.
class TargetBackup {
    GLuint framebuffer_=0,texture_=0;
    std::shared_ptr<gl::ContextEpoch<>> epoch_;
    GLint native_=0,read_=0,draw_=0;
    int width_=0,height_=0;
    bool armed_=false;
public:
    ~TargetBackup(){close();}
    void close()noexcept{
        if(epoch_ && epoch_->isCurrent()){
            if(framebuffer_)glDeleteFramebuffers(1,&framebuffer_);
            if(texture_)glDeleteTextures(1,&texture_);
        }
        framebuffer_=texture_=0;epoch_.reset();width_=height_=0;armed_=false;
    }
    bool begin(){
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timing(residenttiming::Stage::backup);
#endif
        armed_=false;
        std::array<GLint,4> v{};glGetIntegerv(GL_VIEWPORT,v.data());
        const auto w=v[2],h=v[3];
        if(w<=0 || h<=0 || w>1920 || h>1080 || !Replay::safeState(w,h))return false;
        const auto alpha=Replay::integer(GL_ALPHA_BITS);
        if((alpha!=0 && alpha!=8) || !Replay::colorSafe(alpha) ||
           Replay::integer(0x8caa)!=Replay::integer(0x8ca6))return false;
        if(epoch_ && (!epoch_->isCurrent() || w!=width_ || h!=height_))close();
        Replay::State saved;
        if(!epoch_){
            epoch_=gl::ContextEpoch<>::capture();if(!epoch_)return false;
            glGenTextures(1,&texture_);glGenFramebuffers(1,&framebuffer_);
            if(!texture_ || !framebuffer_){saved.restore();close();return false;}
            glBindTexture(GL_TEXTURE_2D,texture_);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer_);glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture_,0);
            if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR){saved.restore();close();return false;}
            width_=w;height_=h;
        }
        native_=saved.draw;read_=saved.readBuffer;draw_=saved.drawBuffer;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,native_);glReadBuffer(read_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer_);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        armed_=glGetError()==GL_NO_ERROR;return armed_;
    }
    bool restore(){
        if(!armed_ || !epoch_ || !epoch_->isCurrent())return false;
        Replay::State saved;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer_);glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native_);glDrawBuffer(draw_);
        glBlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        const bool ok=glGetError()==GL_NO_ERROR;armed_=false;return ok;
    }
    void commit()noexcept{armed_=false;}
};
}
