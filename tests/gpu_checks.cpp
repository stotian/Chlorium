#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include "renderer/ShaderPreprocessor.hpp"
#include "renderer/HSVEncoding.hpp"
#include "renderer/NativeQuadState.hpp"
#include "../resources/shaders/headers/shared.h"
#include "renderer/GLState.hpp"
#include <limits>
#include <chrono>

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
constexpr unsigned StorageBarrier = 0x2000, ComputeShader = 0x91B9;
using Dispatch = void(APIENTRY*)(GLuint, GLuint, GLuint);
using MultiDraw = void(APIENTRY*)(GLenum, GLenum, const void*, GLsizei, GLsizei);
using GetProgramInterface = void(APIENTRY*)(GLuint, GLenum, GLenum, GLint*);
using GetProgramResource = void(APIENTRY*)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*);
static int checks = 0;
static std::filesystem::path root;

void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
std::optional<std::string> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(stream), {});
}
GLuint compile(const char* file, GLenum stage, EShLanguage language) {
    auto source = read(root / "resources/shaders" / file);
    require(source.has_value(), std::string("read ") + file);
    std::string error;
    auto code = chlorium::preprocessShaderSource(language, "#define GLSL\n" + *source, file,
        [](std::string_view name) { return read(root / "resources/shaders/headers" / name); }, error);
    require(code.has_value(), std::string("preprocess ") + file + ": " + error);
    GLuint shader = glCreateShader(stage);
    const char* text = code->c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint success = 0, length = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(std::max(1, length), '\0');
    glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
    require(success, std::string("driver compile ") + file + ": " + log);
    return shader;
}
GLuint link(std::initializer_list<GLuint> shaders) {
    const auto program = glCreateProgram();
    for (auto shader : shaders) glAttachShader(program, shader);
    glLinkProgram(program);
    GLint success = 0, length = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(std::max(1, length), '\0');
    glGetProgramInfoLog(program, log.size(), nullptr, log.data());
    require(success, "driver link: " + log);
    for (auto shader : shaders) glDeleteShader(shader);
    return program;
}
GLuint buffer(unsigned binding, const void* data, size_t size, GLenum target = GL_SHADER_STORAGE_BUFFER) {
    GLuint id;
    glGenBuffers(1, &id);
    glBindBuffer(target, id);
    glBufferData(target, size, data, GL_DYNAMIC_DRAW);
    glBindBufferBase(target, binding, id);
    return id;
}
template <class T> void write(GLuint id, const T& value) {
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(T), &value);
}
template <class T> T get(GLuint id) {
    T value{};
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(T), &value);
    return value;
}
struct Vertex { glm::vec2 position, uv; std::uint32_t sprite; };
static_assert(sizeof(Vertex) == 20 && sizeof(RawSprite) == 48 && sizeof(PreCulledSprite) == 16);
static_assert(sizeof(GroupCombinationState) == 48 && sizeof(ObjectInfo) == 80 && sizeof(GameStateBuffer) == 144);

void nativeQuadScopeChecks(GLuint nativeProgram, GLuint otherProgram) {
    const auto original = GLStateSnapshot::capture();
    GLuint vaos[2]{}, buffers[2]{}, textures[3]{};
    glGenVertexArrays(2, vaos); glGenBuffers(2, buffers); glGenTextures(3, textures);
    GLint alignment = 0;
    glGetIntegerv(0x90DF /* GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT */, &alignment);
    for (auto id : buffers) {
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glBufferData(GL_ARRAY_BUFFER, alignment * 4, nullptr, GL_DYNAMIC_DRAW);
    }
    glBindVertexArray(vaos[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[0]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffers[1]);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 4, buffers[0], alignment, alignment * 2);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[1]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers[1]);
    glUseProgram(nativeProgram);
    glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
    glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    for (unsigned unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, textures[unit]);
    }
    auto sameState = [](const GLStateSnapshot& a, const GLStateSnapshot& b) {
        require(a.vao == b.vao && a.elementBuffer == b.elementBuffer, "native quad restores VAO and native EBO");
        require(a.arrayBuffer == b.arrayBuffer && a.indirectBuffer == b.indirectBuffer, "native quad preserves array and indirect targets");
        require(a.program == b.program && a.activeTexture == b.activeTexture && a.textures == b.textures, "native quad restores program and texture units");
        require(a.blendEnabled == b.blendEnabled && a.blendSrcRGB == b.blendSrcRGB && a.blendDstRGB == b.blendDstRGB &&
            a.blendSrcAlpha == b.blendSrcAlpha && a.blendDstAlpha == b.blendDstAlpha &&
            a.blendEquationRGB == b.blendEquationRGB && a.blendEquationAlpha == b.blendEquationAlpha, "native quad preserves exact native blend state");
        require(a.storageBuffer == b.storageBuffer && a.uniformBuffer == b.uniformBuffer, "native quad restores generic storage and preserves uniform target");
        auto equal = [](const auto& x, const auto& y) { return x.buffer == y.buffer && x.offset == y.offset && x.size == y.size; };
        require(equal(a.indexedUniform, b.indexedUniform), "native quad preserves UBO range");
        for (unsigned slot = 0; slot < a.indexedStorage.size(); ++slot)
            require(equal(a.indexedStorage[slot], b.indexedStorage[slot]), "native quad restores SSBO range and preserves other slots");
    };
    for (bool unbound : {false, true}) {
        if (unbound) glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
        const auto before = GLStateSnapshot::capture();
        const auto minimal = GLStateSnapshot::capture(GLStateScope::NativeQuad);
        glUseProgram(otherProgram); glBindVertexArray(vaos[1]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, buffers[1]);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textures[2]);
        // Exactly the state mutated by the raw native-quad submission.
        minimal.restore();
        sameState(before, GLStateSnapshot::capture());
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &alignment);
        require(alignment == textures[2], "native quad preserves active texture unit 2");
        require(glGetError() == GL_NO_ERROR, "native quad scope has no driver errors");
    }
    // Compare only capture/restore CPU cost, without a draw or a game FPS claim.
    std::array<std::vector<double>, 2> times;
    for (unsigned repeat = 0; repeat < 10; ++repeat) for (unsigned order = 0; order < 2; ++order) {
        const unsigned variant = (order + repeat) % 2;
        const auto scope = variant ? GLStateScope::NativeQuad : GLStateScope::SpriteDraw;
        const auto started = std::chrono::steady_clock::now();
        for (unsigned n = 0; n < 512; ++n) GLStateSnapshot::capture(scope).restore();
        const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / 512;
        if (repeat) times[variant].push_back(elapsed);
    }
    for (auto& samples : times) std::sort(samples.begin(), samples.end());
    std::cout << "State-only CPU median us/call: SpriteDraw=" << times[0][4] << ", NativeQuad=" << times[1][4]
              << " (9 alternating batches, 512 calls each; no GD FPS measurement)\n";
    original.restore();
    glDeleteTextures(3, textures); glDeleteBuffers(2, buffers); glDeleteVertexArrays(2, vaos);
}

#include "NativeVertexParityChecks.hpp"

int main(int argc, char** argv) try {
    root = argc > 1 ? argv[1] : ".";
    WNDCLASSA cls{};
    cls.style = CS_OWNDC; cls.lpfnWndProc = DefWindowProcA;
    cls.hInstance = GetModuleHandleA(nullptr); cls.lpszClassName = "ChloriumOffscreenTests";
    RegisterClassA(&cls);
    HWND window = CreateWindowA(cls.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, cls.hInstance, nullptr);
    require(window != nullptr, "create hidden graphics test surface");
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pixel{};
    pixel.nSize = sizeof(pixel); pixel.nVersion = 1;
    pixel.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pixel.iPixelType = PFD_TYPE_RGBA; pixel.cColorBits = 32; pixel.cDepthBits = 24; pixel.cStencilBits = 8;
    require(SetPixelFormat(dc, ChoosePixelFormat(dc, &pixel), &pixel), "set graphics pixel format");
    HGLRC bootstrap = wglCreateContext(dc);
    require(bootstrap && wglMakeCurrent(dc, bootstrap), "bootstrap OpenGL");
    using CreateContext = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    auto create = reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    require(create != nullptr, "OpenGL context extension");
    const int attributes[]{0x2091, 4, 0x2092, 6, 0x9126, 1, 0};
    HGLRC context = create(dc, nullptr, attributes);
    require(context && wglMakeCurrent(dc, context), "OpenGL 4.6 context");
    wglDeleteContext(bootstrap);
    glewExperimental = GL_TRUE;
    require(glewInit() == GLEW_OK, "load graphics entrypoints");
    while (glGetError() != GL_NO_ERROR) {}
    std::cout << "GPU: " << glGetString(GL_RENDERER) << "\nOpenGL: " << glGetString(GL_VERSION) << '\n';
    GLint blocks = 0, bindings = 0;
    glGetIntegerv(0x90DB, &blocks); glGetIntegerv(0x90DD, &bindings);
    std::cout << "Compute SSBO blocks: " << blocks << ", binding points: " << bindings << '\n';
    auto dispatch = reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));
    auto multi = reinterpret_cast<MultiDraw>(wglGetProcAddress("glMultiDrawElementsIndirect"));
    require(dispatch && multi && glMemoryBarrier, "compute, indirect draw and barrier functions");

    // Exercise the production GL snapshot against actual driver range state.
    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    GLuint rangeBuffer = 0, replacementBuffer = 0;
    glGenBuffers(1, &rangeBuffer); glGenBuffers(1, &replacementBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, rangeBuffer);
    glBufferData(GL_UNIFORM_BUFFER, alignment * 3, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER, GAME_STATE_BUFFER_BINDING, rangeBuffer, alignment, alignment);
    glBindBuffer(GL_ARRAY_BUFFER, replacementBuffer);
    glBufferData(GL_ARRAY_BUFFER, alignment * 3, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, COLOR_CHANNEL_BUFFER_BINDING, rangeBuffer, 0, alignment);
    glBindBuffer(GL_UNIFORM_BUFFER, replacementBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, replacementBuffer);
    glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
    const auto savedGL = GLStateSnapshot::capture();
    glBindBufferBase(GL_UNIFORM_BUFFER, GAME_STATE_BUFFER_BINDING, replacementBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLOR_CHANNEL_BUFFER_BINDING, replacementBuffer);
    glBlendEquation(GL_FUNC_ADD);
    savedGL.restore();
    GLint bound = 0, equation = 0;
    GLint64 start = 0, rangeSize = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, GAME_STATE_BUFFER_BINDING, &bound);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_START, GAME_STATE_BUFFER_BINDING, &start);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, GAME_STATE_BUFFER_BINDING, &rangeSize);
    require(bound == rangeBuffer && start == alignment && rangeSize == alignment, "driver snapshot restores UBO offset and range length");
    glGetIntegeri_v(0x90D3, COLOR_CHANNEL_BUFFER_BINDING, &bound);
    glGetInteger64i_v(0x90D4, COLOR_CHANNEL_BUFFER_BINDING, &start);
    glGetInteger64i_v(0x90D5, COLOR_CHANNEL_BUFFER_BINDING, &rangeSize);
    require(bound == rangeBuffer && start == 0 && rangeSize == alignment, "driver snapshot restores SSBO offset and range length");
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &bound);
    require(bound == replacementBuffer, "driver snapshot restores generic UBO target after indexed binding");
    glGetIntegerv(0x90D3, &bound);
    require(bound == replacementBuffer, "driver snapshot restores generic SSBO target after indexed binding");
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
    require(equation == GL_FUNC_SUBTRACT, "driver snapshot restores RGB blend equation");
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation);
    require(equation == GL_FUNC_REVERSE_SUBTRACT, "driver snapshot restores alpha blend equation");
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, RAW_SPRITE_BUFFER_BINDING, rangeBuffer, 0, alignment);
    const auto spriteDrawGL = GLStateSnapshot::capture(GLStateScope::SpriteDraw);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, RAW_SPRITE_BUFFER_BINDING, replacementBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLOR_CHANNEL_BUFFER_BINDING, replacementBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, GAME_STATE_BUFFER_BINDING, replacementBuffer);
    spriteDrawGL.restore();
    glGetIntegeri_v(0x90D3, RAW_SPRITE_BUFFER_BINDING, &bound);
    glGetInteger64i_v(0x90D5, RAW_SPRITE_BUFFER_BINDING, &rangeSize);
    require(bound == rangeBuffer && rangeSize == alignment, "sprite draw scope restores raw sprite SSBO range on driver");
    glGetIntegeri_v(0x90D3, COLOR_CHANNEL_BUFFER_BINDING, &bound);
    require(bound == replacementBuffer, "sprite draw scope leaves compute-only driver SSBO binding untouched");
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, GAME_STATE_BUFFER_BINDING, &bound);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_START, GAME_STATE_BUFFER_BINDING, &start);
    require(bound == rangeBuffer && start == alignment, "sprite draw scope restores game-state UBO range on driver");
    glBlendEquation(GL_FUNC_ADD);
    glBindBufferBase(GL_UNIFORM_BUFFER, GAME_STATE_BUFFER_BINDING, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, COLOR_CHANNEL_BUFFER_BINDING, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, RAW_SPRITE_BUFFER_BINDING, 0);
    glDeleteBuffers(1, &rangeBuffer); glDeleteBuffers(1, &replacementBuffer);

    auto program = link({compile("objectCulling.comp", ComputeShader, EShLangCompute)});
    auto drawProgram = link({compile("objectDrawing.vert", GL_VERTEX_SHADER, EShLangVertex),
                             compile("objectDrawing.frag", GL_FRAGMENT_SHADER, EShLangFragment)});
    auto getInterface = reinterpret_cast<GetProgramInterface>(wglGetProcAddress("glGetProgramInterfaceiv"));
    auto getResource = reinterpret_cast<GetProgramResource>(wglGetProcAddress("glGetProgramResourceiv"));
    require(getInterface && getResource, "draw program resource reflection functions available");
    GLint activeBlocks = 0, activeBinding = -1;
    constexpr GLenum storageBlockInterface = 0x92E6, activeResources = 0x92F5, bufferBindingProperty = 0x9302;
    getInterface(drawProgram, storageBlockInterface, activeResources, &activeBlocks);
    require(activeBlocks == 1, "draw program has exactly one active SSBO");
    getResource(drawProgram, storageBlockInterface, 0, 1, &bufferBindingProperty, 1, nullptr, &activeBinding);
    require(activeBinding == RAW_SPRITE_BUFFER_BINDING, "draw program's only SSBO is raw sprites at binding 4");
    glGetProgramiv(drawProgram, GL_ACTIVE_UNIFORM_BLOCKS, &activeBlocks);
    require(activeBlocks == 1, "draw program has exactly one active UBO");
    glGetActiveUniformBlockiv(drawProgram, 0, GL_UNIFORM_BLOCK_BINDING, &activeBinding);
    require(activeBinding == GAME_STATE_BUFFER_BINDING, "draw program's only UBO is game state at binding 3");
    auto basic = link({compile("basic.vert", GL_VERTEX_SHADER, EShLangVertex),
                       compile("basic.frag", GL_FRAGMENT_SHADER, EShLangFragment)});
    nativeQuadScopeChecks(basic, drawProgram);
    glDeleteProgram(basic);

    GameStateBuffer state{};
    state.u_mvp = glm::mat4(1.0f);
    state.u_audioScale = 1.0f;
    state.u_cameraPosition = {-1, -1}; state.u_cameraRight = {1, 0}; state.u_cameraUp = {0, 1};
    state.u_cameraSize = {2, 2}; state.u_cameraViewSize = {2, 2}; state.u_winSize = {64, 64};
    auto gameBuffer = buffer(GAME_STATE_BUFFER_BINDING, &state, sizeof(state), GL_UNIFORM_BUFFER);
    ObjectInfo object{};
    object.opacity = 1.0f; object.baseColorChannelId = 1;
    auto objectBuffer = buffer(OBJECT_INFO_BUFFER_BINDING, &object, sizeof(object));
    ColorChannelBuffer colors{};
    colors.u_channelColors[1] = {255, 64, 0, 255};
    buffer(COLOR_CHANNEL_BUFFER_BINDING, &colors, sizeof(colors));
    GroupCombinationState group{};
    group.positionalTransform = glm::mat2(1); group.localTransform = glm::mat2(1); group.opacity = 1;
    auto groupBuffer = buffer(GROUP_STATE_BUFFER_BINDING, &group, sizeof(group));
    InputSprite input{0, SPRITE_TYPE_BASE, 0, 6};
    auto inputBuffer = buffer(INPUT_SPRITE_BUFFER_BINDING, &input, sizeof(input));
    PreCulledSprite pre{};
    auto preBuffer = buffer(PRE_CULLED_SPRITE_BUFFER_BINDING, &pre, sizeof(pre));
    RawSprite raw{};
    auto rawBuffer = buffer(RAW_SPRITE_BUFFER_BINDING, &raw, sizeof(raw));
    const unsigned indices[]{0, 1, 2, 0, 2, 3};
    buffer(INPUT_INDEX_BUFFER_BINDING, indices, sizeof(indices));
    auto culledBuffer = buffer(CULLED_INDEX_BUFFER_BINDING, indices, sizeof(indices));
    const unsigned remap = 0;
    buffer(SPRITE_REMAP_BUFFER_BINDING, &remap, sizeof(remap));
    WorkGroup work{0, 1, 0};
    buffer(WORK_GROUP_BUFFER_BINDING, &work, sizeof(work));
    DrawCommand command{}; command.instanceCount = 1;
    auto commandBuffer = buffer(DRAW_COMMAND_BUFFER_BINDING, &command, sizeof(command));
    auto run = [&] {
        glUseProgram(program);
        dispatch(1, 1, 1);
        glMemoryBarrier(StorageBarrier | GL_COMMAND_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    };
    run();
    command = get<DrawCommand>(commandBuffer);
    raw = get<RawSprite>(rawBuffer);
    require(command.count == 6 && command.firstIndex == 0 && command.instanceCount == 1, "compute writes correct indirect draw command");
    require(std::abs(raw.color.r - 1.0f) < 0.001f && std::abs(raw.color.g - 64.0f / 255) < 0.001f, "CPU/GPU color layout and value agreement");
    require(glm::length(raw.position) < 0.001f, "CPU/GPU group and sprite layout agreement");

    GLuint vao, vbo, framebuffer, texture, white;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    const Vertex vertices[]{{{-.5f, -.5f}, {0, 0}, 0}, {{.5f, -.5f}, {1, 0}, 0},
                            {{.5f, .5f}, {1, 1}, 0}, {{-.5f, .5f}, {0, 1}, 0}};
    glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(8));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex), reinterpret_cast<void*>(16));
    for (int i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, culledBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer);
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "offscreen framebuffer complete");
    glGenTextures(1, &white); glBindTexture(GL_TEXTURE_2D, white);
    const unsigned whitePixel = 0xffffffff;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glViewport(0, 0, 64, 64); glClearColor(0, 0, 0, 1);
    auto draw = [&] {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(drawProgram); glUniform1i(glGetUniformLocation(drawProgram, "u_spriteSheetTexture"), 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, white);
        multi(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, 1, sizeof(DrawCommand));
        std::array<unsigned char, 4> pixel{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    };
    const auto visible = draw();
    require(visible[0] > 250 && visible[1] >= 63 && visible[1] <= 65, "real GPU renders expected colored pixel via compute + indirect draw");
    group.opacity = 0; write(groupBuffer, group); run();
    require(get<DrawCommand>(commandBuffer).count == 0, "hidden group produces no draw indices");
    require(draw()[0] == 0, "hidden group leaves framebuffer black");
    group.opacity = 1; group.offset = {0.25f, -0.5f}; write(groupBuffer, group); run();
    require(glm::length(get<RawSprite>(rawBuffer).position - group.offset) < 0.001f, "group motion reaches GPU sprite position");
    group.offset = {0, 0}; write(groupBuffer, group);
    pre.startPosition = {-200, 0}; pre.radius = 250; write(preBuffer, pre); run();
    require(get<DrawCommand>(commandBuffer).count == 6, "large sprite intersecting camera is not culled by centre alone");
    pre.radius = 0; write(preBuffer, pre); run();
    require(get<DrawCommand>(commandBuffer).count == 0, "small sprite outside camera is culled");
    // A narrow skewed view cannot be culled by dot products against its axes.
    state.u_cameraPosition = {0, 0}; state.u_cameraSize = {1, 100};
    state.u_cameraRight = {1, 0}; state.u_cameraUp = glm::normalize(glm::vec2{1, 1});
    pre.startPosition = state.u_cameraUp * 50.0f + glm::vec2{0.5f, 0};
    write(gameBuffer, state); write(preBuffer, pre); run();
    require(get<DrawCommand>(commandBuffer).count == 6, "skewed camera keeps sprite inside its view parallelogram");
    state.u_cameraRight.x = std::numeric_limits<float>::quiet_NaN();
    write(gameBuffer, state); run();
    require(get<DrawCommand>(commandBuffer).count == 6, "nonfinite camera axis conservatively disables culling");
    state.u_cameraRight = {1, 0}; state.u_cameraUp = {1, 0};
    write(gameBuffer, state); run();
    require(get<DrawCommand>(commandBuffer).count == 6, "singular camera basis conservatively disables culling");
    state.u_cameraPosition = {-1, -1}; state.u_cameraSize = {2, 2}; state.u_cameraUp = {0, 1};
    write(gameBuffer, state);
    pre.startPosition = {0, 0}; pre.groupCombinationIndex = 500; write(preBuffer, pre); run();
    require(get<DrawCommand>(commandBuffer).count == 0, "invalid group index is rejected by compute shader");
    pre.groupCombinationIndex = 0; write(preBuffer, pre);
    input.objectIndex = 500; write(inputBuffer, input); run();
    require(get<DrawCommand>(commandBuffer).count == 0, "invalid object index is rejected by compute shader");
    input.objectIndex = 0; write(inputBuffer, input);
    object.baseColorChannelId = 9000; write(objectBuffer, object); run();
    require(get<DrawCommand>(commandBuffer).count == 0, "invalid color channel is rejected by compute shader");
    object.baseColorChannelId = 1;
    object.flags = OBJECT_FLAG_HAS_BASE_HSV;
    object.baseHSV = {300.0f, 1.0f, 1.0f, 0};
    colors.u_channelColors[1] = {255, 0, 0, 255};
    // Rebind and update the color buffer through its binding point.
    GLint colorsBuffer = 0;
    glGetIntegeri_v(0x90D3, COLOR_CHANNEL_BUFFER_BINDING, &colorsBuffer);
    write(colorsBuffer, colors); write(objectBuffer, object); run();
    auto hueColor = get<RawSprite>(rawBuffer).color;
    require(hueColor.r > .99f && hueColor.b > .98f && hueColor.g < .01f, "300 degree floating HSV hue wraps correctly");
    object.baseHSV = {0.0f, 1.0f, 1.0f, 0};
    colors.u_channelColors[1] = {173, 91, 37, 255};
    write(colorsBuffer, colors); write(objectBuffer, object); run();
    hueColor = get<RawSprite>(rawBuffer).color;
    require(glm::length(glm::vec3(hueColor) - glm::vec3(173, 91, 37) / 255.0f) < 1e-6f,
        "identity floating HSV preserves channel color without 8-bit S/V quantization");
    object.baseHSV = {0.0f, 0.0f, 0.1234567f, HSV_SAT_ADD | HSV_VAL_ADD};
    write(objectBuffer, object); run();
    hueColor = get<RawSprite>(rawBuffer).color;
    require(std::abs(std::max({hueColor.r, hueColor.g, hueColor.b}) - (173.0f / 255 + 0.1234567f)) < 1e-6f,
        "absolute HSV brightness preserves fractional input and adds native value directly");

    // Exercise every lane, partial visibility, stable alpha painter order and
    // thousands of workgroups. The retained serial shader is the control.
    const auto serial = link({compile("../../tests/reference/objectCulling-serial.comp", ComputeShader, EShLangCompute)});
    constexpr unsigned spriteCount = 131072, batchSize = 32, groupCount = spriteCount / batchSize;
    std::array<ObjectInfo, 2> objects{};
    objects[0].opacity = 1; objects[0].baseColorChannelId = 1;
    objects[1] = objects[0]; objects[1].opacity = 0;
    buffer(OBJECT_INFO_BUFFER_BINDING, objects.data(), sizeof(objects));
    std::vector<InputSprite> manyInputs(spriteCount);
    std::vector<PreCulledSprite> manyPre(spriteCount);
    std::vector<unsigned> manyIndices(spriteCount * 6), manyRemap(spriteCount);
    std::vector<WorkGroup> manyWork(groupCount);
    std::vector<DrawCommand> manyCommands(groupCount);
    for (unsigned i = 0; i < spriteCount; ++i) {
        manyInputs[i] = {i % 3 == 0 ? 1u : 0u, SPRITE_TYPE_BASE, i * 6, 6};
        manyRemap[i] = i;
    }
    std::iota(manyIndices.begin(), manyIndices.end(), 0);
    for (unsigned g = 0; g < groupCount; ++g) {
        manyWork[g] = {g * batchSize, (g + 1) * batchSize, g * batchSize * 6};
        manyCommands[g].instanceCount = 1;
    }
    buffer(INPUT_SPRITE_BUFFER_BINDING, manyInputs.data(), manyInputs.size() * sizeof(InputSprite));
    buffer(PRE_CULLED_SPRITE_BUFFER_BINDING, manyPre.data(), manyPre.size() * sizeof(PreCulledSprite));
    buffer(RAW_SPRITE_BUFFER_BINDING, nullptr, spriteCount * sizeof(RawSprite));
    buffer(INPUT_INDEX_BUFFER_BINDING, manyIndices.data(), manyIndices.size() * sizeof(unsigned));
    auto manyCulled = buffer(CULLED_INDEX_BUFFER_BINDING, nullptr, manyIndices.size() * sizeof(unsigned));
    buffer(SPRITE_REMAP_BUFFER_BINDING, manyRemap.data(), manyRemap.size() * sizeof(unsigned));
    buffer(WORK_GROUP_BUFFER_BINDING, manyWork.data(), manyWork.size() * sizeof(WorkGroup));
    auto manyCommandBuffer = buffer(DRAW_COMMAND_BUFFER_BINDING, manyCommands.data(), manyCommands.size() * sizeof(DrawCommand));
    glUseProgram(program); dispatch(groupCount, 1, 1);
    glMemoryBarrier(StorageBarrier | GL_BUFFER_UPDATE_BARRIER_BIT);
    std::vector<unsigned> output(manyIndices.size());
    glBindBuffer(GL_ARRAY_BUFFER, manyCulled);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, output.size() * sizeof(unsigned), output.data());
    glBindBuffer(GL_ARRAY_BUFFER, manyCommandBuffer);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, manyCommands.size() * sizeof(DrawCommand), manyCommands.data());
    bool ordered = true, commandCounts = true;
    for (unsigned g = 0; g < groupCount; ++g) {
        unsigned count = 0;
        for (unsigned i = g * batchSize; i < (g + 1) * batchSize; ++i) {
            if (i % 3 == 0) continue;
            for (unsigned k = 0; k < 6; ++k)
                ordered &= output[g * batchSize * 6 + count++] == i * 6 + k;
        }
        commandCounts &= manyCommands[g].count == count && manyCommands[g].firstIndex == g * batchSize * 6;
    }
    require(ordered, "parallel compaction preserves every visible sprite index in painter order across 4096 groups");
    require(commandCounts, "parallel compaction emits correct indirect ranges across 131072 sprites");
    glUseProgram(serial); dispatch(groupCount, 1, 1);
    glMemoryBarrier(StorageBarrier | GL_BUFFER_UPDATE_BARRIER_BIT);
    std::vector<DrawCommand> serialCommands(groupCount);
    glBindBuffer(GL_ARRAY_BUFFER, manyCommandBuffer);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, serialCommands.size() * sizeof(DrawCommand), serialCommands.data());
    require(std::memcmp(serialCommands.data(), manyCommands.data(), serialCommands.size() * sizeof(DrawCommand)) == 0,
        "parallel commands exactly match retained serial reference");

    auto measure = [&](GLuint p) {
        glUseProgram(p);
        for (int i = 0; i < 3; ++i) { dispatch(groupCount, 1, 1); glMemoryBarrier(StorageBarrier); }
        GLuint queries[2]; glGenQueries(2, queries);
        glQueryCounter(queries[0], GL_TIMESTAMP);
        for (int i = 0; i < 8; ++i) { dispatch(groupCount, 1, 1); glMemoryBarrier(StorageBarrier); }
        glQueryCounter(queries[1], GL_TIMESTAMP);
        GLint64 start = 0, end = 0;
        glGetQueryObjecti64v(queries[0], GL_QUERY_RESULT, &start);
        glGetQueryObjecti64v(queries[1], GL_QUERY_RESULT, &end);
        glDeleteQueries(2, queries);
        return (end - start) / 8e6;
    };
    std::vector<double> serialTimes, parallelTimes;
    for (int i = 0; i < 5; ++i) {
        if (i % 2) { parallelTimes.push_back(measure(program)); serialTimes.push_back(measure(serial)); }
        else { serialTimes.push_back(measure(serial)); parallelTimes.push_back(measure(program)); }
    }
    std::sort(serialTimes.begin(), serialTimes.end()); std::sort(parallelTimes.begin(), parallelTimes.end());
    std::cout << "COMPUTE BENCHMARK (131072 sprites, median GPU ms): serial=" << serialTimes[2]
              << ", parallel=" << parallelTimes[2] << ", ratio=" << serialTimes[2] / parallelTimes[2] << '\n';
    glDeleteProgram(serial);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,white);
    const unsigned char nativeAtlasPixels[]{255,0,0,255, 0,0,255,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,nativeAtlasPixels);
    auto nativeQuadDraw = link({compile("nativeQuad.vert",GL_VERTEX_SHADER,EShLangVertex),
                               compile("nativeQuad.frag",GL_FRAGMENT_SHADER,EShLangFragment)});
    std::array<chlorium::snapshot::NativeQuadState,2> nativeQuads{};
    nativeQuads[0].corners = {{{-.5f,.5f,0,0xffffffffu,0,1},
                               {-.5f,-.5f,0,0xffffffffu,0,0},
                               {.5f,.5f,0,0xffffffffu,.5f,1},
                               {.5f,-.5f,0,0xffffffffu,.5f,0}}};
    nativeQuads[1] = nativeQuads[0];
    for (auto& vertex : nativeQuads[1].corners) { vertex.x += 100; vertex.y += 100; }
    auto nativeQuadBuffer = buffer(RAW_SPRITE_BUFFER_BINDING,nativeQuads.data(),sizeof(nativeQuads));
    GLuint emptyVao = 0; glGenVertexArrays(1,&emptyVao); glBindVertexArray(emptyVao);
    // Deliberately no vertex/index buffers or attributes on this VAO.
    auto drawNativeQuads = [&] {
        glClear(GL_COLOR_BUFFER_BIT); glUseProgram(nativeQuadDraw);
        const glm::mat4 identity(1);
        glUniformMatrix4fv(glGetUniformLocation(nativeQuadDraw,"u_mvp"),1,GL_FALSE,&identity[0][0]);
        glUniform1i(glGetUniformLocation(nativeQuadDraw,"u_spriteSheetTexture"),0);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,2);
        std::array<unsigned char,4> pixel{};
        glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        return pixel;
    };
    auto nativePixel = drawNativeQuads();
    require(nativePixel[0] > 250 && nativePixel[2] < 2,"instanced native quad draws from exact 96-byte records without geometry buffers");
    for (auto& vertex : nativeQuads[0].corners) vertex.u += .5f;
    write(nativeQuadBuffer,nativeQuads);
    nativePixel = drawNativeQuads();
    require(nativePixel[2] > 250 && nativePixel[0] < 2,"instanced native quad preserves current native UVs and instance addressing");
    for (auto& vertex : nativeQuads[0].corners) vertex.x = -vertex.x;
    write(nativeQuadBuffer,nativeQuads);
    nativePixel = drawNativeQuads();
    require(nativePixel[2] > 250,"native mirrored quad keeps both strip triangles visible");
    for (auto& vertex : nativeQuads[0].corners) vertex.z = 2;
    write(nativeQuadBuffer,nativeQuads); nativePixel = drawNativeQuads();
    require(nativePixel[0] == 0 && nativePixel[2] == 0,"exact native vertex depth is retained instead of forcing sprites to z=0");
    for (auto& vertex : nativeQuads[0].corners) { vertex.z = 0; vertex.color = 0x800000ffu; }
    write(nativeQuadBuffer,nativeQuads);
    const unsigned whiteAgain = 0xffffffffu;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&whiteAgain);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    nativePixel = drawNativeQuads();
    require(nativePixel[0] >= 127 && nativePixel[0] <= 129,"instanced path retains native SRC_ALPHA blending without double premultiplication");
    nativeVertexParityChecks();
    glDisable(GL_BLEND); glDeleteProgram(nativeQuadDraw); glDeleteVertexArrays(1,&emptyVao);
    require(glGetError() == GL_NO_ERROR, "no OpenGL errors across shader, compute, upload and draw tests");
    glDeleteProgram(program); glDeleteProgram(drawProgram);
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context);
    ReleaseDC(window, dc); DestroyWindow(window);
    std::cout << "PASS: " << checks << " real GPU checks\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
}
