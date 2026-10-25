#pragma once
// Bounded input census, never a replacement for native Area execution.
#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <exception>
#include <cstddef>

namespace chlorium::research::area {
enum class Kind : std::uint32_t { Enter = 1, EffectInput = 2, Leave = 3, Unwind = 4 };
enum class Phase : std::uint32_t { Actions = 1, Effects = 2 };
enum Fault : std::uint32_t { Overflow = 1, Nesting = 2, Stale = 4, PartialList = 8, Tick = 16, NativeException = 32 };
struct Input {
    std::int32_t target = 0, center = 0, groupIndex = 0, control = 0;
    std::uint32_t flags = 0;
    // Named in adapter: length, offset, moveX/Y, scaleX/Y, rotation, tint,
    // from/to opacity, deadzone, relativeFade. No raw pointers or padding copied.
    std::array<std::uint32_t, 12> valueBits{};
};
struct Record {
    std::uint64_t sequence = 0, call = 0, parent = 0, tick = 0, epoch = 0;
    Kind kind{};
    Phase phase{};
    std::uint32_t depth = 0, actionType = 0, dtBits = 0, visible = 0, slot = 0, inputCount = 0;
    Input input{};
    std::uint32_t reserved = 0; // Explicit wire bytes: no uninitialised tail padding.
};
static_assert(std::is_trivially_copyable_v<Record> && sizeof(Input) == 68 && sizeof(Record) == 144);
static_assert(offsetof(Record, reserved) == 140);
struct Token {
    std::uint64_t session = 0, call = 0, tick = 0;
    explicit operator bool() const { return call != 0; }
};

template <std::size_t Capacity = 2048, std::size_t MaxDepth = 32>
class Recorder {
public:
    static_assert(Capacity > 0 && MaxDepth > 0);
    bool start(std::uint64_t epoch, std::uint32_t ticks, std::size_t budget = Capacity) noexcept {
        if (open_ || depth_ || !epoch || !ticks || !budget || budget > Capacity || session_ == UINT64_MAX) return false;
        ++session_; epoch_ = epoch; remaining_ = ticks; budget_ = budget;
        used_ = 0; faults_ = 0; dropped_ = 0; sequence_ = 0; call_ = 0; tick_ = 0;
        active_ = true;
        return true;
    }
    bool beginTick(std::uint64_t tick) noexcept {
        if (!active_ || open_ || !tick || tick <= tick_) { faults_ |= Tick; return false; }
        open_ = true; tick_ = tick;
        return true;
    }
    bool endTick() noexcept {
        if (!open_ || depth_) { faults_ |= Tick; return false; }
        open_ = false;
        if (--remaining_ == 0) active_ = false;
        return true;
    }
    Token enter(Phase phase, std::uint32_t type, float dt, bool visible, std::uint32_t inputCount) noexcept {
        if (!open_ || depth_ == MaxDepth || call_ == UINT64_MAX) { faults_ |= Nesting; return {}; }
        Record event{};
        event.call = ++call_; event.parent = depth_ ? stack_[depth_ - 1].call : 0;
        event.phase = phase; event.depth = static_cast<std::uint32_t>(depth_);
        event.kind = Kind::Enter; event.actionType = type; event.dtBits = std::bit_cast<std::uint32_t>(dt);
        event.visible = visible; event.inputCount = inputCount;
        stack_[depth_++] = event;
        append(event);
        return {session_, event.call, tick_};
    }
    bool input(Token token, std::uint32_t slot, const Input& value) noexcept {
        if (!top(token)) return false;
        auto event = stack_[depth_ - 1];
        if (slot >= event.inputCount) { faults_ |= PartialList; return false; }
        event.kind = Kind::EffectInput; event.slot = slot; event.input = value;
        append(event);
        return true;
    }
    bool leave(Token token, bool unwound = false) noexcept {
        if (!top(token)) return false;
        auto event = stack_[--depth_]; event.kind = unwound ? Kind::Unwind : Kind::Leave;
        if (unwound) faults_ |= NativeException;
        append(event);
        return true;
    }
    void incompleteList() noexcept { faults_ |= PartialList; }
    bool active() const noexcept { return active_; }
    bool tickOpen() const noexcept { return open_; }
    bool idle() const noexcept { return !open_ && !depth_; }
    // Complete TRANSPORT only; input census never proves full semantic coverage.
    bool transportComplete() const noexcept { return idle() && !active_ && used_ && !faults_; }
    std::uint32_t faults() const noexcept { return faults_; }
    std::uint64_t dropped() const noexcept { return dropped_; }
    std::uint64_t session() const noexcept { return session_; }
    std::span<const Record> records() const noexcept { return {records_.data(), used_}; }
private:
    bool top(Token token) noexcept {
        if (!token || token.session != session_ || token.tick != tick_) { faults_ |= Stale; return false; }
        if (!depth_ || stack_[depth_ - 1].call != token.call) { faults_ |= Nesting; return false; }
        return true;
    }
    void append(Record event) noexcept {
        event.sequence = ++sequence_; event.tick = tick_; event.epoch = epoch_;
        if (used_ == budget_) {
            faults_ |= Overflow;
            if (dropped_ != UINT64_MAX) ++dropped_;
            return;
        }
        records_[used_++] = event;
    }
    std::array<Record, Capacity> records_{};
    std::array<Record, MaxDepth> stack_{};
    std::size_t used_ = 0, depth_ = 0, budget_ = Capacity;
    std::uint64_t session_ = 0, epoch_ = 0, sequence_ = 0, call_ = 0, tick_ = 0, dropped_ = 0;
    std::uint32_t remaining_ = 0, faults_ = 0;
    bool active_ = false, open_ = false;
};

template <class Trace>
class CallScope {
public:
    CallScope(Trace& trace, Phase phase, std::uint32_t type, float dt, bool visible, std::uint32_t count) noexcept
        : trace_(trace), token_(trace.enter(phase, type, dt, visible, count)), exceptions_(std::uncaught_exceptions()) {}
    ~CallScope() noexcept { if (token_) trace_.leave(token_, std::uncaught_exceptions() > exceptions_); }
    Token token() const noexcept { return token_; }
    CallScope(const CallScope&) = delete;
    CallScope& operator=(const CallScope&) = delete;
private:
    Trace& trace_;
    Token token_;
    int exceptions_;
};
}
