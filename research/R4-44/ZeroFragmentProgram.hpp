#pragma once
#ifndef __glew_h__
#include <GL/glew.h>
#endif
#include <windows.h>
#include <array>
#include <string>
#include <string_view>
#include <xmmintrin.h>

namespace chlorium::research::zerofragment {
class Program {
    struct CpuState {
        unsigned fp=_mm_getcsr();DWORD error=GetLastError();bool restoreOnExit=true;
        void restore()const noexcept{_mm_setcsr(fp);SetLastError(error);}
        void handoff()noexcept{restore();restoreOnExit=false;}
        ~CpuState(){if(restoreOnExit)restore();}
    };
    GLuint program=0,original=0;HGLRC context=nullptr;
    GLint sourceMatrix=-1,sourceSampler=-1,targetMatrix=-1,targetSampler=-1;
public:
    const char* gate="disabled";
    Program()=default;Program(const Program&)=delete;Program& operator=(const Program&)=delete;
    ~Program(){close();}
    void close()noexcept{CpuState cpu;if(program&&context==wglGetCurrentContext())glDeleteProgram(program);program=original=0;context=nullptr;}
    bool prepare(GLuint source){
        CpuState cpu;
        if(program)return source==original&&context==wglGetCurrentContext();
        gate="source_shader";if(!source||!glIsProgram(source))return false;
        std::array<GLuint,8> attached{};GLsizei count=0;glGetAttachedShaders(source,8,&count,attached.data());if(count!=2)return false;
        GLuint vertex=0;std::string fragment;
        for(int i=0;i<count;++i){GLint type=0;glGetShaderiv(attached[i],GL_SHADER_TYPE,&type);if(type==GL_VERTEX_SHADER)vertex=attached[i];else if(type==GL_FRAGMENT_SHADER){GLint length=0;glGetShaderiv(attached[i],GL_SHADER_SOURCE_LENGTH,&length);if(length<=0||length>65536)return false;fragment.resize(length);GLsizei copied=0;glGetShaderSource(attached[i],length,&copied,fragment.data());if(copied<=0||copied>=length)return false;fragment.resize(copied);}else return false;}
        constexpr std::string_view expression="gl_FragColor = v_fragmentColor * texture2D(CC_Texture0, v_texCoord);";
        const auto at=fragment.find(expression);if(!vertex||at==std::string::npos||fragment.find(expression,at+expression.size())!=std::string::npos)return false;
        fragment.replace(at,expression.size(),"vec4 chloriumColor=v_fragmentColor * texture2D(CC_Texture0, v_texCoord);\nif(all(equal(chloriumColor,vec4(0.0))))discard;\ngl_FragColor=chloriumColor;");
        const auto shader=glCreateShader(GL_FRAGMENT_SHADER);if(!shader){gate="fragment_name";return false;}const char* text=fragment.c_str();const auto length=static_cast<GLint>(fragment.size());glShaderSource(shader,1,&text,&length);glCompileShader(shader);GLint good=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&good);if(!good){glDeleteShader(shader);gate="fragment_compile";return false;}
        program=glCreateProgram();context=wglGetCurrentContext();if(!program){glDeleteShader(shader);gate="program_name";return false;}
        glAttachShader(program,vertex);glAttachShader(program,shader);glDeleteShader(shader);
        const std::array<const char*,3> names{"a_position","a_color","a_texCoord"};for(unsigned i=0;i<names.size();++i)glBindAttribLocation(program,i,names[i]);glLinkProgram(program);glGetProgramiv(program,GL_LINK_STATUS,&good);if(!good){close();gate="program_link";return false;}
        sourceMatrix=glGetUniformLocation(source,"CC_MVPMatrix");sourceSampler=glGetUniformLocation(source,"CC_Texture0");targetMatrix=glGetUniformLocation(program,"CC_MVPMatrix");targetSampler=glGetUniformLocation(program,"CC_Texture0");
        if(sourceMatrix<0||sourceSampler<0||targetMatrix<0||targetSampler<0){close();gate="native_uniforms";return false;}
        for(unsigned i=0;i<names.size();++i)if(glGetAttribLocation(source,names[i])!=int(i)||glGetAttribLocation(program,names[i])!=int(i)){close();gate="native_attributes";return false;}
        original=source;gate="ready";return true;
    }
    template<class NativeDraw>
    bool withNativeDraw(NativeDraw&& native){
        CpuState cpu;
        if(!program||context!=wglGetCurrentContext())return false;
        GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);if(unsigned(current)!=original){gate="changed_native_program";return false;}
        std::array<GLfloat,16> matrix{};GLint sampler=-1;glGetUniformfv(original,sourceMatrix,matrix.data());glGetUniformiv(original,sourceSampler,&sampler);if(sampler!=0){gate="changed_native_sampler";return false;}
        struct Restore{GLuint source;~Restore(){CpuState nativeEffects;glUseProgram(source);}}restore{original};
        glUseProgram(program);glUniformMatrix4fv(targetMatrix,1,GL_FALSE,matrix.data());glUniform1i(targetSampler,sampler);
        GLint bound=0;glGetIntegerv(GL_CURRENT_PROGRAM,&bound);if(unsigned(bound)!=program){gate="owned_program_not_bound";return false;}
        cpu.handoff();native();gate="native_called";return true;
    }
    bool draw(GLuint indices,unsigned count,unsigned first){
        CpuState cpu;
        if(!indices||!count||count>16384||first>16384-count)return false;
        GLint element=0;glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&element);
        cpu.handoff();const bool drawn=withNativeDraw([&]{glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices);glDrawElements(GL_TRIANGLES,count*6,GL_UNSIGNED_SHORT,reinterpret_cast<void*>(std::size_t(first)*12));glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,element);});
        if(drawn)gate="drawn";return drawn;
    }
};
}
