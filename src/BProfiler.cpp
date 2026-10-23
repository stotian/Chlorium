#include "BProfiler.hpp"
#include "common.hpp"
#include <fmt/format.h>

#if !defined(CHLORIUM_EXPERIMENTAL_STATIC_RENDERER) && !defined(BISMUTH_ENABLE_PROFILER)
// The minimal native audit does not link the experimental common.cpp hooks.
std::chrono::steady_clock::time_point modStartTime = std::chrono::steady_clock::now();
#endif

static bool shouldUseAverages = false;
static bool profilingEnabled = false;
static usize divideNumber = 0;

static std::map<std::string, u64> currentTimes;
static std::map<std::string, u64> savedTimes;

bool BProfiler::isEnabled() { return profilingEnabled; }

double BProfiler::currentMilliseconds(std::string_view name) {
    const auto found = currentTimes.find(std::string(name));
    return found == currentTimes.end() ? 0.0 : static_cast<double>(found->second) / 1000000.0;
}

void BProfiler::setEnabled(bool enabled) {
    profilingEnabled = enabled;
    currentTimes.clear();
    savedTimes.clear();
    divideNumber = 0;
}

static inline void addToMap(std::map<std::string, u64>& map, std::string name, u64 num) {
    auto it = map.find(name);
    if (it != map.end())
        it->second += num;
    else
        map[name] = num;
}

BProfiler::Timer BProfiler::start(std::string_view name) {
    BProfiler::Timer timer { name };
    return timer;
}

bool BProfiler::isUsingAverages() {
    return shouldUseAverages;
}

void BProfiler::useAverages(bool cond) {
    shouldUseAverages = cond;
}

std::string BProfiler::toString() {
    if (divideNumber == 0) return {};
    std::string ret = "";
    for (auto& [name, time] : savedTimes)
        ret += fmt::format("{}: {:.5f}ms\n", name, ((double)time / divideNumber) / 1000000.0);
    return ret;
}

void BProfiler::frameEnd() {
    if (!profilingEnabled) return;
    if (!shouldUseAverages) {
        savedTimes = currentTimes;
        divideNumber = 1;
    } else {
        for (auto& [name, time] : currentTimes)
            addToMap(savedTimes, name, time);
        divideNumber++;
    }
    currentTimes.clear();
}

void BProfiler::end(std::string name, u64 time) {
    addToMap(currentTimes, name, time);
}
