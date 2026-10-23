#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include "renderer/GroupRotation.hpp"

using GLenum = unsigned;
using GLint64 = long long;

// A stateful OpenGL double exercises the real state capture/restore code
// without loading the game or requiring a graphics context.
enum {
    GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_ELEMENT_ARRAY_BUFFER_BINDING,
    GL_DRAW_INDIRECT_BUFFER_BINDING, GL_CURRENT_PROGRAM, GL_ACTIVE_TEXTURE,
    GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA,
    GL_BLEND, GL_TEXTURE_BINDING_2D, GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
    GL_DRAW_INDIRECT_BUFFER, GL_TEXTURE_2D, GL_BLEND_EQUATION_RGB,
    GL_BLEND_EQUATION_ALPHA, GL_UNIFORM_BUFFER_BINDING, GL_UNIFORM_BUFFER_START,
    GL_UNIFORM_BUFFER_SIZE, GL_UNIFORM_BUFFER, GL_MAJOR_VERSION, GL_MINOR_VERSION,
    GL_TEXTURE0 = 100
};
struct IndexedBinding {
    int buffer = 0;
    GLint64 offset = 0, size = 0;
    bool operator==(const IndexedBinding&) const = default;
};
struct FakeGL {
    int vao = 11, vbo = 22, ibo = 33, indirect = 44, program = 55, active = 3;
    int srcRGB = 1, dstRGB = 2, srcAlpha = 3, dstAlpha = 4;
    int equationRGB = 5, equationAlpha = 6, uniform = 777, storage = 888;
    int major = 4, minor = 6;
    bool blend = false;
    std::array<int, 4> textures{101, 202, 303, 404};
    std::array<IndexedBinding, 12> indexedStorage{};
    std::array<IndexedBinding, 4> indexedUniform{};
    bool operator==(const FakeGL&) const = default;
} gl;
void glGetIntegerv(int name, int* value) {
    switch (name) {
        case GL_VERTEX_ARRAY_BINDING: *value = gl.vao; break;
        case GL_ARRAY_BUFFER_BINDING: *value = gl.vbo; break;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: *value = gl.ibo; break;
        case GL_DRAW_INDIRECT_BUFFER_BINDING: *value = gl.indirect; break;
        case GL_CURRENT_PROGRAM: *value = gl.program; break;
        case GL_ACTIVE_TEXTURE: *value = GL_TEXTURE0 + gl.active; break;
        case GL_BLEND_SRC_RGB: *value = gl.srcRGB; break;
        case GL_BLEND_DST_RGB: *value = gl.dstRGB; break;
        case GL_BLEND_SRC_ALPHA: *value = gl.srcAlpha; break;
        case GL_BLEND_DST_ALPHA: *value = gl.dstAlpha; break;
        case GL_BLEND_EQUATION_RGB: *value = gl.equationRGB; break;
        case GL_BLEND_EQUATION_ALPHA: *value = gl.equationAlpha; break;
        case GL_UNIFORM_BUFFER_BINDING: *value = gl.uniform; break;
        case 0x90D3: *value = gl.storage; break;
        case GL_MAJOR_VERSION: *value = gl.major; break;
        case GL_MINOR_VERSION: *value = gl.minor; break;
        case 0x90DD: *value = 12; break;
        case GL_TEXTURE_BINDING_2D: *value = gl.textures.at(gl.active); break;
        default: std::abort();
    }
}
bool glIsEnabled(int) { return gl.blend; }
void glActiveTexture(int unit) { gl.active = unit - GL_TEXTURE0; }
void glBindTexture(int, int texture) { gl.textures.at(gl.active) = texture; }
void glBindVertexArray(int value) { gl.vao = value; }
void glBindBuffer(int target, int value) {
    if (target == GL_ARRAY_BUFFER) gl.vbo = value;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) gl.ibo = value;
    else if (target == GL_DRAW_INDIRECT_BUFFER) gl.indirect = value;
    else if (target == GL_UNIFORM_BUFFER) gl.uniform = value;
    else if (target == 0x90D2) gl.storage = value;
    else std::abort();
}
void glGetIntegeri_v(GLenum target, unsigned index, int* value) {
    if (target == GL_UNIFORM_BUFFER_BINDING) *value = gl.indexedUniform.at(index).buffer;
    else if (target == 0x90D3) *value = gl.indexedStorage.at(index).buffer;
    else std::abort();
}
void glGetInteger64i_v(GLenum target, unsigned index, GLint64* value) {
    if (target == GL_UNIFORM_BUFFER_START) *value = gl.indexedUniform.at(index).offset;
    else if (target == GL_UNIFORM_BUFFER_SIZE) *value = gl.indexedUniform.at(index).size;
    else if (target == 0x90D4) *value = gl.indexedStorage.at(index).offset;
    else if (target == 0x90D5) *value = gl.indexedStorage.at(index).size;
    else std::abort();
}
void glBindBufferRange(GLenum target, unsigned index, int buffer, GLint64 offset, GLint64 size) {
    if (target == GL_UNIFORM_BUFFER) gl.indexedUniform.at(index) = {buffer, offset, size};
    else if (target == 0x90D2) gl.indexedStorage.at(index) = {buffer, offset, size};
    else std::abort();
    glBindBuffer(target, buffer);
}
void glBindBufferBase(GLenum target, unsigned index, int buffer) {
    glBindBufferRange(target, index, buffer, 0, 0);
}
void glUseProgram(int value) { gl.program = value; }
void glBlendEquationSeparate(int rgb, int alpha) { gl.equationRGB = rgb; gl.equationAlpha = alpha; }
void glBlendFuncSeparate(int a, int b, int c, int d) {
    gl.srcRGB = a; gl.dstRGB = b; gl.srcAlpha = c; gl.dstAlpha = d;
}
void glEnable(int) { gl.blend = true; }
void glDisable(int) { gl.blend = false; }
#include "renderer/GLState.hpp"

static int checks = 0;
void require(bool condition, const char* name) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
bool near(glm::vec2 a, glm::vec2 b) { return glm::length(a - b) < 0.0001f; }
struct State {
    glm::mat2 positionalTransform{1.0f}, localTransform{1.0f};
    glm::vec2 offset{0.0f};
};

int main() {
    std::vector<State> states(3);
    const std::vector<std::uint32_t> indices{0, 1};
    applyGroupRotation<State>(states, indices, 90.0f, false, std::nullopt);
    require(near(states[0].localTransform * glm::vec2{1, 0}, {0, -1}), "full clockwise 90 degree delta");
    require(near(states[1].localTransform * glm::vec2{1, 0}, {0, -1}), "rotation reaches second group combination");
    require(near(states[2].localTransform * glm::vec2{1, 0}, {1, 0}), "unrelated group unchanged");
    require(near(states[0].positionalTransform * glm::vec2{1, 0} + states[0].offset, {1, 0}), "centreless rotation preserves positions");

    states.assign(3, State{});
    applyGroupRotation<State>(states, indices, 90.0f, true, glm::vec2{10, 20});
    require(near(states[0].positionalTransform * glm::vec2{12, 20} + states[0].offset, {10, 18}), "orbit about nonzero centre");
    require(near(states[1].localTransform * glm::vec2{1, 0}, {1, 0}), "locked local rotation");
    applyGroupRotation<State>(states, indices, -90.0f, true, glm::vec2{10, 20});
    require(near(states[0].positionalTransform * glm::vec2{12, 20} + states[0].offset, {12, 20}), "inverse orbit returns to start");

    states.assign(3, State{});
    states[0].positionalTransform = glm::mat2{{2, 0}, {0, 3}};
    applyGroupRotation<State>(states, indices, 90.0f, false, glm::vec2{0, 0});
    require(near(states[0].positionalTransform * glm::vec2{1, 1}, {3, -2}), "world rotation composes after existing nonuniform transform");
    const std::vector<std::uint32_t> invalid{999};
    applyGroupRotation<State>(states, invalid, 90.0f, false, std::nullopt);
    require(near(states[0].positionalTransform * glm::vec2{1, 1}, {3, -2}), "unknown group index cannot overwrite memory");

    gl.indexedStorage[0] = {600, 16, 128};
    gl.indexedStorage[4] = {604, 0, 0};
    gl.indexedStorage[11] = {611, 128, 256};
    gl.indexedUniform[3] = {700, 32, 64};
    const auto original = gl;
    const auto outer = GLStateSnapshot::capture();
    require(gl == original, "capture does not alter active texture or other GL state");
    gl.vao = 8; gl.vbo = 9; gl.ibo = 10; gl.indirect = 11; gl.program = 12;
    gl.active = 0; gl.textures[0] = 700; gl.textures[1] = 800;
    gl.srcRGB = 99; gl.dstRGB = 88; gl.srcAlpha = 77; gl.dstAlpha = 66; gl.blend = true;
    gl.equationRGB = 98; gl.equationAlpha = 89;
    glBindBufferBase(0x90D2, 0, 999);
    glBindBufferBase(0x90D2, 11, 911);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, 998);
    const auto middle = gl;
    const auto inner = GLStateSnapshot::capture();
    gl.active = 1; gl.textures[0] = 900; gl.textures[1] = 901; gl.program = 999;
    glBindBufferRange(0x90D2, 4, 904, 48, 96);
    glBindBufferRange(GL_UNIFORM_BUFFER, 3, 903, 64, 128);
    inner.restore();
    require(gl == middle, "nested restore restores inner draw state");
    outer.restore();
    require(gl == original, "outer restore preserves indexed ranges, generic buffers, blend equations and touched texture units");
    const auto untouched = GLStateSnapshot::capture();
    gl.indexedStorage[3] = {333, 12, 24};
    untouched.restore();
    require(gl.indexedStorage[3] == IndexedBinding{333, 12, 24}, "unused SSBO binding 3 is left untouched");
    gl = original;
    const auto spriteDraw = GLStateSnapshot::capture(GLStateScope::SpriteDraw);
    glBindBufferRange(0x90D2, 0, 555, 8, 16);
    glBindBufferBase(0x90D2, 4, 444);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, 334);
    spriteDraw.restore();
    require(gl.indexedStorage[4] == original.indexedStorage[4] && gl.indexedUniform[3] == original.indexedUniform[3],
        "sprite draw scope restores raw sprite SSBO and game-state UBO");
    require(gl.indexedStorage[0] == IndexedBinding{555, 8, 16}, "sprite draw scope leaves compute-only SSBOs untouched");
    require(gl.uniform == original.uniform && gl.storage == original.storage,
        "sprite draw scope restores generic buffer targets after indexed restoration");
    gl = original;
    gl.minor = 2;
    const auto legacy = GLStateSnapshot::capture();
    require(legacy.storageBindingCount == 0, "native fallback on OpenGL 4.2 avoids SSBO state queries");
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, 444);
    legacy.restore();
    require(gl.indexedUniform[3] == original.indexedUniform[3] && gl.uniform == original.uniform,
        "older context still restores UBO range and generic target binding");
    std::cout << "PASS: " << checks << " rendering regression checks\n";
}
