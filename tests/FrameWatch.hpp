#pragma once
// Private visual diagnostic. Reads the final scene asynchronously; its runs
// are excluded from performance comparisons. A dark frame is evidence to
// compare with native playback, not automatically a rendering defect.
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <vector>

namespace chlorium::test {
class FrameWatch {
    struct Pending {
        GLuint buffer = 0;
        GLsync fence = nullptr;
        int width = 0, height = 0;
        uint64_t frame = 0;
        double percent = 0, gameTime = 0, wallElapsed = 0;
    };
    std::array<Pending, 4> pending{};
    std::ofstream csv;
    std::function<void(const std::string&)> log;
    std::string prefix;
    bool active = false, lastDark = false;
    unsigned saved = 0;
    uint64_t queued = 0, measured = 0, skipped = 0;
    HGLRC context = nullptr;

    void collect(Pending& p) {
        if (!p.fence) return;
        const auto status = glClientWaitSync(p.fence, 0, 0);
        if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) return;
        glDeleteSync(p.fence); p.fence = nullptr;
        GLint oldPack; glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &oldPack);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, p.buffer);
        std::vector<unsigned char> row(static_cast<size_t>(p.width) * 4);
        glGetBufferSubData(GL_PIXEL_PACK_BUFFER, static_cast<GLintptr>(p.height / 2) * p.width * 4,
            row.size(), row.data());
        uint64_t sum = 0; size_t black = 0;
        for (size_t i = 0; i < row.size(); i += 4) {
            sum += row[i] + row[i+1] + row[i+2];
            black += row[i] <= 4 && row[i+1] <= 4 && row[i+2] <= 4;
        }
        const double mean = static_cast<double>(sum) / (p.width * 3);
        const bool darkBand = mean < 2 && black >= static_cast<size_t>(p.width * .99);
        bool confirmedDark = false;
        if (darkBand && p.percent > .1) {
            std::vector<unsigned char> image(static_cast<size_t>(p.width) * p.height * 4);
            glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, image.size(), image.data());
            sum = 0; black = 0;
            // Exclude the top/bottom HUD, including the test's FPS/mode text.
            const int yBegin = p.height * 12 / 100, yEnd = p.height * 88 / 100;
            for (size_t i = static_cast<size_t>(yBegin) * p.width * 4;
                 i < static_cast<size_t>(yEnd) * p.width * 4; i += 4) {
                sum += image[i] + image[i+1] + image[i+2];
                black += image[i] <= 4 && image[i+1] <= 4 && image[i+2] <= 4;
            }
            const auto pixels = static_cast<size_t>(p.width) * (yEnd - yBegin);
            const double fullMean = static_cast<double>(sum) / (pixels * 3);
            confirmedDark = fullMean < 4 && black >= static_cast<size_t>(pixels * .99);
            if (confirmedDark && !lastDark && saved < 32) {
                const auto path = fmt::format("{}.dark-{}.ppm", prefix, p.frame);
                std::ofstream file(path, std::ios::binary);
                file << "P6\n" << p.width << ' ' << p.height << "\n255\n";
                std::vector<unsigned char> rgb(static_cast<size_t>(p.width) * 3);
                for (int y = p.height - 1; y >= 0; --y) {
                    for (int x = 0; x < p.width; ++x)
                        std::memcpy(rgb.data() + x * 3, image.data() + (static_cast<size_t>(y) * p.width + x) * 4, 3);
                    file.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
                }
                ++saved;
                log(fmt::format("{{\"event\":\"dark_frame\",\"frame\":{},\"game_time\":{},\"elapsed\":{},\"percent\":{},\"mean_rgb\":{},\"black_fraction\":{},\"saved\":{}}}",
                    p.frame, p.gameTime, p.wallElapsed, p.percent, fullMean, static_cast<double>(black) / pixels, bool(file)));
            }
        }
        csv << p.frame << ',' << p.wallElapsed << ',' << p.gameTime << ',' << p.percent << ',' << mean << ',' << darkBand << ',' << confirmedDark << '\n';
        ++measured; if (measured % 120 == 0) csv.flush();
        lastDark = confirmedDark;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, oldPack);
    }
    void collectReady() {
        std::array<Pending*, 4> order{};
        for (size_t i = 0; i < pending.size(); ++i) order[i] = &pending[i];
        std::sort(order.begin(), order.end(), [](auto a, auto b) { return a->frame < b->frame; });
        for (auto p : order) collect(*p);
    }
public:
    bool enabled() const { return active; }
    unsigned captureCount() const { return saved; }
    void open(std::string output, std::function<void(const std::string&)> callback) {
        active = std::getenv("CHLORIUM_VISUAL_WATCH") != nullptr;
        if (!active) return;
        prefix = std::move(output); log = std::move(callback);
        csv.open(prefix + ".visual.csv");
        csv << "frame,elapsed,game_time,percent,center_row_mean_rgb,dark_band,confirmed_dark\n";
        log("{\"event\":\"visual_watch\",\"performance_comparison\":false,\"fps_overlay\":true,\"source\":\"default_back_buffer_before_swap\"}");
    }
    void draw(uint64_t frame, double percent, double gameTime, double wallElapsed) {
        if (!active || !glFenceSync || !glClientWaitSync || !glGetBufferSubData) return;
        const auto current = wglGetCurrentContext();
        if (!current) return;
        if (context && context != current) {
            // Old context object names must not be used/deleted in a new one.
            pending = {}; lastDark = false;
            log("{\"event\":\"visual_context_changed\"}");
        }
        context = current;
        collectReady();
        auto it = std::find_if(pending.begin(), pending.end(), [](const auto& p) { return !p.fence; });
        if (it == pending.end()) { ++skipped; return; }
        GLint view[4]{}; glGetIntegerv(GL_VIEWPORT, view);
        if (view[2] <= 0 || view[3] <= 0 || static_cast<uint64_t>(view[2]) * view[3] > 4 * 1024 * 1024) return;
        GLint readFB, oldPack, alignment, rowLength, skipRows, skipPixels;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFB);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &oldPack); glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength); glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        GLint oldReadBuffer; glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        auto& p = *it;
        if (!p.buffer) glGenBuffers(1, &p.buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, p.buffer);
        if (p.width != view[2] || p.height != view[3]) {
            glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(view[2]) * view[3] * 4, nullptr, GL_STREAM_READ);
            p.width = view[2]; p.height = view[3];
        }
        p.frame = frame; p.percent = percent; p.gameTime = gameTime; p.wallElapsed = wallElapsed;
        glReadPixels(view[0], view[1], view[2], view[3], GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        p.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); ++queued;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, oldPack);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment); glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, skipRows); glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
        glReadBuffer(oldReadBuffer); glBindFramebuffer(GL_READ_FRAMEBUFFER, readFB);
    }
    void finish() {
        if (!active) return;
        if (wglGetCurrentContext() == context) collectReady();
        csv.flush();
        log(fmt::format("{{\"event\":\"visual_coverage\",\"queued\":{},\"measured\":{},\"skipped\":{},\"dark_captures\":{}}}",
            queued, measured, skipped, saved));
    }
};
}
