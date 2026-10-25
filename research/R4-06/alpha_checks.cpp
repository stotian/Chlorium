#include "IncrementalAlpha.hpp"
#include <bit>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <string>
#include <numeric>

using namespace chlorium::research;
using Clock = std::chrono::steady_clock;
static std::uint64_t checks = 0;
static void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
static bool equalBits(std::span<const float> a, std::span<const float> b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::bit_cast<std::uint32_t>(a[i]) != std::bit_cast<std::uint32_t>(b[i])) return false;
    return true;
}

// Independent full-scan transcription of GroupManager::prepareGroupStateBuffer:
// poll used IDs, then ALL combinations and ALL states after any numeric change.
// No CSR/index helper from the candidate is used by this reference.
struct FullReference {
    AlphaLayout layout;
    std::set<Index> used;
    std::vector<float> alphas, products, states;
    bool forced = true;
    explicit FullReference(AlphaLayout input) : layout(std::move(input)),
        alphas(layout.groupSlots), products(layout.combinations.size()), states(layout.stateToCombination.size(), 1.0f) {
        for (const auto& ids : layout.combinations) for (auto id : ids) used.insert(id);
    }
    void refresh(std::span<const float> snapshot, std::span<const std::uint8_t> disabled, AlphaWork& work) {
        work = {};
        bool dirty = forced;
        forced = false;
        for (auto id : used) {
            ++work.polled;
            const float value = disabled[id] ? 0.0f : snapshot[id];
            if (alphas[id] != value) { alphas[id] = value; dirty = true; }
        }
        if (!dirty) return;
        for (std::size_t c = 0; c < layout.combinations.size(); ++c) {
            ++work.combinations;
            float value = 1.0f;
            for (auto id : layout.combinations[c]) { value *= alphas[id]; ++work.products; }
            products[c] = value;
        }
        for (std::size_t s = 0; s < states.size(); ++s) {
            ++work.stateVisits;
            const auto value = products[layout.stateToCombination[s]];
            if (states[s] != value) { states[s] = value; ++work.writes; }
        }
    }
};

static AlphaLayout fixture(Index groups, Index combinations, Index states, std::uint32_t seed) {
    AlphaLayout layout{groups + 1, {}, {}};
    std::mt19937 random(seed);
    std::set<std::vector<Index>> unique;
    while (unique.size() < combinations) {
        std::set<Index> ids;
        const auto count = 1 + random() % 5;
        for (unsigned i = 0; i < count; ++i) ids.insert(1 + random() % groups);
        unique.emplace(ids.begin(), ids.end());
    }
    layout.combinations.assign(unique.begin(), unique.end());
    for (Index i = 0; i < states; ++i) layout.stateToCombination.push_back((i * 37u) % combinations);
    return layout;
}

static void differential(AlphaStrategy strategy) {
    auto layout = fixture(64, 256, 1024, 0xC410);
    layout.combinations.push_back({}); // Empty product is one.
    layout.stateToCombination.push_back(256);
    IncrementalAlpha candidate(layout, 1, strategy);
    FullReference reference(layout);
    std::vector<float> values(layout.groupSlots, 1.0f);
    std::vector<std::uint8_t> disabled(layout.groupSlots);
    std::mt19937 random(4711);
    AlphaWork actual, expected;
    for (unsigned frame = 0; frame < 3000; ++frame) {
        const auto id = 1 + random() % 64;
        if (frame % 7) values[id] = static_cast<float>(random() % 1025) / 1024.0f;
        if (frame % 17 == 0) disabled[id] ^= 1;
        if (frame % 59 == 0) for (Index group = 1; group < values.size(); ++group) values[group] = (random() % 2) ? 0.5f : 1.0f;
        if (frame % 61 == 0) values[id] = 0.0f;
        if (frame % 67 == 0) values[id] = -0.0f;
        if (frame % 71 == 0) values[id] = std::bit_cast<float>(0x7fc01234u);
        if (frame % 73 == 0) values[id] = std::numeric_limits<float>::infinity();
        if (frame % 79 == 0) values[id] = std::numeric_limits<float>::denorm_min();
        if (frame % 83 == 0) values[id] = -1.0f;
        if (frame % 101 == 0) { reference.forced = true; check(candidate.invalidate(1), "invalidation"); }
        if (frame % 127 == 0) {
            std::fill(reference.states.begin(), reference.states.end(), 1.0f);
            reference.forced = true;
            check(candidate.restore(1, reference.states), "restore");
        }
        reference.refresh(values, disabled, expected);
        check(candidate.refresh(1, values, disabled, actual), "snapshot accepted");
        check(equalBits(candidate.outputs(), reference.states), "differential output bits");
        // Full scan reassigns unchanged NaN outputs (NaN != NaN). Redundant
        // private-buffer writes are deliberately removed; native events aren't.
        check(actual.writes <= expected.writes, "unexpected extra output writes");
        check(candidate.hasDenseRange() ? (!actual.writes ||
            candidate.lastChanged() - candidate.firstChanged() + 1 >= actual.writes) :
            candidate.changedStates().size() == actual.writes, "private dirty coverage");
        const auto before = std::vector<float>(candidate.outputs().begin(), candidate.outputs().end());
        check(!candidate.refresh(2, values, disabled, actual), "stale scene epoch rejected");
        check(equalBits(before, candidate.outputs()), "stale snapshot atomic");
        check(!candidate.restore(2, reference.states), "stale checkpoint rejected");
    }
    check(!candidate.refresh(1, std::span(values).first(2), disabled, actual), "short snapshot rejected");
    auto newLayout = fixture(16, 32, 64, 123);
    IncrementalAlpha remapped(newLayout, 2);
    FullReference newReference(newLayout);
    values.assign(newLayout.groupSlots, 0.25f); disabled.assign(newLayout.groupSlots, 0);
    newReference.refresh(values, disabled, expected);
    check(remapped.refresh(2, values, disabled, actual), "remapped epoch");
    check(equalBits(remapped.outputs(), newReference.states), "remap output");
    try { IncrementalAlpha invalid({4, {{2, 1}}, {0}}, 1); check(false, "unsorted groups accepted"); }
    catch (const std::invalid_argument&) { ++checks; }
    try { IncrementalAlpha invalid({4, {{1}}, {1}}, 1); check(false, "invalid state mapping accepted"); }
    catch (const std::invalid_argument&) { ++checks; }
    IncrementalAlpha empty({0, {}, {}}, 1);
    check(empty.refresh(1, {}, {}, actual), "empty scene");
}

static void roundingBoundary(AlphaStrategy strategy) {
    AlphaLayout layout{5, {{1, 2}, {3, 4}}, {0, 1}};
    IncrementalAlpha candidate(layout, 3, strategy);
    FullReference reference(layout);
    std::vector<float> input{0, 0.1f, 0.3f, 1, 1};
    std::vector<std::uint8_t> disabled(5);
    AlphaWork a, b;
    const int old = std::fegetround();
    check(std::fesetround(FE_TONEAREST) == 0, "set rounding");
    reference.refresh(input, disabled, a); candidate.refresh(3, input, disabled, b);
    check(std::fesetround(FE_UPWARD) == 0, "change rounding");
    input[3] = 0.5f; // Unrelated component: full recomposition also changes 1*2.
    reference.refresh(input, disabled, a); candidate.refresh(3, input, disabled, b);
    const bool same = equalBits(reference.states, candidate.outputs());
    std::fesetround(old);
    check(same, "rounding environment changed on unrelated component");
}

static void exceptionStatusBoundary() {
    // A consumer can clear sticky status between updates. Force the same old
    // invalid multiplication to be regenerated, without synthesizing flags.
    AlphaLayout layout{4, {{1, 2}, {3}}, {0, 1}};
    IncrementalAlpha candidate(layout, 9);
    FullReference reference(layout);
    std::vector<float> input{0, 0, std::numeric_limits<float>::infinity(), 1};
    std::vector<std::uint8_t> disabled(4);
    AlphaWork work;
    reference.refresh(input, disabled, work); candidate.refresh(9, input, disabled, work);
    input[3] = .5f;
    std::feclearexcept(FE_ALL_EXCEPT);
    reference.refresh(input, disabled, work);
    const int referenceFlags = std::fetestexcept(FE_ALL_EXCEPT);
    std::feclearexcept(FE_ALL_EXCEPT);
    candidate.refresh(9, input, disabled, work);
    const int candidateFlags = std::fetestexcept(FE_ALL_EXCEPT);
    check(equalBits(reference.states, candidate.outputs()), "exception fixture output bits");
    check(referenceFlags == candidateFlags, "exception flags after consumer clear");
    std::cout << "{\"event\":\"floating_status_boundary\",\"full_scan_flags\":" << referenceFlags
        << ",\"candidate_flags\":" << candidateFlags << ",\"parity\":" << (referenceFlags == candidateFlags ? "true" : "false")
        << ",\"production_eligible\":false}\n";
    std::feclearexcept(FE_ALL_EXCEPT);
}

static void finiteStatusBoundary() {
    // Finite products can raise inexact/underflow too. Non-finite rejection
    // alone would not make incremental evaluation preserve native FP status.
    for (auto strategy : {AlphaStrategy::ReverseCSR, AlphaStrategy::Adaptive})
    for (const auto pair : {std::pair{.1f, .3f}, std::pair{std::numeric_limits<float>::min(), .1f},
                           std::pair{std::numeric_limits<float>::max(), 2.0f}}) {
        AlphaLayout layout{4, {{1, 2}, {3}}, {0, 1}};
        IncrementalAlpha candidate(layout, 1, strategy);
        FullReference reference(layout);
        std::vector<float> input{0, pair.first, pair.second, 1};
        std::vector<std::uint8_t> disabled(4);
        AlphaWork work;
        reference.refresh(input, disabled, work); candidate.refresh(1, input, disabled, work);
        input[3] = .5f;
        std::feclearexcept(FE_ALL_EXCEPT); reference.refresh(input, disabled, work);
        const auto expected = std::fetestexcept(FE_ALL_EXCEPT);
        const auto expectedSSE = _mm_getcsr() & 0x3fu;
        std::feclearexcept(FE_ALL_EXCEPT); candidate.refresh(1, input, disabled, work);
        const auto actual = std::fetestexcept(FE_ALL_EXCEPT);
        const auto actualSSE = _mm_getcsr() & 0x3fu;
        check(equalBits(reference.states, candidate.outputs()), "finite status output");
        check(expected == actual, "finite inexact/underflow/overflow flags");
        check(expectedSSE == actualSSE, "raw SSE status parity");
        std::feclearexcept(FE_ALL_EXCEPT);
    }
}

static void denormalControlBoundary() {
#if defined(_M_X64) || defined(__SSE2__)
    const auto original = _mm_getcsr();
    AlphaLayout layout{4, {{1, 2}, {3}}, {0, 1}};
    IncrementalAlpha candidate(layout, 1);
    FullReference reference(layout);
    std::vector<float> input{0, std::numeric_limits<float>::min(), .1f, 1};
    std::vector<std::uint8_t> disabled(4);
    AlphaWork a, b;
    reference.refresh(input, disabled, a); candidate.refresh(1, input, disabled, b);
    _mm_setcsr((original | 0x8040u | 0x1f80u) & ~0x3fu);
    input[3] = .5f;
    reference.refresh(input, disabled, a); candidate.refresh(1, input, disabled, b);
    const bool same = equalBits(reference.states, candidate.outputs());
    _mm_setcsr(original);
    check(same, "FTZ/DAZ context invalidation");
#endif
}

static double percentile(std::vector<double> samples, double fraction) {
    std::sort(samples.begin(), samples.end());
    return samples[static_cast<std::size_t>(std::ceil(fraction * samples.size())) - 1];
}
struct BenchResult {
    std::vector<double> micros;
    AlphaWork work;
    std::vector<float> output;
};
template <class Tick>
static BenchResult timed(Tick&& tick, Index groupSlots, unsigned changes) {
    BenchResult result;
    result.micros.reserve(128);
    std::vector<float> input(groupSlots, 1.0f);
    std::vector<std::uint8_t> disabled(groupSlots);
    AlphaWork work;
    tick(input, disabled, work); // Initial full population is excluded, not hidden.
    for (unsigned frame = 0; frame < 128; ++frame) {
        for (Index i = 1; i <= changes; ++i) input[i] = frame % 2 ? 0.4f : 0.6f;
        const auto begin = Clock::now();
        tick(input, disabled, work);
        const auto end = Clock::now();
        result.micros.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
        result.work.polled += work.polled; result.work.edges += work.edges;
        result.work.products += work.products; result.work.combinations += work.combinations;
        result.work.stateVisits += work.stateVisits;
        result.work.densePasses += work.densePasses;
    }
    return result;
}
static void statistics(const BenchResult& result) {
    std::cout << "{\"p50_us\":" << percentile(result.micros, .5)
        << ",\"p95_us\":" << percentile(result.micros, .95)
        << ",\"p99_us\":" << percentile(result.micros, .99)
        << ",\"worst_us\":" << *std::max_element(result.micros.begin(), result.micros.end())
        << ",\"combinations\":" << result.work.combinations
        << ",\"products\":" << result.work.products
        << ",\"state_visits\":" << result.work.stateVisits
        << ",\"dense_passes\":" << result.work.densePasses << "}";
}
static void benchmark(std::ostream& raw) {
    auto layout = fixture(1024, 8192, 16384, 9091);
    for (unsigned changes : {1u, 8u, 1024u}) {
        BenchResult referenceAggregate, candidateAggregate, adaptiveAggregate;
        std::size_t memory = 0;
        for (unsigned repetition = 0; repetition < 6; ++repetition) {
            FullReference reference(layout);
            IncrementalAlpha candidate(layout, 1, AlphaStrategy::ReverseCSR);
            IncrementalAlpha adaptive(layout, 1, AlphaStrategy::Adaptive);
            memory = candidate.dynamicBytes();
            BenchResult native, incremental, hybrid;
            auto runReference = [&] { native = timed([&](auto& input, auto& disabled, auto& work) { reference.refresh(input, disabled, work); }, layout.groupSlots, changes); };
            auto runCandidate = [&] { incremental = timed([&](auto& input, auto& disabled, auto& work) { if (!candidate.refresh(1, input, disabled, work)) throw std::runtime_error("bench epoch"); }, layout.groupSlots, changes); };
            auto runAdaptive = [&] { hybrid = timed([&](auto& input, auto& disabled, auto& work) { if (!adaptive.refresh(1, input, disabled, work)) throw std::runtime_error("bench epoch"); }, layout.groupSlots, changes); };
            if (repetition % 3 == 0) { runReference(); runCandidate(); runAdaptive(); }
            else if (repetition % 3 == 1) { runAdaptive(); runReference(); runCandidate(); }
            else { runCandidate(); runAdaptive(); runReference(); }
            check(equalBits(reference.states, candidate.outputs()), "benchmark final outputs");
            check(equalBits(reference.states, adaptive.outputs()), "adaptive benchmark final outputs");
            for (auto pair : {std::pair{&referenceAggregate, &native}, std::pair{&candidateAggregate, &incremental}, std::pair{&adaptiveAggregate, &hybrid}}) {
                auto& dst = *pair.first; auto& src = *pair.second;
                dst.micros.insert(dst.micros.end(), src.micros.begin(), src.micros.end());
                dst.work.combinations += src.work.combinations; dst.work.products += src.work.products;
                dst.work.stateVisits += src.work.stateVisits;
                dst.work.densePasses += src.work.densePasses;
            }
        }
        std::cout << "{\"event\":\"cpu_microbenchmark\",\"groups\":1024,\"combinations\":8192,\"states\":16384,\"changed_groups\":" << changes
            << ",\"samples_per_variant\":" << candidateAggregate.micros.size() << ",\"candidate_dynamic_bytes\":" << memory << ",\"full_scan\":";
        statistics(referenceAggregate); std::cout << ",\"reverse_csr\":"; statistics(candidateAggregate);
        std::cout << ",\"adaptive\":"; statistics(adaptiveAggregate); std::cout << "}\n";
        for (auto variant : {std::pair{"full_scan", &referenceAggregate}, std::pair{"reverse_csr", &candidateAggregate},
                             std::pair{"adaptive", &adaptiveAggregate}})
            for (std::size_t sample = 0; sample < variant.second->micros.size(); ++sample)
                raw << changes << ',' << variant.first << ',' << sample << ',' << std::setprecision(17)
                    << variant.second->micros[sample] << '\n';
    }
}
int main(int argc, char** argv) {
    try {
        differential(AlphaStrategy::ReverseCSR);
        differential(AlphaStrategy::Adaptive);
        roundingBoundary(AlphaStrategy::ReverseCSR);
        roundingBoundary(AlphaStrategy::Adaptive);
        exceptionStatusBoundary();
        finiteStatusBoundary();
        denormalControlBoundary();
        std::cout << "{\"event\":\"cpu_fixture\",\"checks\":" << checks << ",\"status\":\"passed\"}\n";
        if (argc != 2) throw std::invalid_argument("raw CSV output path required");
        std::ofstream raw(argv[1]);
        if (!raw) throw std::runtime_error("raw CSV open failed");
        raw << "changed_groups,variant,sample,refresh_us\n";
        benchmark(raw);
        raw.flush();
        if (!raw) throw std::runtime_error("raw CSV write failed");
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
