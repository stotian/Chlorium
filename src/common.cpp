#include "common.hpp"
#include "Geode/cocos/CCDirector.h"
#include "renderer/Buffer.hpp"
#include "renderer/Shader.hpp"

using namespace geode::prelude;

std::chrono::steady_clock::time_point modStartTime;

$on_mod(Loaded) {
    modStartTime = std::chrono::steady_clock::now();
}

using ProcGlObjectLabel = void(*)(GLenum, GLuint, GLsizei, const char*);

ProcGlObjectLabel glObjectLabelProc = nullptr;

void glObjectLabel(GLenum id, GLuint name, GLsizei length, const char* label) {
    #ifdef GEODE_WINDOWS
    if (!glObjectLabelProc)
        glObjectLabelProc = (ProcGlObjectLabel)wglGetProcAddress("glObjectLabel");
    if (glObjectLabelProc)
        glObjectLabelProc(id, name, length, label);
    #endif
}

static float fullscreenQuad[] = {
    -1.0, -1.0,
    -1.0,  1.0,
     1.0,  1.0,
    -1.0, -1.0,
     1.0,  1.0,
     1.0, -1.0
};

u32 fullscreenQuadVAO = 0;
Buffer* fullscreenQuadVBO = nullptr;

void drawFullscreenQuad() {
    if (!fullscreenQuadVAO) {
        if (!fullscreenQuadVBO)
            fullscreenQuadVBO = Buffer::createStaticDraw("Fullscreen quad buffer", fullscreenQuad, sizeof(fullscreenQuad));

        glGenVertexArrays(1, &fullscreenQuadVAO);
        glBindVertexArray(fullscreenQuadVAO);
        fullscreenQuadVBO->bindAs(GL_ARRAY_BUFFER);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
        glEnableVertexAttribArray(0);
    }

    glBindVertexArray(fullscreenQuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#include <geode/modify/CCDirector.hpp>
class $modify(CommonCCDirector, CCDirector) {
    void purgeDirector() {
        Shader::clearCache();
        if (fullscreenQuadVAO)
            glDeleteVertexArrays(1, &fullscreenQuadVAO);
        if (fullscreenQuadVBO)
            Buffer::destroy(fullscreenQuadVBO);
        fullscreenQuadVAO = 0;
        fullscreenQuadVBO = nullptr;

        CCDirector::purgeDirector();
    }
};

std::optional<std::string> readResourceFile(const fs::path& path) {
    std::ifstream t(Mod::get()->getResourcesDir() / path, std::ios::binary | std::ios::ate);
    if (!t.is_open())
        return std::nullopt;

    const auto end = t.tellg();
    if (end < 0 || end > 64 * 1024 * 1024) return std::nullopt;
    const auto size = static_cast<size_t>(end);
    std::string buffer(size, '\0');
    t.seekg(0);
    if (size && !t.read(buffer.data(), static_cast<std::streamsize>(size))) return std::nullopt;

    return buffer;
}
