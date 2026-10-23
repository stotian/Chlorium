#include "Buffer.hpp"
#include <stdexcept>
#include <limits>

using namespace geode::prelude;

Buffer::~Buffer() {
    unmap();
    glDeleteBuffers(1, &id);
}

void Buffer::read(void* data, usize size, usize offset) {
    if (!size) return;
    if (!data || offset > this->size || size > this->size - offset)
        throw std::out_of_range("Buffer read outside storage");

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glGetBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
}

void Buffer::write(void* data, usize size, usize offset) {
    if (!size) return;
    if (!data || offset > this->size || size > this->size - offset)
        throw std::out_of_range("Buffer write outside storage");

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
}

void* Buffer::map(i32 type) {
    if (mappedData) return nullptr;
    // Keep a stable CPU address. A glMapBuffer pointer may not remain mapped
    // while the GPU draws, and may never be passed to delete[].
    mappedData = new u8[size]{};
    if (type == GL_READ_ONLY || type == GL_READ_WRITE)
        read(mappedData, size);
    return mappedData;
}

void Buffer::unmap() {
    if (!mappedData) return;

    delete[] mappedData;
    mappedData = nullptr;
}

void Buffer::flush(usize offset, usize size) {
    if (!mappedData) return;
    if (!size) return;
    if (offset > this->size || size > this->size - offset) {
        assert(false && "Buffer upload outside allocated storage");
        return;
    }

    if (shouldMap) {
        GLint previous;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
        glBindBuffer(GL_ARRAY_BUFFER, id);
        void* destination = glMapBufferRange(GL_ARRAY_BUFFER, offset, size,
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
        if (destination) {
            std::memcpy(destination, mappedData + offset, size);
            if (!glUnmapBuffer(GL_ARRAY_BUFFER))
                // Failed unmap invalidates the entire store, not just the
                // range being updated. Restore every byte from CPU staging.
                glBufferSubData(GL_ARRAY_BUFFER, 0, this->size, mappedData);
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, offset, size, mappedData + offset);
        }
        glBindBuffer(GL_ARRAY_BUFFER, previous);
    } else {
        write(mappedData + offset, size, offset);
    }
}

Buffer* Buffer::create(const char* name, usize size, GLenum usage) {
    if (size > static_cast<usize>(std::numeric_limits<std::ptrdiff_t>::max()))
        throw std::length_error("GPU buffer size is not representable");
    u32 buffer = 0;
    glGenBuffers(1, &buffer);
    if (!buffer) throw std::runtime_error("GPU buffer name allocation failed");

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    // glGenBuffers reserves a name; the object exists only after first bind.
    glObjectLabel(GL_BUFFER, buffer, strlen(name), name);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, usage);
    GLint64 allocated = 0;
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
    if (allocated < 0 || static_cast<usize>(allocated) != size) {
        glDeleteBuffers(1, &buffer);
        throw std::runtime_error("GPU buffer storage allocation failed");
    }

    auto ret  = new Buffer();
    ret->id = buffer;
    ret->size = size;
    ret->shouldMap = Mod::get()->getSettingValue<bool>("use_mapped_buffers");

    return ret;
}
