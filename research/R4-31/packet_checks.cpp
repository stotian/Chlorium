#include "PacketRebuildPlan.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <cstddef>
#ifdef NDEBUG
#error These checks require assertions.
#endif
using chlorium::research::PacketRebuildPlan;
std::vector<PacketRebuildPlan::Ticket> take(PacketRebuildPlan& plan) {
    std::vector<PacketRebuildPlan::Ticket> result;
    result.reserve(plan.packetCount());
    assert(plan.takePending(result));
    return result;
}
int main() {
    PacketRebuildPlan plan;
    const std::vector<std::vector<std::uint32_t>> inputs{{0,0}, {1}, {0,1}, {}};
    const std::array<std::uint32_t,4> all{0,1,2,3};
    assert(plan.compile(inputs, 2));
    assert(!plan.canSubmit(all, plan.revision(), true));
    std::vector<PacketRebuildPlan::Ticket> undersized;
    assert(!plan.takePending(undersized));
    assert(undersized.empty());
    auto initial = take(plan); assert(initial.size() == 4);
    for (auto job : initial) assert(plan.publish(job, true));
    assert(plan.canSubmit(all, plan.revision(), true));
    assert(!plan.canSubmit(all, plan.revision(), false));
    const std::array<std::uint32_t,2> reversed{2,0}, repeated{1,1}, outside{0,4};
    assert(!plan.canSubmit(reversed, plan.revision(), true));
    assert(!plan.canSubmit(repeated, plan.revision(), true));
    assert(!plan.canSubmit(outside, plan.revision(), true));
    const auto oldVisibility = plan.revision();
    assert(plan.invalidateInput(0));
    assert(!plan.canSubmit(all, oldVisibility, true));
    auto sparse = take(plan); assert(sparse.size() == 2);
    assert(sparse[0].packet == 0 && sparse[1].packet == 2);
    assert(plan.invalidateInput(1));
    assert(plan.publish(sparse[0], true));
    assert(!plan.publish(sparse[1], true)); // Work from the old input snapshot.
    auto latest = take(plan); assert(latest.size() == 2);
    assert(!plan.publish(latest[0], false));
    assert(plan.publish(latest[1], true));
    assert(!plan.canSubmit(all, plan.revision(), true));
    auto retry = take(plan); assert(retry.size() == 1);
    assert(plan.publish(retry[0], true));
    assert(plan.canSubmit(all, plan.revision(), true));
    assert(!plan.publish(retry[0], false));
    assert(!plan.canSubmit(all, plan.revision(), true));
    for(auto job: take(plan)) assert(plan.publish(job,true));
    assert(plan.invalidateAll());
    auto oldTopology = take(plan);
    assert(plan.compile(inputs, 2));
    assert(!plan.publish(oldTopology[0], true));
    PacketRebuildPlan other; assert(other.compile(inputs, 2));
    assert(!other.publish(take(plan)[0], true));
    const std::vector<std::vector<std::uint32_t>> invalid{{2}};
    assert(!plan.compile(invalid, 2));
    assert(!plan.canSubmit(all, plan.revision(), true));
    assert(plan.compile(inputs, 2));
    assert(!plan.invalidateInput(2));
    assert(!plan.canSubmit({}, plan.revision(), true));
    // Dense activity remains bounded and queues each packet once.
    assert(plan.compile(inputs, 2));
    for (int i=0; i<10000; ++i) assert(plan.invalidateInput(i % 2));
    assert(take(plan).size() == 4);
    // Scene reconstruction at the same address must not revive old work.
    alignas(PacketRebuildPlan) std::byte storage[sizeof(PacketRebuildPlan)];
    auto first = std::construct_at(reinterpret_cast<PacketRebuildPlan*>(storage));
    assert(first->compile(inputs, 2));
    const auto fromPreviousScene = take(*first)[0];
    std::destroy_at(first);
    auto second = std::construct_at(reinterpret_cast<PacketRebuildPlan*>(storage));
    assert(second->compile(inputs, 2));
    assert(!second->publish(fromPreviousScene, true));
    std::destroy_at(second);
    // Operation-count fixture, not a game or a frametime measurement.
    std::vector<std::vector<std::uint32_t>> large(100000);
    for(std::uint32_t i=0;i<large.size();++i) large[i] = {i % 100};
    assert(plan.compile(large, 100));
    for(auto job: take(plan)) assert(plan.publish(job, true));
    assert(plan.invalidateInput(42));
    auto affected = take(plan);
    assert(affected.size() == 1000);
    for(auto job: affected) assert(job.packet % 100 == 42);
    // Separate resident vertices from GPU parameter cells. A transform update
    // must not schedule mesh reconstruction; BOTH lanes gate the native pass.
    PacketRebuildPlan geometry, parameters;
    const std::vector<std::vector<std::uint32_t>> parameterInputs{{0}, {1}};
    assert(geometry.compile(inputs, 2) && parameters.compile(parameterInputs, 2));
    for(auto job: take(geometry)) assert(geometry.publish(job, true));
    for(auto job: take(parameters)) assert(parameters.publish(job, true));
    const std::array<std::uint32_t,2> requiredParameters{0,1};
    auto passReady = [&] {
        return geometry.canSubmit(all, geometry.revision(), true) &&
            parameters.canSubmit(requiredParameters, parameters.revision(), true);
    };
    assert(passReady());
    assert(parameters.invalidateInput(0));
    assert(take(geometry).empty());
    assert(!passReady());
    auto parameterUpdate = take(parameters); assert(parameterUpdate.size() == 1);
    assert(parameters.publish(parameterUpdate[0], true));
    assert(passReady());
    std::cout << "PASS packet invalidation, stale jobs, failed upload, full-pass fallback and painter order\n";
}
