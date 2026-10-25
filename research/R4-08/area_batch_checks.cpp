#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "AreaBatch.hpp"
#include "AreaWaveSchedule.hpp"
#include "renderer/AreaDivisionPatch.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace batch = chlorium::research::area_batch;
static std::size_t checks = 0;
void require(bool ok, const char* why) {
    ++checks;
    if (!ok) throw std::runtime_error(why);
}
struct NativeOracle {
    unsigned char* code = nullptr;
    batch::NativeCurve calculate = nullptr;
    explicit NativeOracle(const char* path) {
        using namespace chlorium::area;
        std::ifstream file(path, std::ios::binary);
        const std::vector<unsigned char> exe{std::istreambuf_iterator<char>(file), {}};
        require(exe.size() >= sizeof(IMAGE_DOS_HEADER) && exe.size() >= sizeof(IMAGE_NT_HEADERS64), "read native executable");
        IMAGE_DOS_HEADER dos{}; std::memcpy(&dos, exe.data(), sizeof(dos));
        require(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 &&
            static_cast<std::size_t>(dos.e_lfanew) <= exe.size() - sizeof(IMAGE_NT_HEADERS64), "bounded PE headers");
        IMAGE_NT_HEADERS64 pe{}; std::memcpy(&pe, exe.data() + dos.e_lfanew, sizeof(pe));
        require(pe.Signature == IMAGE_NT_SIGNATURE && pe.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            pe.FileHeader.TimeDateStamp == 0x69702da0 && pe.OptionalHeader.SizeOfImage == 0xa3a000 &&
            pe.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC, "exact GD 2.2081 image");
        const std::size_t sectionsOffset = dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + pe.FileHeader.SizeOfOptionalHeader;
        require(sectionsOffset <= exe.size() && pe.FileHeader.NumberOfSections <=
            (exe.size() - sectionsOffset) / sizeof(IMAGE_SECTION_HEADER), "bounded PE sections");
        auto atRva = [&](std::size_t rva, std::size_t size) -> const unsigned char* {
            for (unsigned n = 0; n < pe.FileHeader.NumberOfSections; ++n) {
                IMAGE_SECTION_HEADER section{};
                std::memcpy(&section, exe.data() + sectionsOffset + n * sizeof(section), sizeof(section));
                if (rva < section.VirtualAddress) continue;
                const auto offset = rva - section.VirtualAddress;
                const std::size_t raw = section.PointerToRawData;
                if (offset <= section.SizeOfRawData && size <= section.SizeOfRawData - offset &&
                    raw <= exe.size() && offset <= exe.size() - raw && size <= exe.size() - raw - offset)
                    return exe.data() + raw + offset;
            }
            return nullptr;
        };
        const auto source = atRva(functionRva, functionBytes);
        require(source && hash({source, functionBytes}) == functionHash, "complete native curve fingerprint");
        const auto constant = atRva(0x622c24, sizeof(float));
        std::uint32_t oneBits = 0;
        if (constant) std::memcpy(&oneBits, constant, sizeof(oneBits));
        require(constant && oneBits == 0x3f800000, "native curve constant is one");
        code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        require(code != nullptr, "isolated oracle page");
        std::memcpy(code, source, functionBytes); std::memcpy(code + 2048, constant, sizeof(float));
        for (unsigned offset : {0x20u, 0x3bu}) {
            const unsigned char opcode[]{0xf3, 0x0f, 0x10, 0x2d};
            require(!std::memcmp(code + offset, opcode, 4), "native RIP load opcode");
            const int displacement = 2048 - static_cast<int>(offset + 8);
            std::memcpy(code + offset + 4, &displacement, sizeof(displacement));
        }
        DWORD previous = 0;
        require(VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &previous) != 0, "oracle W then RX protection");
        require(FlushInstructionCache(GetCurrentProcess(), code, 4096) != 0, "oracle instruction visibility");
        calculate = reinterpret_cast<batch::NativeCurve>(code);
    }
    ~NativeOracle() { if (code) VirtualFree(code, 0, MEM_RELEASE); }
};

void checkSchedules(batch::NativeCurve native) {
    // Explicit field-only fixtures, not a claim about GD callback semantics.
    std::mt19937 random(0xca05a1);
    for (unsigned trial = 0; trial < 1200; ++trial) {
        const auto count = 2 + trial % 300;
        std::vector<std::uint64_t> reads(count), writes(count);
        std::vector<batch::Operation> operations(count);
        for (unsigned n = 0; n < count; ++n) {
            reads[n] = random() % 32; writes[n] = random() % 32;
            if (!n) { reads[n] = 0; writes[n] = 1; }
            if (n == 1) { reads[n] = 1; writes[n] = 2; }
            operations[n] = {{1,0}, {&reads[n],1}, {&writes[n],1}, true, true};
            if (n > 1 && n % 17 == 0) operations[n].snapshotPure = false;
            if (n > 1 && n % 19 == 0) operations[n].recipe.length = 2;
        }
        const auto plan = batch::WaveSchedule::compile(operations, 7);
        std::array<float,32> expected{}, actual{};
        for (unsigned n = 0; n < expected.size(); ++n) expected[n] = .01f * (n + 1);
        actual = expected;
        for (unsigned n = 0; n < count; ++n)
            expected[writes[n]] = native(expected[reads[n]], operations[n].recipe.length, 0.f);
        std::size_t visited = 0;
        for (const auto& wave : plan.checked(7, operations.size())) {
            require(wave.first == visited && wave.count && wave.count <= 256, "bounded contiguous native-order waves");
            std::vector<float> snapshot(wave.count), results(wave.count);
            // Forward gathers may only cross proven independent field writes.
            for (unsigned j = 0; j < wave.count; ++j) snapshot[j] = actual[reads[wave.first+j]];
            const auto recipe = operations[wave.first].recipe;
            if (wave.batchable) batch::evaluate(snapshot,recipe.length,0.f,results,native);
            else {
                require(wave.count == 1, "opaque callback remains a scalar barrier");
                results[0] = native(snapshot[0],recipe.length,0.f);
            }
            for (unsigned j = 0; j < wave.count; ++j) actual[writes[wave.first+j]] = results[j];
            visited += wave.count;
        }
        require(visited == count, "every operation committed in original order");
        require(expected == actual, "causal snapshot program diverges from native ordered mutations");
        bool refused = false;
        try { (void)plan.checked(8,operations.size()); } catch (const std::invalid_argument&) { refused = true; }
        require(refused, "changed dependency generation rejects old schedule");
        refused = false;
        try { (void)plan.checked(7,operations.size()+1); } catch (const std::invalid_argument&) { refused = true; }
        require(refused, "changed command count rejects old schedule");
    }
}

int main(int argc, char** argv) try {
    require(argc == 3, "native executable and timing CSV required");
    NativeOracle oracle(argv[1]);
    struct RestoreCsr { unsigned saved = _mm_getcsr(); ~RestoreCsr() { _mm_setcsr(saved); } } restore;
    std::mt19937 random(0xa8ea2026);
    std::size_t lanes = 0, packed = 0;
    auto compare = [&](const std::vector<float>& distance, int length, float zone, unsigned csr) {
        std::vector<float> expected(distance.size()), actual(distance.size());
        _mm_setcsr(csr);
        for (std::size_t n = 0; n < distance.size(); ++n) expected[n] = oracle.calculate(distance[n], length, zone);
        const auto expectedCsr = _mm_getcsr();
        _mm_setcsr(csr);
        const auto work = batch::evaluate(distance, length, zone, actual, oracle.calculate);
        const auto actualCsr = _mm_getcsr();
        // Store flags before allocations, formatting, comparisons or timers.
        for (std::size_t n = 0; n < distance.size(); ++n)
            require(std::bit_cast<std::uint32_t>(expected[n]) == std::bit_cast<std::uint32_t>(actual[n]), "Area batch output bits diverge from native");
        require(expectedCsr == actualCsr, "Area batch complete SSE status/control diverges from native");
        require(work.packedLanes + work.nativeLanes == distance.size(), "every input evaluated exactly once");
        lanes += distance.size(); packed += work.packedLanes;
    };
    const std::uint32_t edgeBits[]{0,0x80000000,1,0x80000001,0x007fffff,0x00800000,
        0x3f000000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345};
    std::vector<float> edges;
    for (auto bits : edgeBits) edges.push_back(std::bit_cast<float>(bits));
    for (unsigned rounding : {0u,0x2000u,0x4000u,0x6000u}) for (unsigned denorm : {0u,0x40u,0x8000u,0x8040u}) {
        const unsigned csr = 0x1f80u | rounding | denorm;
        for (auto bits : edgeBits) for (int length : {0,1,-1,1000,16777217,INT_MAX,INT_MIN})
            compare(edges, length, std::bit_cast<float>(bits), csr);
        for (unsigned trial = 0; trial < 1200; ++trial) {
            const auto count = trial % 35;
            std::vector<float> input(count);
            for (auto& d : input) d = std::bit_cast<float>(static_cast<std::uint32_t>(random()));
            const float zone = trial % 3 ? std::bit_cast<float>(static_cast<std::uint32_t>(random())) : .2f;
            compare(input, std::bit_cast<int>(static_cast<std::uint32_t>(random())), zone, csr | (trial % 64));
        }
    }
    _mm_setcsr(0x1f80);
    std::array<float, 9> overlap{};
    bool rejected = false;
    try { batch::evaluate(std::span<const float>(overlap.data(), 8), 1, .2f, std::span<float>(overlap.data()+1, 8), oracle.calculate); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "overlapping snapshots rejected before writing");
    rejected = false;
    try { batch::evaluate(std::span<const float>(overlap.data(), 8), 1, .2f, std::span<float>(overlap.data(), 7), oracle.calculate); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "mismatched snapshots rejected");
    checkSchedules(oracle.calculate);
    std::cout << "PASS " << checks << " checks, " << lanes << " native differential lanes, " << packed << " packed lanes\n";
    std::ofstream csv(argv[2]); require(bool(csv), "timing output opens");
    csv << "zone,variant,repeat,objects,elapsed_us\n";
    // Small bounded snapshots. Native gather/commit, groups, physics and GPU
    // are intentionally absent and must not be counted as an optimized frame.
    std::vector<float> distances(65536), output(distances.size());
    for (std::size_t n = 0; n < distances.size(); ++n) distances[n] = static_cast<float>(n % 2000) - 250.f;
    for (float zone : {0.f,.2f,.5f}) {
        std::array<std::vector<double>, 2> times;
        for (unsigned repeat = 0; repeat < 10; ++repeat) for (unsigned order = 0; order < 2; ++order) {
            const auto variant = (repeat + order) % 2;
            const auto begin = std::chrono::steady_clock::now();
            if (variant) batch::evaluate(distances, 1000, zone, output, oracle.calculate);
            else for (std::size_t n = 0; n < distances.size(); ++n) output[n] = oracle.calculate(distances[n], 1000, zone);
            const auto us = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
            if (repeat) { times[variant].push_back(us); csv << zone << ',' << variant << ',' << repeat << ',' << distances.size() << ',' << us << '\n'; }
            require(std::bit_cast<std::uint32_t>(output[1234]) == std::bit_cast<std::uint32_t>(oracle.calculate(distances[1234],1000,zone)), "benchmark output observed");
        }
        for (auto& values : times) std::sort(values.begin(), values.end());
        std::cout << "CURVE ONLY zone=" << zone << " native_us=" << times[0][4] << " packed_us=" << times[1][4]
                  << " ratio=" << times[0][4]/times[1][4] << " (not GD FPS)\n";
    }
    csv.flush(); require(bool(csv), "timing output complete");
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
