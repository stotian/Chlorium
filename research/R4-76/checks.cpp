#define CHLORIUM_R74_REPLAY_LIBRARY_ONLY 1
#include "../R4-74/joint_replay.cpp"
extern "C" void R76Follow(void*,const mp::Command*,void*);
void advanceFollow(MixedWorld& world,chlorium::research::movement::Oracle::Move movement,std::span<const mp::Command> commands,unsigned prefix){
    for(unsigned k=0;k<prefix;++k){const auto& c=commands[k];store(world.world.layer.data(),0x3e0,c.control[0]);
        if(c.control[1]==2)for(auto* object:world.world.members[k])R76Follow(object,&c,world.world.layer.data());
        else if(c.control[1]==1)for(auto* object:world.world.members[k])R71Rotation(object,&c);
        else movement(world.world.layer.data(),&world.world.arrays[k],c.delta[0],c.delta[1]);}
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==4,"GD, Cocos and mixed.comp required");
    const unsigned saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);const auto cocos=loadNative(argv[2],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};
    r71PointCtor=reinterpret_cast<void*>(GetProcAddress(cocos,"??0CCPoint@cocos2d@@QEAA@MM@Z"));r71PointApply=reinterpret_cast<void*>(GetProcAddress(cocos,"?__CCPointApplyAffineTransform@cocos2d@@YA?AVCCPoint@1@AEBV21@AEBUCCAffineTransform@1@@Z"));
    require(r71PointCtor && r71PointApply && hash(static_cast<unsigned char*>(r71PointApply),0xae)==0x1e32bc1900bee292ull,"pinned native Cocos");
    chlorium::research::movement::Oracle movement(argv[1],reinterpret_cast<void*>(&countArray));std::array<void*,148> table{};table[0x490/8]=reinterpret_cast<void*>(&section);table[0x498/8]=reinterpret_cast<void*>(&end);
    JointFrame frame;frame.commands.resize(192);for(unsigned k=0;k<frame.commands.size();++k){auto& c=frame.commands[k];c.control[0]=k/5;c.control[1]=k%7?2u:unsigned(k%2);c.control[2]=c.control[1]==2?k%4:k%2;
        if(c.control[1]==1){c.linear[0]=c.linear[3]=1;c.linear[1]=.125f;c.linear[2]=-.25f;c.affineCenter[0]=.1f;c.affineCenter[2]=2.5f;}
        else{c.delta[0]=k%5?double(int(k%11)-5)*.1:0.;c.delta[1]=k%6?double(int(k%7)-3)*.125:-0.;}}
    frame.commands[0]={};frame.commands[0].control[1]=2;frame.commands[0].control[2]=3;frame.commands[0].control[0]=7;
    for(unsigned count:{192u,129u,65u,64u,0u}){frame.ranges.push_back({unsigned(frame.indices.size()),count});for(unsigned k=0;k<count;++k)frame.indices.push_back(k && k%17==0?k-1:k);}
    std::vector<mp::State> seed(515);for(unsigned i=0;i<seed.size();++i){auto& s=seed[i];s.position[0]=i%13?double(i)*.1:-0.;s.position[1]=i%11?-double(i)*.125:-0.;s.flags=(i%frame.ranges.size())<<16;s.flags|=(i%7==0?1u:0u)|(i%17==0?2u:0u);s.stamp=i%4;s.offset[0]=.125f;s.offset[1]=-.25f;}
    MixedGpu gpu(argv[3],seed);unsigned compared=0;
    for(unsigned mode:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned prefix:{0u,1u,2u,63u,64u,65u,127u,128u,129u,191u,192u}){
        MixedWorld native(seed,frame.commands,frame.ranges,frame.indices,table.data());_mm_setcsr(mode);advanceFollow(native,movement.native(),frame.commands,prefix);const auto expected=native.states();_mm_setcsr(mode);
        auto actual=runChunks(gpu,seed,frame,prefix,true);require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(mp::State)),"native/full-state Follow mixed program");compared+=unsigned(seed.size());
        if(prefix==1)require(std::bit_cast<std::uint64_t>(actual[0].position[1])==0,"enabled zero Follow preserves native signed-zero addition semantics");}
    MixedWorld native(seed,frame.commands,frame.ranges,frame.indices,table.data());for(unsigned f=0;f<4;++f){for(unsigned k=0;k<frame.commands.size();++k)frame.commands[k].control[0]=f*1000+k/5;
        _mm_setcsr(0x1f80);advanceFollow(native,movement.native(),frame.commands,192);auto expected=native.states();_mm_setcsr(0x1f80);auto actual=runChunks(gpu,seed,frame,192,f==0);require(!std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(mp::State)),"four resident Follow frames");compared+=unsigned(seed.size());}
    std::cout<<"PASS "<<compared<<" complete native/GPU mixed Follow states; four FP modes, 192-step programs, zero axes, duplicates and four resident frames; no GD FPS claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
