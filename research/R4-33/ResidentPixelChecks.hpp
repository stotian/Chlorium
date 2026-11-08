#pragma once
#include <cmath>
unsigned checkResidentPixels(const std::filesystem::path& shaderPath,const unsigned char* native) {
    constexpr unsigned count=16,size=128;
    std::array<Geometry,count> geometry{};std::array<Parameters,count> parameters{};
    std::array<NativeQuadState,count> quads{};std::array<unsigned,count> order{};std::iota(order.begin(),order.end(),0u);
    std::array<unsigned,count*6> indices{};
    for(unsigned i=0;i<count;++i){
        Captured record{};record.sprite.flags=512|1024;
        const float angle=i*.11f,c=std::cos(angle),s=std::sin(angle);
        const float matrix[]{c,s,-s,c,float(i%4)*.42f-.63f,float(i/4)*.42f-.63f};
        std::memcpy(record.sprite.matrix,matrix,sizeof(matrix));
        const float rect[]{-.26f,-.26f,.52f,.52f};std::memcpy(record.sprite.localRect,rect,sizeof(rect));
        if(i==2)record.sprite.flags&=~512u;if(i==5)record.sprite.flags&=~1024u;
        for(unsigned j=0;j<4;++j){auto& v=record.quad.corners[j];v.color=0x60000000u|((i*17+j*31)&255)|(((i*13+j*47)&255)<<8)|(((i*23+j*53)&255)<<16);
            v.u=j>=2?1.f:0.f;v.v=(j&1)?0.f:1.f;if(i%3==0)std::swap(v.u,v.v);}
        record.quad=nativeResult(record,0x1f80,native).quad;quads[i]=record.quad;encodeResident(record,geometry[i],parameters[i]);
        const unsigned triangle[]{0,1,2,3,2,1};for(unsigned n=0;n<6;++n)indices[i*6+n]=i*4+triangle[n];
    }
    const auto fragment=shaderPath.parent_path()/"resident.frag";
    const GLuint programs[]{program(shaderPath.parent_path()/"direct.vert",GL_VERTEX_SHADER,false,fragment),program(shaderPath,GL_VERTEX_SHADER,false,fragment)};
    GLuint buffers[6]{},vaos[2]{},textures[2]{},framebuffer=0;glGenBuffers(6,buffers);glGenVertexArrays(2,vaos);glGenTextures(2,textures);glGenFramebuffers(1,&framebuffer);
    auto upload=[&](unsigned binding,const void* data,std::size_t bytes){glBindBuffer(storage,buffers[binding]);glBufferData(storage,bytes,data,GL_STATIC_DRAW);glBindBufferBase(storage,binding,buffers[binding]);};
    upload(0,geometry.data(),sizeof(geometry));upload(1,parameters.data(),sizeof(parameters));upload(2,order.data(),sizeof(order));upload(3,quads.data(),sizeof(quads));
    glBindVertexArray(vaos[0]);glBindBuffer(GL_ARRAY_BUFFER,buffers[4]);glBufferData(GL_ARRAY_BUFFER,sizeof(quads),quads.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[5]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices.data(),GL_STATIC_DRAW);
    glBindVertexArray(vaos[1]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[5]);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"pixel oracle framebuffer");
    std::array<unsigned,16> texels{};for(unsigned i=0;i<16;++i)texels[i]=0x80000000u|((i*17u)&255u)|(((i*43u)&255u)<<8)|(((i*71u)&255u)<<16);
    glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,texels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glViewport(0,0,size,size);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glClearColor(.1f,.2f,.3f,.5f);
    using Pixels=std::array<unsigned char,size*size*4>;
    auto draw=[&](unsigned variant,const std::array<float,16>& matrix,unsigned first,unsigned n){
        glUseProgram(programs[variant]);const auto mvp=glGetUniformLocation(programs[variant],"CC_MVPMatrix"),effect=glGetUniformLocation(programs[variant],"native_effect"),texture=glGetUniformLocation(programs[variant],"CC_Texture0");
        require(mvp>=0&&effect>=0&&texture>=0,"pixel shader must expose exact uniforms");
        glUniformMatrix4fv(mvp,1,GL_FALSE,matrix.data());glUniform4f(effect,.7f,.4f,.9f,1.f);glUniform1i(texture,0);
        glClear(GL_COLOR_BUFFER_BIT);glBindVertexArray(vaos[variant]);glDrawElements(GL_TRIANGLES,n*6,GL_UNSIGNED_INT,reinterpret_cast<void*>(std::uintptr_t(first*6*sizeof(unsigned))));
        Pixels pixels{};glReadPixels(0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());require(glGetError()==GL_NO_ERROR,"resident pixel draw/readback");return pixels;
    };
    unsigned scenarios=0;
    for(unsigned blend=0;blend<3;++blend)for(bool cull:{false,true})for(bool clipped:{false,true})for(unsigned first:{0u,3u}){
        if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
        glBlendFuncSeparate(blend==1?GL_SRC_ALPHA:GL_ONE,blend==1?GL_ONE_MINUS_SRC_ALPHA:GL_ONE,GL_ONE,GL_ZERO);
        if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
        const std::array<float,16> matrix{.92f,.13f,0,0,-.07f,.89f,0,0,0,0,1,0,clipped?3.f:.04f,-.03f,0,1};
        const auto expected=draw(0,matrix,first,count-first),actual=draw(1,matrix,first,count-first);
        require(actual==expected,"resident rendering must preserve every RGBA8 pixel");++scenarios;
    }
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},zero{};
    const auto visible=draw(1,identity,0,count),black=draw(1,zero,0,count);require(visible!=black,"pixel oracle must reject zero-MVP black output");
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(2,textures);glDeleteBuffers(6,buffers);glDeleteVertexArrays(2,vaos);
    for(auto value:programs)glDeleteProgram(value);return scenarios;
}
