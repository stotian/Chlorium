#pragma once
#include "../R4-47/ResidentRenderer.hpp"
#include "../R4-47/NativeSpriteSources.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace chlorium::research::resident {
// Seed owned storage without drawing; one bounded replay follows inside R4-43's
// restoration transaction. Native preparation remains paid.
class NativeReplay {
    Renderer renderer_;
    std::optional<SpriteSources> sources_;
    std::array<float,16> matrix_{};
    GLuint texture_=0,target_=0;
    bool prepared_=false;
    bool nativeStreamRequested_=false;
    const bool continuous_;
    GLuint nativeProgram_=0;
    std::string residentSource_;
#ifdef CHLORIUM_RESIDENT_FAST_R58
    std::vector<SpriteKey> order_;
    AddressSet seen_;
#endif
    static bool shaderSources(GLuint program,std::string& vertex,std::string& fragment) {
        std::array<GLuint,3> shaders{};GLsizei count=0;glGetAttachedShaders(program,3,&count,shaders.data());
        if(count!=2)return false;
        for(GLsizei n=0;n<count;++n){
            GLint type=0,length=0;glGetShaderiv(shaders[n],GL_SHADER_TYPE,&type);glGetShaderiv(shaders[n],GL_SHADER_SOURCE_LENGTH,&length);
            if(length<=0 || length>65536)return false;
            std::string text(length,'\0');GLsizei read=0;glGetShaderSource(shaders[n],length,&read,text.data());
            if(read<=0 || read>=length)return false;text.resize(read);
            if(type==GL_VERTEX_SHADER && vertex.empty())vertex=std::move(text);
            else if(type==GL_FRAGMENT_SHADER && fragment.empty())fragment=std::move(text);
            else return false;
        }
        return !vertex.empty() && !fragment.empty();
    }
public:
    explicit NativeReplay(bool continuous=false,std::string residentSource={}):continuous_(continuous),residentSource_(std::move(residentSource)){}
    void requestNativeStream(bool requested)noexcept{if(!preparedFrames)nativeStreamRequested_=requested;}
    bool nativeStreamRequested()const noexcept{return nativeStreamRequested_;}
    const NativeVertexStream::Metrics& streamMetrics()const noexcept{return renderer_.nativeStream_.metrics;}
    const char* gate="not_prepared";
    unsigned calls=0,quads=0,reconstructed=0;
    unsigned preparedFrames=0;
    bool reusedStorage=false;
    std::array<std::uint64_t,4> seedUploads{},updateUploads{};
    bool prepare(std::span<const unsigned char> vertices,std::span<const unsigned char> indices,
                 std::span<const pass::SpriteRecord> sources,GLuint program,GLuint textureOverride=0) {
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timingPrepare(residenttiming::Stage::prepare);
#endif
        if(calls && !continuous_)return false;
        const bool retained=prepared_;prepared_=false;
        constexpr auto stride=sizeof(snapshot::NativeQuadState);
        gate="resident_input";
        if(vertices.empty() || vertices.size()%stride || vertices.size()/stride>16384)return false;
        const auto count=vertices.size()/stride;
        constexpr unsigned topology[]{0,1,2,3,2,1};
        if(indices.size()!=count*12 || sources.size()!=count)return false;
        for(std::size_t n=0;n<count*6;++n){std::uint16_t index=0;std::memcpy(&index,indices.data()+n*2,2);if(index!=(n/6)*4+topology[n%6])return false;}
        if(!retained && residentSource_.empty()){
            const auto path=std::getenv("CHLORIUM_RESIDENT_VERTEX_FILE");
            if(!path){gate="resident_source_path";return false;}
            std::ifstream file(std::filesystem::u8path(path),std::ios::binary);
            if(!file){gate="resident_source_file";return false;}
            residentSource_.assign(2057,'\0');file.read(residentSource_.data(),residentSource_.size());
            if(file.gcount()!=2056 || !file.eof()){gate="resident_source_budget";return false;}residentSource_.resize(2056);
        }
        std::string vertex,fragment;
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timingProgram(residenttiming::Stage::program);
#endif
        GLint linked=0,deleted=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);glGetProgramiv(program,GL_DELETE_STATUS,&deleted);
        if(!linked || deleted){gate="native_program_lifetime";return false;}
        gate="native_sources";if(!shaderSources(program,vertex,fragment))return false;
        std::string error;if(!reviewedNativeSources(residentSource_,vertex,fragment,error))return false;
        GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);if(GLuint(current)!=program)return false;
        nativeProgram_=program;
        const auto mvp=glGetUniformLocation(program,"CC_MVPMatrix"),sampler=glGetUniformLocation(program,"CC_Texture0");
        GLint unit=-1;if(mvp<0 || sampler<0)return false;
        glGetUniformfv(program,mvp,matrix_.data());glGetUniformiv(program,sampler,&unit);if(unit!=0)return false;
        GLint active=0,name=0,framebuffer=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&name);glActiveTexture(active);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer);if(!name && !textureOverride)return false;
        texture_=textureOverride?textureOverride:GLuint(name);target_=GLuint(framebuffer);quads=unsigned(vertices.size()/stride);
#ifdef CHLORIUM_RENDERER_TIMING_R60
        timingProgram.finish();
#endif
        const auto oldIdentity=renderer_.identity();
        if(!retained){
            const auto capacity=std::min<std::size_t>(16384,std::max<std::size_t>(256,count*2));
            sources_.emplace(capacity);gate="resident_program";
            if(!renderer_.initializeReviewed(capacity,residentSource_,vertex,fragment,error))return false;
            if(nativeStreamRequested_ && !renderer_.nativeStream_.initialize(capacity,residentSource_)){gate="resident_native_stream_program";return false;}
        }
#ifdef CHLORIUM_RENDERER_TIMING_R60
        residenttiming::Scope timingSources(residenttiming::Stage::sources);
#endif
#ifdef CHLORIUM_RESIDENT_FAST_R58
        order_.clear();order_.reserve(quads);auto& order=order_;
        if(!seen_.reset(quads))return false;
#else
        std::vector<SpriteKey> order;order.reserve(quads);
        std::unordered_set<std::uint64_t> seen;seen.reserve(quads);
#endif
        reconstructed=0;
        if(continuous_ && !sources_->discardAbsent(sources))return false;
        for(unsigned n=0;n<quads;++n){
            snapshot::NativeQuadState quad{};std::memcpy(&quad,vertices.data()+n*stride,stride);
            // Conflicting occurrences cannot share one retained slot.
#ifdef CHLORIUM_RESIDENT_FAST_R58
            if(!seen_.insert(sources[n].sprite)){gate="repeated_source";return false;}
#else
            if(!seen.insert(sources[n].sprite).second){gate="repeated_source";return false;}
#endif
            auto old=sources_->keyFor(sources[n].sprite);
            if(old && old->nativeId!=sources[n].id){if(!sources_->erase(*old))return false;old.reset();}
            const auto key=old?*old:sources_->create(sources[n],quad);
            if(!key || (old && !sources_->publish(key,sources[n],quad)))return false;
            order.push_back(key);reconstructed+=sources_->reconstructs(key);
        }
        if(!sources_->painterOrder(order))return false;
        reusedStorage=retained && oldIdentity && oldIdentity==renderer_.identity();
        ++preparedFrames;prepared_=true;gate="prepared";return true;
    }
    bool seed(std::span<const unsigned char> vertices,std::span<const unsigned char> indices,
              std::span<const pass::SpriteRecord> sources,GLuint program) {
        if(preparedFrames || !prepare(vertices,indices,sources,program))return false;
        gate="resident_seed_upload";if(!renderer_.synchronize(sources_->scene())){prepared_=false;return false;}
        seedUploads=renderer_.uploadedBytes();
        if(nativeStreamRequested_ && !renderer_.seedNativeStream(sources_->scene())){prepared_=false;gate="resident_native_stream_seed";return false;}
        gate="resident_seeded";return true;
    }
    bool draw(bool(*beforeSubmit)(void*)=nullptr,void* checkpoint=nullptr) {
        if(!prepared_ || !sources_ || (calls && !continuous_))return false;
        const auto result=renderer_.drawTarget(sources_->scene(),0,quads,texture_,target_,matrix_,{1,1,1,1},true,nativeStreamRequested_?nativeProgram_:0,beforeSubmit,checkpoint);
        updateUploads=renderer_.uploadedBytes();
        if(result==Renderer::Draw::nativeRequired){gate="resident_declined";return false;}
        ++calls;
        if(result!=Renderer::Draw::submitted){gate="resident_after_submission_error";return false;}
        gate="resident_submitted";return true;
    }
    void close()noexcept{renderer_.close();sources_.reset();prepared_=false;}
};
}
