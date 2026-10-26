#pragma once
#include "NativeQuadState.hpp"
#include "DirtyStateUploads.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <vector>

namespace chlorium::snapshot {
enum class UploadMode { Empty, Direct, Seed, Sparse };
struct UploadWork {
    UploadMode mode=UploadMode::Empty;
    std::size_t compared=0,copied=0,uploaded=0,writes=0;
};
// Sampling selects transport cost only. It NEVER decides whether source bytes
// may be ignored: direct writes the full prefix, sparse compares every record.
class AdaptiveUpload {
public:
    void invalidate() noexcept {valid_=previousCount_=samples_=0;}
    template<class Writer> UploadWork update(std::span<const std::byte> source,std::size_t capacity,Writer&& write) {
        if(source.size()%sizeof(NativeQuadState)) throw std::invalid_argument("adaptive upload record size");
        const auto count=source.size()/sizeof(NativeQuadState);
        if(count>capacity) throw std::invalid_argument("adaptive upload capacity");
        struct FailureGuard {
            AdaptiveUpload& owner;int exceptions=std::uncaught_exceptions();
            ~FailureGuard() {if(std::uncaught_exceptions()>exceptions) owner.invalidate();}
        } guard{*this};
        UploadWork work;
        if(source.empty()) {previousCount_=samples_=0;return work;}
        const auto sampleCount=std::min<std::size_t>(8,count);
        std::size_t changed=0;
        for(std::size_t n=0;n<sampleCount;++n) {
            const auto index=sampleCount==1?0:n*(count-1)/(sampleCount-1);
            const auto value=source.data()+index*sizeof(NativeQuadState);
            if(previousCount_!=count || n>=samples_ || std::memcmp(&sentinels_[n],value,sizeof(NativeQuadState))) ++changed;
            std::memcpy(&sentinels_[n],value,sizeof(NativeQuadState));++work.compared;++work.copied;
        }
        const bool direct=previousCount_!=count || changed*4>=sampleCount;
        previousCount_=count;samples_=sampleCount;
        if(direct) {
            write(std::size_t(0),count,source.data());
            work.mode=UploadMode::Direct;work.uploaded=count;work.writes=1;valid_=0;
            return work;
        }
        if(valid_<count || shadow_.size()!=capacity) {
            shadow_.resize(capacity);
            std::memcpy(shadow_.data(),source.data(),source.size());
            write(std::size_t(0),count,shadow_.data());
            work.mode=UploadMode::Seed;work.copied+=count;work.uploaded=count;work.writes=1;valid_=count;
            return work;
        }
        work.mode=UploadMode::Sparse;DirtyStateUploads dirty(capacity);
        for(std::size_t n=0;n<count;++n) {
#ifdef CHLORIUM_PRIVATE_UPLOAD_MUTANT
            if(n==1) continue;
#endif
            ++work.compared;
            const auto value=source.data()+n*sizeof(NativeQuadState);
            if(std::memcmp(&shadow_[n],value,sizeof(NativeQuadState))) {
                std::memcpy(&shadow_[n],value,sizeof(NativeQuadState));++work.copied;dirty.mark(n);
            }
        }
        for(const auto range:dirty.finish()) {
            write(range.first,range.count,shadow_.data()+range.first);
            work.uploaded+=range.count;++work.writes;
        }
        return work;
    }
private:
    std::array<NativeQuadState,8> sentinels_{};
    std::vector<NativeQuadState> shadow_;
    std::size_t valid_=0,previousCount_=0,samples_=0;
};
}
