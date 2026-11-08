// Regression source prepared during the test stop. Not compiled or executed.
#include "GroupOpacityProgram.hpp"
#include <cassert>
#ifdef NDEBUG
#error These regression checks require assertions enabled.
#endif

using chlorium::research::opacity::GroupOpacityProgram;
using Bits = std::uint32_t;

static Bits eager(const std::vector<int>& groups, std::span<const Bits> dense) {
    auto product = _mm_set_ss(1.f);
    for (const auto group : groups) {
        product = _mm_mul_ss(product, _mm_set_ss(std::bit_cast<float>(dense[std::clamp(group, 0, 9999)])));
        if (_mm_comige_ss(_mm_setzero_ps(), product)) return 0;
    }
    return std::bit_cast<Bits>(_mm_cvtss_f32(product));
}

int main() {
    const auto initial = _mm_getcsr();
    const std::vector<std::vector<int>> members{
        {}, {4, 2, 4}, {4, 2}, {4, 2, 4}, {2, 4}, {-1, 10001, 0},
        {6}, {4, 6}, {7}, {8, 8, 8}, {9, 9, 9, 9, 9, 9},
        {2, 8, 9}, {2, 9, 8}, {9, 8, 2}
    };
    GroupOpacityProgram program(members);
    std::vector<Bits> dense(10000, 0x3f800000u), values(program.groups().size());
    std::vector<Bits> wanted(members.size()), actual(members.size());
    const Bits domain[]{0u, 0x80000000u, 1u, 0x007fffffu, 0x00800000u,
                        0x3f000000u, 0x3eaaaaabu, 0x3f7fffffu, 0x3f800000u};
    for (unsigned rounding = 0; rounding < 4; ++rounding)
        for (unsigned denormals = 0; denormals < 4; ++denormals)
            for (const auto variant : domain) {
                dense[0] = variant; dense[9999] = 0x3f7fffffu;
                dense[2] = 0x3eaaaaabu; dense[4] = variant;
                dense[6] = 1; dense[7] = 0x80000000u;
                dense[8] = 0x3f7fffffu; dense[9] = 0x00800000u;
                for (std::size_t i = 0; i < values.size(); ++i) values[i] = dense[program.groups()[i]];
                const auto control = (initial & ~0xffffu) | 0x1f80u | (rounding << 13) |
                    ((denormals & 1) ? 0x40u : 0u) | ((denormals & 2) ? 0x8000u : 0u);
                for (const unsigned sticky : {0u, 0x3fu}) {
                    _mm_setcsr(control | sticky);
                    for (std::size_t i = 0; i < members.size(); ++i) wanted[i] = eager(members[i], dense);
                    const auto expectedCsr = _mm_getcsr();
                    _mm_setcsr(control | sticky);
                    const bool accepted = program.evaluate(values, actual);
                    const auto actualCsr = _mm_getcsr();
                    assert(accepted && wanted == actual && expectedCsr == actualCsr);
                }
            }

    // Invalid domains/shape and unmasked exceptions must reject before publication.
    std::fill(values.begin(), values.end(), 0x3f800000u);
    for (const Bits bad : {0x7fc00001u, 0x7f800001u, 0x7f800000u, 0xbf000000u, 0x3f800001u}) {
        values.front() = bad;
        std::fill(actual.begin(), actual.end(), 0x12345678u);
        const auto before = actual;
        _mm_setcsr(0x1fbfu);
        const bool accepted = program.evaluate(values, actual);
        const auto afterCsr = _mm_getcsr();
        assert(!accepted && actual == before && afterCsr == 0x1fbfu);
    }
    values.front() = 0x3f800000u;
    assert(!program.evaluate(std::span<const Bits>(values).first(values.size() - 1), actual));
    assert(!program.evaluate(values, std::span<Bits>(actual).first(actual.size() - 1)));
    _mm_setcsr(0x1f00u);
    const bool unmaskedAccepted = program.evaluate(values, actual);
    assert(!unmaskedAccepted && _mm_getcsr() == 0x1f00u);

    // Shared prefix removes object multiplicity; each fresh snapshot is consumed.
    const std::vector<std::vector<int>> repeated(4096, {2, 4, 2, 4});
    GroupOpacityProgram shared(repeated);
    std::vector<Bits> many(repeated.size());
    std::array<Bits, 2> sharedValues{0x3f000000u, 0x3f000000u};
    _mm_setcsr(0x1f80u);
    assert(shared.evaluate(sharedValues, many));
    assert(shared.prefixCount() == 4 && shared.multiplications() == 4);
    assert(std::all_of(many.begin(), many.end(), [](Bits b) { return b == 0x3d800000u; }));
    sharedValues[0] = 0;
    assert(shared.evaluate(sharedValues, many) && shared.multiplications() == 1);
    assert(std::all_of(many.begin(), many.end(), [](Bits b) { return b == 0; }));

    const std::vector<std::vector<int>> aliasMembers{{1}, {2}};
    GroupOpacityProgram alias(aliasMembers);
    std::array<Bits, 3> overlapping{0x3f000000u, 0x3e800000u, 0};
    assert(alias.evaluate(std::span<const Bits>(overlapping).first(2), std::span<Bits>(overlapping).subspan(1)));
    assert(overlapping[1] == 0x3f000000u && overlapping[2] == 0x3e800000u);
    const std::vector<std::vector<int>> emptyMembers;
    GroupOpacityProgram empty(emptyMembers);
    assert(empty.evaluate({}, {}) && empty.multiplications() == 0);
    _mm_setcsr(initial);
}
