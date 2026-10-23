// Dedicated private performance probe. Excluded from normal packages.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GManager.hpp>
#include "renderer/Renderer.hpp"
#include "BProfiler.hpp"
#include <windows.h>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <numeric>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;
bool chloriumAuditUseNative() { return std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr; }
namespace {
bool ready = false, initialized = false, running = false, nodesAdded = false;
bool captured = false;
int menuFrames = 0, draws = 0, queryIndex = 0, activeQuery = -1, progressSecond = -1;
double duration = 12;
Clock::time_point started, lastEnd;
std::ofstream report;
fs::path directory;
std::vector<double> cpu, intervals, gpu;
struct Query { GLuint id[2]{}; bool pending = false, measure = false; } queries[32];
void line(const std::string& text) { report << text << '\n'; report.flush(); }
void stop(bool success, const char* reason) {
    line(fmt::format("{{\"event\":\"finished\",\"success\":{},\"reason\":\"{}\"}}", success, reason));
    report.close(); TerminateProcess(GetCurrentProcess(), success ? 0 : 2);
}
double elapsed() { return std::chrono::duration<double>(Clock::now() - started).count(); }
double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, static_cast<size_t>(p * (values.size() - 1)))];
}
class BeginNode : public CCNode {
    void draw() override {
        auto& query = queries[queryIndex];
        activeQuery = -1;
        if (query.pending) {
            GLint available = 0; glGetQueryObjectiv(query.id[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return;
            GLint64 a = 0, b = 0;
            glGetQueryObjecti64v(query.id[0], GL_QUERY_RESULT, &a);
            glGetQueryObjecti64v(query.id[1], GL_QUERY_RESULT, &b);
            if (query.measure && b >= a) gpu.push_back((b - a) / 1e6);
            query.pending = false;
        }
        if (!query.id[0]) glGenQueries(2, query.id);
        query.measure = elapsed() >= 2;
        glQueryCounter(query.id[0], GL_TIMESTAMP);
        activeQuery = queryIndex;
    }
};
class EndNode : public CCNode {
    void draw() override {
        ++draws;
        if (activeQuery >= 0) {
            auto& query = queries[activeQuery]; glQueryCounter(query.id[1], GL_TIMESTAMP); query.pending = true;
            queryIndex = (queryIndex + 1) % 32; activeQuery = -1;
        }
        if (!captured && elapsed() >= 1) {
            captured = true;
            GLint viewport[4]{}, readFramebuffer = 0, drawFramebuffer = 0, alignment = 4;
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            if (viewport[2] > 0 && viewport[3] > 0 && viewport[2] <= 8192 && viewport[3] <= 8192) {
                std::vector<unsigned char> pixels(static_cast<size_t>(viewport[2]) * viewport[3] * 4);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
                GLint previousBuffer; glGetIntegerv(GL_READ_BUFFER, &previousBuffer);
                glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
                glReadBuffer(drawFramebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(viewport[0], viewport[1], viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glReadBuffer(previousBuffer); glPixelStorei(GL_PACK_ALIGNMENT, alignment);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
                std::ofstream image(directory / "references" / (chloriumAuditUseNative() ? "orbit-native.ppm" : "orbit-chlorium.ppm"), std::ios::binary);
                image << "P6\n" << viewport[2] << ' ' << viewport[3] << "\n255\n";
                for (int y = viewport[3] - 1; y >= 0; --y)
                    for (int x = 0; x < viewport[2]; ++x)
                        image.write(reinterpret_cast<const char*>(pixels.data() + (static_cast<size_t>(y) * viewport[2] + x) * 4), 3);
            }
        }
    }
};
void tick() {
    if (!initialized) {
        initialized = true;
        const auto path = std::getenv("CHLORIUM_AUDIT_OUTPUT");
        if (!path) return;
        report.open(path, std::ios::trunc); directory = fs::path(path).parent_path();
        if (auto seconds = std::getenv("CHLORIUM_BENCHMARK_SECONDS")) duration = std::clamp(std::atof(seconds), 5.0, 240.0);
        line(fmt::format("{{\"event\":\"started\",\"level\":133175713,\"native\":{},\"seconds\":{}}}", chloriumAuditUseNative(), duration));
    }
    if (!report) return;
    if (!running && ready && ++menuFrames > 60) {
        std::ifstream input(directory / "references/orbit.level", std::ios::binary);
        if (!input) { stop(false, "Orbit data missing"); return; }
        std::string data(std::istreambuf_iterator<char>(input), {});
        auto level = GJGameLevel::create();
        level->m_levelName = "ORBIT private benchmark"; level->m_levelString = data.c_str(); level->m_songID = 80273;
        line("{\"event\":\"loading\"}");
        auto scene = PlayLayer::scene(level, false, false);
        if (!scene) { stop(false, "Orbit scene creation failed"); return; }
        CCDirector::get()->replaceScene(scene);
        CCDirector::get()->setNextDeltaTimeZero(true);
        running = true; started = Clock::now(); lastEnd = started;
        BProfiler::setEnabled(true); BProfiler::useAverages(true);
        line("{\"event\":\"loaded\"}");
        return;
    }
    if (!running) return;
    auto play = PlayLayer::get();
    if (!play || !play->m_objectLayer) return;
    if (!nodesAdded) {
        auto begin = new BeginNode; begin->init(); begin->autorelease(); play->addChild(begin, INT_MIN);
        auto end = new EndNode; end->init(); end->autorelease(); play->addChild(end, INT_MAX);
        nodesAdded = true;
        using SwapInterval = BOOL(WINAPI*)(int);
        auto swap = reinterpret_cast<SwapInterval>(wglGetProcAddress("wglSwapIntervalEXT"));
        if (swap) swap(0); // Test context only; never writes player preferences.
    }
    // Keep measuring actual draws when the private window is unfocused.
    if (!draws) CCDirector::get()->drawScene();
    glFlush();
    const double time = elapsed();
    const int second = static_cast<int>(time);
    if (second != progressSecond) {
        progressSecond = second;
        auto ren = Renderer::get();
        line(fmt::format("{{\"event\":\"progress\",\"elapsed\":{},\"game_time\":{},\"player_x\":{},\"player_y\":{},\"objects\":{},\"gpu_active\":{},\"cpu_optimizations\":{}}}",
            time, play->m_gameState.m_totalTime, play->m_player1->getPositionX(), play->m_player1->getPositionY(),
            play->m_objects->count(), ren && ren->isEnabled(), ren && ren->useOptimizations()));
    }
    if (time >= duration + 2) {
        const double averageInterval = intervals.empty() ? 0 : std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();
        line(fmt::format("{{\"event\":\"metrics\",\"frames\":{},\"fps_average\":{},\"cpu_ms_p50\":{},\"cpu_ms_p95\":{},\"cpu_ms_p99\":{},\"frame_ms_p95\":{},\"frame_ms_p99\":{},\"gpu_ms_p50\":{},\"gpu_ms_p95\":{}}}",
            cpu.size(), averageInterval > 0 ? 1000 / averageInterval : 0, percentile(cpu,.5), percentile(cpu,.95), percentile(cpu,.99),
            percentile(intervals,.95), percentile(intervals,.99), percentile(gpu,.5), percentile(gpu,.95)));
        line(fmt::format("{{\"event\":\"cpu_profile\",\"timers\":{}}}", matjson::Value(BProfiler::toString()).dump()));
        stop(true, "Orbit benchmark window completed (automatic no-input test path)");
    }
}
}
class $modify(OrbitBenchmarkMenu, MenuLayer) {
    bool init() { if (!MenuLayer::init()) return false; ready = true; return true; }
};
class $modify(OrbitBenchmarkDirector, CCDisplayLinkDirector) {
    void mainLoop() {
        const auto before = Clock::now(); draws = 0;
        CCDisplayLinkDirector::mainLoop(); tick();
        const auto after = Clock::now();
        if (running) {
            if (elapsed() >= 2) {
                cpu.push_back(std::chrono::duration<double,std::milli>(after - before).count());
                intervals.push_back(std::chrono::duration<double,std::milli>(after - lastEnd).count());
            }
            lastEnd = after;
            BProfiler::frameEnd();
        }
    }
};
// This harness never overrides collisions or destroyPlayer. The authorized
// benchmark level is the auto level 133260800 and must play by its own logic.
class $modify(OrbitBenchmarkSaveGuard, GManager) {
    void save() { if (!std::getenv("CHLORIUM_AUDIT_OUTPUT")) GManager::save(); }
};
