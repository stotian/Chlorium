#pragma once
#include "SpriteEncoding.hpp"
using chlorium::research::resident::Geometry;
using chlorium::research::resident::Parameters;
void encodeParameters(const Captured& record,Parameters& p) {
    chlorium::research::resident::encodeParameters(record.sprite,record.quad,p);
}
void encodeResident(const Captured& record,Geometry& g,Parameters& p) {
    chlorium::research::resident::encodeSprite(record.sprite,record.quad,g,p);
}
void checkResidentVertices(const std::filesystem::path& shaderPath,const std::vector<Captured>& records,
                           const std::vector<NativeQuadState>& expected) {
    std::vector<Geometry> geometry(records.size());std::vector<Parameters> parameters(records.size());
    std::vector<std::uint32_t> order(records.size());std::iota(order.begin(),order.end(),0u);
    std::vector<NativeQuadState> fallback(records.size()),actual(records.size()),poisoned(records.size());
    for(std::size_t i=0;i<records.size();++i){
        auto& g=geometry[i];auto& p=parameters[i];encodeResident(records[i],g,p);
        fallback[i]=records[i].quad;poison(poisoned[i]);
    }
    const auto shader=program(shaderPath,GL_VERTEX_SHADER,true);glUseProgram(shader);
    GLuint vao=0,buffers[5]{};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(5,buffers);
    auto upload=[&](unsigned binding,const void* data,std::size_t bytes){glBindBuffer(storage,buffers[binding]);glBufferData(storage,bytes,data,GL_STATIC_DRAW);glBindBufferBase(storage,binding,buffers[binding]);};
    upload(0,geometry.data(),geometry.size()*sizeof(Geometry));upload(1,parameters.data(),parameters.size()*sizeof(Parameters));
    upload(2,order.data(),order.size()*sizeof(order[0]));upload(3,fallback.data(),fallback.size()*sizeof(fallback[0]));
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[4]);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,poisoned.size()*sizeof(poisoned[0]),poisoned.data(),GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffers[4]);
    auto draw=[&](){glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(records.size()*4));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[4]);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,actual.size()*sizeof(actual[0]),actual.data());require(glGetError()==GL_NO_ERROR,"resident vertex feedback");};
    draw();require(mismatch(actual,expected)==0,"resident vertex stream differs from actual native Cocos");
    // Only the native-order indirection changes; geometry and parameters stay resident.
    std::reverse(order.begin(),order.end());glBindBuffer(storage,buffers[2]);glBufferSubData(storage,0,order.size()*sizeof(order[0]),order.data());
    draw();auto reversed=expected;std::reverse(reversed.begin(),reversed.end());require(mismatch(actual,reversed)==0,"resident painter-order indirection");
    glDeleteBuffers(5,buffers);glDeleteVertexArrays(1,&vao);glDeleteProgram(shader);
}
