// Built only with CHLORIUM_RUNTIME_AUDIT. Never part of the distributed mod.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/GManager.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/LevelDownloadDelegate.hpp>
#ifdef CHLORIUM_AUDIT_HARNESS_ONLY
#include "../resources/shaders/headers/shared.h"
#include <span>
class Renderer {
    struct Groups { std::span<GroupCombinationState> getGroupStates() { return {}; } } groups;
public:
    static Renderer* get() { return nullptr; }
    bool isEnabled() { return false; }
    Groups& getGroupManager() { return groups; }
};
#else
#include "renderer/Renderer.hpp"
#endif
#include <fstream>
#include <cstdlib>
#include <windows.h>

using namespace geode::prelude;
bool chloriumAuditUseNative() {
#ifdef CHLORIUM_AUDIT_HARNESS_ONLY
    return true;
#else
    return std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr;
#endif
}
namespace {
bool menuReady = false;
int frames = 0, phase = 0, test = 0;
std::vector<int> levels{1, 19, 22};
Ref<GJGameLevel> cachedGrief;
fs::path auditDirectory;
struct Frame { std::vector<unsigned char> pixels; int width, height; double fraction; unsigned colors = 0; };
std::optional<Frame> captured;
bool native = false, inspectedCache = false;
std::ofstream report;
bool initialized = false;
bool captureNodeAdded = false;
bool downloadStarted = false;
void line(const std::string& value) {
    if (report) { report << value << '\n'; report.flush(); }
}
void stop(bool success, const std::string& reason) {
    line(fmt::format("{{\"event\":\"finished\",\"success\":{},\"reason\":\"{}\"}}", success, reason));
    report.close();
    // End only this dedicated test process, without writing test play sessions
    // into the player's persistent save data.
    TerminateProcess(GetCurrentProcess(), success ? 0 : 2);
}
class AuditLevelDownload final : public LevelDownloadDelegate {
public:
    void levelDownloadFinished(GJGameLevel* level) override {
        if (!level || level->m_levelString.empty()) { stop(false, "empty downloaded level"); return; }
        fs::create_directories(auditDirectory / "references");
        std::ofstream output(auditDirectory / "references/orbit.level", std::ios::binary);
        output.write(level->m_levelString.data(), level->m_levelString.size()); output.close();
        std::ofstream metadata(auditDirectory / "references/orbit-game-metadata.json");
        metadata << fmt::format("{{\"id\":{},\"name\":{},\"song_id\":{},\"low_detail_available\":{},\"data_bytes\":{}}}",
            level->m_levelID.value(), matjson::Value(std::string(level->m_levelName.c_str())).dump(),
            level->m_songID, level->m_lowDetailMode, level->m_levelString.size()); metadata.close();
        stop(true, "requested level downloaded through the game client");
    }
    void levelDownloadFailed(int response) override {
        line(fmt::format("{{\"event\":\"download_failed\",\"response\":{}}}", response));
        stop(false, "game client could not download the requested level");
    }
} levelDownload;
void capture() {
    if (!report || phase != 1 || frames < 100 || captured) return;
    GLint viewport[4]{}, readFramebuffer = 0, drawFramebuffer = 0, alignment = 4;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    if (viewport[2] <= 0 || viewport[3] <= 0 || viewport[2] > 8192 || viewport[3] > 8192) return;
    Frame frame{{}, viewport[2], viewport[3], 0};
    frame.pixels.resize(static_cast<size_t>(frame.width) * frame.height * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
    GLint previousBuffer = GL_BACK;
    glGetIntegerv(GL_READ_BUFFER, &previousBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glReadBuffer(drawFramebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(viewport[0], viewport[1], frame.width, frame.height, GL_RGBA, GL_UNSIGNED_BYTE, frame.pixels.data());
    glReadBuffer(previousBuffer); glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    size_t lit = 0;
    for (size_t p = 0; p < frame.pixels.size(); p += 4)
        if (frame.pixels[p] > 12 || frame.pixels[p + 1] > 12 || frame.pixels[p + 2] > 12) ++lit;
    frame.fraction = static_cast<double>(lit) / (frame.pixels.size() / 4);
    std::array<bool, 32768> distinct{};
    for (size_t p = 0; p < frame.pixels.size(); p += 16) {
        const auto key = (frame.pixels[p] >> 3) * 1024 + (frame.pixels[p + 1] >> 3) * 32 + (frame.pixels[p + 2] >> 3);
        if (!distinct[key]) { distinct[key] = true; ++frame.colors; }
    }
    const auto name = fmt::format("runtime-{}-level-{}.ppm", native ? "native" : "gpu", levels[test]);
    std::ofstream image(auditDirectory / name, std::ios::binary);
    image << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (int y = frame.height - 1; y >= 0; --y)
        for (int x = 0; x < frame.width; ++x)
            image.write(reinterpret_cast<const char*>(frame.pixels.data() + (static_cast<size_t>(y) * frame.width + x) * 4), 3);
    captured = std::move(frame);
}
class AuditCaptureNode : public CCNode {
    void draw() override { capture(); }
};
void tick() {
    if (!initialized) {
        initialized = true;
        const auto* path = std::getenv("CHLORIUM_AUDIT_OUTPUT");
        if (!path) return;
        report.open(path, std::ios::trunc);
        auditDirectory = fs::path(path).parent_path();
        native = chloriumAuditUseNative();
        line(fmt::format("{{\"event\":\"started\",\"id\":\"{}\",\"enabled\":{}}}", Mod::get()->getID(), Mod::get()->getSettingValue<bool>("enabled")));
    }
    if (!report) return;
    ++frames;
    if (frames > 1800) stop(false, "frame timeout");
    if (phase == 0 && menuReady && frames > 60) {
        if (const auto requested = std::getenv("CHLORIUM_AUDIT_FETCH_ID")) {
            if (!downloadStarted) {
                downloadStarted = true; frames = 0;
                const int id = std::atoi(requested);
                if (id <= 0) { stop(false, "invalid requested level ID"); return; }
                auto manager = GameLevelManager::get();
                auto saved = manager->getSavedLevel(id);
                if (saved && !saved->m_levelString.empty()) { levelDownload.levelDownloadFinished(saved); return; }
                manager->m_levelDownloadDelegate = &levelDownload;
                manager->downloadLevel(id, false, 0);
                line(fmt::format("{{\"event\":\"downloading\",\"level\":{}}}", id));
            }
            return;
        }
        if (!inspectedCache) {
            inspectedCache = true;
            auto manager = GameLevelManager::get();
            for (auto [key, value] : CCDictionaryExt<std::string, CCObject*>(manager->m_onlineLevels)) {
                auto level = typeinfo_cast<GJGameLevel*>(value);
                if (!level) continue;
                auto name = std::string(level->m_levelName.c_str());
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
                if (name != "grief") continue;
                auto saved = manager->getSavedLevel(level);
                if (saved && !saved->m_levelString.empty()) {
                    cachedGrief = saved; levels.push_back(saved->m_levelID.value()); break;
                }
            }
            line(fmt::format("{{\"event\":\"grief_cache\",\"available\":{}}}", cachedGrief != nullptr));
        }
        if (test == levels.size()) { stop(true, "level loads, checkpoints, resets and menu returns completed"); return; }
        auto source = test >= 3 ? cachedGrief.data() : GameLevelManager::get()->getMainLevel(levels[test], false);
        if (!source || source->m_levelString.empty()) { stop(false, "missing built-in level"); return; }
        auto copy = GJGameLevel::create();
        copy->m_levelString = source->m_levelString;
        copy->m_levelName = "Chlorium runtime audit";
        copy->m_audioTrack = source->m_audioTrack;
        copy->m_songID = source->m_songID;
        // A fresh object with ID 0 cannot replace the original level record.
        line(fmt::format("{{\"event\":\"loading\",\"level\":{}}}", levels[test]));
        auto scene = PlayLayer::scene(copy, false, false);
        if (!scene) { stop(false, "scene creation failed"); return; }
        CCDirector::get()->replaceScene(scene);
        phase = 1; frames = 0; menuReady = false; captured.reset(); captureNodeAdded = false;
        return;
    }
    auto play = PlayLayer::get();
    if (phase == 1 && play && !captureNodeAdded) {
        auto node = new AuditCaptureNode;
        node->init(); node->autorelease();
        play->addChild(node, INT_MAX);
        captureNodeAdded = true;
    }
    if (phase == 1 && play && frames > 100) {
        // Some GD configurations suppress rendering while unfocused. Invoke
        // the engine's frame draw explicitly in this dedicated harness.
        if (!captured) CCDirector::get()->drawScene();
        auto ren = Renderer::get();
        if (!native && (!ren || !ren->isEnabled())) {
            if (frames % 60 == 0) line(fmt::format("{{\"event\":\"renderer_missing\",\"objects\":{},\"layer\":{}}}", play->m_objects ? play->m_objects->count() : 0, play->m_objectLayer != nullptr));
            if (frames < 600) return;
            stop(false, "GPU renderer did not initialize"); return;
        }
        if (!captured) { if (frames < 600) return; stop(false, "no rendered framebuffer captured"); return; }
        const double fraction = captured->fraction;
        line(fmt::format("{{\"event\":\"rendered\",\"level\":{},\"lit_fraction\":{},\"colors\":{},\"gpu\":{}}}", levels[test], fraction, captured->colors, !native));
        if (fraction < .05) { stop(false, "black framebuffer"); return; }
        if (captured->colors < 32) { stop(false, "flat-color framebuffer instead of a textured level"); return; }
        play->togglePracticeMode(true);
        auto states = ren ? ren->getGroupManager().getGroupStates() : std::span<GroupCombinationState>{};
        if (!native && states.empty()) { stop(false, "missing checkpoint group state"); return; }
        if (!native) states[0].offset = {7, -3};
        auto checkpoint = play->createCheckpoint();
        if (!checkpoint) { stop(false, "checkpoint creation failed"); return; }
        play->storeCheckpoint(checkpoint);
        if (!native) states[0].offset = {91, 47};
        play->loadFromCheckpoint(checkpoint);
        if (!native && glm::length(states[0].offset - glm::vec2(7, -3)) > .0001f) {
            stop(false, "GPU checkpoint state was not restored"); return;
        }
        line(fmt::format("{{\"event\":\"checkpoint_restored\",\"level\":{}}}", levels[test]));
        play->resetLevel();
        phase = 2; frames = 0;
        return;
    }
    if (phase == 2 && play && frames > 90) {
        line(fmt::format("{{\"event\":\"reset_survived\",\"level\":{}}}", levels[test]));
        CCDirector::get()->replaceScene(MenuLayer::scene(false));
        phase = 3; frames = 0;
        return;
    }
    if (phase == 3 && menuReady && !PlayLayer::get() && frames > 60) {
        if (Renderer::get()) { stop(false, "renderer retained after scene exit"); return; }
        line(fmt::format("{{\"event\":\"menu_returned\",\"level\":{}}}", levels[test]));
        ++test; phase = 0; frames = 0;
    }
}
}
class $modify(ChloriumAuditMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        menuReady = true;
        return true;
    }
};
class $modify(ChloriumAuditDirector, CCDisplayLinkDirector) {
    void mainLoop() { CCDisplayLinkDirector::mainLoop(); tick(); }
};
class $modify(ChloriumAuditSaveGuard, GManager) {
    void save() {
        if (!std::getenv("CHLORIUM_AUDIT_OUTPUT")) GManager::save();
    }
};
