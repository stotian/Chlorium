#pragma once
#include "../R4-75/Emitter.hpp"
#include "ParameterClasses.hpp"
namespace chlorium::research::rootgeometry {
class ClassEmitter {
    GLuint shader_=0,buffers_[5]{};GLint count_=-1;Dispatch dispatch_=nullptr;unsigned size_=0;
    std::vector<resident::Geometry> oldGeometry_;std::vector<resident::Parameters> oldParameters_;std::vector<unsigned> oldSlots_;std::vector<snapshot::NativeQuadState> oldFallback_;
    parameterclasses::Cache classes_;
    template<class T>void upload(unsigned buffer,unsigned binding,std::span<const T> values,std::vector<T>& previous,std::uint64_t& counter){
        glBindBuffer(storage,buffers_[buffer]);glBindBufferBase(storage,binding,buffers_[buffer]);
        for(auto range:parameterclasses::changedRanges<T>(previous,values)){const auto bytes=std::size_t(range[1])*sizeof(T);glBufferSubData(storage,std::size_t(range[0])*sizeof(T),bytes,values.data()+range[0]);counter+=bytes;uploadedBytes+=bytes;}
        previous.assign(values.begin(),values.end());
    }
    void allocate(unsigned count){
        if(size_==count)return;size_=count;oldGeometry_.clear();oldParameters_.clear();oldSlots_.clear();oldFallback_.clear();
        constexpr unsigned stride[]{64,48,96,96,4};for(unsigned i=0;i<5;++i){glBindBuffer(storage,buffers_[i]);glBufferData(storage,std::size_t(count)*stride[i],nullptr,GL_DYNAMIC_DRAW);}++allocations;
    }
public:
    unsigned admitted=0,nonempty=0,fallbacks=0,poseDeclines=0,endRootChanges=0,biasDeclines=0,frames=0,allocations=0,peakClasses=0;
    std::uint64_t uploadedBytes=0,geometryBytes=0,parameterBytes=0,slotBytes=0,fallbackBytes=0,legacyBytes=0;
    explicit ClassEmitter(const std::filesystem::path& source){shader_=program(source);count_=glGetUniformLocation(shader_,"objectCount");require(count_>=0,"class emitter shader interface");dispatch_=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));require(dispatch_!=nullptr,"class compute available");glGenBuffers(5,buffers_);}
    ~ClassEmitter(){glDeleteBuffers(5,buffers_);if(shader_)glDeleteProgram(shader_);}
    GLuint canonicalBuffer()const{return buffers_[2];}
    void check(GLuint residentState,std::span<const mixedprogram::State> states,std::span<const Row* const> rows,std::span<const unsigned> poseSlots){
        require(!rows.empty() && rows.size()<=65536 && poseSlots.size()==rows.size() && !states.empty() && residentState,"bounded retained geometry association");
        std::vector<resident::Geometry> geometry(rows.size());std::vector<resident::Parameters> parameters(rows.size());std::vector<unsigned char> accepted(rows.size());std::vector<snapshot::NativeQuadState> native(rows.size()),fallback;
        for(unsigned i=0;i<rows.size();++i){require(rows[i]!=nullptr,"live copied geometry source");const auto& row=*rows[i];native[i]=row.quad;auto& g=geometry[i];
            const auto result=encodeNativeRow(row,states,poseSlots[i],g,parameters[i]);poseDeclines+=result.poseDeclined;endRootChanges+=result.rootChanged;biasDeclines+=result.biasDeclines;accepted[i]=result.accepted;
            if(result.accepted){++admitted;nonempty+=(row.source.localRect[2]&0x7fffffffu)!=0 && (row.source.localRect[3]&0x7fffffffu)!=0;}else{g.control[3]=unsigned(fallback.size());fallback.push_back(row.quad);++fallbacks;}}
        const auto& classes=classes_.update(parameters,accepted);peakClasses=std::max(peakClasses,unsigned(classes.values.size()));
        const auto canonical=canonicalBuffer();allocate(unsigned(rows.size()));require(canonical==canonicalBuffer(),"canonical buffer identity retained across frame allocation");
        glUseProgram(shader_);glMemoryBarrier(0x2000);glBindBufferBase(storage,0,residentState);
        upload(0,1,std::span<const resident::Geometry>(geometry),oldGeometry_,geometryBytes);
        upload(1,2,std::span<const resident::Parameters>(classes.values),oldParameters_,parameterBytes);
        upload(3,4,std::span<const snapshot::NativeQuadState>(fallback),oldFallback_,fallbackBytes);
        upload(4,5,std::span<const unsigned>(classes.slots),oldSlots_,slotBytes);
        glBindBufferBase(storage,3,canonicalBuffer());glUniform1ui(count_,unsigned(rows.size()));dispatch_((unsigned(rows.size())+63)/64,1,1);glMemoryBarrier(0x2001|GL_BUFFER_UPDATE_BARRIER_BIT);
        // Read through the canonical vertex-buffer binding, not an intermediate GPU copy.
        glBindBuffer(GL_ARRAY_BUFFER,canonicalBuffer());std::vector<snapshot::NativeQuadState> actual(native.size());glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size()*96,actual.data());require(glGetError()==GL_NO_ERROR,"canonical class emission and consumer readback");
        require(!std::memcmp(actual.data(),native.data(),actual.size()*96),"all native/canonical quad bytes, including compressed fallbacks");
        legacyBytes+=rows.size()*(64+48+96);++frames;
    }
};
}
