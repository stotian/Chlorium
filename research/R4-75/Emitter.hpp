#pragma once
#include "Admission.hpp"
namespace chlorium::research::rootgeometry {
inline std::vector<Row> readGeometry(const std::filesystem::path& file,const std::filesystem::path& rootFile){
    require(std::filesystem::file_size(file)>=sizeof(Header) && std::filesystem::file_size(file)<=sizeof(Header)+256*sizeof(Row),"bounded native geometry file");
    std::ifstream input(file,std::ios::binary);Header h;input.read(reinterpret_cast<char*>(&h),sizeof(h));
    require(input && !std::memcmp(h.magic,"CHLGEO75",8) && h.version==75 && h.rowBytes==sizeof(Row) && h.frames==2 && h.records && h.records<=256 && h.actors && h.actors<=128 && !h.reasons,"complete native geometry header");
    rotationtrace::Header root;std::ifstream rootInput(rootFile,std::ios::binary);rootInput.read(reinterpret_cast<char*>(&root),sizeof(root));
    require(rootInput && h.owner==root.owner && h.ownerUid==root.ownerUid && h.thread==root.thread && h.percent==root.percent && h.actors==root.actors,"geometry belongs to the exact native root capture");
    require(std::filesystem::file_size(file)==sizeof(h)+h.records*sizeof(Row),"native geometry exact byte count");std::vector<Row> rows(h.records);input.read(reinterpret_cast<char*>(rows.data()),rows.size()*sizeof(Row));require(bool(input),"complete native geometry rows");
    std::map<std::pair<unsigned,std::uint64_t>,bool> seen;unsigned matches=0,frames=0;
    for(const auto& row:rows){require(row.frame>=1 && row.frame<=2 && !row.reserved && !(row.controls&~65535u) && !(row.root.flags&~255u) && !row.root.reserved[0] && !row.root.reserved[1] && !(row.roles&~511u),"known native geometry row metadata");
        require(row.source.sprite && row.source.id && row.source.root==row.source.sprite && row.source.rootId==row.source.id && !(row.source.flags&~2047u) && row.source.quadMatches<=1,"geometry receiver identity and source flags");
        require(seen.emplace(std::pair{row.frame,row.source.sprite},true).second,"unique native geometry receiver per frame");matches+=row.source.quadMatches;frames|=1u<<row.frame;}
    require(matches==h.matched && frames==6,"native geometry match counters and both frames");return rows;
}
class Emitter {
    GLuint shader_=0,buffers_[4]{};GLint count_=-1;Dispatch dispatch_=nullptr;
public:
    unsigned admitted=0,nonempty=0,fallbacks=0,poseDeclines=0,endRootChanges=0,biasDeclines=0;
    explicit Emitter(const std::filesystem::path& shader){shader_=program(shader);count_=glGetUniformLocation(shader_,"objectCount");require(count_>=0,"root geometry shader interface");dispatch_=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));require(dispatch_!=nullptr,"root geometry dispatch");glGenBuffers(4,buffers_);}
    ~Emitter(){glDeleteBuffers(4,buffers_);if(shader_)glDeleteProgram(shader_);}
    void check(GLuint residentState,std::span<const mixedprogram::State> states,std::span<const Row* const> rows,std::span<const unsigned> poseSlots){
        require(poseSlots.size()==rows.size() && !states.empty(),"complete geometry/pose association");std::vector<resident::Geometry> geometry(rows.size());std::vector<resident::Parameters> parameters(rows.size());std::vector<snapshot::NativeQuadState> native(rows.size()),fallback(rows.size());
        for(unsigned i=0;i<rows.size();++i){const auto& row=*rows[i];native[i]=row.quad;const auto result=encodeNativeRow(row,states,poseSlots[i],geometry[i],parameters[i]);poseDeclines+=result.poseDeclined;endRootChanges+=result.rootChanged;biasDeclines+=result.biasDeclines;
            if(result.accepted){++admitted;nonempty+=(row.source.localRect[2]&0x7fffffffu)!=0 && (row.source.localRect[3]&0x7fffffffu)!=0;}else{++fallbacks;fallback[i]=native[i];}
        }
        glUseProgram(shader_);glMemoryBarrier(0x2000);glBindBufferBase(storage,0,residentState);
        auto upload=[&](unsigned buffer,unsigned binding,const auto values){glBindBuffer(storage,buffers_[buffer]);glBufferData(storage,values.size_bytes(),values.data(),GL_DYNAMIC_DRAW);glBindBufferBase(storage,binding,buffers_[buffer]);};
        upload(0,1,std::span<const resident::Geometry>(geometry));upload(1,2,std::span<const resident::Parameters>(parameters));upload(3,4,std::span<const snapshot::NativeQuadState>(fallback));
        glBindBuffer(storage,buffers_[2]);glBufferData(storage,native.size()*96,nullptr,GL_DYNAMIC_DRAW);glBindBufferBase(storage,3,buffers_[2]);glUniform1ui(count_,unsigned(rows.size()));dispatch_((unsigned(rows.size())+63)/64,1,1);
        glMemoryBarrier(0x2001|GL_BUFFER_UPDATE_BARRIER_BIT);std::vector<snapshot::NativeQuadState> actual(native.size());glBindBuffer(storage,buffers_[2]);glGetBufferSubData(storage,0,actual.size()*96,actual.data());require(glGetError()==GL_NO_ERROR,"owned real-geometry emission");
        for(unsigned i=0;i<actual.size();++i)if(std::memcmp(&actual[i],&native[i],96)){std::cerr<<"real geometry mismatch actor="<<i<<" GPU="<<geometry[i].control[0]<<'\n';throw std::runtime_error("GPU/native complete quad mismatch");}
    }
};
}
