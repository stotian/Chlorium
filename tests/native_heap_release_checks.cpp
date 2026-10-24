#include "renderer/NativeHeapRelease.hpp"
#include <vector>
#include <cstring>
#include <cstdio>

int main() {
    struct Block { unsigned char* data; std::size_t size; unsigned char value; };
    std::vector<Block> blocks;
    const auto heap = GetProcessHeap();
    for (unsigned i = 0; i < 16384; ++i) {
        const auto size = std::size_t(64) << (i & 7);
        auto memory = static_cast<unsigned char*>(HeapAlloc(heap, 0, size));
        if (!memory) return 1;
        const auto value = static_cast<unsigned char>(i * 37 + 13);
        std::memset(memory, value, size);
        blocks.push_back({memory, size, value});
    }
    for (std::size_t i = 0; i < blocks.size(); ++i)
        if ((i & 63) >= 8) { HeapFree(heap, 0, blocks[i].data); blocks[i].data = nullptr; }
    const auto result = chlorium::memory::releaseUnusedHeapCaches();
    if (!result.succeeded || result.memoryReadMask != 3) return 2;
    unsigned checked = 0;
    for (const auto& block : blocks) if (block.data) {
        for (std::size_t j = 0; j < block.size; ++j) if (block.data[j] != block.value) return 3;
        HeapFree(heap, 0, block.data);
        ++checked;
    }
    std::printf("Heap cache release: %u live blocks unchanged; private before=%llu after=%llu; %.3f ms\n",
        checked, result.privateBefore, result.privateAfter, result.elapsedMs);
}
