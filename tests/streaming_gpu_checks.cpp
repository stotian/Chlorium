#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#include "renderer/StreamingUpload.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace chlorium::streaming;
using Clock = std::chrono::steady_clock;
unsigned checks = 0;
void require(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
struct Vertex { float x, y, z; unsigned char rgba[4]; float u, v; };
static_assert(sizeof(Vertex) == 24);
struct Quad { Vertex vertices[4]; };
static_assert(sizeof(Quad) == 96);
struct Api {
    void* mapWriteInvalidateRange(size_t bytes) {
        return glMapBufferRange(GL_ARRAY_BUFFER, 0, bytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    }
    bool unmap() { return glUnmapBuffer(GL_ARRAY_BUFFER) == GL_TRUE; }
    void orphan(size_t bytes) { glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW); }
};
void upload(const void* data, size_t bytes, size_t capacity, int method) {
    if (!method) { glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data); return; }
    Api api;
    const auto result = prepareUpload(api, data, bytes, capacity,
        method == 1 ? Mode::MappedRange : Mode::OrphanFullStore);
    require(result != Result::CorruptMapping, "hardware mapping became corrupt");
    if (result == Result::NativeUpload) glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data);
}
std::vector<Quad> geometry(unsigned count, unsigned phase) {
    std::vector<Quad> result(count);
    for (unsigned i = 0; i < count; ++i) {
        const float x = -1.f + float(i % 32) / 16.f, y = -1.f + float((i / 32) % 32) / 16.f;
        const float positions[4][2]{{x,y},{x+.0625f,y},{x+.0625f,y+.0625f},{x,y+.0625f}};
        const float uv[4][2]{{0,0},{1,0},{1,1},{0,1}};
        for (unsigned v = 0; v < 4; ++v) {
            auto& vertex = result[i].vertices[v];
            vertex.x = positions[v][0]; vertex.y = positions[v][1];
            vertex.rgba[0] = (i * 17 + phase * 19) % 255;
            vertex.rgba[1] = (i * 31 + phase * 37) % 255;
            vertex.rgba[2] = (i * 13 + phase * 23) % 255;
            vertex.rgba[3] = 180;
            vertex.u = uv[v][0]; vertex.v = uv[v][1];
        }
    }
    return result;
}
void bindVertices(GLuint vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), nullptr);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), reinterpret_cast<void*>(12));
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void*>(16));
}

int main() try {
    struct Fake {
        std::array<unsigned char, 16> data{};
        bool mapFails = false, valid = true;
        unsigned maps = 0, orphans = 0;
        void* mapWriteInvalidateRange(size_t) { ++maps; return mapFails ? nullptr : data.data(); }
        bool unmap() { return valid; }
        void orphan(size_t) { ++orphans; }
    } fake;
    const std::array<unsigned char, 16> input{1,2,3,4,5,6,7,8};
    fake.mapFails = true;
    require(prepareUpload(fake, input.data(), 16, 16, Mode::MappedRange) == Result::NativeUpload, "failed map retains native upload");
    fake.mapFails = false; fake.valid = false;
    require(prepareUpload(fake, input.data(), 16, 16, Mode::MappedRange) == Result::CorruptMapping, "failed unmap requests whole-store recovery");
    prepareUpload(fake, input.data(), 8, 16, Mode::OrphanFullStore);
    require(fake.orphans == 0, "partial prefix never orphans an unwritten tail");
    prepareUpload(fake, input.data(), 16, 16, Mode::OrphanFullStore);
    require(fake.orphans == 1, "only whole-store replacement may orphan");
    const auto maps = fake.maps;
    require(prepareUpload(fake, input.data(), 17, 16, Mode::MappedRange) == Result::NativeUpload && fake.maps == maps, "invalid range has no GL work");

    WNDCLASSA cls{}; cls.style = CS_OWNDC; cls.lpfnWndProc = DefWindowProcA;
    cls.hInstance = GetModuleHandleA(nullptr); cls.lpszClassName = "ChloriumStreamingGPU";
    RegisterClassA(&cls);
    const auto window = CreateWindowA(cls.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    require(window != nullptr, "hidden graphics surface");
    const auto dc = GetDC(window);
    PIXELFORMATDESCRIPTOR p{}; p.nSize = sizeof(p); p.nVersion = 1;
    p.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL; p.iPixelType = PFD_TYPE_RGBA; p.cColorBits = 32;
    require(SetPixelFormat(dc, ChoosePixelFormat(dc, &p), &p), "pixel format");
    const auto context = wglCreateContext(dc);
    require(context && wglMakeCurrent(dc, context), "native GPU context");
    glewExperimental = GL_TRUE;
    require(glewInit() == GLEW_OK && glMapBufferRange && glGenFramebuffers && glFenceSync && glClientWaitSync, "hardware streaming, fences and framebuffer API");
    while (glGetError() != GL_NO_ERROR) {}
    std::cout << "GPU=" << glGetString(GL_RENDERER) << "\nOpenGL=" << glGetString(GL_VERSION) << '\n';

    GLuint target = 0, fbo = 0, texture = 0, vbo = 0, ebo = 0;
    glGenTextures(1, &target); glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "render target complete");
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char texels[]{255,255,255,255, 127,255,127,255, 255,127,127,255, 127,127,255,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DITHER); glViewport(0,0,64,64); glClearColor(.1f,.2f,.3f,1);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
    std::vector<unsigned short> indices(16384 * 6);
    for (unsigned i=0;i<16384;++i) {
        const unsigned pattern[]{0,1,2,0,2,3};
        for (unsigned k=0;k<6;++k) indices[i*6+k] = static_cast<unsigned short>(i*4+pattern[k]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(indices[0]),indices.data(),GL_STATIC_DRAW);

    for (const unsigned count : {1024u,16384u}) {
        const auto a = geometry(count,0), b = geometry(count,1);
        const auto bytes = a.size()*sizeof(Quad);
        std::vector<unsigned char> reference, pixels(64*64*4), readback(bytes+256);
        for (int method=0;method<3;++method) {
            glBindBuffer(GL_ARRAY_BUFFER,vbo);
            std::vector<unsigned char> initial(bytes+256,0x5a);
            glBufferData(GL_ARRAY_BUFFER,initial.size(),initial.data(),GL_DYNAMIC_DRAW);
            upload(a.data(),bytes,initial.size(),method);
            glGetBufferSubData(GL_ARRAY_BUFFER,0,readback.size(),readback.data());
            require(!std::memcmp(readback.data(),a.data(),bytes), "active GPU bytes match original native bytes");
            require(std::all_of(readback.begin()+bytes,readback.end(),[](auto v){return v==0x5a;}), "unwritten GPU tail preserved");
            // Also cover full-size orphaning rather than only its fallback.
            glBufferData(GL_ARRAY_BUFFER,bytes,nullptr,GL_DYNAMIC_DRAW);
            upload(a.data(),bytes,bytes,method);
            glClear(GL_COLOR_BUFFER_BIT); bindVertices(vbo);
            glDrawElements(GL_TRIANGLES,count*6,GL_UNSIGNED_SHORT,nullptr);
            glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            if (!method) reference=pixels;
            else require(pixels==reference,"textured blended drawing is pixel-identical to native upload");
            require(glGetError()==GL_NO_ERROR,"no errors after native/mapped/orphan draw");
        }
        require(reference[0]!=26 || reference[1]!=51 || reference[2]!=76,"test draws actual visible pixels");
        std::array<GLuint,3> ring{};
        std::array<GLsync,3> fences{};
        glGenBuffers(3,ring.data());
        for (const auto id : ring) { glBindBuffer(GL_ARRAY_BUFFER,id); glBufferData(GL_ARRAY_BUFFER,bytes,nullptr,GL_DYNAMIC_DRAW); }
        unsigned nextSlot=0, ringUploads=0, ringFallbacks=0;
        auto drawBatch = [&](const std::vector<Quad>& source, int method) {
            GLuint buffer=vbo; int selected=-1;
            if (method==3) {
                for (unsigned probe=0;probe<3;++probe) {
                    const auto slot=(nextSlot+probe)%3;
                    if (fences[slot]) {
                        const auto ready=glClientWaitSync(fences[slot],0,0);
                        if (ready!=GL_ALREADY_SIGNALED && ready!=GL_CONDITION_SATISFIED) continue;
                        glDeleteSync(fences[slot]); fences[slot]=nullptr;
                    }
                    buffer=ring[slot]; selected=slot; nextSlot=(slot+1)%3; ++ringUploads; break;
                }
                if (selected<0) ++ringFallbacks;
            }
            glBindBuffer(GL_ARRAY_BUFFER,buffer);
            upload(source.data(),bytes,bytes,method==3 ? 0:method);
            // All paths perform native-style per-draw attribute setup.
            bindVertices(buffer);
            glDrawElements(GL_TRIANGLES,count*6,GL_UNSIGNED_SHORT,nullptr);
            if (selected>=0) fences[selected]=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
        };
        glClear(GL_COLOR_BUFFER_BIT); drawBatch(a,3);
        glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        require(pixels==reference,"fenced ring drawing is pixel-identical to native upload");
        std::array<std::vector<double>,4> totalTimes, submitTimes;
        for (int round=0;round<3;++round) for (int order=0;order<4;++order) {
            const int method=(order+round)%4;
            glBindBuffer(GL_ARRAY_BUFFER,vbo); glBufferData(GL_ARRAY_BUFFER,bytes,a.data(),GL_DYNAMIC_DRAW);
            bindVertices(vbo);
            auto frame = [&](int index) {
                glClear(GL_COLOR_BUFFER_BIT);
                for (int batch=0;batch<4;++batch) {
                    const auto& source=(index+batch)%2 ? a:b;
                    drawBatch(source,method);
                }
            };
            for (int i=0;i<4;++i) frame(i);
            glFinish();
            const auto begin=Clock::now();
            for (int i=0;i<24;++i) {
                const auto start=Clock::now(); frame(i);
                submitTimes[method].push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            }
            glFinish();
            totalTimes[method].push_back(std::chrono::duration<double,std::milli>(Clock::now()-begin).count()/24);
            require(glGetError()==GL_NO_ERROR,"no errors during streaming comparison");
        }
        for (int method=0;method<4;++method) {
            std::sort(totalTimes[method].begin(),totalTimes[method].end());
            std::sort(submitTimes[method].begin(),submitTimes[method].end());
            std::cout << "quads=" << count << " method=" << (method==0?"native":method==1?"mapped":method==2?"orphan":"ring")
                      << " total_frame_ms_median=" << totalTimes[method][1]
                      << " submit_p99_ms=" << submitTimes[method][size_t(.99*(submitTimes[method].size()-1))] << '\n';
        }
        std::cout << "ring_uploads=" << ringUploads << " ring_busy_fallbacks=" << ringFallbacks << '\n';
        for (auto fence : fences) if (fence) glDeleteSync(fence);
        glDeleteBuffers(3,ring.data());
    }
    glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo); glDeleteFramebuffers(1,&fbo);
    glDeleteTextures(1,&target); glDeleteTextures(1,&texture);
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
    std::cout << "PASS " << checks << " focused streaming checks; timings are not GD FPS\n";
} catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
