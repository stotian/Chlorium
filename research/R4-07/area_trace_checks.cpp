#include "AreaTrace.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cfenv>
#include <cstring>
#include <cerrno>

using namespace chlorium::research::area;
unsigned checks = 0;
void require(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
int main() try {
    Recorder<64> trace;
    require(trace.start(4, 1), "start"); require(trace.beginTick(10), "tick");
    unsigned nativeCalls = 0;
    std::vector<float> inputs{-0.0f, std::bit_cast<float>(0x7fc01234u)};
    {
        CallScope outer(trace, Phase::Actions, 0, .01f, false, 0);
        ++nativeCalls;
        {
            CallScope effects(trace, Phase::Effects, 3, .01f, false, 2);
            for (std::uint32_t i = 0; i < inputs.size(); ++i) {
                Input input{}; input.target = 7; input.center = 9; input.valueBits[0] = std::bit_cast<std::uint32_t>(inputs[i]);
                require(trace.input(effects.token(), i, input), "input");
            }
            ++nativeCalls;
            CallScope reentrant(trace, Phase::Actions, 0, .02f, true, 0);
            ++nativeCalls;
        }
    }
    require(nativeCalls == 3, "native exactly once per call");
    require(trace.endTick() && trace.transportComplete(), "completed transport");
    const auto records = trace.records();
    require(records.size() == 8, "ordered entry/input/exit count");
    for (std::size_t i = 0; i < records.size(); ++i) {
        require(records[i].sequence == i + 1, "monotonic sequence");
        require(records[i].epoch == 4 && records[i].tick == 10, "session/tick identity");
        require(records[i].reserved == 0, "wire reserved bytes zero");
    }
    require(records[2].input.valueBits[0] == 0x80000000u && records[3].input.valueBits[0] == 0x7fc01234u, "float bits without arithmetic");
    require(records[4].parent == records[1].call && records[4].depth == 2, "reentrant causal parent");
    require(!trace.active(), "bounded tick window ends");

    Recorder<2> full;
    require(full.start(1, 1), "small budget"); require(full.beginTick(1), "small tick");
    { CallScope a(full, Phase::Actions, 0, 0, false, 0); ++nativeCalls;
      CallScope b(full, Phase::Effects, 1, 0, false, 0); ++nativeCalls; }
    require(full.endTick(), "overflow still unwinds");
    require(!full.transportComplete() && (full.faults() & Overflow) && full.dropped() == 2, "overflow rejects trace");
    require(nativeCalls == 5, "overflow preserves native execution");

    Recorder<16> lifetime;
    require(lifetime.start(2, 1) && lifetime.beginTick(1), "lifetime start");
    auto list = std::make_unique<std::vector<Input>>(1);
    { CallScope call(lifetime, Phase::Effects, 0, 0, false, 1);
      require(lifetime.input(call.token(), 0, (*list)[0]), "input copied first"); list.reset(); }
    require(lifetime.endTick() && lifetime.transportComplete(), "no list read after destruction");

    Recorder<16> throws;
    require(throws.start(1, 1) && throws.beginTick(1), "exception start");
    try { CallScope call(throws, Phase::Actions, 0, 0, false, 0); throw std::runtime_error("native exception"); }
    catch (const std::runtime_error&) { ++checks; }
    require(throws.endTick() && !throws.transportComplete() && (throws.faults() & NativeException), "native exception never normal completion");
    require(throws.records().back().kind == Kind::Unwind, "explicit unwind outcome");

    Recorder<16> stale;
    require(stale.start(1, 1) && stale.beginTick(1), "stale start");
    auto old = stale.enter(Phase::Actions, 0, 0, false, 0);
    require(!stale.start(2, 1), "busy session replacement rejected");
    require(stale.leave(old) && stale.endTick(), "old call balanced");
    require(stale.start(2, 1) && stale.beginTick(1), "new epoch");
    require(!stale.leave(old), "stale session token rejected");
    require(stale.endTick() && !stale.transportComplete() && (stale.faults() & Stale), "stale never complete");

    Recorder<16, 1> deep;
    require(deep.start(1, 1) && deep.beginTick(1), "depth start");
    { CallScope a(deep, Phase::Actions, 0, 0, false, 0);
      CallScope b(deep, Phase::Effects, 0, 0, false, 0); require(!b.token(), "excess depth rejected"); }
    require(deep.endTick() && !deep.transportComplete(), "depth error persists");
    Recorder<16> wrong;
    require(wrong.start(1, 1) && wrong.beginTick(1), "order start");
    auto a = wrong.enter(Phase::Actions, 0, 0, false, 0);
    auto b = wrong.enter(Phase::Effects, 0, 0, false, 0);
    require(!wrong.leave(a), "wrong call order rejected");
    require(wrong.leave(b) && wrong.leave(a) && wrong.endTick() && !wrong.transportComplete(), "order error not erased");
    Recorder<8> transparent;
    std::feclearexcept(FE_ALL_EXCEPT); std::feraiseexcept(FE_INEXACT); errno = EDOM;
    const auto beforeFlags = std::fetestexcept(FE_ALL_EXCEPT);
    require(transparent.start(1, 1) && transparent.beginTick(1), "transparent session");
    { CallScope call(transparent, Phase::Actions, 0, std::bit_cast<float>(0x7f801234u), false, 0); }
    require(transparent.endTick(), "transparent tick");
    require(std::fetestexcept(FE_ALL_EXCEPT) == beforeFlags && errno == EDOM, "census preserves FP flags and errno");
    require(transparent.records()[0].dtBits == 0x7f801234u, "signalling NaN dt copied without evaluation");
    std::feclearexcept(FE_ALL_EXCEPT);
    std::cout << "{\"event\":\"area_input_census_fixture\",\"checks\":" << checks
        << ",\"status\":\"passed\",\"semantic_coverage\":\"partial_input_only\",\"record_bytes\":" << sizeof(Record) << "}\n";
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
