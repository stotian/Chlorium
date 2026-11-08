#pragma once
#include <cstddef>
#include <cstdint>
namespace chlorium::research::continuous {
struct Stats {
    std::uint32_t version=57,enabled=0;
    std::uint64_t frames=0,atlasCalls=0,submitted=0,nativeFallbacks=0;
    std::uint64_t consideredQuads=0,submittedQuads=0,reconstructed=0,rollbacks=0,fatal=0;
    std::uint64_t committed=0,committedQuads=0;
    std::uint64_t declines[8]{};
};
}
