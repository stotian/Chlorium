#define CHLORIUM_R74_REPLAY_LIBRARY_ONLY 1
#include "../R4-74/joint_replay.cpp"
#include "RootRuntime.hpp"
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3,"native root witness and mixed.comp required");const auto rows=readTrace(argv[1]);auto roster=chlorium::research::liveprogram::seed(rows,1);auto previous=roster.initial;std::vector<unsigned char> disabled(roster.keys.size(),0);
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);chlorium::research::canonicaldraw::RootRuntime gpu(argv[2],roster.initial);unsigned compared=0;
    for(unsigned f=1;f<=2;++f){auto frame=chlorium::research::liveprogram::compile(rows,f,roster,previous,disabled);auto actual=gpu.advance(frame);for(unsigned i=0;i<actual.size();++i)if(!disabled[i]){require(!std::memcmp(actual[i].position,frame.expected[i].position,16),"live runtime/native resident root frame");++compared;}previous=frame.expected;}
    require(compared>0,"nonempty live root runtime witness");std::cout<<"PASS "<<compared<<" actual root positions over two frames; one GPU seed, no per-chunk readback, native positions not reseeded; no GD FPS claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
