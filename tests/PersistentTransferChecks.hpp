#pragma once
#define CHLORIUM_PRIVATE_RING_CHECKS 1
#include "../research/R4-17/PersistentTransfer.hpp"
void persistentTransferChecks() {
    const auto saved=GLStateSnapshot::capture();
    GLuint target=0,other=0;glGenBuffers(1,&target);glGenBuffers(1,&other);
    std::array<unsigned char,512> expected{},actual{},cpu{};
    glBindBuffer(GL_ARRAY_BUFFER,target);glBufferData(GL_ARRAY_BUFFER,expected.size(),expected.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_COPY_READ_BUFFER,other);glBufferData(GL_COPY_READ_BUFFER,16,nullptr,GL_DYNAMIC_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER,other);
    auto ring=chlorium::research::PersistentTransfer::create(128);
    require(bool(ring),"coherent persistent transfer available on local GPU");
    std::uint32_t random=0xa37046c1u;
    auto next=[&] {random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned n=0;n<256;++n) {
        for(auto& b:cpu)b=static_cast<unsigned char>(next());
        const auto offset=next()%expected.size(),count=1+next()%std::min<std::size_t>(128,expected.size()-offset);
        // A refused transfer changes no canonical bytes. Finish only in this
        // diagnostic fixture; production must fall back instead of waiting.
        if(!ring->upload(target,offset,count,cpu.data()+offset)) {
            glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size(),actual.data());
            require(actual==expected,"busy transfer never mutates canonical storage");
            glFinish();require(ring->upload(target,offset,count,cpu.data()+offset),"completed pages become reusable");
        }
        std::copy_n(cpu.data()+offset,count,expected.data()+offset);
        glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size(),actual.data());
        require(actual==expected,"persistent transfer preserves GPU authority outside dirty range");
        GLint read=0,write=0,array=0;glGetIntegerv(GL_COPY_READ_BUFFER,&read);
        glGetIntegerv(GL_COPY_WRITE_BUFFER,&write);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        require(read==other&&write==other&&array==target,"persistent transfer preserves all touched bindings");
    }
    require(!ring->upload(target,500,13,cpu.data()),"oversized destination transfer rejected");
    require(!ring->upload(target,0,129,cpu.data()),"oversized ring reservation rejected");
    auto mapped=glMapBufferRange(GL_ARRAY_BUFFER,0,expected.size(),GL_MAP_READ_BIT);
    require(mapped!=nullptr&&!ring->upload(target,0,1,cpu.data()),"mapped canonical target rejected");glUnmapBuffer(GL_ARRAY_BUFFER);
    auto tiny=chlorium::research::PersistentTransfer::create(64);
    require(bool(tiny),"bounded small transfer ring available");
    for(unsigned n=0;n<3;++n) require(tiny->upload(target,0,64,cpu.data()),"three distinct pages accepted without reuse");
    tiny->denyReuse=true;
    glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size(),actual.data());const auto before=actual;
    require(!tiny->upload(target,0,64,expected.data()),"unsignalled page reuse refused without blocking");
    glGetBufferSubData(GL_ARRAY_BUFFER,0,actual.size(),actual.data());
    require(actual==before&&tiny->work.uploads==3&&tiny->work.busy==1,"refused page leaves native store and counters intact");
    tiny->denyReuse=false;glFinish();
    require(tiny->upload(target,0,64,expected.data()),"signalled ring wraps safely");
    ring.reset();tiny.reset();
    glBindBuffer(GL_COPY_READ_BUFFER,0);glBindBuffer(GL_COPY_WRITE_BUFFER,0);
    saved.restore();glDeleteBuffers(1,&target);glDeleteBuffers(1,&other);
    require(glGetError()==GL_NO_ERROR,"persistent transfer fixture has no GL errors");
    std::cout<<"Persistent transfers: byte authority, nonblocking fences, wrap and bindings verified\n";
}
