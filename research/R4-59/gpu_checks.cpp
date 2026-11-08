#define CHLORIUM_RESIDENT_FAST_R58 1
#define main previousGpuChecks
#include "../R4-47/gpu_scene_checks.cpp"
#undef main
#include "NativeUploadDeferral.hpp"

static void uploadChecks(){
    using namespace chlorium::research::continuous;
    std::array<std::uint32_t,48> before{},after{},copied{};
    before.fill(17);after.fill(91);
    GLuint buffer=0,foreign=0;glGenBuffers(1,&buffer);glGenBuffers(1,&foreign);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(before),before.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,foreign);glBufferData(GL_ARRAY_BUFFER,sizeof(before),before.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBindBuffer(GL_COPY_READ_BUFFER,foreign);
    bool dirty=true;unsigned callbacks=0;
    const auto native=[&](unsigned first,unsigned count){
        ++callbacks;
        if(dirty){glBufferSubData(GL_ARRAY_BUFFER,first*96,count*96,after.data()+first*24);dirty=false;}
    };
    {
        NativeUploadDeferral::Scope deferred(buffer,dirty,sizeof(after),true);
        require(deferred.skipped() && !dirty,"qualified scope defers native upload");
        GLint binding=0;glGetIntegerv(GL_COPY_READ_BUFFER,&binding);
        require(GLuint(binding)==foreign,"buffer validation restores the previous binding");
        native(0,2);require(!dirty && callbacks==1,"native callback executes once with upload deferred");
    }
    glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(copied),copied.data());
    require(dirty && copied==before,"native mirror stays untouched and dirty is restored");
    const bool pending=dirty;native(1,1);dirty|=retainPartialDirty(pending,1,1,2);
    glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(copied),copied.data());
    require(dirty && std::equal(copied.begin(),copied.begin()+24,before.begin()) &&
            std::equal(copied.begin()+24,copied.end(),after.begin()+24),"partial native fallback updates its range and keeps remaining data dirty");
    native(0,2);glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(copied),copied.data());
    require(copied==after && !dirty && callbacks==3,"full native fallback repairs the complete mirror exactly once");
    {
        NativeUploadDeferral::Scope clean(buffer,dirty,sizeof(after),true);
        require(!clean.skipped() && !dirty,"already-clean mirror needs no deferral");
    }
    dirty=true;
    {
        NativeUploadDeferral::Scope rejected(buffer,dirty,sizeof(after),false);
        require(!rejected.skipped() && dirty,"unqualified caller retains native upload");
    }
    {
        NativeUploadDeferral::Scope rejected(0,dirty,sizeof(after),true);
        require(!rejected.skipped() && dirty,"invalid buffer retains native upload");
    }
    {
        NativeUploadDeferral::Scope rejected(buffer,dirty,sizeof(after)+96,true);
        require(!rejected.skipped() && dirty,"out-of-storage upload cannot be deferred");
    }
    {
        require(glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY)!=nullptr,"map native mirror fault fixture");
        NativeUploadDeferral::Scope mapped(buffer,dirty,sizeof(after),true);
        require(!mapped.skipped() && dirty,"mapped mirror retains driver validation");
        native(0,2);require(glGetError()==GL_INVALID_OPERATION,"native mapped-buffer error is preserved");
        require(glUnmapBuffer(GL_ARRAY_BUFFER)==GL_TRUE,"unmap native mirror fault fixture");
    }
    dirty=true;
    {
        NativeUploadDeferral::Scope outer(buffer,dirty,sizeof(after),true);
        bool otherDirty=true;
        {NativeUploadDeferral::Scope inner(foreign,otherDirty,sizeof(after),true);
         require(inner.skipped() && outer.skipped() && !otherDirty && !dirty,"nested independent atlases defer locally");}
        require(otherDirty && !dirty,"nested scope restores only its atlas dirty flag");
    }
    require(dirty,"outer scope restores its native dirty flag");
    try{NativeUploadDeferral::Scope aborted(buffer,dirty,sizeof(after),true);throw std::runtime_error("owned unwind");}
    catch(const std::runtime_error&){}
    require(dirty,"exception cannot lose native mirror invalidation");
    native(0,2);require(!dirty,"native callback recovers after exception");
    require(!retainPartialDirty(true,0,2,2) && !retainPartialDirty(false,1,1,2),"full and clean native draws do not retain dirty unnecessarily");
    glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_COPY_READ_BUFFER,0);
    glDeleteBuffers(1,&buffer);glDeleteBuffers(1,&foreign);
    require(glGetError()==GL_NO_ERROR,"deferred upload leaves no GL error");
}
int main(int argc,char** argv)try{
    if(previousGpuChecks(argc,argv))return 1;
    Graphics graphics;graphics.create(true,6);uploadChecks();
    std::cout<<"R59 local dirty-scope upload deferral contracts passed; NOT game performance\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
