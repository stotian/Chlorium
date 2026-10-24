#ifdef CHLORIUM_EXPERIMENTAL_HEAP_RELEASE
#include "NativeHeapRelease.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
using namespace geode::prelude;
namespace {
chlorium::memory::HeapReleaseStats lastResult;
}
extern "C" __declspec(dllexport) bool ChloriumReadHeapReleaseStats(
    chlorium::memory::HeapReleaseStats* result, std::size_t size) {
    if (!result || size != sizeof(*result)) return false;
    *result = lastResult;
    return true;
}
class $modify(ChloriumHeapReleaseLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        // One queued operation after level construction, never a render/update
        // loop task. If the layer was left meanwhile, do no work for it.
        const WeakRef<PlayLayer> layer(this);
        Loader::get()->queueInMainThread([layer] {
            const auto current = layer.lock();
            if (!current || current.data() != PlayLayer::get() || !Mod::get()->getSettingValue<bool>("enabled")) return;
            auto result = chlorium::memory::releaseUnusedHeapCaches();
            result.attempts += lastResult.attempts;
            lastResult = result;
            log::info("Chlorium heap release: success={} error={} memory_valid={} private_before={} private_after={} working_before={} working_after={} elapsed_ms={}",
                result.succeeded, result.error, result.memoryReadMask, result.privateBefore, result.privateAfter,
                result.workingBefore, result.workingAfter, result.elapsedMs);
        });
    }
};
#endif
