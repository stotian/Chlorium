// Compile-only research adapter. NOT in any Geode mod target or normal package.
#ifndef CHLORIUM_AREA_TRACE_RESEARCH
#error This adapter requires an explicit isolated research build
#endif
#include "AreaTrace.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/EnterEffectInstance.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <atomic>
#include <cstring>
#include <limits>

using namespace geode::prelude;
namespace {
using namespace chlorium::research::area;
Recorder<> trace;
std::atomic<DWORD> owner{0};
std::atomic<bool> armed{false};
static_assert(std::atomic<DWORD>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free);
std::atomic<std::uint64_t> foreignCalls{0};
std::uint64_t phaseOrdinal = 0;
bool enabled() noexcept {
    if (!armed.load(std::memory_order_acquire)) return false;
    if (owner.load(std::memory_order_acquire) != GetCurrentThreadId()) { foreignCalls.fetch_add(1, std::memory_order_relaxed); return false; }
    return trace.active() || trace.tickOpen();
}
Input snapshot(const EnterEffectInstance& effect) noexcept {
    Input value{};
    value.target = effect.m_targetID; value.center = effect.m_centerID;
    value.groupIndex = effect.m_targetGroupIndex; value.control = effect.m_controlID;
    value.flags = static_cast<std::uint32_t>(effect.m_paused) |
        (static_cast<std::uint32_t>(effect.m_paused2) << 1) |
        (static_cast<std::uint32_t>(effect.m_reversed) << 2);
    const std::array<float, 12> fields{effect.m_length, effect.m_offset, effect.m_moveX, effect.m_moveY,
        effect.m_scaleX, effect.m_scaleY, effect.m_rotation, effect.m_tint,
        effect.m_fromOpacity, effect.m_toOpacity, effect.m_deadzone, effect.m_relativeFade};
    for (std::size_t i = 0; i < fields.size(); ++i) value.valueBits[i] = std::bit_cast<std::uint32_t>(fields[i]);
    return value;
}
}
extern "C" __declspec(dllexport) bool ChloriumStartAreaInputCensus(std::uint64_t epoch,
    std::uint32_t phases, std::uint32_t recordBudget) noexcept {
    // Explicit activation only. Starting is not evidence that a hook ran.
    if (!phases || phases > 4) return false;
    DWORD expected = 0;
    const auto caller = GetCurrentThreadId();
    const bool claimed = owner.compare_exchange_strong(expected, caller, std::memory_order_acq_rel);
    if (!claimed && expected != caller) return false;
    if (!trace.start(epoch, phases, recordBudget)) {
        if (claimed) owner.store(0, std::memory_order_release);
        return false;
    }
    phaseOrdinal = 0; foreignCalls.store(0, std::memory_order_relaxed);
    armed.store(true, std::memory_order_release);
    return true;
}
extern "C" __declspec(dllexport) std::size_t ChloriumCopyAreaInputCensus(
    chlorium::research::area::Record* destination, std::size_t capacity, std::uint32_t* transportComplete) noexcept {
    if (transportComplete) *transportComplete = 0;
    if (GetCurrentThreadId() != owner.load(std::memory_order_acquire) || !trace.idle()) return 0;
    const auto data = trace.records();
    if (!destination || capacity < data.size()) return data.size();
    std::memcpy(destination, data.data(), data.size_bytes());
    if (transportComplete) *transportComplete = trace.transportComplete() && foreignCalls.load(std::memory_order_relaxed) == 0;
    return data.size();
}
class $modify(ChloriumAreaInputCensus, GJBaseGameLayer) {
    void processAreaActions(float dt, bool visible) {
        if (!enabled() || static_cast<GJBaseGameLayer*>(PlayLayer::get()) != static_cast<GJBaseGameLayer*>(this)) {
            GJBaseGameLayer::processAreaActions(dt, visible); return;
        }
        // This is a phase ordinal, NOT an assumed physics/render frame number.
        const bool root = !trace.tickOpen();
        if (root && !trace.beginTick(++phaseOrdinal)) { GJBaseGameLayer::processAreaActions(dt, visible); return; }
        struct TickScope {
            bool root;
            ~TickScope() noexcept {
                if (root) { trace.endTick(); if (!trace.active()) armed.store(false, std::memory_order_release); }
            }
        } tick{root};
        CallScope call(trace, Phase::Actions, 0, dt, visible, 0);
        GJBaseGameLayer::processAreaActions(dt, visible);
    }
    void processAreaEffects(gd::vector<EnterEffectInstance>* effects, GJAreaActionType type, float dt, bool visible) {
        if (!enabled() || !trace.tickOpen() || static_cast<GJBaseGameLayer*>(PlayLayer::get()) != static_cast<GJBaseGameLayer*>(this)) {
            GJBaseGameLayer::processAreaEffects(effects, type, dt, visible); return;
        }
        const auto count = effects ? effects->size() : 0;
        constexpr std::size_t effectBudget = 128;
        CallScope call(trace, Phase::Effects, static_cast<std::uint32_t>(type), dt, visible,
            static_cast<std::uint32_t>(std::min<std::size_t>(count, UINT32_MAX)));
        if (!effects || count > effectBudget) trace.incompleteList();
        if (effects && call.token()) for (std::size_t i = 0; i < std::min(count, effectBudget); ++i)
            trace.input(call.token(), static_cast<std::uint32_t>(i), snapshot((*effects)[i]));
        // No vector/instance/pointer dereference after this call: mods/native
        // code may mutate or destroy that storage before returning.
        GJBaseGameLayer::processAreaEffects(effects, type, dt, visible);
    }
};
