#pragma once
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
#include <cstdint>
#include <xmmintrin.h>
#include "PassCapture.hpp"
class PlayLayer;
namespace chlorium::research::pass {
struct PreserveFP {
    unsigned saved = _mm_getcsr();
    PreserveFP() = default;
    PreserveFP(const PreserveFP&) = delete;
    ~PreserveFP() { _mm_setcsr(saved); }
};
void beginFrame(bool eligible, std::uint64_t frame, double percent) noexcept;
void prepareLevel(PlayLayer* play) noexcept;
void endFrame() noexcept;
bool finish() noexcept;
bool producerReplayRequested() noexcept;
bool producerReplayPackets(std::span<const SpriteRecord> oracle,std::span<const unsigned char> vertices,
                          std::vector<SpriteRecord>& sources,std::vector<unsigned char>& payload) noexcept;
}
#endif
