#pragma once
#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <cstdint>

namespace chlorium::memory {
struct HeapReleaseStats {
    std::uint32_t version = 1, attempts = 0, succeeded = 0, error = 0;
    std::uint32_t memoryReadMask = 0, reserved = 0;
    std::uint64_t privateBefore = 0, privateAfter = 0, workingBefore = 0, workingAfter = 0;
    double elapsedMs = 0;
};
static_assert(sizeof(HeapReleaseStats) == 64);

inline HeapReleaseStats releaseUnusedHeapCaches() {
    HeapReleaseStats result;
    result.attempts = 1;
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        result.memoryReadMask |= 1;
        result.privateBefore = counters.PrivateUsage;
        result.workingBefore = counters.WorkingSetSize;
    }
    HEAP_OPTIMIZE_RESOURCES_INFORMATION request{HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION, 0};
    const auto start = std::chrono::steady_clock::now();
    // Windows optimizes free LFH caches in this process only. Never trim the
    // live working set, purge game assets, or touch another process's memory.
    result.succeeded = HeapSetInformation(nullptr, HeapOptimizeResources, &request, sizeof(request)) != FALSE;
    result.error = result.succeeded ? 0 : GetLastError();
    result.elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        result.memoryReadMask |= 2;
        result.privateAfter = counters.PrivateUsage;
        result.workingAfter = counters.WorkingSetSize;
    }
    return result;
}
}
