#include "renderer/DirtyStateUploads.hpp"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using chlorium::snapshot::DirtyStateUploads;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int main() try {
    DirtyStateUploads empty(0); empty.mark(0);
    require(empty.finish().empty(), "empty store has no uploads");
    DirtyStateUploads edge(65); edge.mark(64);
    auto tail = edge.finish();
    require(tail.size() == 1 && tail[0].first == 64 && tail[0].count == 1, "tail block is clamped to the owned store");
    DirtyStateUploads scattered(20000);
    for (unsigned i = 0; i < 40; ++i) scattered.mark(i * 256);
    require(scattered.finish().size() == 1, "excessive sparse calls collapse to one broad upload");
    std::mt19937 random(0xc410);
    std::size_t compared = 0;
    for (const auto stride : {64u, 80u, 96u}) for (const auto count : {1u, 63u, 64u, 65u, 4097u}) {
        std::vector<unsigned char> cpu(count * stride), gpu(cpu);
        for (unsigned frame = 0; frame < 80; ++frame) {
            DirtyStateUploads plan(count);
            // The last half exercises out-of-order dirtiness too. The caller
            // uses monotonic traversal, but an accidental reorder stays safe.
            for (unsigned n = 0; n < count; ++n) {
                const unsigned i = frame < 40 ? n : random() % count;
                if (frame % 4 == 0 || random() % 31 == 0) {
                    const auto byte = i * stride + random() % stride;
                    cpu[byte] ^= 1 + random() % 255;
                    plan.mark(i);
                }
            }
            for (auto range : plan.finish()) {
                require(range.first <= count && range.count <= count - range.first, "upload stays inside owned storage");
                std::memcpy(gpu.data() + range.first * stride, cpu.data() + range.first * stride, range.count * stride);
            }
            require(cpu == gpu, "every sparse/dense/out-of-order state change reaches GPU storage");
            ++compared;
        }
    }
    std::cout << "PASS: " << compared << " owned snapshot upload plans preserve every byte\n";
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
