#include "FrameProgram.hpp"
#include <cassert>
#include <iostream>
namespace lp=chlorium::research::liveprogram;
int main(){
    std::vector<lp::Row> rows(2);for(unsigned i=0;i<2;++i){auto& r=rows[i];r.address=0x10000;r.uid=7;r.frame=i+1;r.closed=1;r.controls=0x1f80;r.before.position[0]=1.+double(i)*.25;r.after=r.before;r.after.position[0]+=.25;r.command.delta[0]=.25;}
    auto roster=lp::seed(rows,1);assert(roster.keys.size()==1);std::vector<unsigned char> disabled(1,0);auto one=lp::compile(rows,1,roster,roster.initial,disabled);assert(one.commands.size()==1 && one.indices.size()==1 && !disabled[0]);
    auto two=lp::compile(rows,2,roster,one.expected,disabled);assert(two.expected[0].position[0]==1.5 && !disabled[0]);
    auto wrong=rows;wrong[1].before.position[0]+=1;auto rejected=lp::compile(wrong,2,roster,one.expected,disabled);assert(disabled[0] && rejected.commands.empty() && rejected.ranges[0][1]==0);
    disabled[0]=0;wrong=rows;wrong[1].command.control[1]=3;rejected=lp::compile(wrong,2,roster,one.expected,disabled);assert(disabled[0] && rejected.commands.empty());
    auto duplicate=roster;duplicate.keys.push_back(duplicate.keys[0]);duplicate.initial.push_back(duplicate.initial[0]);disabled.assign(2,0);bool failed=false;try{lp::compile(rows,1,duplicate,duplicate.initial,disabled);}catch(const std::invalid_argument&){failed=true;}assert(failed);
    std::vector<lp::Row> history=rows;for(unsigned frame=3;frame<50003;++frame){auto row=rows[1];row.frame=frame;history.push_back(row);}assert(lp::rowsForFrame(history,2).size()==1);assert(lp::rowsForFrame(history,25000).size()==1);assert(lp::rowsForFrame(history,50003).empty());auto sliced=lp::compile(history,2,roster,one.expected,disabled);assert(sliced.commands.size()==two.commands.size() && !std::memcmp(sliced.commands.data(),two.commands.data(),two.commands.size()*sizeof(lp::Command)) && sliced.indices==two.indices && sliced.expected[0].position[0]==two.expected[0].position[0]);
    auto repeated=std::vector<lp::Row>{rows[0],rows[0]};repeated[1].before=repeated[0].after;repeated[1].after=repeated[1].before;repeated[1].after.position[0]+=.25;disabled.assign(1,0);auto doubled=lp::compile(repeated,1,roster,roster.initial,disabled);assert(doubled.commands.size()==1 && doubled.indices==std::vector<unsigned>({0,0}) && !disabled[0]);
    std::vector<lp::Row> overflow(8193,rows[0]);for(unsigned i=0;i<overflow.size();++i){overflow[i].before.position[0]=1.+double(i)*.25;overflow[i].after.position[0]=1.+double(i+1)*.25;overflow[i].command.control[0]=i;}
    failed=false;try{lp::compile(overflow,1,roster,roster.initial,disabled);}catch(const std::invalid_argument&){failed=true;}assert(failed);
    std::cout<<"PASS native frame continuity, unsupported commands, actor identities, duplicate visits and command budget\n";
}
