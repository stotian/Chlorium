#include "../R4-33/GpuFixture.hpp"
#include "ResidentRenderer.hpp"
#include "NativeSpriteSources.hpp"
#include "../R4-49/NativeReplay.hpp"
#include "../R4-57/TargetBackup.hpp"
#ifdef CHLORIUM_RESIDENT_FAST_R58
#include "../R4-58/NativeDrawSuppression.hpp"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace chlorium::research::resident;
static PFNGLMEMORYBARRIERPROC driverBarrier=nullptr;
static std::uint64_t overwriteBarriers=0;
static void APIENTRY observeBarrier(GLbitfield bits){if(bits&0x2000)++overwriteBarriers;driverBarrier(bits);}
struct BarrierObserver {
    PFNGLMEMORYBARRIERPROC saved=glMemoryBarrier;
    BarrierObserver(){driverBarrier=saved;glMemoryBarrier=observeBarrier;}
    ~BarrierObserver(){glMemoryBarrier=saved;}
};

static std::string source(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);require(bool(file),"read resident source");
    std::string text((std::istreambuf_iterator<char>(file)),{});
    require(!text.empty() && text.size()<=65536,"bounded resident source");return text;
}
int main(int argc,char** argv)try{
    require(argc==2 || argc==4,"usage: gpu_scene_checks.exe R4-33-shader-directory [captured-native.vert captured-native.frag]");
    Graphics graphics;graphics.create(argc==4,argc==4?6:3);
    BarrierObserver barriers;
    const std::filesystem::path shaders=argv[1];std::string error;
    Renderer renderer;
    auto initialized=renderer.initialize(8,source(shaders/"resident.vert"),source(shaders/"resident.frag"),error);
    require(initialized,error.c_str());
    GLuint framebuffer=0,textures[2]{},nativeVao=0,nativeVbo=0,nativeIbo=0;
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(2,textures);glBindTexture(GL_TEXTURE_2D,textures[0]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[0],0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"owned color target");
    glBindTexture(GL_TEXTURE_2D,textures[1]);
    const std::array<std::uint32_t,4> texels{0xffffffff,0x80408020,0x40000080,0};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(.07f,.11f,.13f,.17f);
    glGenVertexArrays(1,&nativeVao);glBindVertexArray(nativeVao);
    glGenBuffers(1,&nativeVbo);glBindBuffer(GL_ARRAY_BUFFER,nativeVbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));glEnableVertexAttribArray(2);
    std::array<unsigned,48> indices{};constexpr unsigned topology[]{0,1,2,3,2,1};
    for(unsigned n=0;n<8;++n)for(unsigned c=0;c<6;++c)indices[n*6+c]=n*4+topology[c];
    glGenBuffers(1,&nativeIbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,nativeIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices.data(),GL_STATIC_DRAW);
    const auto nativeProgram=program(shaders/"direct.vert",GL_VERTEX_SHADER,false,shaders/"resident.frag");
    const std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,4> effect{.7f,.4f,.9f,1};
    Scene scene(8);std::array<Handle,3> handles{};
    std::array<Geometry,3> geometry{};std::array<Parameters,3> parameters{};std::array<NativeQuadState,3> expected{};
    auto encode=[&](unsigned n){
        auto& g=geometry[n];auto& p=parameters[n];chlorium::research::pass::SpriteRecord record{};
        std::copy_n(g.rect,4,record.localRect);std::copy_n(p.matrix,4,record.matrix);
        std::copy_n(p.translationZFlags,2,record.matrix+4);record.flags=p.translationZFlags[3];record.z=p.translationZFlags[2];
        require(chlorium::research::corners::materialize(record,expected[n]),"closed CPU arithmetic oracle");
        for(unsigned c=0;c<4;++c){expected[n].corners[c].color=p.colors[c];std::memcpy(&expected[n].corners[c].u,g.uv+c*2,8);}
    };
    for(unsigned n=0;n<3;++n){
        auto& g=geometry[n];auto& p=parameters[n];g.control[0]=1;g.rect[2]=g.rect[3]=0x3f800000;
        for(unsigned c=0;c<4;++c){g.uv[c*2]=c>=2?0x3f800000:0;g.uv[c*2+1]=(c&1)?0:0x3f800000;}
        p.matrix[0]=p.matrix[3]=0x3f400000;p.translationZFlags[0]=std::bit_cast<std::uint32_t>(-.7f+float(n)*.3f);
        p.translationZFlags[1]=std::bit_cast<std::uint32_t>(-.5f+float(n)*.1f);p.translationZFlags[3]=512|1024;
        for(unsigned c=0;c<4;++c)p.colors[c]=0x80604020u+n*0x00100008u+c;
        encode(n);handles[n]=scene.create(g,p);require(bool(handles[n]),"resident handle");
    }
    require(scene.painterOrder(handles),"initial painter order");
    using Pixels=std::array<unsigned char,64*64*4>;
    auto image=[](){Pixels pixels{};glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());require(glGetError()==GL_NO_ERROR,"read owned target");return pixels;};
    auto reference=[&](std::span<const NativeQuadState> quads){
        glClear(GL_COLOR_BUFFER_BIT);glUseProgram(nativeProgram);
        glUniformMatrix4fv(glGetUniformLocation(nativeProgram,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());
        glUniform4fv(glGetUniformLocation(nativeProgram,"native_effect"),1,effect.data());
        glUniform1i(glGetUniformLocation(nativeProgram,"CC_Texture0"),0);
        glBindVertexArray(nativeVao);glBindBuffer(GL_ARRAY_BUFFER,nativeVbo);
        glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(quads.size_bytes()),quads.data(),GL_DYNAMIC_DRAW);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);
        glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(quads.size()*6),GL_UNSIGNED_INT,nullptr);return image();
    };
    unsigned cases=0;
    GLuint foreignStorage[5]{};glGenBuffers(5,foreignStorage);
    GLint storageAlignment=0;glGetIntegerv(0x90df,&storageAlignment);
    require(storageAlignment>0 && storageAlignment<=1024*1024,"SSBO range alignment budget");
    std::vector<unsigned char> foreignBytes(std::size_t(storageAlignment)*3+128,0x6b);
    for(unsigned n=0;n<5;++n){glBindBuffer(GLStateSnapshot::storageTarget,foreignStorage[n]);glBufferData(GLStateSnapshot::storageTarget,foreignBytes.size(),foreignBytes.data(),GL_STATIC_DRAW);}
    auto compare=[&](Scene& target,std::span<const NativeQuadState> quads){
        const auto baseline=reference(quads);glClear(GL_COLOR_BUFFER_BIT);const auto clear=image();
        require(clear!=baseline,"visible reference control");
        for(unsigned n=0;n<4;++n)glBindBufferRange(GLStateSnapshot::storageTarget,n,foreignStorage[n],storageAlignment,64);
        glBindBuffer(GLStateSnapshot::storageTarget,foreignStorage[4]);glActiveTexture(GL_TEXTURE3);
        const auto state=GLStateSnapshot::capture();
        std::array<GLStateSnapshot::IndexedBufferBinding,4> beforeRanges{};
        for(unsigned n=0;n<4;++n)beforeRanges[n]=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,n);
        auto binding3=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,3);
        require(renderer.draw(target,0,quads.size(),textures[1],framebuffer,matrix,effect)==Renderer::Draw::submitted,"resident submission");
        GLint vao=0,program=0,storage=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GLStateSnapshot::storageBinding,&storage);
        require(vao==state.vao && program==state.program && storage==state.storageBuffer,"restore caller generic bindings");
        GLint activeTexture=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);require(activeTexture==GL_TEXTURE3,"restore caller active texture unit");
        for(unsigned n=0;n<4;++n){
            const auto range=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,n);
            require(range.buffer==beforeRanges[n].buffer && range.offset==beforeRanges[n].offset && range.size==beforeRanges[n].size,"restore every indexed range exactly");
        }
        auto after3=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,3);
        require(binding3.buffer==after3.buffer && binding3.offset==after3.offset && binding3.size==after3.size,"restore SSBO3 omitted by old generic guard");
        require(image()==baseline,"retained renderer exact fixture pixels");++cases;
    };
    compare(scene,expected);compare(scene,expected);
    SpriteSources sources(8);std::array<chlorium::research::pass::SpriteRecord,3> nativeSources{};
    std::array<SpriteKey,3> sourceOrder{};
    for(unsigned n=0;n<3;++n){
        auto& input=nativeSources[n];const auto& g=geometry[n];const auto& p=parameters[n];
        input.sprite=0x1000+n;input.id=n+1;input.quadMatches=1;
        std::copy_n(g.rect,4,input.localRect);std::copy_n(p.matrix,4,input.matrix);
        std::copy_n(p.translationZFlags,2,input.matrix+4);input.z=p.translationZFlags[2];input.flags=p.translationZFlags[3];
        sourceOrder[n]=sources.create(input,expected[n]);require(bool(sourceOrder[n]),"register native source");
    }
    require(sources.painterOrder(sourceOrder),"native source order");compare(sources.scene(),expected);
    auto zeroColorReference=expected;auto staleColorCopy=expected[1];
    for(auto& corner:zeroColorReference[1].corners)corner.color=0;
    for(auto& corner:staleColorCopy.corners){corner.color=0;corner.x=999;}
    require(sources.publishStyle(sourceOrder[1],staleColorCopy),"style-only event preserves newer retained positions");
    // With ONE/ZERO, zero-color triangles must write black. Collapsing them or
    // discarding zero fragments would incorrectly preserve the old background.
    glBlendFunc(GL_ONE,GL_ZERO);compare(sources.scene(),zeroColorReference);
    glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    require(sources.publishStyle(sourceOrder[1],expected[1]),"restore native style");
    auto sourceChanged=expected;
    nativeSources[1].matrix[4]=std::bit_cast<std::uint32_t>(.2f);
    require(chlorium::research::corners::materialize(nativeSources[1],sourceChanged[1]),"changed source reference");
    require(sources.publish(sourceOrder[1],nativeSources[1],sourceChanged[1]),"publish source delta");compare(sources.scene(),sourceChanged);
    sourceChanged[1].corners[0].x+=.125f;
    require(sources.publish(sourceOrder[1],nativeSources[1],sourceChanged[1]),"source mismatch fallback");compare(sources.scene(),sourceChanged);
    // Switching back must reseed the direct scene; its old consumer ID alone is insufficient.
    compare(scene,expected);
    parameters[1].translationZFlags[0]=std::bit_cast<std::uint32_t>(.1f);encode(1);
    require(scene.parameters(handles[1],parameters[1]),"pose notification");compare(scene,expected);
    std::reverse(handles.begin(),handles.end());std::reverse(expected.begin(),expected.end());
    require(scene.painterOrder(handles),"order-only notification");compare(scene,expected);
    Geometry unsupported{};NativeQuadState fallback=expected[0];fallback.corners[0].x+=.125f;fallback.corners[2].color=0x70402010;
    require(scene.replace(handles[0],unsupported,Parameters{},&fallback),"arbitrary native-quad lane");expected[0]=fallback;compare(scene,expected);
    require(scene.erase(handles[0]),"erase ordered sprite");
    require(renderer.draw(scene,0,3,textures[1],framebuffer,matrix,effect)==Renderer::Draw::nativeRequired,"stale painter order cannot draw");
    require(scene.painterOrder(std::span<const Handle>(handles).subspan(1)),"repair order after erase");
    compare(scene,std::span<const NativeQuadState>(expected).subspan(1));
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    require(renderer.draw(scene,0,2,textures[1],framebuffer,matrix,effect)==Renderer::Draw::nativeRequired,"wrong target rejects before drawing");
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glEnable(GL_DEPTH_TEST);
    require(renderer.draw(scene,0,2,textures[1],framebuffer,matrix,effect)==Renderer::Draw::nativeRequired,"unsupported depth path rejects");glDisable(GL_DEPTH_TEST);
    require(renderer.draw(scene,3,2,textures[1],framebuffer,matrix,effect)==Renderer::Draw::nativeRequired,"invalid subrange rejects");
    GLuint extraTarget=0;glGenTextures(1,&extraTarget);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,extraTarget);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,extraTarget,0);
    const GLenum targets[]{GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,targets);
    const auto beforeMrt=image();
    require(renderer.draw(scene,0,2,textures[1],framebuffer,matrix,effect)==Renderer::Draw::nativeRequired && image()==beforeMrt,"unknown MRT target rejects without writes");
    glDrawBuffers(1,targets);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,0,0);glDeleteTextures(1,&extraTarget);
    renderer.close();require(!renderer.identity(),"release invalidates GPU consumer");
    initialized=renderer.initialize(8,source(shaders/"resident.vert"),source(shaders/"resident.frag"),error);
    require(initialized,error.c_str());
    compare(scene,std::span<const NativeQuadState>(expected).subspan(1));
    if(argc==4){
        renderer.close();
        const auto resident=source(shaders/"resident.vert"),nativeVertex=source(argv[2]),nativeFragment=source(argv[3]);
        require(!renderer.initializeReviewed(8,resident+"\n",nativeVertex,nativeFragment,error) && !renderer.identity(),"changed resident shader rejects before allocation");
        require(!renderer.initializeReviewed(8,resident,nativeVertex+"\n",nativeFragment,error) && !renderer.identity(),"unknown native vertex rejects before allocation");
        require(!renderer.initializeReviewed(8,resident,nativeVertex,nativeFragment+"\n",error) && !renderer.identity(),"unknown native fragment rejects before allocation");
        initialized=renderer.initializeReviewed(8,resident,nativeVertex,nativeFragment,error);
        require(initialized,error.c_str());effect={1,1,1,1};
        // Finite RGBA8 fixture: the reference's extra multiplication by one is
        // exact here. This is not a comparison against the native linked binary.
        compare(scene,std::span<const NativeQuadState>(expected).subspan(1));
        compare(sources.scene(),sourceChanged);
        const auto legacy=program(argv[2],GL_VERTEX_SHADER,false,argv[3]);
        glBindAttribLocation(legacy,0,"a_position");glBindAttribLocation(legacy,1,"a_color");glBindAttribLocation(legacy,2,"a_texCoord");glLinkProgram(legacy);
        GLint linked=0;glGetProgramiv(legacy,GL_LINK_STATUS,&linked);require(linked==GL_TRUE,"owned original Cocos text program");
        const auto quads=std::span<const NativeQuadState>(expected).subspan(1);
        glUseProgram(legacy);glUniformMatrix4fv(glGetUniformLocation(legacy,"CC_MVPMatrix"),1,GL_FALSE,matrix.data());glUniform1i(glGetUniformLocation(legacy,"CC_Texture0"),0);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);glBindVertexArray(nativeVao);glBindBuffer(GL_ARRAY_BUFFER,nativeVbo);
        glBufferData(GL_ARRAY_BUFFER,quads.size_bytes(),quads.data(),GL_STATIC_DRAW);glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,GLsizei(quads.size()*6),GL_UNSIGNED_INT,nullptr);const auto nativeImage=image();
        std::vector<std::uint16_t> nativeIndices(quads.size()*6);
        for(std::size_t n=0;n<nativeIndices.size();++n)nativeIndices[n]=std::uint16_t((n/6)*4+topology[n%6]);
        const std::span<const unsigned char> vertices(reinterpret_cast<const unsigned char*>(quads.data()),quads.size_bytes());
        const std::span<const unsigned char> indexBytes(reinterpret_cast<const unsigned char*>(nativeIndices.data()),nativeIndices.size()*2);
        const auto priorPath=std::getenv("CHLORIUM_RESIDENT_VERTEX_FILE");const std::string savedPath=priorPath?priorPath:"";
        require(_putenv_s("CHLORIUM_RESIDENT_VERTEX_FILE",(shaders/"resident.vert").string().c_str())==0,"owned replay shader path");
        std::array<chlorium::research::pass::SpriteRecord,2> replaySources{nativeSources[1],nativeSources[0]};
        replaySources[0].matrix[4]=parameters[1].translationZFlags[0];
        NativeReplay nativeReplay;
        require(nativeReplay.seed(vertices,indexBytes,replaySources,legacy),nativeReplay.gate);
        require(image()==nativeImage && nativeReplay.calls==0 && nativeReplay.seedUploads[0]>0,"seed initializes GPU storage without framebuffer writes");
        require(nativeReplay.prepare(vertices,indexBytes,replaySources,legacy),nativeReplay.gate);
        require(nativeReplay.reconstructed==2,"native replay must execute reconstructed geometry, not only fallback quads");
        glClear(GL_COLOR_BUFFER_BIT);require(nativeReplay.draw(),nativeReplay.gate);
        require(image()==nativeImage,"resident replay differs from original Cocos shader on owned target");++cases;
        require(nativeReplay.reusedStorage && nativeReplay.preparedFrames==2 && nativeReplay.updateUploads==std::array<std::uint64_t,4>{},"unchanged second snapshot reuses storage with zero uploads");
        require(!nativeReplay.draw(),"bounded replay cannot draw twice");nativeReplay.close();
        NativeReplay changedReplay;require(changedReplay.seed(vertices,indexBytes,replaySources,legacy),changedReplay.gate);
        auto moved=std::vector<NativeQuadState>(quads.begin(),quads.end());
        replaySources[0].matrix[4]=std::bit_cast<std::uint32_t>(-.2f);
        require(chlorium::research::corners::materialize(replaySources[0],moved[0]),"changed retained pose reference");
        glBufferData(GL_ARRAY_BUFFER,moved.size()*sizeof(NativeQuadState),moved.data(),GL_STATIC_DRAW);glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,GLsizei(moved.size()*6),GL_UNSIGNED_INT,nullptr);const auto movedImage=image();
        require(movedImage!=nativeImage,"changed pose image control");
        const std::span<const unsigned char> movedBytes(reinterpret_cast<const unsigned char*>(moved.data()),moved.size()*sizeof(NativeQuadState));
        require(changedReplay.prepare(movedBytes,indexBytes,replaySources,legacy),changedReplay.gate);
        glClear(GL_COLOR_BUFFER_BIT);require(changedReplay.draw(),changedReplay.gate);
        require(image()==movedImage && changedReplay.reusedStorage,"changed cached pose matches original Cocos image");++cases;
        const auto& uploads=changedReplay.updateUploads;
        require(uploads[0]==0 && uploads[1]>0 && uploads[2]==0 && uploads[3]==0,"pose update uploads only parameter lane");changedReplay.close();
        NativeReplay rejectedReplay;require(rejectedReplay.seed(vertices,indexBytes,replaySources,legacy),rejectedReplay.gate);
        const auto beforeRejection=image();
        require(!rejectedReplay.prepare(vertices,indexBytes.first(indexBytes.size()-2),replaySources,legacy),"incomplete second snapshot rejects");
        require(!rejectedReplay.draw() && image()==beforeRejection,"failed snapshot cannot replay stale seed");rejectedReplay.close();
        replaySources[0].matrix[4]=parameters[1].translationZFlags[0];
        glBufferData(GL_ARRAY_BUFFER,quads.size_bytes(),quads.data(),GL_STATIC_DRAW);glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,GLsizei(quads.size()*6),GL_UNSIGNED_INT,nullptr);
        require(image()==nativeImage,"native stream reference reset");
        GLuint foreignFeedback=0;glGenTransformFeedbacks(1,&foreignFeedback);glBindTransformFeedback(0x8e22,foreignFeedback);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,foreignStorage[4],storageAlignment,64);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,foreignStorage[3]);
        const auto feedbackRange=GLStateSnapshot::captureIndexed(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,GL_TRANSFORM_FEEDBACK_BUFFER_START,GL_TRANSFORM_FEEDBACK_BUFFER_SIZE,0);
        glBindBufferRange(GLStateSnapshot::storageTarget,4,foreignStorage[4],storageAlignment,64);
        glBindBufferRange(GLStateSnapshot::storageTarget,5,foreignStorage[3],storageAlignment,64);
        const auto maskOutputRange=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,5);
        const auto indexOutputRange=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,4);
        NativeReplay streamReplay;streamReplay.requestNativeStream(true);
        const auto beforeSeedBarriers=overwriteBarriers;
        require(streamReplay.seed(vertices,indexBytes,replaySources,legacy),streamReplay.gate);
        require(overwriteBarriers==beforeSeedBarriers,"fresh output has no prior readers to wait for");
        require(image()==nativeImage,"native stream seed does not draw");
        require(streamReplay.prepare(vertices,indexBytes,replaySources,legacy),streamReplay.gate);
        glClear(GL_COLOR_BUFFER_BIT);require(streamReplay.draw(),streamReplay.gate);
        require(image()==nativeImage && streamReplay.reusedStorage,"GPU quads through actual native program match reference");++cases;
        require(overwriteBarriers==beforeSeedBarriers,"unchanged cached stream requires no overwrite barrier");
        require(streamReplay.streamMetrics().quadPasses==1 && streamReplay.streamMetrics().indexPasses==1 && streamReplay.streamMetrics().reuses==1 && streamReplay.streamMetrics().lastSlots==0,"second unchanged native stream does no quad or index work");
        GLint feedback=0,feedbackBuffer=0;glGetIntegerv(0x8e25,&feedback);glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,&feedbackBuffer);
        const auto restoredFeedback=GLStateSnapshot::captureIndexed(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,GL_TRANSFORM_FEEDBACK_BUFFER_START,GL_TRANSFORM_FEEDBACK_BUFFER_SIZE,0);
        require(GLuint(feedback)==foreignFeedback && GLuint(feedbackBuffer)==foreignStorage[3] &&
            restoredFeedback.buffer==feedbackRange.buffer && restoredFeedback.offset==feedbackRange.offset && restoredFeedback.size==feedbackRange.size,"native stream restores foreign feedback object, range and generic binding");
        const auto indexOutputRestored=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,4);
        require(indexOutputRestored.buffer==indexOutputRange.buffer && indexOutputRestored.offset==indexOutputRange.offset && indexOutputRestored.size==indexOutputRange.size,"index compute restores foreign SSBO4 range");
        const auto maskOutputRestored=GLStateSnapshot::captureIndexed(GLStateSnapshot::storageBinding,GLStateSnapshot::storageStart,GLStateSnapshot::storageSize,5);
        require(maskOutputRestored.buffer==maskOutputRange.buffer && maskOutputRestored.offset==maskOutputRange.offset && maskOutputRestored.size==maskOutputRange.size,"quad compute restores foreign SSBO5 range");
        streamReplay.close();glBindTransformFeedback(0x8e22,0);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,0);glDeleteTransformFeedbacks(1,&foreignFeedback);
        const auto baseSources=replaySources;
        const std::vector<NativeQuadState> baseQuads(quads.begin(),quads.end());
        auto checkStreamDelta=[&](std::span<const NativeQuadState> after,std::span<const chlorium::research::pass::SpriteRecord> afterSources,
                                  std::uint64_t changedSlots,bool changedOrder){
            NativeReplay delta;delta.requestNativeStream(true);
            require(delta.seed(vertices,indexBytes,baseSources,legacy),delta.gate);
            std::vector<std::uint16_t> reorderedIndices(after.size()*6);
            for(std::size_t n=0;n<reorderedIndices.size();++n)reorderedIndices[n]=std::uint16_t(n/6*4+topology[n%6]);
            const std::span<const unsigned char> nextVertices(reinterpret_cast<const unsigned char*>(after.data()),after.size_bytes());
            const std::span<const unsigned char> nextIndices(reinterpret_cast<const unsigned char*>(reorderedIndices.data()),reorderedIndices.size()*2);
            glBindVertexArray(nativeVao);glBindBuffer(GL_ARRAY_BUFFER,nativeVbo);
            glBufferData(GL_ARRAY_BUFFER,after.size_bytes(),after.data(),GL_STATIC_DRAW);glClear(GL_COLOR_BUFFER_BIT);
            glDrawElements(GL_TRIANGLES,GLsizei(after.size()*6),GL_UNSIGNED_INT,nullptr);const auto wanted=image();
            require(delta.prepare(nextVertices,nextIndices,afterSources,legacy),delta.gate);
            const auto beforeUpdateBarriers=overwriteBarriers;
            glClear(GL_COLOR_BUFFER_BIT);require(delta.draw(),delta.gate);
            require(overwriteBarriers==beforeUpdateBarriers+1,"cached GPU rewrites wait for prior vertex/index reads");
            require(image()==wanted,"partial GPU slot/index update differs from native image");++cases;
            const auto stats=delta.streamMetrics();
            require(stats.lastSlots==changedSlots && stats.quadPasses==1+(changedSlots!=0) && stats.indexPasses==1+changedOrder,"only exact dirty GPU slots and changed order execute");
            delta.close();
        };
        auto poseSources=baseSources;auto poseQuads=baseQuads;
        poseSources[0].matrix[4]=std::bit_cast<std::uint32_t>(-.2f);
        require(chlorium::research::corners::materialize(poseSources[0],poseQuads[0]),"partial GPU pose reference");
        checkStreamDelta(poseQuads,poseSources,1,false);
        auto reverseSources=baseSources;auto reverseQuads=baseQuads;
        std::reverse(reverseSources.begin(),reverseSources.end());std::reverse(reverseQuads.begin(),reverseQuads.end());
        checkStreamDelta(reverseQuads,reverseSources,0,true);
        checkStreamDelta(std::span<const NativeQuadState>(baseQuads).subspan(1),std::span<const chlorium::research::pass::SpriteRecord>(baseSources).subspan(1),0,true);
        auto fallbackSources=baseSources;auto fallbackQuads=baseQuads;
        fallbackSources[0].quadMatches=0;fallbackQuads[0].corners[0].x+=.125f;fallbackQuads[0].corners[2].color=0x50402010;
        checkStreamDelta(fallbackQuads,fallbackSources,1,false);
        auto replacementSources=baseSources;++replacementSources[0].id;
        checkStreamDelta(baseQuads,replacementSources,1,true);
        std::vector<chlorium::research::pass::SpriteRecord> addedSources(baseSources.begin(),baseSources.end());
        auto addedQuads=baseQuads;auto added=baseSources[0];added.sprite=0x8000;added.id=777;added.matrix[4]=std::bit_cast<std::uint32_t>(.25f);
        auto addedQuad=baseQuads[0];require(chlorium::research::corners::materialize(added,addedQuad),"added GPU slot reference");
        addedSources.push_back(added);addedQuads.push_back(addedQuad);checkStreamDelta(addedQuads,addedSources,1,true);
        auto zeroQuads=baseQuads;for(auto& corner:zeroQuads[0].corners)corner.color=0;
        glBlendFunc(GL_ONE,GL_ZERO);checkStreamDelta(zeroQuads,baseSources,1,false);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        GLuint query=0;glGenQueries(1,&query);glBeginQuery(GL_PRIMITIVES_GENERATED,query);
        NativeReplay observedSeed;observedSeed.requestNativeStream(true);const auto beforeObserved=image();
        require(!observedSeed.seed(vertices,indexBytes,baseSources,legacy),"active native primitive observer blocks GPU seed work");
        GLint stillActive=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&stillActive);
        require(GLuint(stillActive)==query && !observedSeed.draw() && image()==beforeObserved,"query rejection preserves observer and framebuffer");
        glEndQuery(GL_PRIMITIVES_GENERATED);glDeleteQueries(1,&query);observedSeed.close();

        NativeReplay continuous(true);continuous.requestNativeStream(true);
        chlorium::research::batchreplay::TargetBackup rollback;
        std::uint64_t originalCalls=0;
        for(unsigned step=0;step<320;++step){
            auto nextSources=baseSources;auto nextQuads=baseQuads;
            const unsigned amount=step%3?2:1;
            for(unsigned n=0;n<amount;++n){
                nextSources[n].sprite=0x100000+std::uint64_t(step/2)*32+n*8;
                nextSources[n].id=1000+step/2*2+n;
                nextSources[n].matrix[4]=std::bit_cast<std::uint32_t>(-.6f+.1f*float((step+n)%9));
                require(chlorium::research::corners::materialize(nextSources[n],nextQuads[n]),"continuous pose oracle");
                for(auto& corner:nextQuads[n].corners)corner.color=0x80604020u+step%16;
            }
            nextQuads.resize(amount);
            glBlendFunc(step%3?GL_ONE:GL_SRC_ALPHA,step%3?GL_ONE_MINUS_SRC_ALPHA:GL_ONE);
            glUseProgram(legacy);glBindVertexArray(nativeVao);glBindBuffer(GL_ARRAY_BUFFER,nativeVbo);
            glBufferData(GL_ARRAY_BUFFER,nextQuads.size()*96,nextQuads.data(),GL_DYNAMIC_DRAW);
            glClear(GL_COLOR_BUFFER_BIT);glDrawElements(GL_TRIANGLES,amount*6,GL_UNSIGNED_INT,nullptr);
            const auto expectedImage=image();glClear(GL_COLOR_BUFFER_BIT);const auto empty=image();
            require(expectedImage!=empty,"continuous visible native reference");
            const std::span<const unsigned char> nextVertices(reinterpret_cast<const unsigned char*>(nextQuads.data()),nextQuads.size()*96);
            require(continuous.prepare(nextVertices,indexBytes.first(amount*12),std::span(nextSources).first(amount),legacy),continuous.gate);
#ifdef CHLORIUM_RESIDENT_FAST_R58
            struct Checkpoint {chlorium::research::batchreplay::TargetBackup& backup;unsigned calls=0;} checkpoint{rollback};
            const auto save=+[](void* opaque){auto& state=*static_cast<Checkpoint*>(opaque);++state.calls;return state.backup.begin();};
            if(!step){
                const auto prior=continuous.calls;
                glEnable(GL_DEPTH_TEST);
                require(!continuous.draw(save,&checkpoint) && !checkpoint.calls && continuous.calls==prior && image()==empty,"pre-submit rejection cannot copy or alter the target");
                glDisable(GL_DEPTH_TEST);
                const auto refuse=+[](void* opaque){++*static_cast<unsigned*>(opaque);return false;};
                unsigned refused=0;
                require(!continuous.draw(refuse,&refused) && refused==1 && continuous.calls==prior && image()==empty,"failed checkpoint prevents submission without changing pixels");
            }
            require(continuous.draw(save,&checkpoint) && checkpoint.calls==1,continuous.gate);
#else
            require(rollback.begin(),"continuous GPU rollback snapshot");
            require(continuous.draw(),continuous.gate);
#endif
            glBindVertexArray(nativeVao);
            if(step%19==0){
                glClear(GL_COLOR_BUFFER_BIT);require(rollback.restore(),"recover a damaged candidate target");
                require(image()==empty,"rollback recovers before-target exactly");
                glDrawElements(GL_TRIANGLES,amount*6,GL_UNSIGNED_INT,nullptr);++originalCalls;
            }else{
                glEnable(GL_RASTERIZER_DISCARD);glDrawElements(GL_TRIANGLES,amount*6,GL_UNSIGNED_INT,nullptr);++originalCalls;
                glDisable(GL_RASTERIZER_DISCARD);rollback.commit();
            }
            require(originalCalls==step+1 && image()==expectedImage,"continuous rendering, eviction and original callback exactly once");
            require(!glIsEnabled(GL_RASTERIZER_DISCARD),"native raster state restored");++cases;
        }
#ifdef CHLORIUM_RESIDENT_FAST_R58
        {
            chlorium::research::continuous::NativeDrawSuppression suppress;
            GLuint generated=0;glGenQueries(1,&generated);
            const std::array<std::uint32_t,4> uploaded{17,23,42,91};
            GLuint scratch=0;glGenBuffers(1,&scratch);
            const auto before=image();unsigned callbacks=0;
            require(suppress.begin(),"owned zero predicate starts suppression");
            require(!suppress.begin(),"nested suppression cannot disturb owned predicate");
            glBeginQuery(GL_PRIMITIVES_GENERATED,generated);
            ++callbacks;glBindBuffer(GL_COPY_WRITE_BUFFER,scratch);
            glBufferData(GL_COPY_WRITE_BUFFER,sizeof(uploaded),uploaded.data(),GL_DYNAMIC_DRAW);
            glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
            glEndQuery(GL_PRIMITIVES_GENERATED);require(suppress.end(),"owned conditional scope closes");
            GLuint primitives=1;glGetQueryObjectuiv(generated,GL_QUERY_RESULT,&primitives);
            std::array<std::uint32_t,4> copied{};
            glGetBufferSubData(GL_COPY_WRITE_BUFFER,0,sizeof(copied),copied.data());
            require(callbacks==1 && primitives==0 && copied==uploaded && image()==before,"suppression preserves upload/callback/pixels and prevents duplicate primitives");
            glClear(GL_COLOR_BUFFER_BIT);require(image()!=before,"ending suppression restores rendering commands");
            suppress.close();require(suppress.begin(),"predicate can be recreated after close");require(suppress.end(),"recreated conditional scope closes");
            glBindBuffer(GL_COPY_WRITE_BUFFER,0);glDeleteBuffers(1,&scratch);glDeleteQueries(1,&generated);
            require(glGetError()==GL_NO_ERROR,"suppression leaves no GL error");++cases;
        }
#endif
        require(continuous.calls==320 && continuous.reusedStorage,"continuous path remains active beyond one replay and cache capacity churn");
        continuous.close();glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);

        require(_putenv_s("CHLORIUM_RESIDENT_VERTEX_FILE",savedPath.c_str())==0,"restore owned replay environment");
        glUseProgram(0);glDeleteProgram(legacy);
    }
    renderer.close();glDeleteProgram(nativeProgram);glDeleteBuffers(1,&nativeVbo);glDeleteBuffers(1,&nativeIbo);
    glDeleteBuffers(5,foreignStorage);
    glDeleteVertexArrays(1,&nativeVao);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(2,textures);
    std::cout<<"owned retained renderer cases="<<cases<<"; NOT game FPS or native frame parity\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
