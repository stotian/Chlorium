#pragma once
#include "GroupProgram.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace chlorium::research::groupprogram {
struct PacketFrame {std::vector<Command> commands;std::vector<Range> ranges;std::vector<unsigned> indices;std::vector<std::array<double,2>> expected;};
struct Packet {std::vector<State> states;std::array<PacketFrame,2> frames;};
inline Packet readPacket(const std::filesystem::path& path,unsigned mxcsr){
    auto require=[](bool valid,const char* message){if(!valid)throw std::runtime_error(message);};
    require(std::filesystem::file_size(path)<=64*1024*1024,"packet byte budget");
    std::ifstream input(path,std::ios::binary);
    auto read=[&](void* target,std::size_t bytes){input.read(static_cast<char*>(target),bytes);require(bool(input),"truncated packet");};
    struct Header{char magic[8];unsigned version,count,ranges,frames,stateBytes,commandBytes,rangeBytes,indexBytes;}header;
    static_assert(sizeof(Header)==40);read(&header,sizeof(header));
    require(!std::memcmp(header.magic,"CHLPRG64",8) && header.version==64 && header.count && header.count<=maxObjects &&
        header.ranges && header.ranges<=maxRanges && header.frames==2 && header.stateBytes==32 && header.commandBytes==32 &&
        header.rangeBytes==8 && header.indexBytes==4,"bounded packet layout");
    Packet packet;packet.states.resize(header.count);read(packet.states.data(),packet.states.size()*sizeof(State));
    for(auto& frame:packet.frames){
        std::array<unsigned,4> shape;read(shape.data(),sizeof(shape));
        require(shape[0]<=maxCommands && shape[1]<=header.ranges*maxSteps && shape[2]==header.count && !shape[3],"bounded frame layout");
        frame.commands.resize(shape[0]);frame.indices.resize(shape[1]);frame.ranges.resize(header.ranges);frame.expected.resize(header.count);
        read(frame.commands.data(),frame.commands.size()*sizeof(Command));read(frame.ranges.data(),frame.ranges.size()*sizeof(Range));
        read(frame.indices.data(),frame.indices.size()*sizeof(unsigned));read(frame.expected.data(),frame.expected.size()*sizeof(frame.expected[0]));
        require(seedEligible(packet.states,frame.commands,frame.ranges,frame.indices,mxcsr),"packet semantic validation");
        for(const auto& position:frame.expected)require(std::isfinite(position[0]) && std::isfinite(position[1]),"finite native witnesses");
    }
    require(input.peek()==std::char_traits<char>::eof(),"no trailing packet bytes");return packet;
}
}
