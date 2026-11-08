#include "RendererTiming.hpp"
#include <iostream>
#include <stdexcept>
using namespace chlorium::research::residenttiming;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main()try{
    require(stats.frequency>0,"monotonic counter frequency");
    const auto at=[](Stage stage){return static_cast<unsigned>(stage);};
    SetLastError(12345);
    {Scope disabled(Stage::draw,false);BatchScope disabledBatch(false);}
    require(stats.calls[at(Stage::draw)]==0 && batch==nullptr,"disabled scopes do nothing");
    {BatchScope outer(true);{Scope atlas(Stage::atlas);}
     {BatchScope inner(true);Scope atlas(Stage::atlas);}}
    require(GetLastError()==12345,"clock probes preserve Win32 error state");
    require(stats.calls[at(Stage::atlas)]==2 && stats.calls[at(Stage::batchTotal)]==1 &&
            stats.calls[at(Stage::batchOutsideAtlas)]==2,"nested batches have one root total");
    require(stats.ticks[at(Stage::batchOutsideAtlas)]+stats.ticks[at(Stage::atlas)]==stats.ticks[at(Stage::batchTotal)],"nested batch time is not counted twice");
    {Scope once(Stage::draw);once.finish();once.finish();}
    require(stats.calls[at(Stage::draw)]==1,"explicit finish and destructor record only once");
    require(batch==nullptr,"batch context restored");
    std::cout<<"R60 CPU timing bookkeeping passed; no performance claim\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
