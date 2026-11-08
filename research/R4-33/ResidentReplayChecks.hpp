#pragma once
struct ReplayWork {std::uint64_t geometryBytes=0,parameterBytes=0,fallbackBytes=0,orderBytes=0,nativeQuadBytes=0;unsigned retained=0,added=0;};
ReplayWork checkResidentReplay(const std::filesystem::path& shaderPath,const std::vector<Captured>& records) {
    using Key=std::array<std::uint64_t,12>;
    std::map<Key,unsigned> slots;
    std::vector<Geometry> geometry;std::vector<Parameters> parameters;std::vector<NativeQuadState> fallback;
    const auto capacity=records.size();require(capacity<=2*chlorium::research::pass::maxQuads+4096,"resident replay budget");
    geometry.reserve(capacity);parameters.reserve(capacity);fallback.reserve(capacity);
    GLuint vao=0,buffers[5]{};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(5,buffers);
    std::vector<NativeQuadState> uninitialized(capacity);for(auto& quad:uninitialized)poison(quad);
    for(unsigned i=0;i<4;++i){glBindBuffer(storage,buffers[i]);
        const auto stride=i==0?sizeof(Geometry):i==1?sizeof(Parameters):i==2?sizeof(unsigned):sizeof(NativeQuadState);
        glBufferData(storage,capacity*stride,i==3?uninitialized.data():nullptr,GL_DYNAMIC_DRAW);glBindBufferBase(storage,i,buffers[i]);}
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[4]);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,capacity*sizeof(NativeQuadState),nullptr,GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffers[4]);
    const auto shader=program(shaderPath,GL_VERTEX_SHADER,true);glUseProgram(shader);
    ReplayWork second;
    // shortcut: replay only two immutable captures; an engine adapter needs dirty notifications and fenced lifetime reuse.
    for(unsigned frame=0;frame<2;++frame){
        std::vector<unsigned> order,dirtyGeometry,dirtyParameters,dirtyFallback;std::vector<NativeQuadState> expected;
        std::map<Key,bool> seen;ReplayWork work;
        for(const auto& record:records)if(record.frame==frame){
            require(seen.emplace(record.resource,true).second,"ambiguous resident owner: use native pass");
            auto [at,fresh]=slots.try_emplace(record.resource,static_cast<unsigned>(geometry.size()));const auto slot=at->second;
            Geometry g{};Parameters p{};encodeResident(record,g,p);
            if(fresh){geometry.emplace_back();parameters.emplace_back();fallback.emplace_back();++work.added;}else ++work.retained;
            const bool wasSupported=geometry[slot].control[0]!=0;
            if(fresh||std::memcmp(&geometry[slot],&g,sizeof(g))){geometry[slot]=g;dirtyGeometry.push_back(slot);}
            if(fresh||std::memcmp(&parameters[slot],&p,sizeof(p))){parameters[slot]=p;dirtyParameters.push_back(slot);}
            if(!g.control[0]&&(fresh||wasSupported||std::memcmp(&fallback[slot],&record.quad,sizeof(record.quad)))){fallback[slot]=record.quad;dirtyFallback.push_back(slot);}
            order.push_back(slot);expected.push_back(record.quad);
        }
        require(!order.empty()&&geometry.size()<=capacity,"bounded complete resident order");
        auto patch=[&](unsigned binding,const auto& values,std::vector<unsigned>& dirty){
            using Value=typename std::decay_t<decltype(values)>::value_type;std::uint64_t bytes=0;
            std::sort(dirty.begin(),dirty.end());glBindBuffer(storage,buffers[binding]);
            // Dense parameter changes use one upload, rather than thousands of driver calls.
            if(!dirty.empty()&&dirty.size()*4>values.size()){
                glBufferSubData(storage,0,values.size()*sizeof(Value),values.data());return std::uint64_t(values.size()*sizeof(Value));
            }
            for(std::size_t begin=0;begin<dirty.size();){std::size_t end=begin+1;
                while(end<dirty.size()&&dirty[end]==dirty[end-1]+1)++end;
                const auto count=dirty[end-1]-dirty[begin]+1;
                glBufferSubData(storage,dirty[begin]*sizeof(Value),count*sizeof(Value),values.data()+dirty[begin]);bytes+=count*sizeof(Value);begin=end;
            }
            return bytes;
        };
        work.geometryBytes=patch(0,geometry,dirtyGeometry);work.parameterBytes=patch(1,parameters,dirtyParameters);work.fallbackBytes=patch(3,fallback,dirtyFallback);
        glBindBuffer(storage,buffers[2]);work.orderBytes=order.size()*sizeof(order[0]);glBufferSubData(storage,0,work.orderBytes,order.data());
        // Every upload and the complete order are validated BEFORE the first draw.
        require(glGetError()==GL_NO_ERROR,"resident uploads: otherwise select the whole native pass");
        std::vector<NativeQuadState> poisoned(order.size()),actual(order.size());for(auto& value:poisoned)poison(value);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[4]);glBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,poisoned.size()*sizeof(poisoned[0]),poisoned.data());
        glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(order.size()*4));glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,actual.size()*sizeof(actual[0]),actual.data());
        require(glGetError()==GL_NO_ERROR&&mismatch(actual,expected)==0,"persistent two-frame replay must match captured native quads");
        work.nativeQuadBytes=expected.size()*sizeof(expected[0]);if(frame)second=work;
    }
    glDeleteBuffers(5,buffers);glDeleteVertexArrays(1,&vao);glDeleteProgram(shader);return second;
}
void checkReplayTransitions(const std::filesystem::path& shaderPath) {
    std::vector<Captured> records;
    for(unsigned frame=0;frame<2;++frame)for(unsigned n=0;n<5;++n){
        Captured record{};record.frame=frame;record.resource[10]=100+n;record.resource[11]=1;
        auto& source=record.sprite;source.flags=512|1024;
        source.matrix[0]=source.matrix[3]=0x3f800000u;
        if(n==0&&frame)source.flags|=64; // Zero native fallback still MUST be initialized.
        if(n==1&&!frame)source.flags|=64;
        if(n==2){for(auto& vertex:record.quad.corners){vertex.color=frame?0xff00ff00:0xffff0000;vertex.u=frame?.5f:0.f;}}
        if(n==3){source.localRect[2]=frame?0x40000000u:0x3f800000u;source.localRect[3]=0x3f800000u;}
        if(n==4){source.localRect[2]=source.localRect[3]=0x3f800000u;if(frame)source.flags&=~512u;}
        if(chlorium::research::corners::domain(source)){_mm_setcsr(0x1f80);require(chlorium::research::corners::materialize(source,record.quad),"synthetic resident transition");}
        records.push_back(record);
    }
    checkResidentReplay(shaderPath,records);
}
