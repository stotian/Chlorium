#pragma once

#include <common.hpp>
#include <profiler.hpp>

class GPUTimer {
public:
    inline GPUTimer(const char* name)
        : name(name)
    {
        profiler::addTimer(this);
    }

    inline ~GPUTimer() {
        if (queries[0]) glDeleteQueries(2, queries);
        profiler::removeTimer(this);
    }

    inline void start(bool force = false) {
        if (!(force || profiler::isTakingSnapshot) || active) return;
        if (pending) {
            GLint available = 0;
            glGetQueryObjectiv(queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return;
            GLint64 begin = 0, end = 0;
            glGetQueryObjecti64v(queries[0], GL_QUERY_RESULT, &begin);
            glGetQueryObjecti64v(queries[1], GL_QUERY_RESULT, &end);
            time = end - begin;
            pending = false;
        }
        if (!queries[0]) glGenQueries(2, queries);
        glQueryCounter(queries[0], GL_TIMESTAMP);
        active = true;
    }

    inline void stop() {
        if (!active) return;
        glQueryCounter(queries[1], GL_TIMESTAMP);
        active = false;
        pending = true;
    }

    inline const char* getName() const { return name; }
    inline i64 getTime() const { return time; }

private:
    i64 time = 0;
    const char* name;
    u32 queries[2]{};
    bool active = false, pending = false;
};
