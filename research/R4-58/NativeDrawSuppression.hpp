#pragma once
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include <windows.h>
#include "../../src/renderer/ContextEpoch.hpp"

namespace chlorium::research::continuous {
// A zero-sample predicate suppresses rendering, not native buffer/state updates.
// https://registry.khronos.org/OpenGL/extensions/NV/NV_conditional_render.txt
class NativeDrawSuppression {
    GLuint query_=0;
    bool active_=false;
    std::shared_ptr<gl::ContextEpoch<>> epoch_;
public:
    NativeDrawSuppression()=default;
    NativeDrawSuppression(const NativeDrawSuppression&)=delete;
    NativeDrawSuppression& operator=(const NativeDrawSuppression&)=delete;
    ~NativeDrawSuppression(){close();}
    bool end()noexcept{
        if(!active_)return true;
        glEndConditionalRender();active_=false;return glGetError()==GL_NO_ERROR;
    }
    void close()noexcept{
        if(epoch_ && epoch_->isCurrent()){
            end();if(query_)glDeleteQueries(1,&query_);
        }
        query_=0;active_=false;epoch_.reset();
    }
    bool begin(){
        if(active_ || !glBeginConditionalRender || !glEndConditionalRender)return false;
        if(epoch_ && !epoch_->isCurrent())close();
        if(!query_){
            GLint current=0;glGetQueryiv(GL_SAMPLES_PASSED,GL_CURRENT_QUERY,&current);
            if(current || glGetError()!=GL_NO_ERROR)return false;
            epoch_=gl::ContextEpoch<>::capture();if(!epoch_)return false;
            glGenQueries(1,&query_);if(!query_)return false;
            glBeginQuery(GL_SAMPLES_PASSED,query_);
            if(glGetError()!=GL_NO_ERROR){close();return false;}
            glEndQuery(GL_SAMPLES_PASSED);
            if(glGetError()!=GL_NO_ERROR){close();return false;}
        }
        // WAIT prevents NO_WAIT from accidentally executing a duplicate draw.
        // A prior error must not make a successful begin look rejected and leak the predicate.
        if(glGetError()!=GL_NO_ERROR)return false;
        glBeginConditionalRender(query_,GL_QUERY_WAIT);
        active_=glGetError()==GL_NO_ERROR;return active_;
    }
};
}
