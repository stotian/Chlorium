#pragma once
// Private diagnostic only. Read failures are coverage gaps, never black frames.
#include "VisualSignature.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <cmath>
#include <sstream>
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
    bool active = false, lastDark = false, haveHash = false;
    unsigned saved = 0, anomalySaved = 0, lastFlags = 0;
    unsigned probeSaved = 0;
    std::vector<double> probes;
    size_t nextProbe = 0;
    uint64_t attempted = 0, queued = 0, measured = 0, skipped = 0, invalid = 0, unresolved = 0;
    uint64_t lastFrame = 0, lastHash = 0;
    double hashStartWall = 0, hashStartGame = 0;
    HGLRC context = nullptr;

    bool errors(const char* stage, uint64_t frame) {
        bool found = false;
        for (int i=0;i<16;++i) {
            const auto error = glGetError();
            if (error == GL_NO_ERROR) break;
            found = true;
            if (invalid < 32) log(fmt::format("{{\"event\":\"visual_gl_error\",\"stage\":\"{}\",\"frame\":{},\"code\":{}}}", stage, frame, error));
        }
        return found;
    }
    void reject(const Pending& p, const char* stage) {
        ++invalid; lastDark = false; lastFlags = 0; haveHash = false;
        csv << p.frame << ',' << p.wallElapsed << ',' << p.gameTime << ',' << p.percent
            << ",0,0,0,0,0,0,0\n";
        if (invalid <= 32) log(fmt::format("{{\"event\":\"visual_read_error\",\"stage\":\"{}\",\"frame\":{},\"percent\":{}}}", stage,p.frame,p.percent));
    }
    void save(const Pending& p, const std::vector<unsigned char>& rgb, bool dark, unsigned flags, const VisualSignature& sample) {
        const auto path=fmt::format("{}.{}-{}.ppm",prefix,dark?"dark":"anomaly",p.frame);
        std::ofstream file(path,std::ios::binary);
        file << "P6\n" << p.width << ' ' << p.height << "\n255\n";
        file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size()); file.flush();
        if(dark) ++saved; else ++anomalySaved;
        log(fmt::format("{{\"event\":\"{}\",\"frame\":{},\"game_time\":{},\"elapsed\":{},\"percent\":{},\"mean_rgb\":{},\"black_fraction\":{},\"flags\":{},\"saved\":{}}}",
            dark?"dark_frame":"visual_anomaly",p.frame,p.gameTime,p.wallElapsed,p.percent,sample.mean,sample.blackFraction,flags,bool(file)));
    }
    void collect(Pending& p, bool wait = false) {
        if (!p.fence) return;
        const auto status=glClientWaitSync(p.fence,wait?GL_SYNC_FLUSH_COMMANDS_BIT:0,wait?25000000:0);
        if(status==GL_TIMEOUT_EXPIRED) return;
        glDeleteSync(p.fence); p.fence=nullptr;
        if(status!=GL_ALREADY_SIGNALED && status!=GL_CONDITION_SATISFIED) {
            errors("fence",p.frame); reject(p,"fence"); return;
        }
        GLint oldPack=0; glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&oldPack);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,p.buffer);
        const auto bytes=static_cast<GLsizeiptr>(p.width)*p.height*4;
        const auto pixels=static_cast<const unsigned char*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,bytes,GL_MAP_READ_BIT));
        const bool mapError=errors("map",p.frame);
        if(!pixels || mapError) {
            if(pixels) glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPack); reject(p,"map"); return;
        }
        auto sample=analyzeVisual(pixels,p.width,p.height);
        double repeatedMs=0;
        if(haveHash && p.frame==lastFrame+1 && sample.hash==lastHash && p.gameTime>=hashStartGame) {
            if(p.gameTime-hashStartGame>.25) repeatedMs=(p.wallElapsed-hashStartWall)*1000;
        } else { hashStartWall=p.wallElapsed; hashStartGame=p.gameTime; }
        unsigned flags=sample.partial;
        if(!sample.dark && !flags && repeatedMs>=500) flags|=32; // candidate, not proof of a frozen frame
        const bool captureDark=sample.dark && p.percent>.1 && !lastDark && saved<32;
        const bool captureOther=!sample.dark && flags && (flags&~lastFlags) && anomalySaved<16;
        const bool captureProbe=nextProbe<probes.size() && p.percent>=probes[nextProbe];
        std::vector<unsigned char> rgb;
        if(captureDark || captureOther || captureProbe) {
            rgb.resize(static_cast<size_t>(p.width)*p.height*3);
            for(int y=0;y<p.height;++y) for(int x=0;x<p.width;++x)
                std::memcpy(rgb.data()+(static_cast<size_t>(p.height-1-y)*p.width+x)*3,
                    pixels+(static_cast<size_t>(y)*p.width+x)*4,3);
        }
        const bool intact=glUnmapBuffer(GL_PIXEL_PACK_BUFFER)==GL_TRUE;
        glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPack);
        const bool readError=errors("unmap_restore",p.frame);
        if(!sample.valid || !intact || readError) { reject(p,"unmap_or_restore"); return; }
        if(captureDark || captureOther) save(p,rgb,captureDark,flags,sample);
        if(captureProbe) {
            const auto path=fmt::format("{}.probe-{}.ppm",prefix,p.frame);
            std::ofstream file(path,std::ios::binary);
            file << "P6\n" << p.width << ' ' << p.height << "\n255\n";
            file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size()); file.flush(); ++probeSaved;
            while(nextProbe<probes.size() && p.percent>=probes[nextProbe]) {
                log(fmt::format("{{\"event\":\"visual_probe\",\"frame\":{},\"requested_percent\":{},\"percent\":{},\"game_time\":{},\"saved\":{}}}",
                    p.frame,probes[nextProbe++],p.percent,p.gameTime,bool(file)));
            }
        }
        csv << p.frame << ',' << p.wallElapsed << ',' << p.gameTime << ',' << p.percent << ','
            << sample.centerMean << ',' << (sample.centerMean<2) << ',' << sample.dark << ",1,"
            << flags << ',' << sample.hash << ',' << repeatedMs << '\n';
        ++measured; if(measured%120==0) csv.flush();
        lastDark=sample.dark; lastFlags=flags; haveHash=true; lastHash=sample.hash; lastFrame=p.frame;
    }
    void collectReady(bool wait = false) {
        std::array<Pending*,4> order{};
        for(size_t i=0;i<pending.size();++i) order[i]=&pending[i];
        std::sort(order.begin(),order.end(),[](auto a,auto b){return a->frame<b->frame;});
        // Keep rows chronological even if an earlier fence is not ready.
        for(auto p:order) { collect(*p,wait); if(p->fence) break; }
    }
public:
    bool enabled() const { return active; }
    unsigned captureCount() const { return saved+anomalySaved+probeSaved; }
    bool coverageComplete() const { return !active || (queued>0 && measured==queued && !invalid && !skipped && !unresolved); }
    void open(std::string output,std::function<void(const std::string&)> callback) {
        active=std::getenv("CHLORIUM_VISUAL_WATCH")!=nullptr;
        if(!active) return;
        prefix=std::move(output); log=std::move(callback);
        if(const auto requested=std::getenv("CHLORIUM_VISUAL_PROBES")) {
            std::stringstream stream(requested);std::string value;
            while(std::getline(stream,value,',') && probes.size()<16) {
                char* end=nullptr;const double percent=std::strtod(value.c_str(),&end);
                if(end==value.c_str()+value.size() && std::isfinite(percent) && percent>=0 && percent<=100) probes.push_back(percent);
                else ++invalid;
            }
            std::sort(probes.begin(),probes.end());probes.erase(std::unique(probes.begin(),probes.end()),probes.end());
        }
        csv.open(prefix+".visual.csv");
        if(!csv) { ++invalid; log("{\"event\":\"visual_read_error\",\"stage\":\"output_open\",\"frame\":0,\"percent\":0}"); }
        csv << "frame,elapsed,game_time,percent,center_row_mean_rgb,dark_band,confirmed_dark,valid,partial_flags,signature,repeated_ms\n";
        log("{\"event\":\"visual_watch\",\"version\":2,\"performance_comparison\":false,\"fps_overlay\":true,\"source\":\"default_back_buffer_before_swap\",\"roi\":\"full_width_y25_to85_percent\",\"partial_flags\":\"left2_right4_bottom8_top16_repeated32\"}");
    }
    void draw(uint64_t frame,double percent,double gameTime,double wallElapsed) {
        if(!active) return;
        ++attempted;
        Pending sample{}; sample.frame=frame; sample.percent=percent; sample.gameTime=gameTime; sample.wallElapsed=wallElapsed;
        const auto current=wglGetCurrentContext();
        if(!current || !glFenceSync || !glClientWaitSync || !glMapBufferRange) { reject(sample,"unsupported_context_or_api"); return; }
        if(context && context!=current) {
            for(auto& p:pending) if(p.fence) reject(p,"context_change_pending");
            pending={}; lastDark=false; haveHash=false; lastFlags=0;
            log("{\"event\":\"visual_context_changed\"}");
        }
        context=current;
        // Consuming errors is diagnostic-only. Record native errors separately
        // instead of attributing them to glReadPixels or counting zeros as black.
        if(errors("preexisting",frame)) { reject(sample,"preexisting_gl_error"); return; }
        collectReady();
        auto it=std::find_if(pending.begin(),pending.end(),[](const auto& p){return !p.fence;});
        if(it==pending.end()) { ++skipped; return; }
        RECT client{}; const auto window=WindowFromDC(wglGetCurrentDC());
        if(!window || !GetClientRect(window,&client) || client.right<16 || client.bottom<16 ||
           static_cast<uint64_t>(client.right)*client.bottom>4*1024*1024) { reject(sample,"drawable_dimensions"); return; }
        GLint readFB=0,oldPack=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFB); glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&oldPack);
        glGetIntegerv(GL_PACK_ALIGNMENT,&alignment); glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
        GLint oldReadBuffer=0; glGetIntegerv(GL_READ_BUFFER,&oldReadBuffer); glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
        auto& p=*it;
        if(!p.buffer) glGenBuffers(1,&p.buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,p.buffer);
        const auto byteCount=static_cast<GLsizeiptr>(client.right)*client.bottom*4;
        if(p.width!=client.right || p.height!=client.bottom) {
            glBufferData(GL_PIXEL_PACK_BUFFER,byteCount,nullptr,GL_STREAM_READ);
            p.width=client.right; p.height=client.bottom;
        }
        p.frame=frame; p.percent=percent; p.gameTime=gameTime; p.wallElapsed=wallElapsed;
        GLint allocated=0; glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER,GL_BUFFER_SIZE,&allocated);
        const bool allocationError=errors("allocation",frame);
        bool ok=p.buffer && allocated==byteCount && !allocationError;
        if(ok) {
            glReadPixels(0,0,p.width,p.height,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            ok=!errors("read_pixels",frame);
            if(ok) { p.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0); ok=p.fence && !errors("create_fence",frame); }
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPack);
        glPixelStorei(GL_PACK_ALIGNMENT,alignment); glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS,skipRows); glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
        glReadBuffer(oldReadBuffer); glBindFramebuffer(GL_READ_FRAMEBUFFER,readFB);
        const bool restoreError=errors("restore",frame);
        if(!ok || restoreError) {
            if(p.fence) {glDeleteSync(p.fence);p.fence=nullptr;}
            reject(p,"queue_or_restore"); return;
        }
        ++queued;
    }
    void finish() {
        if(!active) return;
        if(wglGetCurrentContext()==context) {
            collectReady(true); // bounded drain, at most four waits of 25 ms
            for(auto& p:pending) {
                if(p.fence) {++unresolved; glDeleteSync(p.fence);p.fence=nullptr;reject(p,"pending_at_finish");}
                if(p.buffer) {glDeleteBuffers(1,&p.buffer);p.buffer=0;}
            }
        } else for(auto& p:pending) if(p.fence) {++unresolved;reject(p,"finish_context_unavailable");}
        csv.flush();
        log(fmt::format("{{\"event\":\"visual_coverage\",\"version\":2,\"attempted\":{},\"queued\":{},\"measured\":{},\"skipped\":{},\"invalid\":{},\"pending_at_finish\":{},\"dark_captures\":{},\"anomaly_captures\":{},\"complete\":{}}}",
            attempted,queued,measured,skipped,invalid,unresolved,saved,anomalySaved,coverageComplete()));
    }
};
}
