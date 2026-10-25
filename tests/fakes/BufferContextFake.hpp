#pragma once
// CPU-only fake GL has no Windows context. Real context semantics are covered
// by research/R4-04/epoch_gpu_checks.cpp against the Intel driver and real Buffer.
#include <memory>
namespace chlorium::research::gl {
inline bool fakeBufferContextCurrent = true;
template <class Provider = void> class ContextEpoch {
public:
    bool isCurrent() const { return fakeBufferContextCurrent; }
    bool destroyOwnedBuffer(GLuint id) const {
        if (!isCurrent()) return false;
        glDeleteBuffers(1, &id);
        return true;
    }
};
inline std::shared_ptr<ContextEpoch<>> acquireContextEpoch() {
    if (!fakeBufferContextCurrent) return {};
    return std::make_shared<ContextEpoch<>>();
}
}
