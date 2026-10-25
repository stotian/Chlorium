#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#include <fmt/format.h>
#include "FrameWatch.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

unsigned checks=0;
void require(bool value,const char* why) { ++checks; if(!value) throw std::runtime_error(why); }
void* GLAPIENTRY failMap(GLenum,GLintptr,GLsizeiptr,GLbitfield) { return nullptr; }
GLenum GLAPIENTRY failWait(GLsync,GLbitfield,GLuint64) { return GL_WAIT_FAILED; }
GLsync GLAPIENTRY failFence(GLenum,GLbitfield) { return nullptr; }
void GLAPIENTRY failAllocate(GLenum,GLsizeiptr,const void*,GLenum) {}
PFNGLUNMAPBUFFERPROC realUnmap=nullptr;
GLboolean GLAPIENTRY failUnmap(GLenum target) { realUnmap(target); return GL_FALSE; }
PFNGLMAPBUFFERRANGEPROC realMap=nullptr;
GLuint mappedAtFailure=0;
void* GLAPIENTRY mapWithError(GLenum target,GLintptr offset,GLsizeiptr size,GLbitfield access) {
    const auto pointer=realMap(target,offset,size,access);
    GLint bound=0;glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&bound);mappedAtFailure=bound;
    glBindBuffer(0xffff,0); // Force logging while the diagnostic PBO is mapped.
    return pointer;
}
GLsync GLAPIENTRY fenceWithError(GLenum,GLbitfield) { glBindBuffer(0xffff,0);return nullptr; }

int main(int argc,char** argv) try {
    require(argc==2,"output directory required");
    std::filesystem::create_directories(argv[1]);
    _putenv_s("CHLORIUM_VISUAL_WATCH","1");
    WNDCLASSA cls{}; cls.style=CS_OWNDC; cls.lpfnWndProc=DefWindowProcA;
    cls.hInstance=GetModuleHandleA(nullptr); cls.lpszClassName="ChloriumVisualChecks";
    RegisterClassA(&cls);
    const auto window=CreateWindowA(cls.lpszClassName,"",WS_POPUP,0,0,128,80,nullptr,nullptr,cls.hInstance,nullptr);
    require(window!=nullptr,"hidden diagnostic surface");
    const auto dc=GetDC(window);
    PIXELFORMATDESCRIPTOR format{}; format.nSize=sizeof(format);format.nVersion=1;
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32;
    require(SetPixelFormat(dc,ChoosePixelFormat(dc,&format),&format),"double-buffered pixel format");
    const auto context=wglCreateContext(dc);
    require(context && wglMakeCurrent(dc,context),"GPU context");
    glewExperimental=GL_TRUE;
    require(glewInit()==GLEW_OK && glMapBufferRange && glFenceSync,"diagnostic APIs");
    while(glGetError()!=GL_NO_ERROR) {}
    std::cout << "GL vendor: " << glGetString(GL_VENDOR) << "; renderer: " << glGetString(GL_RENDERER)
        << "; version: " << glGetString(GL_VERSION) << '\n';
    glViewport(0,0,128,80); glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();

    GLuint previousPack=0,texture=0,fbo=0;
    glGenBuffers(1,&previousPack);glBindBuffer(GL_PIXEL_PACK_BUFFER,previousPack);
    std::array<unsigned char,128> sentinel{};sentinel.fill(0x55);
    glBufferData(GL_PIXEL_PACK_BUFFER,sentinel.size(),sentinel.data(),GL_STATIC_READ);
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    require(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"previous read framebuffer");
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT,8);glPixelStorei(GL_PACK_ROW_LENGTH,7);
    glPixelStorei(GL_PACK_SKIP_ROWS,2);glPixelStorei(GL_PACK_SKIP_PIXELS,3);
    auto restored=[&] {
        const std::array<GLenum,8> names{GL_PIXEL_PACK_BUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,
            GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_BUFFER,GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS};
        const std::array<GLint,8> expected{static_cast<GLint>(previousPack),static_cast<GLint>(fbo),0,GL_COLOR_ATTACHMENT0,8,7,2,3};
        for(size_t i=0;i<names.size();++i) {GLint got=0;glGetIntegerv(names[i],&got);require(got==expected[i],"caller GL state restored");}
        require(glGetError()==GL_NO_ERROR,"no diagnostic GL error leaked");
    };
    using chlorium::test::FrameWatch;
    std::vector<std::string> log;
    auto open=[&](FrameWatch& watch,const char* name) {
        log.clear(); watch.open((std::filesystem::path(argv[1])/name).string(),[&](const auto& text){log.push_back(text);});
    };
    auto noFalseBlack=[&] {
        require(std::none_of(log.begin(),log.end(),[](const auto& text){return text.find("\"event\":\"dark_frame\"")!=std::string::npos;}),"failed or white read is never a black frame");
    };
    {
        _putenv_s("CHLORIUM_VISUAL_PROBES","12,12.5,13");
        FrameWatch good;open(good,"good");
        for(unsigned i=0;i<8;++i){good.draw(i+1,10+i,.1*i,.1*i);glFinish();restored();}
        good.finish();require(good.coverageComplete(),"all queued reads including the last are measured");restored();noFalseBlack();
        require(std::count_if(log.begin(),log.end(),[](const auto& text){return text.find("\"event\":\"visual_probe\"")!=std::string::npos;})==3,
            "all requested visual probes recorded, including two requests crossed by one frame");
        _putenv_s("CHLORIUM_VISUAL_PROBES","");
        std::array<unsigned char,128> unchanged{};glGetBufferSubData(GL_PIXEL_PACK_BUFFER,0,unchanged.size(),unchanged.data());
        require(unchanged==sentinel,"caller pixel buffer contents preserved");
    }
    {
        FrameWatch bad;open(bad,"preexisting-error");glBindBuffer(0xffff,0);
        bad.draw(1,10,0,0);bad.finish();require(!bad.coverageComplete(),"preexisting error invalidates coverage");restored();noFalseBlack();
    }
    {
        FrameWatch bounded;open(bounded,"pretarget-boundary");glBindBuffer(0xffff,0);
        bounded.observeBeforeTarget(1,9.9);
        require(std::any_of(log.begin(),log.end(),[](const auto& text){return text.find("\"event\":\"pretarget_gl_error\"")!=std::string::npos;}),
            "stale pretarget error is retained as an explicit out-of-coverage event");
        bounded.draw(2,10,0,0);glFinish();bounded.finish();
        require(bounded.coverageComplete(),"observed loading error does not contaminate a later valid target frame");restored();noFalseBlack();
    }
    {
        FrameWatch strict;open(strict,"target-error-after-boundary");
        strict.observeBeforeTarget(1,9.9);glBindBuffer(0xffff,0);
        strict.draw(2,10,0,0);strict.finish();
        require(!strict.coverageComplete(),"a fresh target-frame error remains a strict failure");restored();noFalseBlack();
    }
    auto runFailure=[&](const char* name,auto install,auto restore) {
        FrameWatch bad;open(bad,name);bad.draw(1,10,0,0);glFinish();
        install();bad.draw(2,10.1,.1,.1);restore();bad.finish();
        require(!bad.coverageComplete(),"injected readback failure invalidates verdict");restored();noFalseBlack();
    };
    const auto map=__glewMapBufferRange;
    runFailure("failed-map",[]{__glewMapBufferRange=failMap;},[&]{__glewMapBufferRange=map;});
    realUnmap=__glewUnmapBuffer;
    runFailure("corrupt-unmap",[]{__glewUnmapBuffer=failUnmap;},[]{__glewUnmapBuffer=realUnmap;});
    const auto wait=__glewClientWaitSync;
    runFailure("failed-fence-wait",[]{__glewClientWaitSync=failWait;},[&]{__glewClientWaitSync=wait;});
    const auto fence=__glewFenceSync;
    runFailure("null-fence",[]{__glewFenceSync=failFence;},[&]{__glewFenceSync=fence;});
    const auto allocate=__glewBufferData;
    {
        FrameWatch bad;open(bad,"failed-allocation");
        __glewBufferData=failAllocate;bad.draw(1,10,0,0);__glewBufferData=allocate;
        bad.finish();require(!bad.coverageComplete(),"failed allocation invalidates verdict");restored();noFalseBlack();
    }
    {
        // Read-buffer selection belongs to the FBO, including FBO zero.
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_FRONT);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        FrameWatch sample;open(sample,"default-read-selector");sample.draw(1,10,0,0);glFinish();sample.finish();
        require(sample.coverageComplete(),"default selector fixture read");restored();
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);GLint selector=0;glGetIntegerv(GL_READ_BUFFER,&selector);
        require(selector==GL_FRONT,"default framebuffer selector preserved");glReadBuffer(GL_BACK);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);restored();
    }
    auto throwingLog=[&](const std::string& text) {
        log.push_back(text);
        if(text.find("\"event\":\"visual_gl_error\"")!=std::string::npos) throw std::runtime_error("injected callback exception");
    };
    {
        FrameWatch sample;log.clear();sample.open((std::filesystem::path(argv[1])/"throw-while-mapped").string(),throwingLog);
        sample.draw(1,10,0,0);glFinish();
        realMap=__glewMapBufferRange;__glewMapBufferRange=mapWithError;
        bool threw=false;
        try {sample.draw(2,10.1,.1,.1);}catch(const std::runtime_error&){threw=true;}
        __glewMapBufferRange=realMap;
        require(threw,"callback exception reached mapped-buffer cleanup");restored();
        glBindBuffer(GL_PIXEL_PACK_BUFFER,mappedAtFailure);GLint mapped=GL_TRUE;glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER,GL_BUFFER_MAPPED,&mapped);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,previousPack);
        require(mapped==GL_FALSE,"mapped diagnostic PBO released on exception");
        sample.finish();require(!sample.coverageComplete(),"exception invalidates coverage");restored();noFalseBlack();
    }
    {
        FrameWatch sample;log.clear();sample.open((std::filesystem::path(argv[1])/"throw-during-read").string(),throwingLog);
        const auto savedFence=__glewFenceSync;__glewFenceSync=fenceWithError;
        bool threw=false;
        try {sample.draw(1,10,0,0);}catch(const std::runtime_error&){threw=true;}
        __glewFenceSync=savedFence;
        require(threw,"callback exception reached redirected framebuffer cleanup");restored();
        sample.finish();require(!sample.coverageComplete(),"read exception invalidates coverage");restored();noFalseBlack();
    }
    GLuint invalidatedSentinel=0;
    {
        FrameWatch sample;open(sample,"context-generation-invalidated");
        sample.draw(1,10,0,0);glFinish();
        auto epoch=chlorium::research::gl::acquireContextEpoch();
        require(epoch && epoch->isCurrent(),"diagnostic context identity");invalidatedSentinel=epoch->sentinel();
        require(chlorium::research::gl::labelObject(GL_VERTEX_ARRAY,invalidatedSentinel,-1,"fixture:recreated-context"),"invalidate context stamp");
        sample.draw(2,10.1,.1,.1);sample.finish();
        require(!sample.coverageComplete(),"stale context generation invalidates readback coverage");
        require(std::any_of(log.begin(),log.end(),[](const auto& text){return text.find("context_or_generation_changed")!=std::string::npos;}),
            "generation failure is explicitly reported");
        restored();noFalseBlack();
    }
    glDeleteVertexArrays(1,&invalidatedSentinel);
    std::cout << checks << " GPU readback/state checks passed; failures never became black frames\n";
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
    return 0;
} catch(const std::exception& error) {std::cerr << error.what()<<'\n';return 1;}
