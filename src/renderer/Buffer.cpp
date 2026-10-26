#include "Buffer.hpp"
#include <stdexcept>
#include <limits>
#include <memory>
#include <algorithm>

using namespace geode::prelude;

Buffer::~Buffer() {
    unmap();
    if (id && context) context->destroyOwnedBuffer(id);
}
void Buffer::requireContext() const {
    if (!context || !context->isCurrent()) throw std::runtime_error("GPU buffer context/epoch is stale");
}

void Buffer::read(void* data, usize size, usize offset) {
    if (!size) return;
    requireContext();
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
    requireContext();
    if (!data || offset > this->size || size > this->size - offset)
        throw std::out_of_range("Buffer write outside storage");

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
}
void Buffer::exchangeNativeStore(u32& nativeStore,usize nativeBytes) {
    requireContext();
    if(mappedData || !id || !nativeStore || nativeStore==id || !nativeBytes)
        throw std::invalid_argument("invalid native buffer ownership exchange");
    std::swap(id,nativeStore);size=nativeBytes;
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
    requireContext();
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
    auto ret = std::make_unique<Buffer>();
    ret->context = chlorium::research::gl::acquireContextEpoch();
    if (!ret->context) throw std::runtime_error("GPU buffer requires a current context");
    u32 buffer = 0;
    glGenBuffers(1, &buffer);
    if (!buffer) throw std::runtime_error("GPU buffer name allocation failed");
    ret->id = buffer;

    i32 previouslyBoundBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previouslyBoundBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    // glGenBuffers reserves a name; the object exists only after first bind.
    if (name) glObjectLabel(GL_BUFFER, buffer, static_cast<GLsizei>(std::min<std::size_t>(strlen(name), 128)), name);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, usage);
    GLint64 allocated = 0;
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);

    glBindBuffer(GL_ARRAY_BUFFER, previouslyBoundBuffer);
    if (allocated < 0 || static_cast<usize>(allocated) != size) {
        throw std::runtime_error("GPU buffer storage allocation failed");
    }

    ret->size = size;
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_QUADS
    ret->shouldMap = false; // Owned quad storage uses explicit sparse sub-updates.
#else
    ret->shouldMap = Mod::get()->getSettingValue<bool>("use_mapped_buffers");
#endif

    return ret.release();
}
