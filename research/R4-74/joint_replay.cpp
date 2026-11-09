#define CHLORIUM_R72_REPLAY_LIBRARY_ONLY 1
#include "../R4-72/replay_checks.cpp"
using ActorKey=std::pair<std::uint64_t,unsigned>;
struct JointActor {std::vector<const rt::Row*> rows[2];unsigned group=0;};
struct JointFrame {std::vector<mp::Command> commands;std::vector<Range> ranges;std::vector<unsigned> indices;std::vector<std::vector<std::pair<unsigned,const rt::Row*>>> witnesses;};
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3,"joint R74 witness and mixed.comp required");const auto rows=readTrace(argv[1]);
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};_mm_setcsr(0x1f80);
    std::map<ActorKey,JointActor> candidates;for(const auto& row:rows)candidates[{row.address,row.uid}].rows[row.frame-1].push_back(&row);
    unsigned missingFrames=0,discontinuous=0,unsupported=0,tooMany=0;std::vector<ActorKey> keys;std::vector<JointActor*> actors;
    const std::array<Range,1> oneRange{{{0,1}}};const std::array<unsigned,1> oneIndex{0};
    for(auto& [key,actor]:candidates){
        if(actor.rows[0].empty() || actor.rows[1].empty()){++missingFrames;continue;}
        if(actor.rows[0].size()>64 || actor.rows[1].size()>64){++tooMany;continue;}
        const rt::Row* previous=nullptr;bool continuity=true,eligible=true;
        for(const auto& frame:actor.rows)for(const auto* row:frame){
            if(previous && (std::memcmp(previous->after.position,row->before.position,16) || std::memcmp(previous->after.offset,row->before.offset,8) || ((previous->after.flags^row->before.flags)&3u)))continuity=false;
            if(!mp::fixtureInput({&row->before,1},{&row->command,1},oneRange,oneIndex,row->controls))eligible=false;
            if(std::memcmp(row->before.offset,row->after.offset,8) || ((row->before.flags^row->after.flags)&3u))continuity=false;
            previous=row;
        }
        if(!continuity){++discontinuous;continue;}if(!eligible){++unsupported;continue;}keys.push_back(key);actors.push_back(&actor);
    }
    std::cout<<"joint eligibility actors="<<candidates.size()<<" continuous="<<actors.size()<<" missing_frames="<<missingFrames<<" discontinuous="<<discontinuous<<" unsupported="<<unsupported<<" over64="<<tooMany<<'\n';
    require(!actors.empty(),"no complete continuous native root programs; retain native ownership");
    std::map<ActorKey,unsigned> slots;for(unsigned i=0;i<keys.size();++i)slots[keys[i]]=i;
    JointFrame frames[2];for(auto& frame:frames)frame.witnesses.resize(actors.size());
    for(const auto& row:rows){auto slot=slots.find({row.address,row.uid});if(slot==slots.end())continue;auto& frame=frames[row.frame-1];
        if(frame.commands.empty() || std::memcmp(&frame.commands.back(),&row.command,sizeof(mp::Command)))frame.commands.push_back(row.command);
        frame.witnesses[slot->second].push_back({unsigned(frame.commands.size()-1),&row});
    }
    std::map<std::vector<unsigned>,unsigned> classes;std::vector<unsigned> representatives;
    for(unsigned i=0;i<actors.size();++i){std::vector<unsigned> key;for(unsigned frame=0;frame<2;++frame){for(const auto& [command,row]:frames[frame].witnesses[i])key.push_back(command);key.push_back(UINT_MAX);}
        auto at=classes.find(key);unsigned group=0;if(at==classes.end()){group=unsigned(classes.size());classes.emplace(std::move(key),group);representatives.push_back(i);}else group=at->second;actors[i]->group=group;
    }
    std::vector<mp::State> initial;for(const auto* actor:actors){auto state=actor->rows[0].front()->before;state.flags|=actor->group<<16;initial.push_back(state);}
    for(auto& frame:frames)for(auto actor:representatives){frame.ranges.push_back({unsigned(frame.indices.size()),unsigned(frame.witnesses[actor].size())});for(const auto& [command,row]:frame.witnesses[actor])frame.indices.push_back(command);}
    chlorium::test::SystemInputGuard input;Graphics graphics;graphics.create(true,6);MixedGpu gpu(argv[2],initial);std::uint64_t compared=0;unsigned failures=0;
    for(unsigned frame=0;frame<2;++frame){auto seed=initial;
        for(unsigned i=0;i<actors.size();++i){seed[i]=actors[i]->rows[frame].front()->before;seed[i].flags|=actors[i]->group<<16;}
        for(unsigned prefix=0;prefix<=frames[frame].commands.size();++prefix){_mm_setcsr(0x1f80);const auto actual=gpu.run(seed,frames[frame].commands,frames[frame].ranges,frames[frame].indices,prefix,true);
            for(unsigned i=0;i<actors.size();++i){const double* expected=seed[i].position;for(const auto& [command,row]:frames[frame].witnesses[i]){if(command>=prefix)break;expected=row->after.position;}
                if(std::memcmp(actual[i].position,expected,16)){if(failures<5)std::cerr<<"joint prefix mismatch frame="<<frame<<" prefix="<<prefix<<" actor="<<i<<'\n';++failures;}++compared;}
        }
    }
    unsigned residentFailures=0;for(unsigned frame=0;frame<2;++frame){_mm_setcsr(0x1f80);const auto actual=gpu.run(initial,frames[frame].commands,frames[frame].ranges,frames[frame].indices,unsigned(frames[frame].commands.size()),frame==0);
        for(unsigned i=0;i<actors.size();++i){residentFailures+=std::memcmp(actual[i].position,actors[i]->rows[frame].back()->after.position,16)!=0;++compared;}}
    const auto commandBytes=(frames[0].commands.size()+frames[1].commands.size())*sizeof(mp::Command);const auto indexBytes=(frames[0].indices.size()+frames[1].indices.size())*4;
    std::cout<<"{\"diagnostic_only\":true,\"captured_actors\":"<<candidates.size()<<",\"resident_actors\":"<<actors.size()<<",\"joint_classes\":"<<classes.size()<<",\"positions_compared\":"<<compared<<",\"prefix_mismatches\":"<<failures<<",\"resident_mismatches\":"<<residentFailures<<",\"seed_position_upload_frames\":1,\"initial_state_bytes\":"<<initial.size()*sizeof(mp::State)<<",\"shared_command_bytes\":"<<commandBytes<<",\"program_index_bytes\":"<<indexBytes<<",\"global_reader_closure\":false,\"game_fps_evidence\":false}\n";
    require(!failures && !residentFailures,"native/GPU joint command prefixes and resident frame bridge");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
