#pragma once

#include <array>

enum class GLStateScope { Full, SpriteDraw };

// Used with OpenGL already included. Save each texture unit before switching
// units; querying the binding on one unit and restoring it on another corrupts
// the game's textures. Values are per snapshot so nested draws are safe.
struct GLStateSnapshot {
    int vao, arrayBuffer, elementBuffer, indirectBuffer, program, activeTexture;
    int blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
    int blendEquationRGB, blendEquationAlpha;
    int uniformBuffer, storageBuffer;
    bool blendEnabled;
    std::array<int, 2> textures;

    struct IndexedBufferBinding {
        int buffer = 0;
        GLint64 offset = 0;
        GLint64 size = 0;
    };
    // Only these indexed bindings are touched by the renderer. SSBO binding 3
    // is unused: GameStateBuffer occupies binding 3 in the separate UBO space.
    static constexpr std::array<unsigned, 11> storageBindings = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr GLenum storageTarget = 0x90D2; // GL_SHADER_STORAGE_BUFFER
    static constexpr GLenum storageBinding = 0x90D3;
    static constexpr GLenum storageStart = 0x90D4;
    static constexpr GLenum storageSize = 0x90D5;
    std::array<IndexedBufferBinding, storageBindings.size()> indexedStorage;
    IndexedBufferBinding indexedUniform;
    int storageBindingCount = 0;
    bool indexedUniformSupported = false;
    GLStateScope scope = GLStateScope::Full;

    bool capturesStorageBinding(unsigned index) const {
        return scope == GLStateScope::Full || index == 4; // RAW_SPRITE_BUFFER_BINDING
    }

    static IndexedBufferBinding captureIndexed(GLenum binding, GLenum start, GLenum size, unsigned index) {
        IndexedBufferBinding result;
        glGetIntegeri_v(binding, index, &result.buffer);
        if (result.buffer) {
            glGetInteger64i_v(start, index, &result.offset);
            glGetInteger64i_v(size, index, &result.size);
        }
        return result;
    }

    static void restoreIndexed(GLenum target, unsigned index, const IndexedBufferBinding& binding) {
        // A range binding must retain its offset/length. Replacing every
        // indexed binding with glBindBufferBase exposes the entire buffer to
        // the native shader and changes its addressing/array length.
        if (binding.buffer && binding.size > 0)
            glBindBufferRange(target, index, binding.buffer, binding.offset, binding.size);
        else
            glBindBufferBase(target, index, binding.buffer);
    }

    static GLStateSnapshot capture(GLStateScope scope = GLStateScope::Full) {
        GLStateSnapshot state{};
        state.scope = scope;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.elementBuffer);
        glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &state.indirectBuffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
        state.blendEnabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blendEquationRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.blendEquationAlpha);
        // Renderer::create also captures state before checking GPU support.
        // Do not query SSBO enums on an older context during native fallback.
        if (glGetIntegeri_v && glGetInteger64i_v && glBindBufferBase && glBindBufferRange) {
            state.indexedUniformSupported = true;
            glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &state.uniformBuffer);
            state.indexedUniform = captureIndexed(GL_UNIFORM_BUFFER_BINDING,
                GL_UNIFORM_BUFFER_START, GL_UNIFORM_BUFFER_SIZE, 3);
            int major = 0, minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);
            if (major > 4 || (major == 4 && minor >= 3)) {
                glGetIntegerv(0x90DD, &state.storageBindingCount); // GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
                glGetIntegerv(storageBinding, &state.storageBuffer);
                for (unsigned slot = 0; slot < storageBindings.size(); ++slot)
                    if (state.capturesStorageBinding(storageBindings[slot]) &&
                        storageBindings[slot] < static_cast<unsigned>(state.storageBindingCount))
                        state.indexedStorage[slot] = captureIndexed(storageBinding, storageStart, storageSize, storageBindings[slot]);
            }
        }
        for (unsigned unit = 0; unit < state.textures.size(); ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.textures[unit]);
        }
        glActiveTexture(state.activeTexture);
        return state;
    }

    void restore() const {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        if (vao || elementBuffer) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        if (indexedUniformSupported) {
            restoreIndexed(GL_UNIFORM_BUFFER, 3, indexedUniform);
            // Indexed binding calls also modify the generic target binding.
            glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
        }
        if (storageBindingCount > 0) {
            for (unsigned slot = 0; slot < storageBindings.size(); ++slot)
                if (capturesStorageBinding(storageBindings[slot]) &&
                    storageBindings[slot] < static_cast<unsigned>(storageBindingCount))
                    restoreIndexed(storageTarget, storageBindings[slot], indexedStorage[slot]);
            glBindBuffer(storageTarget, storageBuffer);
        }
        glUseProgram(program);
        glBlendEquationSeparate(blendEquationRGB, blendEquationAlpha);
        glBlendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);
        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        for (unsigned unit = 0; unit < textures.size(); ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, textures[unit]);
        }
        glActiveTexture(activeTexture);
    }
};
