#define CHLORIUM_R74_REPLAY_LIBRARY_ONLY 1
#include "joint_replay.cpp"
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4,"GD, Cocos and mixed shader required");
    const unsigned saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);auto cocos=loadNative(argv[2],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};
    r71PointCtor=reinterpret_cast<void*>(GetProcAddress(cocos,"??0CCPoint@cocos2d@@QEAA@MM@Z"));
    r71PointApply=reinterpret_cast<void*>(GetProcAddress(cocos,"?__CCPointApplyAffineTransform@cocos2d@@YA?AVCCPoint@1@AEBV21@AEBUCCAffineTransform@1@@Z"));
    require(r71PointCtor && r71PointApply && hash(static_cast<unsigned char*>(r71PointApply),0xae)==0x1e32bc1900bee292ull,"pinned Cocos affine");
    chlorium::research::movement::Oracle movement(argv[1],reinterpret_cast<void*>(&countArray));std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    JointFrame frame;frame.commands.resize(192);
    for(unsigned i=0;i<frame.commands.size();++i){auto& c=frame.commands[i];c.control[0]=i/3;c.control[1]=i%3==0;c.control[2]=i%2;
        if(c.control[1]){c.linear[0]=c.linear[3]=1;c.linear[1]=.125f;c.linear[2]=-.25f;c.affineCenter[0]=.1f;c.affineCenter[1]=-.125f;c.affineCenter[2]=2.5f;c.move[0]=.125f;}
        else{c.delta[0]=double(int(i%7)-3)*.1;c.delta[1]=double(int(i%5)-2)*.125;}}
    for(unsigned count:{192u,129u,65u,64u,0u}){frame.ranges.push_back({unsigned(frame.indices.size()),count});for(unsigned i=0;i<count;++i)frame.indices.push_back(i && i%17==0?i-1:i);}
    std::vector<mp::State> seed(35);for(unsigned i=0;i<seed.size();++i){auto& s=seed[i];s.position[0]=double(i)*.1;s.position[1]=-double(i)*.125;s.flags=(i%frame.ranges.size())<<16;s.flags|=(i%11==0?1u:0u)|(i%13==0?2u:0u);s.offset[0]=.125f;s.offset[1]=-.25f;s.stamp=UINT_MAX;}
    MixedGpu gpu(argv[3],seed);unsigned compared=0;
    for(unsigned mode:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned prefix:{0u,1u,63u,64u,65u,127u,128u,129u,191u,192u}){
        MixedWorld native(seed,frame.commands,frame.ranges,frame.indices,table.data());_mm_setcsr(mode);native.advance(movement.native(),frame.commands,prefix);const auto expected=native.states();_mm_setcsr(mode);
        const auto actual=runChunks(gpu,seed,frame,prefix,true);require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(mp::State)),"native/full-state chunk boundary equivalence");compared+=unsigned(seed.size());}
    MixedWorld native(seed,frame.commands,frame.ranges,frame.indices,table.data());
    for(unsigned f=0;f<4;++f){for(unsigned i=0;i<frame.commands.size();++i)frame.commands[i].control[0]=f*1000+i/3;_mm_setcsr(0x1f80);native.advance(movement.native(),frame.commands,192);auto expected=native.states();_mm_setcsr(0x1f80);
        auto actual=runChunks(gpu,seed,frame,192,f==0);require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(mp::State)),"four resident frames without position reseed");compared+=unsigned(seed.size());}
    auto bad=frame;bad.ranges[0][0]=UINT_MAX;bool rejected=false;try{runChunks(gpu,seed,bad,192,true);}catch(...){rejected=true;}require(rejected,"overflowing range rejected before dispatch");
    bad=frame;bad.indices[64]=0;rejected=false;try{runChunks(gpu,seed,bad,192,true);}catch(...){rejected=true;}require(rejected,"cross-chunk disorder rejected before dispatch");
    std::cout<<"PASS "<<compared<<" full native/GPU states; 192-step programs, duplicates, empty classes, four FP modes and four resident frames; no game FPS claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
