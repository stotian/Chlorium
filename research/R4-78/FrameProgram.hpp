#pragma once
#include "../R4-72/TraceAdapter.hpp"
#include <algorithm>
#include <map>
#include <cstring>
#include <stdexcept>
namespace chlorium::research::liveprogram {
using State=mixedprogram::State;using Command=mixedprogram::Command;using Row=rotationtrace::Row;using Key=std::pair<std::uint64_t,unsigned>;using Range=mixedprogram::Range;
struct Roster {std::vector<Key> keys;std::vector<State> initial;};
struct Frame {std::vector<Command> commands;std::vector<Range> ranges;std::vector<unsigned> indices;std::vector<State> expected;};
inline std::span<const Row> rowsForFrame(std::span<const Row> rows,unsigned frame){
    const auto first=std::lower_bound(rows.begin(),rows.end(),frame,[](const Row& row,unsigned value){return row.frame<value;});
    const auto last=std::upper_bound(first,rows.end(),frame,[](unsigned value,const Row& row){return value<row.frame;});
    return {first,static_cast<std::size_t>(last-first)};
}
inline Roster seed(std::span<const Row> rows,unsigned frame){
    if(rows.size()>65536)throw std::invalid_argument("bounded live seed rows");Roster roster;std::map<Key,unsigned> slots;
    for(const auto& row:rowsForFrame(rows,frame))if(row.closed){Key key{row.address,row.uid};if(slots.contains(key))continue;if(roster.keys.size()==128)throw std::invalid_argument("bounded live actors");
        const auto slot=unsigned(roster.keys.size());slots[key]=slot;roster.keys.push_back(key);auto state=row.before;state.flags|=slot<<16;roster.initial.push_back(state);}
    return roster;
}
inline Frame compile(std::span<const Row> rows,unsigned frame,const Roster& roster,std::span<const State> previous,std::vector<unsigned char>& disabled){
    const auto count=roster.keys.size();if(rows.size()>65536 || !count || count>128 || roster.initial.size()!=count || previous.size()!=count || disabled.size()!=count)throw std::invalid_argument("bounded live frame association");
    Frame out;out.expected.assign(previous.begin(),previous.end());out.ranges.resize(count);std::map<Key,unsigned> slots;for(unsigned i=0;i<count;++i){if(disabled[i]>1 || !slots.emplace(roster.keys[i],i).second)throw std::invalid_argument("unique live actor identities");}
    const std::array<Range,1> range{{{0,1}}};const std::array<unsigned,1> index{0};
    const auto frameRows=rowsForFrame(rows,frame);
    for(const auto& row:frameRows){auto found=slots.find({row.address,row.uid});if(found==slots.end())continue;const auto slot=found->second;auto& prior=out.expected[slot];
        if(row.closed!=1 || std::memcmp(prior.position,row.before.position,16) || std::memcmp(prior.offset,row.before.offset,8) || ((prior.flags^row.before.flags)&3u) || std::memcmp(row.before.offset,row.after.offset,8) || ((row.before.flags^row.after.flags)&3u) || !mixedprogram::fixtureInput({&row.before,1},{&row.command,1},range,index,row.controls))disabled[slot]=1;
        prior=row.after;
    }
    std::vector<std::vector<unsigned>> programs(count);
    for(const auto& row:frameRows){auto found=slots.find({row.address,row.uid});if(found==slots.end() || disabled[found->second])continue;
        if(out.commands.empty() || std::memcmp(&out.commands.back(),&row.command,sizeof(Command)))out.commands.push_back(row.command);if(out.commands.size()>8192)throw std::invalid_argument("bounded live command table");programs[found->second].push_back(unsigned(out.commands.size()-1));}
    for(unsigned i=0;i<count;++i){if(programs[i].size()>8192)throw std::invalid_argument("bounded live actor program");out.ranges[i]={unsigned(out.indices.size()),unsigned(programs[i].size())};out.indices.insert(out.indices.end(),programs[i].begin(),programs[i].end());}
    return out;
}
}
