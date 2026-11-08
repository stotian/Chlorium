#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
#if !defined(CHLORIUM_MEASUREMENT_ONLY) || !defined(CHLORIUM_CANONICAL_AUDIT_V10)
#error Opacity shadow observations require the separate canonical diagnostic auditor.
#endif
#include "NativeOpacityShadow.hpp"
#include "GroupOpacityProgram.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/GJEffectManager.hpp>
#include <cstdlib>
#include <fstream>

using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumOpacityShadowIdentity[] =
    "CHLORIUM_OPACITY_PREFIX_SHADOW_V1";

namespace chlorium::research::opacity {
namespace {
constexpr std::size_t maxObjects = 512, maxLookups = 16384;
struct Lookup { int group; std::uint32_t bits; std::uintptr_t manager; };
struct Record { std::size_t begin, count; unsigned before, after; std::uint32_t result; };
struct Capture {
    std::array<Lookup, maxLookups> lookups{};
    std::array<Record, maxObjects> objects{};
    std::size_t lookupCount = 0, objectCount = 0;
    unsigned long long frame = 0;
    double percent = -1;
    bool recording = false, truncated = false;
    const char* rejected = nullptr;
};
thread_local Capture storage;
thread_local Capture* active = nullptr;
thread_local bool attempted = false;

struct Replay { std::uint32_t bits; std::size_t consumed; };
Replay eager(const Capture& capture, const Record& record) {
    auto product = _mm_set_ss(1.f);
    for (std::size_t i = 0; i < record.count; ++i) {
        const auto bits = capture.lookups[record.begin + i].bits;
        product = _mm_mul_ss(product, _mm_set_ss(std::bit_cast<float>(bits)));
        if (_mm_comige_ss(_mm_setzero_ps(), product)) return {0, i + 1};
    }
    return {std::bit_cast<std::uint32_t>(_mm_cvtss_f32(product)), record.count};
}

matjson::Value compare(const Capture& capture) {
    auto result = matjson::makeObject({{"diagnostic_only", true},
        {"scope", "bounded observed getter prefixes; not complete memberships"},
        {"frame", capture.frame}, {"percent", capture.percent},
        {"objects", capture.objectCount}, {"recorded_lookups", capture.lookupCount},
        {"truncated", capture.truncated}, {"status", "inconclusive"}});
    auto reject = [&](const char* reason) { result["status"] = reason; return result; };
    if (capture.rejected) return reject(capture.rejected);
    if (!capture.objectCount || !capture.lookupCount) return reject("no_group_work_observed");
    std::array<std::uint32_t, 10000> dense{};
    std::array<bool, 10000> seen{};
    std::uintptr_t manager = 0;
    const auto control = capture.objects[0].before & ~0x3fu;
    if ((control & 0x1f80u) != 0x1f80u) return reject("unmasked_fp_exceptions");
    std::vector<std::vector<int>> membership(capture.objectCount);
    for (std::size_t i = 0; i < capture.objectCount; ++i) {
        const auto& record = capture.objects[i];
        if (record.begin > capture.lookupCount || record.count > capture.lookupCount - record.begin)
            return reject("invalid_capture_range");
        if ((record.before & ~0x3fu) != control || (record.after & ~0x3fu) != control)
            return reject("fp_control_changed");
        for (std::size_t j = 0; j < record.count; ++j) {
            const auto& lookup = capture.lookups[record.begin + j];
            if (manager && manager != lookup.manager) return reject("multiple_effect_managers");
            manager = lookup.manager;
            const int group = std::clamp(lookup.group, 0, 9999);
            if (lookup.bits > 0x3f800000u && lookup.bits != 0x80000000u)
                return reject("unsupported_opacity_domain");
            if (seen[group] && dense[group] != lookup.bits) return reject("group_value_changed");
            seen[group] = true; dense[group] = lookup.bits;
            membership[i].push_back(lookup.group);
        }
    }
    GroupOpacityProgram program(membership);
    std::vector<std::uint32_t> values, actual(capture.objectCount);
    for (const auto group : program.groups()) values.push_back(dense[group]);
    std::size_t nativeMismatch = 0, nativeFlagMismatch = 0, prefixMismatch = 0;
    unsigned raisedWhileClear = 0;
    for (std::size_t i = 0; i < capture.objectCount; ++i) {
        const auto& record = capture.objects[i];
        _mm_setcsr(record.before);
        const auto expected = eager(capture, record);
        const auto flags = _mm_getcsr();
        nativeMismatch += expected.bits != record.result || expected.consumed != record.count;
        nativeFlagMismatch += flags != record.after;
        raisedWhileClear |= (record.after & ~record.before) & 0x3fu;
    }
    _mm_setcsr(control);
    const bool accepted = program.evaluate(values, actual);
    const auto programFlags = _mm_getcsr();
    _mm_setcsr(control);
    std::uint32_t eagerDigest = 2166136261u;
    for (std::size_t i = 0; i < capture.objectCount; ++i) {
        eagerDigest = (eagerDigest ^ eager(capture, capture.objects[i]).bits) * 16777619u;
        prefixMismatch += accepted && actual[i] != capture.objects[i].result;
    }
    const auto eagerFlags = _mm_getcsr();
    result["native_result_mismatches"] = nativeMismatch;
    result["native_fp_mismatches"] = nativeFlagMismatch;
    result["prefix_result_mismatches"] = prefixMismatch;
    result["fp_flags_raised_while_clear"] = raisedWhileClear;
    result["eager_result_digest"] = eagerDigest;
    result["prefix_fp_matches_eager"] = programFlags == eagerFlags;
    result["unique_groups"] = program.groups().size();
    result["prefixes"] = program.prefixCount();
    result["prefix_multiplications"] = program.multiplications();
    result["status"] = accepted && !nativeMismatch && !nativeFlagMismatch && !prefixMismatch &&
        programFlags == eagerFlags ? "observed_parity" : "mismatch";
    return result;
}
}

bool nativeOpacityShadowPending() noexcept { return !attempted; }
bool beginNativeOpacityShadow(bool eligible, unsigned long long frame, double percent) noexcept {
    if (active) { active->rejected = "nested_visibility"; return false; }
    if (!eligible || attempted) return false;
    attempted = true; storage.frame = frame; storage.percent = percent; active = &storage;
    return true;
}
void abandonNativeOpacityShadow() noexcept { active = nullptr; }
void finishNativeOpacityShadow() noexcept {
    auto* capture = active;
    active = nullptr;
    if (!capture) return;
    SavedOpacityFPState preserve;
    _mm_setcsr((_mm_getcsr() & ~0x3fu) | 0x1f80u);
    try {
        const auto result = compare(*capture);
        if (const auto path = std::getenv("CHLORIUM_AUDIT_OUTPUT")) {
            std::ofstream output(std::string(path) + ".opacity-shadow.json", std::ios::out | std::ios::noreplace);
            if (!output) { log::warn("Opacity shadow: cannot open diagnostic receipt"); return; }
            output << result.dump();
            if (!output) log::warn("Opacity shadow: incomplete diagnostic receipt");
        }
    } catch (...) {
        // Missing receipt means failed diagnosis; never unwind into gameplay.
    }
}
}

class $modify(ChloriumOpacityShadowObject, GameObject) {
    float groupOpacityMod() {
        using namespace chlorium::research::opacity;
        auto* capture = active;
        if (!capture || capture->rejected) return GameObject::groupOpacityMod();
        if (capture->recording) {
            capture->rejected = "reentrant_opacity";
            return GameObject::groupOpacityMod();
        }
        if (capture->objectCount == maxObjects) {
            capture->truncated = true;
            return GameObject::groupOpacityMod();
        }
        auto& record = capture->objects[capture->objectCount];
        record.begin = capture->lookupCount;
        record.before = _mm_getcsr();
        capture->recording = true;
        struct End { Capture& capture; ~End() { capture.recording = false; } } end{*capture};
        const auto value = GameObject::groupOpacityMod();
        record.after = _mm_getcsr();
        record.result = std::bit_cast<std::uint32_t>(value);
        record.count = capture->lookupCount - record.begin;
        ++capture->objectCount;
        return value;
    }
};

class $modify(ChloriumOpacityShadowLookup, GJEffectManager) {
    float opacityModForGroup(int group) {
        const auto value = GJEffectManager::opacityModForGroup(group);
        using namespace chlorium::research::opacity;
        if (auto* capture = active; capture && capture->recording && !capture->rejected) {
            if (capture->lookupCount == maxLookups) capture->rejected = "lookup_budget_exceeded";
            else capture->lookups[capture->lookupCount++] =
                {group, std::bit_cast<std::uint32_t>(value), reinterpret_cast<std::uintptr_t>(this)};
        }
        return value;
    }
};
#endif
