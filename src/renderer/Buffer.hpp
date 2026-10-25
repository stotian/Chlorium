#pragma once

#include "Geode/cocos/platform/win32/CCGL.h"
#include <common.hpp>
#ifdef CHLORIUM_BUFFER_CONTEXT_HEADER
#include CHLORIUM_BUFFER_CONTEXT_HEADER
#else
#include "ContextEpoch.hpp"
#endif

#define GL_SHADER_STORAGE_BUFFER 0x90D2
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

class Buffer {
public:
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer();

    inline usize getSize() const { return size; }
    inline u32 getId() const { return id; }

    void read(void* data, usize size, usize offset = 0);

    void write(void* data, usize size, usize offset = 0);

    inline void bindAs(GLenum binding) {
        requireContext();
        glBindBuffer(binding, id);
    }

    inline void bindAsUniformBuffer(u32 binding) {
        requireContext();
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, id);
    }

    inline void bindAsStorageBuffer(u32 binding) {
        requireContext();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id);
    }

    void* map(i32 type);

    inline void* mapWriteOnly() {
        return map(GL_WRITE_ONLY);
    }

    inline void* mapReadWrite() {
        return map(GL_READ_WRITE);
    }

    void unmap();

    void flush(usize offset, usize size);

public:
    static Buffer* create(const char* name, usize size, GLenum usage);

    inline static void destroy(Buffer* buffer) {
        delete buffer;
    }

    /*
        Buffer to be written to once by the CPU
        and used by the GPU
    */
    inline static Buffer* createStaticDraw(const char* name, void* data, usize size) {
        auto ret = create(name, size, GL_STATIC_DRAW);
        if (data)
            ret->write(data, size);
        return ret;
    }

    /*
        Buffer to be written to many times repeatedly by the CPU
        and used by the GPU
    */
    inline static Buffer* createDynamicDraw(const char* name, usize size) {
        return create(name, size, GL_DYNAMIC_DRAW);
    }

    /*
        Buffer to be written to many times repeatedly by the GPU
        and used by the CPU
    */
    inline static Buffer* createDynamicCopy(const char* name, usize size) {
        return create(name, size, GL_DYNAMIC_COPY);
    }

    /*
        Buffer to be written to and read from by the GPU exclusively
    */
    static Buffer* createGPUExclusive(const char* name, usize size) {
        return create(name, size, GL_DYNAMIC_DRAW); // GL_DYNAMIC_DRAW for now
    }

private:
    void requireContext() const;
    // GL names are local to their owning context. Never delete a stale name
    // in a replacement context, where that number may belong to the game.
    std::shared_ptr<chlorium::research::gl::ContextEpoch<>> context;
    usize size = 0;
    u32 id = 0;
    bool shouldMap = true;
    u8* mappedData = nullptr;
};
