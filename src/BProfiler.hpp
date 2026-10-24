#pragma once

#include "common.hpp"
#include <string_view>

class BProfiler {
public:
    class Timer;

public:
    static Timer start(std::string_view name);
    static bool isEnabled();
    static void setEnabled(bool enabled);
    // Gate one frame without deleting the accumulated session results.
    static void beginFrameSampling(bool enabled);

    static bool isUsingAverages();

    static void useAverages(bool cond);

    static std::string toString();
    static double currentMilliseconds(std::string_view name);

    static void frameEnd(bool includeFrame = true);

private:
    static void end(std::string name, u64 time);

public:
    struct Timer {
    private:
        std::string name;
        u64 lastTime;
        bool active;

    public:
        inline Timer(std::string_view name)
            : name(BProfiler::isEnabled() ? std::string{name} : std::string{}),
              lastTime(BProfiler::isEnabled() ? getTime() : 0),
              active(BProfiler::isEnabled()) {}

        inline void end() {
            if (active) BProfiler::end(name, getTime() - lastTime);
        }
    };
};
