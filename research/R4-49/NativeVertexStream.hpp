#pragma once
#include <bit>

namespace chlorium::research::resident {
// Owned native-layout quads persist by slot; an exact mask selects GPU writers.
// A separate GPU index stream preserves painter order through the native program.
class NativeVertexStream {
    GLuint program_=0,indexProgram_=0,buffer_=0,indices_=0,changesBuffer_=0,vao_=0;
    std::shared_ptr<gl::ContextEpoch<>> epoch_;
    std::size_t capacity_=0,outputCount_=0;
    std::vector<std::uint64_t> dirtyWords_;
    std::vector<std::array<std::uint32_t,4>> changes_;
    bool quadsDirty_=false;
    bool orderDirty_=true,ready_=false;
    using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);
    Dispatch dispatch_=nullptr;
    GLint indexCount_=-1;
    struct State {
        GLint program=0,vao=0,array=0,storage=0;
        std::array<GLStateSnapshot::IndexedBufferBinding,2> outputs{};
        State(){
            glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
            glGetIntegerv(GLStateSnapshot::storageBinding,&storage);
            for(unsigned n=0;n<2;++n)outputs[n]=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,4+n);
        }
        ~State(){
            for(unsigned n=0;n<2;++n)GLStateSnapshot::restoreIndexed(GLStateSnapshot::storageTarget,4+n,outputs[n]);glBindBuffer(GLStateSnapshot::storageTarget,storage);
            glBindBuffer(GL_ARRAY_BUFFER,array);glUseProgram(program);glBindVertexArray(vao);
        }
    };
    static GLuint link(std::string_view source){
        const auto shader=glCreateShader(0x91b9);if(!shader)return 0;
        const char* text=source.data();const auto size=GLint(source.size());glShaderSource(shader,1,&text,&size);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){glDeleteShader(shader);return 0;}
        const auto program=glCreateProgram();if(!program){glDeleteShader(shader);return 0;}
        glAttachShader(program,shader);
        glLinkProgram(program);glDeleteShader(shader);glGetProgramiv(program,GL_LINK_STATUS,&ok);
        if(!ok){glDeleteProgram(program);return 0;}return program;
    }
public:
    struct Metrics {std::uint64_t quadPasses=0,indexPasses=0,reuses=0,slots=0,lastSlots=0;};
    Metrics metrics;
    NativeVertexStream()=default;
    NativeVertexStream(const NativeVertexStream&)=delete;
    NativeVertexStream& operator=(const NativeVertexStream&)=delete;
    ~NativeVertexStream(){close();}
    void close()noexcept{
        if(epoch_ && epoch_->isCurrent()){
            if(program_)glDeleteProgram(program_);if(indexProgram_)glDeleteProgram(indexProgram_);
            if(buffer_)glDeleteBuffers(1,&buffer_);if(indices_)glDeleteBuffers(1,&indices_);
            if(vao_)glDeleteVertexArrays(1,&vao_);if(changesBuffer_)glDeleteBuffers(1,&changesBuffer_);
        }
        program_=indexProgram_=buffer_=indices_=changesBuffer_=vao_=0;capacity_=outputCount_=0;epoch_.reset();
        dirtyWords_.clear();changes_.clear();quadsDirty_=false;orderDirty_=true;ready_=false;dispatch_=nullptr;indexCount_=-1;
    }
    bool initialize(std::size_t capacity,std::string_view source){
        GLboolean active=GL_FALSE;glGetBooleanv(0x8e24,&active);
        dispatch_=gl::currentProc<Dispatch>("glDispatchCompute");
        if(epoch_ || active || !capacity || capacity>16384 || !glMemoryBarrier || !dispatch_)return false;
        // Only this pinned graph has the reviewed native corner arithmetic.
        if(source.size()!=2056 || sourceHash(source)!=0x59251a3c19c8d368ull)return false;
        std::string slotSource(source);
        auto replace=[&](std::string_view before,std::string_view after){const auto at=slotSource.find(before);
            if(at==std::string::npos || slotSource.find(before,at+before.size())!=std::string::npos)return false;
            slotSource.replace(at,before.size(),after);return true;};
        if(!replace("out vec3 nativePosition;","vec3 nativePosition;") ||
           !replace("flat out uint nativeColor;","uint nativeColor;") ||
           !replace("flat out uvec2 nativeUV;","uvec2 nativeUV;") ||
           !replace("out vec4 renderColor;","") || !replace("out vec2 texCoord;","") || !replace("uniform mat4 CC_MVPMatrix;","") ||
           !replace("void main() {",R"GLSL(layout(local_size_x=64) in;
layout(std430,binding=4) writeonly buffer OutputQuads {uint outputWords[];};
layout(std430,binding=5) readonly buffer Changes {uvec4 changes[];};
void main() {)GLSL") ||
           !replace("uint i=slots[uint(gl_VertexID)/4u],corner=uint(gl_VertexID)%4u;",R"GLSL(uvec4 change=changes[gl_WorkGroupID.x];uint lane=gl_LocalInvocationID.x;
    uint mask=lane<32u?change.y:change.z;if((mask&(1u<<(lane&31u)))==0u)return;
    uint i=change.x+lane;)GLSL") ||
           !replace("Dynamic p=parameters[i];Geometry g=geometry[i];","Dynamic p=parameters[i];Geometry g=geometry[i];\n    for(uint corner=0u;corner<4u;++corner){") ||
           !replace("renderColor=unpackUnorm4x8(nativeColor);texCoord=uintBitsToFloat(nativeUV);\n    gl_Position=CC_MVPMatrix*vec4(nativePosition,1);",R"GLSL(uint at=i*24u+corner*6u;
    outputWords[at]=floatBitsToUint(nativePosition.x);outputWords[at+1u]=floatBitsToUint(nativePosition.y);outputWords[at+2u]=floatBitsToUint(nativePosition.z);
    outputWords[at+3u]=nativeColor;outputWords[at+4u]=nativeUV.x;outputWords[at+5u]=nativeUV.y;
    })GLSL"))return false;
        State saved;epoch_=gl::ContextEpoch<>::capture();if(!epoch_)return false;
        program_=link(slotSource);
        constexpr std::string_view indexSource=R"GLSL(#version 430 core
layout(local_size_x=64) in;
layout(std430,binding=2) readonly buffer Order {uint slots[];};
layout(std430,binding=4) writeonly buffer Indices {uint indices[];};
uniform uint count;
void main(){uint n=gl_GlobalInvocationID.x;if(n>=count)return;uint base=slots[n]*4u;uint outAt=n*6u;
indices[outAt]=base;indices[outAt+1u]=base+1u;indices[outAt+2u]=base+2u;
indices[outAt+3u]=base+3u;indices[outAt+4u]=base+2u;indices[outAt+5u]=base+1u;}
)GLSL";
        indexProgram_=link(indexSource);if(!program_ || !indexProgram_)return false;
        indexCount_=glGetUniformLocation(indexProgram_,"count");if(indexCount_<0)return false;
        glGenBuffers(1,&buffer_);glGenBuffers(1,&indices_);glGenBuffers(1,&changesBuffer_);glGenVertexArrays(1,&vao_);
        if(!buffer_ || !indices_ || !vao_ || !changesBuffer_)return false;
        glBindBuffer(GL_ARRAY_BUFFER,changesBuffer_);glBufferData(GL_ARRAY_BUFFER,((capacity+63)/64)*16,nullptr,GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,indices_);glBufferData(GL_ARRAY_BUFFER,capacity*6*sizeof(std::uint32_t),nullptr,GL_DYNAMIC_COPY);
        glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferData(GL_ARRAY_BUFFER,capacity*sizeof(NativeQuadState),nullptr,GL_DYNAMIC_COPY);
        glBindVertexArray(vao_);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);
        glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));
        for(unsigned n=0;n<3;++n)glEnableVertexAttribArray(n);
        if(glGetError()!=GL_NO_ERROR)return false;
        capacity_=capacity;dirtyWords_.assign((capacity+63)/64,0);changes_.reserve(dirtyWords_.size());metrics={};return true;
    }
    bool publishChanges(std::span<const std::uint64_t> mask,bool seed,bool orderChanged)noexcept{
        if(!capacity_)return true;
        const auto tail=capacity_%64;const auto lastMask=tail?((std::uint64_t{1}<<tail)-1):UINT64_MAX;
        if(mask.size()!=dirtyWords_.size() || (mask.back()&~lastMask))return false;
        for(std::size_t n=0;n<mask.size();++n){const auto bits=seed?(n+1==mask.size()?lastMask:UINT64_MAX):mask[n];dirtyWords_[n]|=bits;quadsDirty_|=bits!=0;}
        orderDirty_|=orderChanged;return true;
    }
    bool materialize(std::size_t count){
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timing(residenttiming::Stage::materialize);
#endif
        metrics.lastSlots=0;
        if(!capacity_ || count>capacity_ || !epoch_ || !epoch_->isCurrent() || glIsEnabled(GL_RASTERIZER_DISCARD))return false;
        GLboolean active=GL_FALSE;glGetBooleanv(0x8e24,&active);if(active)return false;
        if(ready_ && !quadsDirty_ && !orderDirty_ && outputCount_==count){++metrics.reuses;return true;}
        if(!ready_ && !quadsDirty_)return false;
        State saved;std::uint64_t slots=0;
        // Order SSBO overwrites after previous vertex/index fetches and shader writes.
        if(ready_)glMemoryBarrier(0x00002000);
        if(quadsDirty_){
            changes_.clear();
            for(std::size_t n=0;n<dirtyWords_.size();++n)if(const auto bits=dirtyWords_[n]){
                changes_.push_back({std::uint32_t(n*64),std::uint32_t(bits),std::uint32_t(bits>>32),0});slots+=std::popcount(bits);
            }
            glUseProgram(program_);glBindBuffer(GLStateSnapshot::storageTarget,changesBuffer_);
            glBufferSubData(GLStateSnapshot::storageTarget,0,changes_.size()*16,changes_.data());
            glBindBufferBase(GLStateSnapshot::storageTarget,4,buffer_);glBindBufferBase(GLStateSnapshot::storageTarget,5,changesBuffer_);
            if(glGetError()!=GL_NO_ERROR)return false;
            dispatch_(GLuint(changes_.size()),1,1);glMemoryBarrier(0x00000001);
        }
        const bool indexUpdate=orderDirty_ || outputCount_!=count;
        if(indexUpdate && count){
            glUseProgram(indexProgram_);glUniform1ui(indexCount_,GLuint(count));
            glBindBufferBase(GLStateSnapshot::storageTarget,4,indices_);dispatch_(GLuint((count+63)/64),1,1);glMemoryBarrier(0x00000002);
        }
        if(glGetError()!=GL_NO_ERROR)return false;
        std::fill(dirtyWords_.begin(),dirtyWords_.end(),0);quadsDirty_=false;orderDirty_=false;ready_=true;outputCount_=count;
        metrics.quadPasses+=slots!=0;metrics.indexPasses+=indexUpdate&&count;metrics.slots+=slots;metrics.lastSlots=slots;return true;
    }
    GLuint vao()const noexcept{return capacity_?vao_:0;}
    // Owned diagnostic download only; never publishes into a native sprite.
    bool copyOwnedQuads(std::span<NativeQuadState> out){
        if(!ready_ || quadsDirty_ || out.size()>capacity_ || !epoch_ || !epoch_->isCurrent())return false;
        State saved;glMemoryBarrier(0x00000200);glBindBuffer(GLStateSnapshot::storageTarget,buffer_);
        glGetBufferSubData(GLStateSnapshot::storageTarget,0,out.size_bytes(),out.data());return glGetError()==GL_NO_ERROR;
    }
};
}
