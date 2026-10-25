#pragma once
#include "../R4-11/PoseProgram.hpp"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

namespace chlorium::research::causal {
// Model domain only: positions and ordered float-affine commands. This is not
// a replacement for GD Area actions, rotations, collision or callbacks.
struct Position { float x,y; };
struct Effect { float a,b,c,d,tx,ty; };
inline Position apply(Position p,Effect e) {
    using namespace pose;
    return {add(add(mul(e.a,p.x),mul(e.c,p.y)),e.tx),
            add(add(mul(e.b,p.x),mul(e.d,p.y)),e.ty)};
}
struct Handle { std::uint32_t slot,generation,epoch; };
struct Work {
    std::uint64_t commands=0,applied=0,materialized=0,barrierSlots=0,
        unknownBarriers=0,storageBarriers=0;
};
enum class Capability { Unknown, PurePositionSnapshot };
class World {
    struct Membership { std::uint32_t group; std::size_t cursor; };
    struct Slot {
        Position value{};
        std::vector<Membership> groups;
        std::uint32_t generation=1;
        bool alive=false;
    };
    struct Command { std::uint64_t ordinal; Effect effect; };
    struct Group { std::vector<Command> commands; };
public:
    class Snapshot {
        std::vector<Slot> slots;
        friend class World;
    };
    explicit World(std::size_t commandBudget=4096):budget_(commandBudget) {
        if(!budget_) throw std::invalid_argument("zero IR storage budget");
    }
    Handle create(Position position) {
        if(!free_.empty()) {
            const auto n=free_.back();free_.pop_back();
            auto& slot=slots_[n];slot.value=position;slot.alive=true;slot.groups.clear();
            return {static_cast<std::uint32_t>(n),slot.generation,epoch_};
        }
        if(slots_.size()>=std::numeric_limits<std::uint32_t>::max()) throw std::length_error("IR handles exhausted");
        slots_.push_back({position,{},1,true});
        return {static_cast<std::uint32_t>(slots_.size()-1),1,epoch_};
    }
    void join(Handle handle,std::uint32_t group) {
        auto& slot=checked(handle); materialize(slot);
        for(auto m:slot.groups) if(m.group==group) throw std::invalid_argument("duplicate IR membership");
        slot.groups.push_back({group,groups_[group].commands.size()});
    }
    void leave(Handle handle,std::uint32_t group) {
        auto& slot=checked(handle);materialize(slot);
        const auto found=std::find_if(slot.groups.begin(),slot.groups.end(),[&](auto m){return m.group==group;});
        if(found==slot.groups.end()) throw std::invalid_argument("missing IR membership");
        slot.groups.erase(found);
    }
    void destroy(Handle handle) {
        auto& slot=checked(handle);materialize(slot);slot.alive=false;slot.groups.clear();
        if(++slot.generation==0) throw std::overflow_error("IR handle generation exhausted");
        free_.push_back(handle.slot);
    }
    bool enqueue(std::uint32_t group,Effect effect,Capability capability) {
        if(capability!=Capability::PurePositionSnapshot) {
            barrier();++work_.unknownBarriers;return false;
        }
        if(pending_==budget_) { barrier();++work_.storageBarriers; }
        if(ordinal_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("IR command order exhausted");
        groups_[group].commands.push_back({++ordinal_,effect});++pending_;++work_.commands;return true;
    }
    Position read(Handle handle) { auto& slot=checked(handle);materialize(slot);return slot.value; }
    void write(Handle handle,Position value) {
        auto& slot=checked(handle);materialize(slot);slot.value=value;
    }
    // Unknown native readers/callbacks MUST execute only after this barrier.
    // Its full cost and the pending command debt remain visible in Work.
    void barrier() {
        for(auto& slot:slots_) if(slot.alive) {
            ++work_.barrierSlots;
#ifndef CHLORIUM_PRIVATE_CAUSAL_BARRIER_MUTANT
            materialize(slot);
#endif
            for(auto& membership:slot.groups) membership.cursor=0;
        }
        for(auto& [id,group]:groups_) group.commands.clear();
        pending_=0;
    }
    Snapshot checkpoint() { barrier();Snapshot result;result.slots=slots_;return result; }
    void restore(const Snapshot& snapshot) {
        if(epoch_==std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("IR world epoch exhausted");
        slots_=snapshot.slots;groups_.clear();pending_=0;ordinal_=0;++epoch_;
        free_.clear();
        for(std::uint32_t n=0;n<slots_.size();++n) if(!slots_[n].alive) free_.push_back(n);
        for(auto& slot:slots_) if(slot.alive) for(auto& m:slot.groups) {m.cursor=0;groups_.try_emplace(m.group);}
    }
    Handle handleAt(std::uint32_t index) const {
        if(index>=slots_.size()||!slots_[index].alive) throw std::invalid_argument("missing IR object");
        return {index,slots_[index].generation,epoch_};
    }
    Work work() const {return work_;}
    std::size_t pendingCommands() const {return pending_;}
private:
    Slot& checked(Handle handle) {
        if(handle.epoch!=epoch_||handle.slot>=slots_.size()) throw std::invalid_argument("stale IR world handle");
        auto& slot=slots_[handle.slot];
        if(!slot.alive||slot.generation!=handle.generation) throw std::invalid_argument("stale IR object handle");
        return slot;
    }
    void materialize(Slot& slot) {
        bool dirty=false;
        for(;;) {
            Membership* selected=nullptr;const Command* next=nullptr;
            for(auto& m:slot.groups) {
                const auto& commands=groups_.at(m.group).commands;
                if(m.cursor<commands.size()&&(!next
#ifndef CHLORIUM_PRIVATE_CAUSAL_ORDER_MUTANT
                    ||commands[m.cursor].ordinal<next->ordinal
#endif
                    )) {
                    selected=&m;next=&commands[m.cursor];
                }
            }
            if(!next) break;
            slot.value=apply(slot.value,next->effect);++selected->cursor;++work_.applied;dirty=true;
        }
        if(dirty) ++work_.materialized;
    }
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    std::unordered_map<std::uint32_t,Group> groups_;
    std::size_t budget_,pending_=0;
    std::uint64_t ordinal_=0;
    std::uint32_t epoch_=1;
    Work work_{};
};
}
