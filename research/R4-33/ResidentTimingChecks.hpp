#pragma once
// Isolated transfer/vertex kernel cost. Shared arithmetic is retained in BOTH
// paths; no missing gameplay/callback work is credited as an optimization.
void residentTimings(const std::filesystem::path& shaderPath,const std::vector<Captured>& all,const std::filesystem::path& output) {
    std::vector<Captured> records;for(const auto& record:all)if(!record.frame)records.push_back(record);
    require(!records.empty(),"timing capture frame");const auto count=records.size();
    std::ofstream csv(output,std::ios::out|std::ios::noreplace);require(bool(csv),"refuse existing kernel timings");
    csv<<"batch,mode,iteration,quads,cpu_issue_ms,gpu_draw_ms,batch_wall_ms,game_fps_evidence\n";
    std::vector<Geometry> geometry(count);std::vector<Parameters> parameters(count);std::vector<unsigned> order(count);
    std::vector<NativeQuadState> quads(count);std::iota(order.begin(),order.end(),0u);
    for(std::size_t i=0;i<count;++i){encodeResident(records[i],geometry[i],parameters[i]);quads[i]=records[i].quad;}
    GLuint buffers[6]{},vaos[2]{};glGenBuffers(6,buffers);glGenVertexArrays(2,vaos);
    auto upload=[&](unsigned binding,const void* data,std::size_t bytes){glBindBuffer(storage,buffers[binding]);glBufferData(storage,bytes,data,GL_DYNAMIC_DRAW);glBindBufferBase(storage,binding,buffers[binding]);};
    upload(0,geometry.data(),geometry.size()*sizeof(Geometry));upload(1,parameters.data(),parameters.size()*sizeof(Parameters));
    upload(2,order.data(),order.size()*sizeof(unsigned));upload(3,quads.data(),quads.size()*sizeof(quads[0]));
    glBindVertexArray(vaos[0]);glBindBuffer(GL_ARRAY_BUFFER,buffers[4]);glBufferData(GL_ARRAY_BUFFER,count*sizeof(NativeQuadState),quads.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,24,reinterpret_cast<void*>(12));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,24,reinterpret_cast<void*>(16));glEnableVertexAttribArray(2);
    const GLuint programs[]{program(shaderPath.parent_path()/"direct.vert",GL_VERTEX_SHADER,true),program(shaderPath,GL_VERTEX_SHADER,true)};
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[5]);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,count*sizeof(NativeQuadState),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffers[5]);
    constexpr unsigned repeats=16;GLuint queries[repeats]{};glGenQueries(repeats,queries);
    std::array<std::vector<double>,2> wall;
    glEnable(GL_RASTERIZER_DISCARD);
    for(unsigned batch=0;batch<10;++batch)for(unsigned turn=0;turn<2;++turn){
        const unsigned mode=(batch+turn)%2;glUseProgram(programs[mode]);glBindVertexArray(vaos[mode]);glFinish();
        std::array<double,repeats> issue{};const auto begin=std::chrono::steady_clock::now();
        for(unsigned n=0;n<repeats;++n){const auto start=std::chrono::steady_clock::now();
            std::uint64_t checksum=0;
            for(std::size_t i=0;i<count;++i){
                // The native position workload is still required by the current engine contract.
                chlorium::research::corners::materialize(records[i].sprite,quads[i]);
                for(const auto& v:quads[i].corners)checksum+=std::bit_cast<std::uint32_t>(v.x)^std::bit_cast<std::uint32_t>(v.y)^std::bit_cast<std::uint32_t>(v.z);
                if(mode)encodeParameters(records[i],parameters[i]);
            }
            static volatile std::uint64_t retainedArithmeticSink=0;retainedArithmeticSink=checksum;
            if(mode){glBindBuffer(storage,buffers[1]);glBufferSubData(storage,0,parameters.size()*sizeof(Parameters),parameters.data());}
            else{glBindBuffer(GL_ARRAY_BUFFER,buffers[4]);glBufferSubData(GL_ARRAY_BUFFER,0,quads.size()*sizeof(quads[0]),quads.data());}
            glBeginQuery(0x88bf,queries[n]);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(count*4));glEndTransformFeedback();glEndQuery(0x88bf);
            issue[n]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        }
        glFinish();const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/repeats;
        require(glGetError()==GL_NO_ERROR,"resident timing commands");
        if(batch)wall[mode].push_back(elapsed);
        for(unsigned n=0;n<repeats;++n){GLuint64 nanos=0;glGetQueryObjectui64v(queries[n],GL_QUERY_RESULT,&nanos);
            if(batch)csv<<batch<<','<<(mode?"resident_shared_cpu_retained":"direct_shared_cpu_retained")<<','<<n<<','<<count<<','<<issue[n]<<','<<nanos/1e6<<','<<elapsed<<",false\n";}
    }
    glDisable(GL_RASTERIZER_DISCARD);glDeleteQueries(repeats,queries);glDeleteBuffers(6,buffers);glDeleteVertexArrays(2,vaos);for(auto value:programs)glDeleteProgram(value);
    require(bool(csv),"write kernel timings");
    for(auto& values:wall)std::sort(values.begin(),values.end());
    std::cout<<"Isolated shared-CPU-retained batch median ms: direct="<<wall[0][4]<<", resident="<<wall[1][4]<<"; NOT gameplay frametimes or FPS\n";
}
