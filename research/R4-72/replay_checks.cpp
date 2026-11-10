#define CHLORIUM_R71_FIXTURE_ONLY 1
#include "../R4-71/checks.cpp"
#include "TraceAdapter.hpp"
namespace rt=chlorium::research::rotationtrace;
static std::vector<rt::Row> readTrace(const std::filesystem::path& path){
    require(std::filesystem::file_size(path)>=sizeof(rt::Header) && std::filesystem::file_size(path)<=sizeof(rt::Header)+65536*sizeof(rt::Row),"bounded native rotation witness file");
    std::ifstream input(path,std::ios::binary);rt::Header header;input.read(reinterpret_cast<char*>(&header),sizeof(header));
    require(input && ((!std::memcmp(header.magic,"CHLROT72",8) && header.version==72) || (!std::memcmp(header.magic,"CHLMIX74",8) && header.version==74)) && header.rowBytes==sizeof(rt::Row) && header.frames==2 && !header.reasons && header.rows && header.rows<=65536 && header.actors && header.actors<=128 && header.calls>=header.rows && header.owner && header.ownerUid && header.thread,"complete native rotation witness header");
    require(std::filesystem::file_size(path)==sizeof(header)+header.rows*sizeof(rt::Row),"native witness exact length");std::vector<rt::Row> rows(header.rows);input.read(reinterpret_cast<char*>(rows.data()),rows.size()*sizeof(rt::Row));require(bool(input),"complete native witness read");
    unsigned prior=0;std::map<std::pair<std::uint64_t,unsigned>,bool> actors;unsigned frames=0;
    for(const auto& row:rows){require(row.closed==1 && row.address && !(row.address&7) && row.uid && row.frame>=prior && row.frame>=1 && row.frame<=2 && !(row.reserved&~(header.version==74?0x3ff03u:0u)) && !(row.controls&~0xffffu),"closed native row identity, frame and control metadata");prior=row.frame;frames|=1u<<row.frame;actors[{row.address,row.uid}]=true;
        require(!(row.before.flags&~255u) && !(row.after.flags&~255u) && !row.before.reserved[0] && !row.before.reserved[1] && !row.after.reserved[0] && !row.after.reserved[1] && row.command.control[1]<=1 && (header.version==74 || row.command.control[1]==1),"known native state representation");}
    require(frames==6 && actors.size()==header.actors,"both native witness frames and actor identities");return rows;
}
#ifndef CHLORIUM_R72_REPLAY_LIBRARY_ONLY
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3,"native R72 witness and mixed.comp required");const auto rows=readTrace(argv[1]);
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);unsigned admitted=0,rejected=0,positionFailures=0,fullStates=0,offsetChanges=0,uniquePrograms=0;
    for(unsigned base=0;base<rows.size();base+=4096){std::vector<mp::State> seed;std::vector<mp::Command> commands;std::vector<Range> ranges;std::vector<unsigned> indices;std::vector<const rt::Row*> references;
        std::map<std::array<unsigned char,sizeof(mp::Command)>,unsigned> programIds;
        for(unsigned i=base;i<std::min<unsigned>(unsigned(rows.size()),base+4096);++i){const auto& row=rows[i];const std::array<Range,1> oneRange{{{0,1}}};const std::array<unsigned,1> oneIndex{0};
            if(!mp::fixtureInput({&row.before,1},{&row.command,1},oneRange,oneIndex,row.controls)){++rejected;continue;}
            std::array<unsigned char,sizeof(mp::Command)> key;std::memcpy(key.data(),&row.command,key.size());auto found=programIds.find(key);unsigned id=0;
            if(found==programIds.end()){id=unsigned(commands.size());programIds.emplace(key,id);commands.push_back(row.command);ranges.push_back({unsigned(indices.size()),1});indices.push_back(id);}else id=found->second;
            auto state=row.before;state.flags|=id<<16;seed.push_back(state);references.push_back(&row);
        }
        if(seed.empty())continue;_mm_setcsr(0x1f80);MixedGpu gpu(argv[2],seed);const auto actual=gpu.run(seed,commands,ranges,indices,unsigned(commands.size()),true);uniquePrograms+=unsigned(commands.size());
        for(unsigned i=0;i<actual.size();++i){auto expected=references[i]->after;expected.flags|=seed[i].flags&0xffff0000u;
            if(std::memcmp(actual[i].position,expected.position,16)){if(positionFailures<5)std::cerr<<"root mismatch row="<<base+i<<" expected="<<std::hexfloat<<expected.position[0]<<','<<expected.position[1]<<" actual="<<actual[i].position[0]<<','<<actual[i].position[1]<<'\n';++positionFailures;}
            fullStates+=!std::memcmp(&actual[i],&expected,sizeof(mp::State));offsetChanges+=std::memcmp(references[i]->before.offset,expected.offset,8)!=0;++admitted;
        }
    }
    std::cout<<"{\"diagnostic_only\":true,\"captured_rows\":"<<rows.size()<<",\"admitted_rows\":"<<admitted<<",\"rejected_rows\":"<<rejected<<",\"position_mismatches\":"<<positionFailures<<",\"complete_state_matches\":"<<fullStates<<",\"changed_offsets\":"<<offsetChanges<<",\"shared_programs_per_batch_sum\":"<<uniquePrograms<<",\"global_reader_closure\":false,\"game_fps_evidence\":false}\n";
    require(admitted && !positionFailures,"captured native/GPU root-position witnesses");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#endif
