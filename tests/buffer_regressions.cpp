#include "renderer/Buffer.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>

bool geode::prelude::mappedUploads = false;
static GLuint bound = 99, nextID = 1;
static std::map<GLuint, std::vector<u8>> memory;
static bool driverMapped = false, failMapping = false, failUnmapping = false;
static bool failAllocation = false;
static int mapCalls = 0, uploadCalls = 0, checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void glGetIntegerv(GLenum, GLint* value) { *value = bound; }
void glBindBuffer(GLenum, GLuint value) { bound = value; }
void glGenBuffers(GLsizei count, GLuint* ids) { while (count--) *ids++ = nextID++; }
void glDeleteBuffers(GLsizei count, const GLuint* ids) { while (count--) memory.erase(*ids++); }
void glObjectLabel(GLenum, GLuint, GLsizei, const char*) {}
void glBindBufferBase(GLenum, GLuint, GLuint) {}
void glBufferData(GLenum, std::size_t size, const void* data, GLenum) {
    memory[bound].resize(failAllocation ? 0 : size);
    if (data) std::memcpy(memory[bound].data(), data, size);
}
void glGetBufferParameteri64v(GLenum, GLenum, GLint64* size) { *size = memory.at(bound).size(); }
void glGetBufferSubData(GLenum, std::size_t offset, std::size_t size, void* output) {
    std::memcpy(output, memory.at(bound).data() + offset, size);
}
void glBufferSubData(GLenum, std::size_t offset, std::size_t size, const void* data) {
    require(!driverMapped, "subdata upload is never issued to a mapped buffer");
    ++uploadCalls;
    std::memcpy(memory.at(bound).data() + offset, data, size);
}
void* glMapBufferRange(GLenum, std::size_t offset, std::size_t, unsigned flags) {
    ++mapCalls;
    require(flags & GL_MAP_WRITE_BIT, "mapped upload has write access");
    if (failMapping) return nullptr;
    driverMapped = true;
    return memory.at(bound).data() + offset;
}
bool glUnmapBuffer(GLenum) {
    require(driverMapped, "unmap has a matching map");
    driverMapped = false;
    if (failUnmapping) {
        std::fill(memory.at(bound).begin(), memory.at(bound).end(), 0);
        return false;
    }
    return true;
}

int main() {
    for (bool mapped : {false, true}) {
        geode::prelude::mappedUploads = mapped;
        const auto previous = bound;
        auto* buffer = Buffer::createDynamicDraw("test", 16);
        const auto id = buffer->getId();
        require(bound == previous, "buffer allocation preserves binding");
        auto* staging = static_cast<u8*>(buffer->mapWriteOnly());
        require(bound == previous, "CPU mapping preserves binding");
        require(std::all_of(staging, staging + 16, [](u8 v) { return v == 0; }), "write staging starts initialized");
        staging[3] = 71; staging[4] = 72;
        buffer->flush(3, 2);
        require(memory.at(id)[3] == 71 && memory.at(id)[4] == 72 && memory.at(id)[2] == 0, "partial upload writes correct range");
        require(bound == previous && !driverMapped, "buffer is unmapped and binding restored before rendering");
        staging[3] = 91;
        buffer->flush(3, 1);
        require(memory.at(id)[3] == 91, "stable CPU pointer supports later frames");
        buffer->unmap();
        auto* readable = static_cast<u8*>(buffer->mapReadWrite());
        require(readable[3] == 91 && readable[4] == 72, "readwrite mapping preserves existing content");
        require(buffer->mapWriteOnly() == nullptr, "second CPU map rejected");
        if (mapped) {
            failMapping = true;
            readable[9] = 111;
            buffer->flush(9, 1);
            require(memory.at(id)[9] == 111 && !driverMapped && bound == previous, "driver map failure falls back to subdata");
            failMapping = false;
            failUnmapping = true;
            readable[10] = 123;
            buffer->flush(10, 1);
            require(memory.at(id)[10] == 123 && !driverMapped && bound == previous, "unmap corruption recovers from CPU staging");
            require(memory.at(id)[3] == 91 && memory.at(id)[4] == 72, "failed unmap restores bytes outside partial upload range");
            failUnmapping = false;
        }
        bool readRejected = false, writeRejected = false;
        u8 value = 0;
        try { buffer->read(&value, 2, static_cast<usize>(-1)); }
        catch (const std::out_of_range&) { readRejected = true; }
        try { buffer->write(&value, 2, 15); }
        catch (const std::out_of_range&) { writeRejected = true; }
        require(readRejected && writeRejected && bound == previous, "invalid and overflowing ranges rejected before GL access");
        Buffer::destroy(buffer);
        require(!memory.contains(id) && !driverMapped, "destruction frees CPU staging and GPU buffer");
    }
    require(mapCalls > 0 && uploadCalls > 0, "both upload paths exercised");
    const auto liveBefore = memory.size();
    const auto boundBefore = bound;
    failAllocation = true;
    bool rejected = false;
    try { Buffer::destroy(Buffer::createDynamicDraw("allocation failure", 16)); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected && bound == boundBefore && memory.size() == liveBefore, "failed GPU storage allocation is reported and rolled back");
    std::cout << "PASS: " << checks << " buffer regression checks\n";
}
