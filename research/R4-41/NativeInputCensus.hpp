#pragma once
#include <Geode/Geode.hpp>
#include "../R4-32/NativePassCapture.hpp"
#include "../R4-43/NativeBatchReplay.hpp"
#include "../R4-44/ZeroFragmentProgram.hpp"
#include "../R4-46/NativeHandoffContract.hpp"
#include "../R4-49/NativeReplay.hpp"
#include <bit>
#include <array>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#include <utility>

extern "C" __declspec(dllexport) const char chloriumOpaqueInputCensusIdentity[]="CHLORIUM_OPAQUE_INPUT_CENSUS_R41";
extern "C" __declspec(dllexport) const char chloriumNativeBatchPixelIdentity[]="CHLORIUM_NATIVE_BATCH_PIXEL_PAIR_R41";
extern "C" __declspec(dllexport) const char chloriumNativeBatchReplayIdentity[]="CHLORIUM_NATIVE_BATCH_REPLAY_R43";
extern "C" __declspec(dllexport) const char chloriumZeroFragmentIdentity[]="CHLORIUM_ZERO_FRAGMENT_DISCARD_R44";
extern "C" __declspec(dllexport) const char chloriumNativeHandoffIdentity[]="CHLORIUM_ZERO_NATIVE_HANDOFF_R46";
extern "C" __declspec(dllexport) const char chloriumResidentNativeReplayIdentity[]="CHLORIUM_RESIDENT_NATIVE_REPLAY_R49";
extern "C" __declspec(dllexport) const char chloriumResidentNativeCacheIdentity[]="CHLORIUM_RESIDENT_NATIVE_CACHE_R50";
extern "C" __declspec(dllexport) const char chloriumResidentNativeStreamIdentity[]="CHLORIUM_RESIDENT_NATIVE_STREAM_R51";
extern "C" __declspec(dllexport) const char chloriumResidentNativeSlotCacheIdentity[]="CHLORIUM_RESIDENT_NATIVE_SLOT_CACHE_R52";
extern "C" __declspec(dllexport) const char chloriumResidentExactUpdatesIdentity[]="CHLORIUM_RESIDENT_EXACT_GPU_UPDATES_R53";
extern "C" __declspec(dllexport) const char chloriumResidentOverwriteBarrierIdentity[]="CHLORIUM_RESIDENT_OVERWRITE_BARRIER_R56";
namespace chlorium::research::opaqueinputs {
class Census {
    struct Texture {
        std::uintptr_t owner=0;unsigned uid=0,name=0;int width=0,height=0,alphaBits=0;
        unsigned seenFrames=0;bool stable=true;std::vector<unsigned char> rgba;
    };
    bool enabled=false,written=false,failed=false,pixelEnabled=false,replayEnabled=false,zeroEnabled=false,handoffEnabled=false,residentEnabled=false;
    batchreplay::Replay replay;
    zerofragment::Program zeroProgram;
    unsigned completedHandoffCalls=0;
    resident::NativeReplay residentReplay;
    unsigned completedResidentCalls=0,completedReconstructed=0;
    bool residentSeedAttempted=false,residentSeedReady=false;
    bool producerReplayReady=false;
    unsigned producerReplayCount=0;
    std::uintptr_t residentAtlas=0;
    unsigned residentAtlasId=0;
    struct PixelPair {
        bool attempted=false,valid=false;
        unsigned ordinal=0,frame=0;
        GLint framebuffer=0,program=0;
        std::array<GLint,4> viewport{};
        std::vector<unsigned char> before,after;
        std::vector<unsigned char> vertices,indices,binary;
        GLenum binaryFormat=0;
        matjson::Value metadata=matjson::Value::object();
    } pixel;
    std::vector<Texture> textures;
    std::size_t bytes=0;
    matjson::Value rows=matjson::Value::array(),programs=matjson::Value::array();
    static constexpr std::size_t maxBytes=32*1024*1024;
    static int integer(GLenum parameter){GLint value=0;glGetIntegerv(parameter,&value);return value;}
    static int textureInteger(GLenum parameter){GLint value=0;glGetTexParameteriv(GL_TEXTURE_2D,parameter,&value);return value;}
    static int levelInteger(GLenum parameter){GLint value=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,parameter,&value);return value;}
    static std::vector<unsigned char> buffer(GLuint name,std::size_t offset,std::size_t count){
        if(!name||!GLEW_VERSION_3_1||!glIsBuffer(name)||count>8*1024*1024)return {};
        const auto old=integer(GL_COPY_READ_BUFFER);struct Restore{GLint old;~Restore(){glBindBuffer(GL_COPY_READ_BUFFER,old);}}restore{old};
        glBindBuffer(GL_COPY_READ_BUFFER,name);GLint size=0;glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);
        if(size<0||offset>std::size_t(size)||count>std::size_t(size)-offset)return {};
        std::vector<unsigned char> result(count);glGetBufferSubData(GL_COPY_READ_BUFFER,offset,count,result.data());return result;
    }
    static bool packSafe(){
        if((GLEW_VERSION_2_1||GLEW_ARB_pixel_buffer_object)&&integer(0x88ed)!=0)return false;
        for(auto parameter:{GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_IMAGE_HEIGHT,GL_PACK_SKIP_IMAGES})if(integer(parameter))return false;
        const auto alignment=integer(GL_PACK_ALIGNMENT);return alignment==1||alignment==2||alignment==4;
    }
    static bool positionOnlyProgram(cocos2d::CCTextureAtlas* atlas){
        const auto current=integer(GL_CURRENT_PROGRAM);if(!current)return false;
        std::array<GLuint,8> shaders{};GLsizei count=0;glGetAttachedShaders(current,8,&count,shaders.data());if(count!=2)return false;
        unsigned accepted=0;
        for(int i=0;i<count;++i){GLint length=0,type=0;glGetShaderiv(shaders[i],GL_SHADER_SOURCE_LENGTH,&length);glGetShaderiv(shaders[i],GL_SHADER_TYPE,&type);if(length<=0||length>65536)return false;
            std::string source(length,'\0');GLsizei copied=0;glGetShaderSource(shaders[i],length,&copied,source.data());if(copied<=0||copied>=length)return false;source.resize(copied);
            std::uint64_t hash=14695981039346656037ull;for(unsigned char byte:source)hash=(hash^byte)*1099511628211ull;
            if(type==GL_VERTEX_SHADER&&hash==0xa2318967aee60148ull)accepted|=1;else if(type==GL_FRAGMENT_SHADER&&hash==0xa9c8e0b64e142972ull)accepted|=2;else return false;
        }if(accepted!=3)return false;
        const auto location=glGetUniformLocation(current,"CC_MVPMatrix");if(location<0)return false;std::array<float,16> matrix{};glGetUniformfv(current,location,matrix.data());for(auto value:matrix)if((std::bit_cast<std::uint32_t>(value)&0x7fffffffu)>0x4f800000u)return false;
        const std::array<const char*,3> names{"a_position","a_color","a_texCoord"};
        for(unsigned i=0;i<3;++i){if(glGetAttribLocation(current,names[i])!=int(i))return false;GLint enabled=0,size=0,type=0,normalized=0,stride=0,buffer=0;void* pointer=nullptr;
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&type);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&normalized);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);
            if(!enabled||size!=(i==0?3:i==1?4:2)||type!=(i==1?GL_UNSIGNED_BYTE:GL_FLOAT)||normalized!=(i==1)||stride!=24||unsigned(buffer)!=atlas->m_pBuffersVBO[0]||reinterpret_cast<std::uintptr_t>(pointer)!=(i==0?0:i==1?12:16))return false;
        }return true;
    }
    static bool handoffContract(cocos2d::CCTextureAtlas* atlas){
        const auto module=GetModuleHandleW(L"libcocos2d.dll");if(!handoff::nativeAtlasBody(module)||!handoff::standardTexture(module,atlas->m_pTexture))return false;
        const auto base=reinterpret_cast<std::uintptr_t>(module);
        for(const auto mod:geode::Loader::get()->getAllMods()){
            if(mod==geode::Mod::get())continue;
            if(mod->isLoaded()&&mod->getID()!="geode.node-ids"&&mod->getID()!="geode.loader")return false;
            for(const auto hook:mod->getHooks())if(hook->isEnabled()&&(hook->getAddress()==base+0xa6350||hook->getAddress()==base+0x10760||hook->getAddress()==base+0x87fe0||hook->getAddress()==base+0x881e0))return false;
        }return true;
    }
    unsigned texture(cocos2d::CCTexture2D* source,unsigned frame,matjson::Value& row){
        const unsigned name=source->getName(),uid=source->m_uID;const auto owner=reinterpret_cast<std::uintptr_t>(source);
        const int width=levelInteger(GL_TEXTURE_WIDTH),height=levelInteger(GL_TEXTURE_HEIGHT),alpha=levelInteger(GL_TEXTURE_ALPHA_SIZE);
        row["width"]=width;row["height"]=height;row["alpha_bits"]=alpha;row["internal_format"]=levelInteger(GL_TEXTURE_INTERNAL_FORMAT);
        if(width<=0||height<=0||width>4096||height>4096||alpha>8||!packSafe())return UINT_MAX;
        const auto size=std::size_t(width)*height*4;if(size>maxBytes)return UINT_MAX;
        for(unsigned i=0;i<textures.size();++i){auto& entry=textures[i];if(entry.owner!=owner||entry.uid!=uid||entry.name!=name)continue;
            if(entry.width!=width||entry.height!=height||entry.alphaBits!=alpha){entry.stable=false;return i;}
            if(!(entry.seenFrames&(1u<<frame))){
                // Zero poison is conservative if a driver read fails; never reuse old opaque bytes.
                std::vector<unsigned char> second(size);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,second.data());
                entry.stable&=entry.rgba==second;entry.seenFrames|=1u<<frame;
            }return i;
        }
        if(textures.size()>=8||size>maxBytes-bytes)return UINT_MAX;
        Texture entry;entry.owner=owner;entry.uid=uid;entry.name=name;entry.width=width;entry.height=height;entry.alphaBits=alpha;entry.seenFrames=1u<<frame;entry.rgba.resize(size);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,entry.rgba.data());bytes+=size;textures.push_back(std::move(entry));return static_cast<unsigned>(textures.size()-1);
    }
    void program(GLuint name,unsigned frame,matjson::Value& row){
        if(!name||!glIsProgram(name)||!glGetAttachedShaders||!glGetActiveUniform)return;
        GLint linked=0,uniforms=0,maxName=0;glGetProgramiv(name,GL_LINK_STATUS,&linked);glGetProgramiv(name,GL_ACTIVE_UNIFORMS,&uniforms);glGetProgramiv(name,GL_ACTIVE_UNIFORM_MAX_LENGTH,&maxName);
        row["program_linked"]=bool(linked);if(!linked||uniforms<0||uniforms>512||maxName<0||maxName>1024)return;
        std::vector<char> text(std::max(maxName,1));
        matjson::Value names=matjson::Value::array();
        for(int i=0;i<uniforms;++i){GLsizei length=0;GLint size=0;GLenum type=0;glGetActiveUniform(name,i,static_cast<GLsizei>(text.size()),&length,&size,&type,text.data());const std::string_view key(text.data(),length);names.push(std::string(key));
            if(key=="CC_MVPMatrix"&&size==1&&type==GL_FLOAT_MAT4){const auto location=glGetUniformLocation(name,text.data());if(location>=0){std::array<float,16> matrix{};glGetUniformfv(name,location,matrix.data());matjson::Value bits=matjson::Value::array();for(auto value:matrix)bits.push(std::bit_cast<std::uint32_t>(value));row["mvp_bits"]=std::move(bits);}}
            if(key=="CC_Texture0"&&size==1&&type==GL_SAMPLER_2D){const auto location=glGetUniformLocation(name,text.data());if(location>=0){GLint unit=-1;glGetUniformiv(name,location,&unit);row["sampler_unit"]=unit;}}
        }
        row["active_uniforms"]=std::move(names);
        for(const auto& prior:programs)if(prior["name"].asUInt().unwrapOr(UINT_MAX)==name&&prior["frame"].asUInt().unwrapOr(UINT_MAX)==frame)return;
        if(programs.size()>=64)return;
        matjson::Value definition=matjson::Value::object();definition["name"]=name;definition["frame"]=frame;
        std::array<GLuint,8> attached{};GLsizei count=0;glGetAttachedShaders(name,static_cast<GLsizei>(attached.size()),&count,attached.data());matjson::Value sources=matjson::Value::array();
        for(GLsizei i=0;i<count;++i){GLint length=0,type=0;glGetShaderiv(attached[i],GL_SHADER_SOURCE_LENGTH,&length);glGetShaderiv(attached[i],GL_SHADER_TYPE,&type);if(length<=0||length>65536)continue;
            std::string source(length,'\0');GLsizei copied=0;glGetShaderSource(attached[i],length,&copied,source.data());source.resize(copied);matjson::Value item=matjson::Value::object();item["type"]=type;item["source"]=std::move(source);sources.push(std::move(item));
        }
        definition["attached_sources_not_linked_binary_proof"]=std::move(sources);programs.push(std::move(definition));
    }
public:
    void configure()noexcept{const auto flag=std::getenv("CHLORIUM_OPAQUE_INPUT_CENSUS");enabled=flag&&std::string_view(flag)=="1";const auto pixels=std::getenv("CHLORIUM_OPAQUE_PIXEL_PAIR");pixelEnabled=pixels&&std::string_view(pixels)=="1";const auto replayFlag=std::getenv("CHLORIUM_NATIVE_BATCH_REPLAY");replayEnabled=replayFlag&&std::string_view(replayFlag)=="1";const auto zeroFlag=std::getenv("CHLORIUM_ZERO_FRAGMENT_DISCARD");zeroEnabled=zeroFlag&&std::string_view(zeroFlag)=="1";const auto handoffFlag=std::getenv("CHLORIUM_ZERO_NATIVE_HANDOFF");handoffEnabled=handoffFlag&&std::string_view(handoffFlag)=="1";const auto residentFlag=std::getenv("CHLORIUM_RESIDENT_NATIVE_REPLAY");residentEnabled=residentFlag&&std::string_view(residentFlag)=="1";const auto streamFlag=std::getenv("CHLORIUM_RESIDENT_NATIVE_STREAM");residentReplay.requestNativeStream(streamFlag&&std::string_view(streamFlag)=="1");}
    void before(cocos2d::CCTextureAtlas* atlas,unsigned frame,unsigned ordinal,unsigned count,cocos2d::CCGLProgram* expected)noexcept{
        if(!enabled||!pixelEnabled||failed||pixel.attempted||frame!=(residentEnabled?2u:1u)||count<256||!GLEW_VERSION_3_0)return;
        if(residentEnabled && (!residentSeedReady || reinterpret_cast<std::uintptr_t>(atlas)!=residentAtlas || atlas->m_uID!=residentAtlasId))return;
        pass::PreserveFP fp;const auto error=GetLastError();struct Restore{DWORD error;~Restore(){SetLastError(error);}}restore{error};
        try{
            pixel.metadata["gate"]="basic_draw_state";
            if(!packSafe()||integer(GL_SAMPLES)||glIsEnabled(GL_DEPTH_TEST)||glIsEnabled(GL_STENCIL_TEST)||glIsEnabled(GL_COLOR_LOGIC_OP)||glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)||glIsEnabled(GL_RASTERIZER_DISCARD))return;
            pixel.metadata["gate"]="blend";
            if(!glIsEnabled(GL_BLEND)||integer(GL_BLEND_SRC_RGB)!=GL_ONE||integer(GL_BLEND_DST_RGB)!=GL_ONE_MINUS_SRC_ALPHA||integer(GL_BLEND_SRC_ALPHA)!=GL_ONE||integer(GL_BLEND_DST_ALPHA)!=GL_ONE_MINUS_SRC_ALPHA||integer(GL_BLEND_EQUATION_RGB)!=GL_FUNC_ADD||integer(GL_BLEND_EQUATION_ALPHA)!=GL_FUNC_ADD)return;
            pixel.metadata["gate"]="framebuffer_and_program";
            if(integer(0x8ca6)!=integer(0x8caa)||integer(GL_CURRENT_PROGRAM)!=GLint(expected?expected->getProgram():0))return;
            const bool core=GLEW_VERSION_3_2&&(integer(0x9126)&1);
            pixel.metadata["legacy_alpha_test"]=!core&&glIsEnabled(GL_ALPHA_TEST);pixel.metadata["framebuffer_srgb"]=bool(glIsEnabled(0x8db9));
            if(!core){pixel.metadata["alpha_func"]=integer(GL_ALPHA_TEST_FUNC);GLfloat ref=0;glGetFloatv(GL_ALPHA_TEST_REF,&ref);pixel.metadata["alpha_ref_bits"]=std::bit_cast<std::uint32_t>(ref);}else{pixel.metadata["alpha_func"]=GL_ALWAYS;pixel.metadata["alpha_ref_bits"]=0;}
            std::array<GLboolean,4> channels{};glGetBooleanv(GL_COLOR_WRITEMASK,channels.data());matjson::Value masks=matjson::Value::array();for(auto value:channels)masks.push(bool(value));pixel.metadata["color_mask"]=std::move(masks);
            pixel.metadata["gate"]="polygon";
            std::array<GLint,2> polygon{};glGetIntegerv(GL_POLYGON_MODE,polygon.data());if(polygon[0]!=GL_FILL||polygon[1]!=GL_FILL)return;
            pixel.metadata["gate"]="clip_planes";
            const auto clips=integer(0x0d32);if(clips<0||clips>8)return;for(int i=0;i<clips;++i)if(glIsEnabled(0x3000+i))return;
            pixel.metadata["gate"]="viewport";
            glGetIntegerv(GL_VIEWPORT,pixel.viewport.data());const auto& v=pixel.viewport;
            if(v[0]||v[1]||v[2]<=0||v[3]<=0||v[2]>1920||v[3]>1080||std::size_t(v[2])*v[3]*8>16*1024*1024)return;
            pixel.metadata["gate"]="color_bits";
            const auto r=integer(GL_RED_BITS),g=integer(GL_GREEN_BITS),b=integer(GL_BLUE_BITS),a=integer(GL_ALPHA_BITS);
            pixel.metadata["red_bits"]=r;pixel.metadata["green_bits"]=g;pixel.metadata["blue_bits"]=b;pixel.metadata["alpha_bits"]=a;
            if(r!=8||g!=8||b!=8||(a!=0&&a!=8))return;
            pixel.framebuffer=integer(0x8ca6);pixel.program=integer(GL_CURRENT_PROGRAM);
            pixel.metadata["gate"]="read_draw_buffer";const auto read=integer(GL_READ_BUFFER),draw=integer(GL_DRAW_BUFFER0);pixel.metadata["read_buffer"]=read;pixel.metadata["draw_buffer"]=draw;
            const auto normalize=[&](int buffer){return pixel.framebuffer==0&&!integer(GL_STEREO)?(buffer==GL_BACK?GL_BACK_LEFT:buffer==GL_FRONT?GL_FRONT_LEFT:buffer):buffer;};
            if(normalize(read)!=normalize(draw))return;
            GLint encoding=0;glGetFramebufferAttachmentParameteriv(0x8ca9,pixel.framebuffer?draw:GL_BACK_LEFT,0x8210,&encoding);pixel.metadata["color_encoding"]=encoding;pixel.metadata["gate"]="color_encoding";if(encoding!=GL_LINEAR&&encoding!=0x8c40)return;
            pixel.attempted=true;pixel.frame=frame;pixel.ordinal=ordinal;pixel.before.resize(std::size_t(v[2])*v[3]*4);pixel.after.resize(pixel.before.size());
            auto& meta=pixel.metadata;meta["frame"]=frame;meta["ordinal"]=ordinal;meta["count"]=count;meta["width"]=v[2];meta["height"]=v[3];meta["alpha_bits"]=a;meta["framebuffer"]=pixel.framebuffer;meta["program"]=pixel.program;meta["dither"]=bool(glIsEnabled(GL_DITHER));meta["cull_face"]=bool(glIsEnabled(GL_CULL_FACE));meta["cull_mode"]=integer(GL_CULL_FACE_MODE);meta["front_face"]=integer(GL_FRONT_FACE);meta["scissor"]=bool(glIsEnabled(GL_SCISSOR_TEST));meta["depth_bits"]=integer(GL_DEPTH_BITS);meta["stencil_bits"]=integer(GL_STENCIL_BITS);
            std::array<GLint,4> scissor{};glGetIntegerv(GL_SCISSOR_BOX,scissor.data());matjson::Value box=matjson::Value::array();for(auto value:scissor)box.push(value);meta["scissor_box"]=std::move(box);
            glReadPixels(0,0,v[2],v[3],GL_RGBA,GL_UNSIGNED_BYTE,pixel.before.data());
            if(replayEnabled)replay.prepare(v[2],v[3],encoding,a,pixel.before);
            pixel.metadata["gate"]="read_before_complete";
        }catch(...){failed=true;}
    }
    template<class Redraw>
    void observe(cocos2d::CCTextureAtlas* atlas,cocos2d::CCGLProgram* expected,unsigned frame,unsigned ordinal,unsigned count,unsigned first,std::span<const pass::SpriteRecord> spriteSources,Redraw&& redraw)noexcept{
        if(!enabled||failed||!count||frame<1||frame>2)return;
        pass::PreserveFP fp;const auto error=GetLastError();struct Restore{DWORD error;~Restore(){SetLastError(error);}}restore{error};
        try{
            if(rows.size()>=4096){failed=true;return;}
            if(residentEnabled && pixelEnabled && replayEnabled && !zeroEnabled && !handoffEnabled &&
               frame==1 && !residentSeedAttempted && count>=256 && !first && positionOnlyProgram(atlas) && handoffContract(atlas)){
                residentSeedAttempted=true;residentAtlas=reinterpret_cast<std::uintptr_t>(atlas);residentAtlasId=atlas->m_uID;
                const auto vertices=buffer(atlas->m_pBuffersVBO[0],0,std::size_t(count)*96);
                const auto indices=buffer(atlas->m_pBuffersVBO[1],0,std::size_t(count)*12);
                residentSeedReady=residentReplay.seed(vertices,indices,spriteSources,expected->getProgram());
            }
            if(pixel.attempted&&!pixel.valid&&frame==pixel.frame&&ordinal==pixel.ordinal){
                std::array<GLint,4> viewport{};glGetIntegerv(GL_VIEWPORT,viewport.data());
                if(viewport==pixel.viewport&&integer(0x8ca6)==pixel.framebuffer&&integer(0x8caa)==pixel.framebuffer&&integer(GL_CURRENT_PROGRAM)==pixel.program&&packSafe()){
                    glReadPixels(0,0,viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_BYTE,pixel.after.data());pixel.valid=pixel.before!=pixel.after;
                    static_assert(offsetof(cocos2d::CCTextureAtlas,m_pBuffersVBO)==0x40);
                    pixel.vertices=buffer(atlas->m_pBuffersVBO[0],std::size_t(first)*96,std::size_t(count)*96);
                    pixel.indices=buffer(atlas->m_pBuffersVBO[1],std::size_t(first)*12,std::size_t(count)*12);
                    pixel.metadata["gpu_vertices_match_cpu"]=pixel.vertices.size()==std::size_t(count)*96&&!std::memcmp(pixel.vertices.data(),atlas->m_pQuads+first,pixel.vertices.size());pixel.metadata["first"]=first;
                    matjson::Value attributes=matjson::Value::array();
                    for(unsigned i=0;i<3;++i){matjson::Value attribute=matjson::Value::object();for(auto [label,pname]:{std::pair{"enabled",GL_VERTEX_ATTRIB_ARRAY_ENABLED},std::pair{"size",GL_VERTEX_ATTRIB_ARRAY_SIZE},std::pair{"type",GL_VERTEX_ATTRIB_ARRAY_TYPE},std::pair{"normalized",GL_VERTEX_ATTRIB_ARRAY_NORMALIZED},std::pair{"stride",GL_VERTEX_ATTRIB_ARRAY_STRIDE},std::pair{"buffer",GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING}}){GLint value=0;glGetVertexAttribiv(i,pname,&value);attribute[label]=value;}void* pointer=nullptr;glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);attribute["offset"]=reinterpret_cast<std::uintptr_t>(pointer);attributes.push(std::move(attribute));}
                    pixel.metadata["vertex_attributes"]=std::move(attributes);
                    if(GLEW_ARB_get_program_binary&&glGetProgramBinary){GLint length=0;glGetProgramiv(pixel.program,GL_PROGRAM_BINARY_LENGTH,&length);if(length>0&&length<=4*1024*1024){pixel.binary.resize(length);GLsizei copied=0;glGetProgramBinary(pixel.program,length,&copied,&pixel.binaryFormat,pixel.binary.data());if(copied>=0&&copied<=length)pixel.binary.resize(copied);else pixel.binary.clear();}}
                    if(replayEnabled&&pixel.valid){const bool contract=first==0&&positionOnlyProgram(atlas);std::function<bool()> alternate;
                        unsigned nativeHandoffCalls=0;
                        if(zeroEnabled&&contract&&!pixel.metadata["legacy_alpha_test"].asBool().unwrapOr(true))alternate=[&]{if(!zeroProgram.prepare(pixel.program))return false;if(handoffEnabled){zeroProgram.gate="native_handoff_contract";if(!handoffContract(atlas))return false;return zeroProgram.withNativeDraw([&]{++nativeHandoffCalls;redraw();});}return zeroProgram.draw(atlas->m_pBuffersVBO[1],count,first);};
                        if(residentEnabled&&contract&&!zeroEnabled&&!handoffEnabled&&!pixel.metadata["legacy_alpha_test"].asBool().unwrapOr(true))alternate=[&]{
                            residentReplay.gate="native_contract";if(!handoffContract(atlas))return false;
                            if(pass::producerReplayRequested()){
                                std::vector<pass::SpriteRecord> packets;std::vector<unsigned char> vertices;
                                residentReplay.gate="producer_packets";
                                if(!pass::producerReplayPackets(spriteSources,pixel.vertices,packets,vertices))return false;
                                producerReplayReady=true;producerReplayCount=unsigned(packets.size());
                                return residentReplay.prepare(vertices,pixel.indices,packets,pixel.program)&&residentReplay.draw();
                            }
                            return residentReplay.prepare(pixel.vertices,pixel.indices,spriteSources,pixel.program)&&residentReplay.draw();
                        };
                        replay.run(pixel.before,pixel.after,pixel.vertices,pixel.indices,count,contract,std::forward<Redraw>(redraw),std::move(alternate));completedHandoffCalls=nativeHandoffCalls;completedResidentCalls=residentReplay.calls;completedReconstructed=residentReplay.reconstructed;zeroProgram.close();residentReplay.close();const auto& r=replay.result;
                        auto result=matjson::Value::object();result["diagnostic_only"]=true;result["game_fps_evidence"]=false;result["gate"]=r.gate;result["attempted"]=r.attempted;result["gl_failure"]=r.glFailure;result["source_shader_contract_checked_not_linked_executable_proof"]=contract;result["baseline_run"]=r.baselineRun;result["candidate_run"]=r.candidateRun;result["quads"]=r.quads;result["removed_point_quads"]=r.removedPoints;result["copy_before_different_bytes"]=r.copiedBeforeDiff;result["copy_after_different_bytes"]=r.copiedAfterDiff;result["restore_before_different_bytes"]=r.restoredBeforeDiff;result["native_baseline_different_bytes"]=r.baselineDiff;result["compacted_different_bytes"]=r.candidateDiff;result["restore_after_different_bytes"]=r.restoredAfterDiff;result["recovered_native"]=r.recoveredNative;result["zero_fragment_requested"]=zeroEnabled;result["zero_fragment_gate"]=zeroProgram.gate;result["zero_fragment_run"]=zeroEnabled&&r.alternateRun;result["zero_fragment_different_bytes"]=zeroEnabled?r.alternateDiff:0;result["native_handoff_requested"]=handoffEnabled;result["native_handoff_calls"]=nativeHandoffCalls;result["resident_requested"]=residentEnabled;result["resident_native_stream_requested"]=residentReplay.nativeStreamRequested();result["producer_packets_requested"]=pass::producerReplayRequested();result["producer_packets_oracle_validated"]=producerReplayReady;result["producer_packets_count"]=producerReplayCount;result["native_snapshot_still_captured"]=true;const auto streamStats=residentReplay.streamMetrics();result["resident_stream_quad_passes"]=streamStats.quadPasses;result["resident_stream_index_passes"]=streamStats.indexPasses;result["resident_stream_cache_reuses"]=streamStats.reuses;result["resident_stream_materialized_slots"]=streamStats.slots;result["resident_stream_update_slots"]=streamStats.lastSlots;result["resident_seed_attempted"]=residentSeedAttempted;result["resident_seed_ready"]=residentSeedReady;result["resident_prepared_frames"]=residentReplay.preparedFrames;result["resident_reused_storage"]=residentReplay.reusedStorage;auto seedBytes=matjson::Value::array(),updateBytes=matjson::Value::array();for(auto n:residentReplay.seedUploads)seedBytes.push(n);for(auto n:residentReplay.updateUploads)updateBytes.push(n);result["resident_seed_upload_bytes_geometry_parameters_order_fallback"]=std::move(seedBytes);result["resident_update_upload_bytes_geometry_parameters_order_fallback"]=std::move(updateBytes);result["resident_gate"]=residentReplay.gate;result["resident_calls"]=completedResidentCalls;result["resident_reconstructed_quads"]=completedReconstructed;result["resident_fallback_quads"]=residentReplay.quads-completedReconstructed;result["resident_run"]=residentEnabled&&r.alternateRun;result["resident_different_bytes"]=residentEnabled?r.alternateDiff:0;pixel.metadata["same_target_replay"]=std::move(result);}
                }
            }
            matjson::Value row=matjson::Value::object();row["frame"]=frame;row["ordinal"]=ordinal;row["count"]=count;
            const auto active=integer(GL_ACTIVE_TEXTURE),bound=integer(GL_TEXTURE_BINDING_2D),current=integer(GL_CURRENT_PROGRAM);
            row["context"]=fmt::format("{:x}",reinterpret_cast<std::uintptr_t>(wglGetCurrentContext()));row["active_texture"]=active;row["texture_name"]=bound;row["program"]=current;
            row["expected_program"]=expected?expected->getProgram():0;row["expected_texture"]=atlas->m_pTexture?atlas->m_pTexture->getName():0;
            row["samples"]=integer(GL_SAMPLES);row["depth_test"]=bool(glIsEnabled(GL_DEPTH_TEST));row["stencil_test"]=bool(glIsEnabled(GL_STENCIL_TEST));
            row["blend"]=bool(glIsEnabled(GL_BLEND));row["blend_src_rgb"]=integer(GL_BLEND_SRC_RGB);row["blend_dst_rgb"]=integer(GL_BLEND_DST_RGB);row["blend_src_alpha"]=integer(GL_BLEND_SRC_ALPHA);row["blend_dst_alpha"]=integer(GL_BLEND_DST_ALPHA);row["blend_equation_rgb"]=integer(GL_BLEND_EQUATION_RGB);row["blend_equation_alpha"]=integer(GL_BLEND_EQUATION_ALPHA);
            row["sampler_binding"]=(GLEW_VERSION_3_3||GLEW_ARB_sampler_objects)?integer(0x8919):0;
            const auto glVersion=glGetString(GL_VERSION),renderer=glGetString(GL_RENDERER);row["gl_version"]=glVersion?reinterpret_cast<const char*>(glVersion):"unknown";row["renderer"]=renderer?reinterpret_cast<const char*>(renderer):"unknown";
            program(current,frame,row);
            if(active==GL_TEXTURE0&&atlas->m_pTexture&&unsigned(bound)==atlas->m_pTexture->getName()){
                row["min_filter"]=textureInteger(GL_TEXTURE_MIN_FILTER);row["mag_filter"]=textureInteger(GL_TEXTURE_MAG_FILTER);row["wrap_s"]=textureInteger(GL_TEXTURE_WRAP_S);row["wrap_t"]=textureInteger(GL_TEXTURE_WRAP_T);row["base_level"]=textureInteger(GL_TEXTURE_BASE_LEVEL);
                row["alpha_swizzle"]=(GLEW_VERSION_3_3||GLEW_ARB_texture_swizzle)?textureInteger(0x8e45):GL_ALPHA;
                GLfloat aniso=1;if(GLEW_EXT_texture_filter_anisotropic)glGetTexParameterfv(GL_TEXTURE_2D,0x84fe,&aniso);row["anisotropy_bits"]=std::bit_cast<std::uint32_t>(aniso);
                const auto at=texture(atlas->m_pTexture,frame,row);if(at!=UINT_MAX)row["texture_snapshot"]=at;else row["texture_snapshot_unavailable"]=true;
            }rows.push(std::move(row));
        }catch(...){failed=true;}
    }
    bool comparisonPassed()const noexcept{
        if(residentEnabled&&(!enabled||!pixelEnabled||!replayEnabled||zeroEnabled||handoffEnabled))return false;
        return !enabled||(!failed&&(!replayEnabled||(pixel.valid&&batchreplay::comparisonPassed(replay.result,zeroEnabled||residentEnabled,handoffEnabled,completedHandoffCalls)&&(!residentEnabled||(residentSeedReady&&residentReplay.reusedStorage&&residentReplay.preparedFrames==2&&completedResidentCalls==1&&completedReconstructed>0&&(!pass::producerReplayRequested()||(producerReplayReady&&producerReplayCount==residentReplay.quads)))))));
    }
    void finish(const char* output){
        replay.close();
        zeroProgram.close();
        residentReplay.close();
        if(!enabled||!output||written)return;written=true;matjson::Value result=matjson::Value::object();result["diagnostic_only"]=true;result["fps_evidence"]=false;result["failed"]=failed;result["observed_after_native_atlas_call"]=true;result["gl_error_queue_consumed"]=replayEnabled||residentEnabled;
        result["scope"]="two native capture frames, no GL bindings/parameters changed; attached shader sources and readback are eligibility observations, not proof of linked shader execution or a game speedup";
        matjson::Value images=matjson::Value::array();
        for(unsigned i=0;i<textures.size();++i){const auto& texture=textures[i];const auto path=std::string(output)+".opaque-texture-"+std::to_string(i)+".rgba";std::ofstream bytes(path,std::ios::binary|std::ios::out|std::ios::noreplace);if(!bytes)throw std::runtime_error("new owned texture receipt only");bytes.write(reinterpret_cast<const char*>(texture.rgba.data()),texture.rgba.size());if(!bytes)throw std::runtime_error("complete texture receipt");
            matjson::Value item=matjson::Value::object();item["index"]=i;item["name"]=texture.name;item["owner"]=fmt::format("{:x}",texture.owner);item["owner_uid"]=texture.uid;item["width"]=texture.width;item["height"]=texture.height;item["alpha_bits"]=texture.alphaBits;item["seen_frames"]=texture.seenFrames;item["bytes_identical_between_observed_frames"]=texture.stable;item["file"]=std::filesystem::path(path).filename().string();images.push(std::move(item));
        }
        result["textures"]=std::move(images);result["programs"]=std::move(programs);result["draws"]=std::move(rows);std::ofstream stream(std::string(output)+".opaque-inputs.json",std::ios::out|std::ios::noreplace);if(!stream)throw std::runtime_error("new opaque census receipt only");stream<<result.dump(2)<<'\n';if(!stream)throw std::runtime_error("complete opaque census receipt");
        if(pixelEnabled){pixel.metadata["renderer_comparison_passed"]=comparisonPassed();pixel.metadata["attempted"]=pixel.attempted;pixel.metadata["valid_changed_pixels"]=pixel.valid;pixel.metadata["diagnostic_only"]=true;pixel.metadata["fps_evidence"]=false;
            for(auto [suffix,data]:{std::pair{".batch-vbo.bin",&pixel.vertices},std::pair{".batch-ibo.bin",&pixel.indices},std::pair{".batch-program.bin",&pixel.binary}})if(!data->empty()){
                const auto path=std::string(output)+suffix;std::ofstream file(path,std::ios::binary|std::ios::out|std::ios::noreplace);if(!file)throw std::runtime_error("new native GPU input bytes only");file.write(reinterpret_cast<const char*>(data->data()),data->size());if(!file)throw std::runtime_error("complete native GPU input bytes");pixel.metadata[suffix]=std::filesystem::path(path).filename().string();}
            pixel.metadata["program_binary_format"]=pixel.binaryFormat;
            if(pixel.valid)for(unsigned which=0;which<2;++which){const auto& data=which?pixel.after:pixel.before;const auto path=std::string(output)+(which?".batch-after.rgba":".batch-before.rgba");std::ofstream bytes(path,std::ios::out|std::ios::binary|std::ios::noreplace);if(!bytes)throw std::runtime_error("new batch pixels only");bytes.write(reinterpret_cast<const char*>(data.data()),data.size());if(!bytes)throw std::runtime_error("complete batch pixels");pixel.metadata[which?"after_file":"before_file"]=std::filesystem::path(path).filename().string();}
            std::ofstream meta(std::string(output)+".batch-pixels.json",std::ios::out|std::ios::noreplace);if(!meta)throw std::runtime_error("new batch pixel metadata only");meta<<pixel.metadata.dump(2)<<'\n';if(!meta)throw std::runtime_error("complete batch pixel metadata");
        }
    }
};
inline Census* census=new Census;
}
