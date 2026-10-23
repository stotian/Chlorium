#pragma once
#include <cstddef>
#include <cstring>

namespace chlorium::streaming {
enum class Mode { MappedRange, OrphanFullStore };
enum class Result { Uploaded, NativeUpload, CorruptMapping };

// Bound original VBO; bytes describes the complete active prefix. There is
// deliberately no UNSYNCHRONIZED flag: preserve in-flight native draws.
template <class Api>
Result prepareUpload(Api& api, const void* data, std::size_t bytes,
                     std::size_t capacityBytes, Mode mode) {
    if (!data || !bytes || bytes > capacityBytes) return Result::NativeUpload;
    if (mode == Mode::OrphanFullStore) {
        // Preserve an unwritten tail and always retain the original dirty
        // upload, including if a driver cannot allocate replacement storage.
        if (bytes == capacityBytes) api.orphan(capacityBytes);
        return Result::NativeUpload;
    }
    auto mapped = api.mapWriteInvalidateRange(bytes);
    if (!mapped) return Result::NativeUpload;
    std::memcpy(mapped, data, bytes);
    return api.unmap() ? Result::Uploaded : Result::CorruptMapping;
}
}
