#pragma once
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include <windows.h>
#include "../../src/renderer/NativeQuadState.hpp"
#include <array>
#include <cstring>
#include <span>
#include <vector>
#include <functional>
#include <algorithm>
#include <string_view>

namespace chlorium::research::batchreplay {
struct Result {
    const char* gate="disabled";
    bool attempted=false,baselineRun=false,candidateRun=false,recoveredNative=false,alternateRun=false,glFailure=false;
    std::size_t copiedBeforeDiff=0,copiedAfterDiff=0,restoredBeforeDiff=0,baselineDiff=0,candidateDiff=0,restoredAfterDiff=0;
    unsigned quads=0,removedPoints=0;
    std::size_t alternateDiff=0;
};
inline bool comparisonPassed(const Result& r,bool alternateRequired,bool handoffRequired,unsigned nativeCalls)noexcept{
    return std::string_view(r.gate)=="compared"&&!r.glFailure&&r.attempted&&r.baselineRun&&
        !r.copiedBeforeDiff&&!r.copiedAfterDiff&&!r.restoredBeforeDiff&&!r.baselineDiff&&!r.candidateDiff&&!r.restoredAfterDiff&&
        (!alternateRequired||(r.alternateRun&&!r.alternateDiff))&&(!handoffRequired||nativeCalls==1);
}
inline std::size_t differences(std::span<const unsigned char> a,std::span<const unsigned char> b){if(a.size()!=b.size())return SIZE_MAX;std::size_t changed=0;for(std::size_t i=0;i<a.size();++i)changed+=a[i]!=b[i];return changed;}
inline bool pointQuad(const snapshot::NativeQuadState& quad){
    std::array<unsigned,3> bits{};std::memcpy(bits.data(),&quad.corners[0],12);for(auto value:bits)if((value&0x7fffffffu)>0x4f800000u)return false;
    for(unsigned i=1;i<4;++i)if(std::memcmp(&quad.corners[0],&quad.corners[i],12))return false;return true;
}
class Replay {
    friend class TargetBackup;
    GLuint framebuffers[2]{},textures[2]{},indexBuffer=0;
    HGLRC context=nullptr;GLint native=0,nativeRead=0,nativeDraw=0;
    int width=0,height=0;bool prepared=false;
    std::vector<unsigned char> scratch;
    static GLint integer(GLenum parameter){GLint value=0;glGetIntegerv(parameter,&value);return value;}
    static bool colorSafe(int alphaBits){
        const auto draw=integer(GL_DRAW_BUFFER0);
        const bool defaultMono=!integer(0x8ca6)&&!integer(GL_STEREO);
        const auto normalize=[&](GLint buffer){return defaultMono?(buffer==GL_BACK?GL_BACK_LEFT:buffer==GL_FRONT?GL_FRONT_LEFT:buffer):buffer;};
        if(draw==GL_NONE || normalize(draw)!=normalize(integer(GL_READ_BUFFER)))return false;
        const auto count=integer(GL_MAX_DRAW_BUFFERS);
        for(GLint n=1;n<count;++n)if(integer(GL_DRAW_BUFFER0+n)!=GL_NONE)return false;
        GLenum attachment=draw;
        if(!integer(0x8ca6)){
            if(draw==GL_BACK)attachment=GL_BACK_LEFT;
            else if(draw==GL_FRONT)attachment=GL_FRONT_LEFT;
            else if(draw!=GL_BACK_LEFT&&draw!=GL_BACK_RIGHT&&draw!=GL_FRONT_LEFT&&draw!=GL_FRONT_RIGHT)return false;
        }
        GLint type=0;glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,attachment,0x8211,&type);
        if(type!=GL_UNSIGNED_NORMALIZED)return false;
        for(unsigned n=0;n<4;++n){GLint bits=0;glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,attachment,0x8212+n,&bits);if(bits!=(n==3?alphaBits:8))return false;}
        return true;
    }
    static bool safeState(int w,int h){
        if(integer(0x88ed)||integer(0x88ef)||integer(GL_ACTIVE_TEXTURE)!=GL_TEXTURE0||integer(GL_SAMPLES)||glIsEnabled(GL_SCISSOR_TEST)||glIsEnabled(0x8db9)||glIsEnabled(GL_DEPTH_TEST)||glIsEnabled(GL_STENCIL_TEST)||glIsEnabled(GL_COLOR_LOGIC_OP)||glIsEnabled(GL_RASTERIZER_DISCARD)||glIsEnabled(GL_POLYGON_SMOOTH)||glIsEnabled(0x8f9d))return false;
        const auto major=integer(GL_MAJOR_VERSION),minor=integer(GL_MINOR_VERSION);if((major>4||(major==4&&minor>=3))&&glIsEnabled(0x8d69))return false;
        for(auto parameter:{GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_IMAGE_HEIGHT,GL_PACK_SKIP_IMAGES})if(integer(parameter))return false;
        const auto alignment=integer(GL_PACK_ALIGNMENT);if(alignment!=1&&alignment!=2&&alignment!=4)return false;
        std::array<GLint,4> viewport{};glGetIntegerv(GL_VIEWPORT,viewport.data());if(viewport!=std::array<GLint,4>{0,0,w,h})return false;
        std::array<GLboolean,4> mask{};glGetBooleanv(GL_COLOR_WRITEMASK,mask.data());for(auto channel:mask)if(!channel)return false;
        std::array<GLint,2> polygon{};glGetIntegerv(GL_POLYGON_MODE,polygon.data());if(polygon[0]!=GL_FILL||polygon[1]!=GL_FILL)return false;
        if((GLEW_VERSION_4_0||GLEW_ARB_transform_feedback2)&&integer(0x8e24))return false;
        for(auto target:{GL_SAMPLES_PASSED,GL_PRIMITIVES_GENERATED,GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN}){GLint active=0;glGetQueryiv(target,GL_CURRENT_QUERY,&active);if(active)return false;}
        if(major>3||(major==3&&minor>=3)){GLint active=0;glGetQueryiv(0x8c2f,GL_CURRENT_QUERY,&active);if(active)return false;}
        if(major>4||(major==4&&minor>=3)){GLint active=0;glGetQueryiv(0x8d6a,GL_CURRENT_QUERY,&active);if(active)return false;}
        return true;
    }
    struct State {
        GLint read=integer(0x8caa),draw=integer(0x8ca6),readBuffer=integer(GL_READ_BUFFER),drawBuffer=integer(GL_DRAW_BUFFER0);
        GLint texture=integer(GL_TEXTURE_BINDING_2D),element=integer(GL_ELEMENT_ARRAY_BUFFER_BINDING),copy=integer(GL_COPY_WRITE_BUFFER);
        void restore()const{glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glReadBuffer(readBuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glDrawBuffer(drawBuffer);glBindTexture(GL_TEXTURE_2D,texture);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,element);glBindBuffer(GL_COPY_WRITE_BUFFER,copy);}
        ~State(){restore();}
    };
    void bindNative(){glBindFramebuffer(GL_READ_FRAMEBUFFER,native);glReadBuffer(nativeRead);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native);glDrawBuffer(nativeDraw);}
    void save(unsigned index){glBindFramebuffer(GL_READ_FRAMEBUFFER,native);glReadBuffer(nativeRead);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffers[index]);glDrawBuffer(GL_COLOR_ATTACHMENT0);glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);if(glGetError()!=GL_NO_ERROR)result.glFailure=true;}
    void restore(unsigned index){glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffers[index]);glReadBuffer(GL_COLOR_ATTACHMENT0);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native);glDrawBuffer(nativeDraw);glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);bindNative();if(glGetError()!=GL_NO_ERROR)result.glFailure=true;}
    std::size_t readDifference(GLuint framebuffer,GLenum buffer,std::span<const unsigned char> expected){glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);glReadBuffer(buffer);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,scratch.data());if(glGetError()!=GL_NO_ERROR){result.glFailure=true;return SIZE_MAX;}return differences(scratch,expected);}
public:
    Result result;
    Replay()=default;Replay(const Replay&)=delete;Replay& operator=(const Replay&)=delete;
    ~Replay(){close();}
    void close()noexcept{if(context&&wglGetCurrentContext()==context){glDeleteFramebuffers(2,framebuffers);glDeleteTextures(2,textures);if(indexBuffer)glDeleteBuffers(1,&indexBuffer);}framebuffers[0]=framebuffers[1]=textures[0]=textures[1]=indexBuffer=0;context=nullptr;prepared=false;}
    bool prepare(int w,int h,int encoding,int alphaBits,std::span<const unsigned char> before){
        result.attempted=true;result.gate="preflight";
        if(prepared||!GLEW_VERSION_3_1||w<=0||h<=0||w>1920||h>1080||before.size()!=std::size_t(w)*h*4||
           (encoding!=GL_LINEAR&&encoding!=0x8c40)||(alphaBits!=0&&alphaBits!=8)||!safeState(w,h)||integer(0x8ca6)!=integer(0x8caa))return false;
        result.gate="color_attachment_format";if(!colorSafe(alphaBits))return false;
        State saved;width=w;height=h;native=saved.draw;nativeRead=saved.readBuffer;nativeDraw=saved.drawBuffer;context=wglGetCurrentContext();scratch.resize(before.size());
        const GLenum format=encoding==0x8c40?(alphaBits?0x8c43:0x8c41):(alphaBits?GL_RGBA8:GL_RGB8);
        glGenFramebuffers(2,framebuffers);glGenTextures(2,textures);
        if(!framebuffers[0]||!framebuffers[1]||!textures[0]||!textures[1]){result.gate="owned_names";saved.restore();close();return false;}
        for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[i],0);glReadBuffer(GL_COLOR_ATTACHMENT0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
            if(!textures[i]||!framebuffers[i]||glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){result.gate="owned_storage";saved.restore();close();return false;}}
        save(0);result.copiedBeforeDiff=readDifference(framebuffers[0],GL_COLOR_ATTACHMENT0,before);
        if(result.copiedBeforeDiff){result.gate="before_copy_differs";saved.restore();close();return false;}
        prepared=true;result.gate="before_saved";return true;
    }
    template<class Redraw>
    void run(std::span<const unsigned char> before,std::span<const unsigned char> after,std::span<const unsigned char> vertices,
             std::span<const unsigned char> nativeIndices,unsigned count,bool positionOnlyShader,Redraw&& redraw,std::function<bool()> alternate={}){
        if(!prepared)return;
        if(wglGetCurrentContext()!=context){result.gate="changed_context";close();return;}
        State saved;
        if(saved.draw!=native||saved.read!=native||after.size()!=scratch.size()||!safeState(width,height)){result.gate="changed_native_target";saved.restore();close();return;}
        result.quads=count;std::vector<unsigned short> compact;
        constexpr unsigned topology[]{0,1,2,3,2,1};bool compactSupported=positionOnlyShader&&count&&count<=16384&&vertices.size()==std::size_t(count)*96&&nativeIndices.size()==std::size_t(count)*12;
        if(compactSupported){compact.reserve(std::size_t(count)*6);
            for(unsigned i=0;i<count;++i){snapshot::NativeQuadState quad;std::memcpy(&quad,vertices.data()+i*96,96);const bool point=pointQuad(quad);result.removedPoints+=point;
                for(unsigned k=0;k<6;++k){unsigned short actual=0;std::memcpy(&actual,nativeIndices.data()+i*12+k*2,2);if(actual!=i*4+topology[k])compactSupported=false;if(!point)compact.push_back(actual);}}
            if(compactSupported&&result.removedPoints){glGenBuffers(1,&indexBuffer);if(indexBuffer){glBindBuffer(GL_COPY_WRITE_BUFFER,indexBuffer);glBufferData(GL_COPY_WRITE_BUFFER,compact.size()*2,compact.data(),GL_STATIC_DRAW);GLint size=0;glGetBufferParameteriv(GL_COPY_WRITE_BUFFER,GL_BUFFER_SIZE,&size);compactSupported=size==compact.size()*2;}else compactSupported=false;}}
        save(1);result.copiedAfterDiff=readDifference(framebuffers[1],GL_COLOR_ATTACHMENT0,after);
        if(result.copiedAfterDiff){result.gate="after_copy_differs";saved.restore();close();return;}
        bool changed=false;
        try {
            changed=true;restore(0);result.restoredBeforeDiff=readDifference(native,nativeRead,before);
            if(!result.restoredBeforeDiff){saved.restore();redraw();result.baselineRun=true;result.baselineDiff=readDifference(native,nativeRead,after);
                if(!result.baselineDiff&&compactSupported&&result.removedPoints){restore(0);result.restoredBeforeDiff=readDifference(native,nativeRead,before);
                    if(!result.restoredBeforeDiff){saved.restore();glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBuffer);glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(compact.size()),GL_UNSIGNED_SHORT,nullptr);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,saved.element);result.candidateRun=true;result.candidateDiff=readDifference(native,nativeRead,after);}}
            }
            result.gate="compared";
            if(result.baselineRun&&!result.baselineDiff&&alternate){restore(0);result.restoredBeforeDiff=std::max(result.restoredBeforeDiff,readDifference(native,nativeRead,before));if(!result.restoredBeforeDiff){saved.restore();if(alternate()){result.alternateRun=true;result.alternateDiff=readDifference(native,nativeRead,after);}}}
        }catch(...){result.gate="replay_exception";}
        if(changed){restore(1);result.restoredAfterDiff=readDifference(native,nativeRead,after);
            if(result.restoredAfterDiff){restore(0);if(!readDifference(native,nativeRead,before)){saved.restore();redraw();result.recoveredNative=readDifference(native,nativeRead,after)==0;}}}
        saved.restore();close();
    }
};
}
