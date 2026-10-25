// Isolated diagnostic DLL only. No replacement, skipping or reordering.
#ifndef CHLORIUM_AREA_WORK_CENSUS
#error Explicit Area work census build required
#endif
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>

using namespace geode::prelude;
namespace {
using Clock = std::chrono::steady_clock;
struct Cost { std::uint64_t calls = 0, members = 0; double milliseconds = 0; };
struct Sample {
    double percent = 0, milliseconds = 0, dtSum = 0;
    std::uint64_t values = 0, phases = 0, visiblePhases = 0;
    unsigned frame = 0;
    bool visible = false;
    std::array<Cost, 5> stages{};
};
thread_local Sample* active = nullptr;
#ifdef CHLORIUM_AREA_CENSUS_FRAMES
std::array<Sample, 32> samples;
#else
std::array<Sample, 4> samples;
#endif
unsigned used = 0;
bool written = false;
const char* names[]{"move_group", "rotate_group", "transform_group", "tint_group", "transform_objects"};
template <class F> void measured(unsigned stage, CCArray* objects, F&& native) {
    auto sample = active;
    if (!sample) { native(); return; }
    auto& cost = sample->stages[stage];
    ++cost.calls; cost.members += objects ? objects->count() : 0;
    const auto begin = Clock::now();
    native();
    cost.milliseconds += std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
}
void write(bool flush = false) {
    if (written || !used || (!flush && used != samples.size())) return;
    written = true;
    const auto path = std::getenv("CHLORIUM_AUDIT_OUTPUT");
    if (!path) return;
    std::ofstream out(std::string(path) + ".area-work.jsonl", std::ios::trunc);
    for (unsigned n = 0; n < used; ++n) {
        const auto& sample = samples[n];
        matjson::Value stages = matjson::Value::array();
        for (unsigned stage = 0; stage < sample.stages.size(); ++stage) {
            const auto& cost = sample.stages[stage];
            stages.push(matjson::makeObject({{"name",names[stage]}, {"calls",cost.calls},
                {"members",cost.members}, {"inclusive_ms",cost.milliseconds}}));
        }
        out << matjson::makeObject({{"schema",1}, {"phase",n}, {"percent",sample.percent},
            {"area_inclusive_ms",sample.milliseconds}, {"value_calls",sample.values}, {"visible",sample.visible},
            {"director_frame",sample.frame}, {"physics_phases",sample.phases}, {"visible_phases",sample.visiblePhases}, {"dt_sum",sample.dtSum},
            {"stages",std::move(stages)}, {"diagnostic_only",true}, {"native_calls_preserved",true}}).dump(matjson::NO_INDENTATION) << '\n';
    }
    out.flush();
    if (!out) log::error("Area work census output incomplete; no valid census proof");
}
}
extern "C" __declspec(dllexport) void ChloriumFlushAreaWorkCensus() { write(true); }
class $modify(ChloriumAreaWorkCensus, GJBaseGameLayer) {
    void processAreaActions(float dt, bool visible) {
#ifdef CHLORIUM_AREA_CENSUS_VISIBLE_ONLY
        if (!visible) { GJBaseGameLayer::processAreaActions(dt,visible); return; }
#endif
        const auto play = PlayLayer::get();
        if (active || written || !play || static_cast<GJBaseGameLayer*>(play) != this ||
            play->getCurrentPercent() < 96.f) {
            GJBaseGameLayer::processAreaActions(dt, visible); return;
        }
#ifdef CHLORIUM_AREA_CENSUS_FRAMES
        if (play->getCurrentPercent() >= 97.f) {
            write(true); GJBaseGameLayer::processAreaActions(dt,visible); return;
        }
        const auto frame = CCDirector::get()->getTotalFrames();
        if (!used || samples[used-1].frame != frame) {
            if (used == samples.size()) { write(true); GJBaseGameLayer::processAreaActions(dt,visible); return; }
            samples[used].frame = frame; samples[used].percent = play->getCurrentPercent(); ++used;
        }
        auto& sample = samples[used-1];
#else
        if (used == samples.size()) { GJBaseGameLayer::processAreaActions(dt,visible); return; }
        auto& sample = samples[used++]; sample.percent = play->getCurrentPercent();
#endif
        sample.visible = visible; ++sample.phases; sample.visiblePhases += visible; sample.dtSum += dt;
        struct Scope { ~Scope() { active = nullptr; } } scope;
        active = &sample;
        const auto begin = Clock::now();
        GJBaseGameLayer::processAreaActions(dt, visible);
        sample.milliseconds += std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
        active = nullptr;
#ifndef CHLORIUM_AREA_CENSUS_FRAMES
        write(); // At most one bounded write after all four native phases.
#endif
    }
    float getAreaObjectValue(EnterEffectInstance* instance, GameObject* object, CCPoint& position, bool& show) {
        if (active) ++active->values;
        return GJBaseGameLayer::getAreaObjectValue(instance, object, position, show);
    }
    void processAreaMoveGroupAction(CCArray* a, EnterEffectInstance* e, CCPoint p, int a0, int a1, int b0, int b1, int i, bool t, bool r) {
        measured(0,a,[&]{ GJBaseGameLayer::processAreaMoveGroupAction(a,e,p,a0,a1,b0,b1,i,t,r); });
    }
    void processAreaRotateGroupAction(CCArray* a, EnterEffectInstance* e, CCPoint p, int a0, int a1, int b0, int b1, int i, bool t, bool r) {
        measured(1,a,[&]{ GJBaseGameLayer::processAreaRotateGroupAction(a,e,p,a0,a1,b0,b1,i,t,r); });
    }
    void processAreaTransformGroupAction(CCArray* a, EnterEffectInstance* e, CCPoint p, int a0, int a1, int b0, int b1, int i, bool t, bool r) {
        measured(2,a,[&]{ GJBaseGameLayer::processAreaTransformGroupAction(a,e,p,a0,a1,b0,b1,i,t,r); });
    }
    void processAreaTintGroupAction(CCArray* a, EnterEffectInstance* e, CCPoint p, bool t) {
        measured(3,a,[&]{ GJBaseGameLayer::processAreaTintGroupAction(a,e,p,t); });
    }
    void transformAreaObjects(GameObject* object, CCArray* a, float x, float y, bool reset) {
        measured(4,a,[&]{ GJBaseGameLayer::transformAreaObjects(object,a,x,y,reset); });
    }
};
