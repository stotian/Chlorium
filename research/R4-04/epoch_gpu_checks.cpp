#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#include "../../src/renderer/ContextEpoch.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
#ifndef GL_BUFFER
#define GL_BUFFER 0x82e0
#endif
// Execute the actual Buffer implementation, substituting only the unused
// Geode settings provider. This test never loads Geode or Geometry Dash.
void glObjectLabel(GLenum kind, GLuint name, GLsizei length, const char* value) {
    chlorium::research::gl::labelObject(kind,name,length,value);
}
#define CHLORIUM_EXPERIMENTAL_NATIVE_QUADS 1
#include "../../src/renderer/Buffer.cpp"
#ifdef CHLORIUM_BUFFER_CONTEXT_HEADER
#error The GPU fixture must exercise the real context identity, never CPU context stubs
#endif
bool geode::prelude::mappedUploads = false;

using namespace chlorium::research::gl;
unsigned checks = 0;
void require(bool value, const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
struct RecycledHandle {
    // Only the private fixture lies about HGLRC. Real GL namespaces still change.
    static HGLRC current() noexcept { return wglGetCurrentContext() ? reinterpret_cast<HGLRC>(1) : nullptr; }
};
struct Surface {
    HWND window = nullptr; HDC dc = nullptr; HGLRC context = nullptr;
    Surface(HINSTANCE instance) {
        window = CreateWindowA("ChloriumEpochFixture", "", WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, instance, nullptr);
        require(window != nullptr, "hidden surface"); dc = GetDC(window);
        PIXELFORMATDESCRIPTOR pf{};pf.nSize=sizeof(pf);pf.nVersion=1;
        pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pf.iPixelType=PFD_TYPE_RGBA;pf.cColorBits=32;
        require(SetPixelFormat(dc, ChoosePixelFormat(dc, &pf), &pf), "pixel format");
        context = wglCreateContext(dc);require(context != nullptr,"context");
    }
    void select() { require(wglMakeCurrent(dc,context),"current context"); }
    ~Surface() {
        if (wglGetCurrentContext() == context) wglMakeCurrent(nullptr,nullptr);
        if (context) wglDeleteContext(context); if (dc) ReleaseDC(window,dc); if (window) DestroyWindow(window);
    }
};
int main() try {
    const auto instance=GetModuleHandleA(nullptr);
    WNDCLASSA cls{};cls.style=CS_OWNDC;cls.lpfnWndProc=DefWindowProcA;cls.hInstance=instance;cls.lpszClassName="ChloriumEpochFixture";
    require(RegisterClassA(&cls),"window class");
    Surface a(instance), b(instance);
    require(wglShareLists(a.context,b.context),"shared buffer/program namespace");
    a.select();glewExperimental=GL_TRUE;require(glewInit()==GLEW_OK && currentProc<LabelProc>("glObjectLabel") && currentProc<ReadLabelProc>("glGetObjectLabel"),"identity APIs");
    while(glGetError()!=GL_NO_ERROR) {}
    std::cout << "GL renderer: " << glGetString(GL_RENDERER) << "; version: " << glGetString(GL_VERSION) << '\n';
    EpochRegistry<> registry;
    auto first=registry.acquire();require(first && first->isCurrent(),"identity created");
    const auto firstEpoch=first->epoch(); const auto firstSentinel=first->sentinel();
    GLint bound=-1;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&bound);require(bound==0,"capture preserved VAO binding");
    require(registry.acquire()==first,"same living context reuses epoch");
    auto fakeA=ContextEpoch<RecycledHandle>::capture();require(fakeA && fakeA->isCurrent(),"recycled-handle fixture A");
    GLuint shared=0;glGenBuffers(1,&shared);glBindBuffer(GL_ARRAY_BUFFER,shared);
    std::array<unsigned char,4> bytes{1,2,3,4};glBufferData(GL_ARRAY_BUFFER,bytes.size(),bytes.data(),GL_STATIC_DRAW);
    auto owned = std::unique_ptr<Buffer>(Buffer::createDynamicDraw("fixture-owned-buffer",4));
    owned->write(bytes.data(),bytes.size()); const auto ownedName=owned->getId();
    std::array<unsigned char,4> roundtrip{};owned->read(roundtrip.data(),roundtrip.size());
    require(roundtrip==bytes,"actual Buffer roundtrip");
    b.select();require(glIsBuffer(shared),"buffer actually shared across contexts");
    require(!first->isCurrent(),"different real context rejected");
    bool rejected=false;
    try { owned->read(roundtrip.data(),roundtrip.size()); } catch(const std::runtime_error&) {rejected=true;}
    require(rejected,"actual Buffer refuses foreign-context read");
    owned.reset();require(glIsBuffer(ownedName),"actual Buffer destruction preserves foreign namespace");
    require(!fakeA->isCurrent(),"same reported raw handle does not prove namespace identity");
    auto second=registry.acquire();require(second && second->epoch()!=firstEpoch && second->isCurrent(),"context B distinct epoch");
    require(!first->destroyOwnedBuffer(shared) && glIsBuffer(shared),"foreign owner cannot delete shared buffer");
    a.select();require(first->isCurrent(),"A identity resumes");glDeleteBuffers(1,&ownedName);
    // A real object remains at the old numeric name, but no longer has our label.
    require(labelObject(GL_VERTEX_ARRAY,firstSentinel,-1,"foreign-replacement"),"sentinel relabel API");
    require(glIsVertexArray(firstSentinel),"numeric sentinel name remains valid");
    require(!first->isCurrent(),"relabelled/reused numeric identity rejected");
    require(!first->destroyOwnedBuffer(shared) && glIsBuffer(shared),"stale generation cannot delete resources");
    auto replacement=registry.acquire();require(replacement && replacement->epoch()!=firstEpoch,"new namespace stamp issued");
    first.reset();require(glIsVertexArray(firstSentinel),"stale stamp destruction preserves foreign object");
    require(replacement->destroyOwnedBuffer(shared) && !glIsBuffer(shared),"validated owner can delete own buffer");
    const auto sentinel=replacement->sentinel();replacement.reset();require(!glIsVertexArray(sentinel),"owned sentinel cleaned in matching context");
    fakeA.reset();glDeleteVertexArrays(1,&firstSentinel);
    b.select();second.reset();require(glGetError()==GL_NO_ERROR,"no GL errors from registry");
    wglMakeCurrent(nullptr,nullptr);require(!registry.acquire(),"no-context fallback");
    std::cout << "{\"event\":\"context_epoch_gpu_fixture\",\"checks\":" << checks
        << ",\"status\":\"passed\",\"raw_handle_reuse\":\"simulated_with_real_context_switch\"}\n";
    return 0;
} catch(const std::exception& error) {std::cerr << "FAIL: " << error.what() << '\n';return 1;}
