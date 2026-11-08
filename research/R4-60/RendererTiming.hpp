#pragma once
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace chlorium::research::residenttiming {
enum class Stage : unsigned {batchOutsideAtlas,atlas,collect,indices,prepare,program,sources,upload,materialize,backup,draw,native,batchTotal,count};
constexpr auto stages=static_cast<unsigned>(Stage::count);
struct Stats {
    std::uint64_t version=60,count=stages,frequency=0;
    std::array<std::uint64_t,stages> ticks{},calls{};
};
inline Stats stats=[] {
    Stats value;LARGE_INTEGER frequency{};const auto error=GetLastError();
    if(QueryPerformanceFrequency(&frequency) && frequency.QuadPart>0)value.frequency=frequency.QuadPart;
    SetLastError(error);return value;
}();
inline std::uint64_t now()noexcept{
    LARGE_INTEGER value{};const auto error=GetLastError();const auto ok=QueryPerformanceCounter(&value);
    SetLastError(error);return ok && value.QuadPart>0?std::uint64_t(value.QuadPart):0;
}
inline void add(Stage stage,std::uint64_t ticks)noexcept{
    const auto index=static_cast<unsigned>(stage);if(index>=stages)return;
    stats.ticks[index]+=ticks;++stats.calls[index];
}
struct Batch {std::uint64_t atlas=0,children=0;};
inline thread_local Batch* batch=nullptr;
class Scope {
    Stage stage_;std::uint64_t start_=0;
public:
    explicit Scope(Stage stage,bool enabled=true):stage_(stage){if(enabled && static_cast<unsigned>(stage)<stages)start_=now();}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    void finish()noexcept{
        if(!start_)return;const auto end=now(),start=start_;start_=0;
        if(end<start)return;const auto ticks=end-start;add(stage_,ticks);
        if(stage_==Stage::atlas && batch)batch->atlas+=ticks;
    }
    ~Scope(){finish();}
};
class BatchScope {
    Batch own_{};Batch* previous_=nullptr;std::uint64_t start_=0;
public:
    explicit BatchScope(bool enabled){if(enabled){start_=now();if(start_){previous_=batch;batch=&own_;}}}
    BatchScope(const BatchScope&)=delete;BatchScope& operator=(const BatchScope&)=delete;
    ~BatchScope(){
        if(!start_)return;const auto end=now();batch=previous_;if(end<start_)return;
        const auto elapsed=end-start_,spent=own_.atlas+own_.children;
        add(Stage::batchOutsideAtlas,elapsed>spent?elapsed-spent:0);
        if(previous_)previous_->children+=elapsed;else add(Stage::batchTotal,elapsed);
    }
};
static_assert(sizeof(Stats)==(3+2*stages)*sizeof(std::uint64_t));
}
