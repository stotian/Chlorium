#pragma once
#include "NativeCornerOracleFixture.hpp"
#include <GL/glew.h>

namespace {
struct Graphics {
    HWND window=nullptr;HDC dc=nullptr;HGLRC context=nullptr,bootstrap=nullptr;
    Graphics()=default;
    void create(bool compatibility=false,int minorVersion=3,bool doubleBuffered=false) {
        WNDCLASSW cls{};cls.style=CS_OWNDC;cls.lpfnWndProc=DefWindowProcW;
        cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"ChloriumIsolatedCorners";
        require(RegisterClassW(&cls)!=0,"register hidden graphics surface");
        window=CreateWindowW(cls.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
        require(window!=nullptr,"create hidden graphics surface");dc=GetDC(window);require(dc!=nullptr,"graphics DC");
        PIXELFORMATDESCRIPTOR pixel{};pixel.nSize=sizeof(pixel);pixel.nVersion=1;
        pixel.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|(doubleBuffered?PFD_DOUBLEBUFFER:0);pixel.iPixelType=PFD_TYPE_RGBA;pixel.cColorBits=32;
        require(SetPixelFormat(dc,ChoosePixelFormat(dc,&pixel),&pixel),"graphics pixel format");
        bootstrap=wglCreateContext(dc);require(bootstrap&&wglMakeCurrent(dc,bootstrap),"bootstrap graphics context");
        using Create=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
        auto create=reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));require(create!=nullptr,"context extension");
        const int attributes[]{0x2091,4,0x2092,minorVersion,0x9126,compatibility?2:1,0};context=create(dc,nullptr,attributes);
        require(context&&wglMakeCurrent(dc,context),"OpenGL 4.3 context");wglDeleteContext(bootstrap);bootstrap=nullptr;
        glewExperimental=GL_TRUE;require(glewInit()==GLEW_OK,"load graphics functions");
        while(glGetError()!=GL_NO_ERROR){}
    }
    ~Graphics(){wglMakeCurrent(nullptr,nullptr);if(context)wglDeleteContext(context);if(bootstrap)wglDeleteContext(bootstrap);
        if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);UnregisterClassW(L"ChloriumIsolatedCorners",GetModuleHandleW(nullptr));}
};
GLuint shaderFromFile(const std::filesystem::path& path,GLenum stage) {
    std::ifstream input(path);require(bool(input),"read corner shader");
    std::string text((std::istreambuf_iterator<char>(input)),{});require(text.size()<64*1024,"shader budget");
    const auto shader=glCreateShader(stage);require(shader!=0,"create corner shader");
    const char* pointer=text.c_str();glShaderSource(shader,1,&pointer,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::cerr<<log<<'\n';glDeleteShader(shader);throw std::runtime_error("compile corner shader");}
    return shader;
}
GLuint program(const std::filesystem::path& path,GLenum stage=0x91b9,bool feedback=false,const std::filesystem::path& fragment={}) {
    const auto shader=shaderFromFile(path,stage);
    auto value=glCreateProgram();glAttachShader(value,shader);
    if(!fragment.empty()){const auto frag=shaderFromFile(fragment,GL_FRAGMENT_SHADER);glAttachShader(value,frag);glDeleteShader(frag);}
    if(feedback){const char* varying[]{"nativePosition","nativeColor","nativeUV"};glTransformFeedbackVaryings(value,3,varying,GL_INTERLEAVED_ATTRIBS);}
    glLinkProgram(value);glDeleteShader(shader);GLint ok=0;glGetProgramiv(value,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096]{};glGetProgramInfoLog(value,sizeof(log),nullptr,log);std::cerr<<log<<'\n';glDeleteProgram(value);throw std::runtime_error("link corner shader");}
    return value;
}
}
