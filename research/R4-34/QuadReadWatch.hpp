#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace chlorium::research::readwatch {
// Diagnostic only: four thread-local data breakpoints, no instruction patch.
// Existing debugger/single-step ownership rejects before touching registers.
class Watch {
    friend struct WatchChecks;
    static constexpr DWORD64 ours=0xffff0055ull;
    DWORD64 desired=ours,slotMask=15;
    bool execution=false;
    inline static thread_local Watch* current=nullptr;
    void* handler=nullptr;
    DWORD owner=0;
    std::uintptr_t stackBase=0,stackLimit=0;
    std::array<DWORD64,4> addresses{};
    std::array<DWORD64,6> saved{};
    struct Change {Watch* watch;HANDLE thread;bool arm;bool success=false;};
    static void restore(CONTEXT& c,Watch& w) noexcept {
        DWORD64* registers[]{&c.Dr0,&c.Dr1,&c.Dr2,&c.Dr3};
        for(unsigned i=0;i<4;++i){if(!(w.slotMask&(1ull<<i)))continue;const auto enable=3ull<<(i*2),control=15ull<<(16+i*4),mask=enable|control;
            if(*registers[i]==w.addresses[i]&&(c.Dr7&mask)==(w.desired&mask)){
                *registers[i]=w.saved[i];c.Dr7=(c.Dr7&~mask)|(w.saved[5]&mask);c.Dr6=(c.Dr6&~(1ull<<i))|(w.saved[4]&(1ull<<i));
            }else w.ownershipLost=true;
        }
        w.armed=false;
    }
    static LONG WINAPI exception(EXCEPTION_POINTERS* info) noexcept {
        auto w=current;if(!w)return EXCEPTION_CONTINUE_SEARCH;
        auto& c=*info->ContextRecord;const auto code=info->ExceptionRecord->ExceptionCode;
        if(code!=EXCEPTION_SINGLE_STEP||!w->armed||!(c.Dr6&w->slotMask))return EXCEPTION_CONTINUE_SEARCH;
        // Never consume another debugger's pending slot or single-step event.
        if((c.Dr6&15&~w->slotMask)||(c.Dr6&(1ull<<14)))return EXCEPTION_CONTINUE_SEARCH;
        const DWORD64 values[]{c.Dr0,c.Dr1,c.Dr2,c.Dr3};
        for(unsigned i=0;i<4;++i)if(c.Dr6&(1ull<<i)){
            const auto mask=(3ull<<(i*2))|(15ull<<(16+i*4));
            if(values[i]!=w->addresses[i]||(c.Dr7&mask)!=(w->desired&mask)){w->ownershipLost=true;return EXCEPTION_CONTINUE_SEARCH;}
            ++w->hits[i];
        }
        // Execution traps precede the instruction; RF lets it run even after log overflow.
        if(w->execution)c.EFlags|=0x10000;
        if(w->count==w->events.size()){w->overflow=true;c.Dr6&=~w->slotMask;return EXCEPTION_CONTINUE_EXECUTION;}
        auto& event=w->events[w->count++];event={c.Rip,c.Dr6&15,c.Rsp,c.Rcx,c.Rdx,c.Rdi,c.R8};
        for(unsigned i=0;i<event.stackWords.size();++i){const auto address=c.Rsp+i*8;
            if(address<w->stackLimit||address>w->stackBase||w->stackBase-address<8)break;
            bool overlaps=false;for(auto watched:w->addresses)overlaps|=address<watched+4&&watched<address+8;
            if(overlaps)continue;
            for(unsigned b=0;b<8;++b)event.stackWords[i]|=std::uint64_t(*reinterpret_cast<const volatile unsigned char*>(address+b))<<(8*b);
        }
        c.Dr6&=~w->slotMask;return EXCEPTION_CONTINUE_EXECUTION;
    }
    static DWORD WINAPI changeThread(void* input) noexcept {
        auto& change=*static_cast<Change*>(input);auto& w=*change.watch;
        if(SuspendThread(change.thread)==DWORD(-1))return 0;
        CONTEXT c{};c.ContextFlags=CONTEXT_DEBUG_REGISTERS|CONTEXT_CONTROL;
        if(GetThreadContext(change.thread,&c)){
            const auto original=c;
            if(change.arm){
                if((c.Dr7&0xff)||(c.EFlags&0x100))w.busy=true;
                else{
                    w.saved={c.Dr0,c.Dr1,c.Dr2,c.Dr3,c.Dr6,c.Dr7};
                    DWORD64* registers[]{&c.Dr0,&c.Dr1,&c.Dr2,&c.Dr3};
                    DWORD64 ownedControl=0;
                    for(unsigned i=0;i<4;++i)if(w.slotMask&(1ull<<i)){
                        *registers[i]=w.addresses[i];ownedControl|=(3ull<<(i*2))|(15ull<<(16+i*4));
                    }
                    c.Dr6&=~w.slotMask;c.Dr7=(c.Dr7&~ownedControl)|w.desired;
                    c.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                    if(SetThreadContext(change.thread,&c)){
                        CONTEXT verified{};verified.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                        change.success=GetThreadContext(change.thread,&verified)&&verified.Dr0==c.Dr0&&verified.Dr1==c.Dr1&&verified.Dr2==c.Dr2&&verified.Dr3==c.Dr3&&(verified.Dr7&ownedControl)==w.desired;
                    }
                    if(!change.success){CONTEXT undo=original;undo.ContextFlags=CONTEXT_DEBUG_REGISTERS;if(!SetThreadContext(change.thread,&undo))TerminateProcess(GetCurrentProcess(),91);}
                    w.armed=change.success;
                }
            }else{
                restore(c,w);c.ContextFlags=CONTEXT_DEBUG_REGISTERS;change.success=SetThreadContext(change.thread,&c)!=FALSE;
                if(!change.success)TerminateProcess(GetCurrentProcess(),91);
            }
        }
        if(ResumeThread(change.thread)==DWORD(-1))TerminateProcess(GetCurrentProcess(),92);
        return 0;
    }
    bool request(bool arm) noexcept {
        HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,GetCurrentThreadId());if(!thread)return false;
        Change change{this,thread,arm};HANDLE helper=CreateThread(nullptr,0,&changeThread,&change,0,nullptr);
        if(!helper){CloseHandle(thread);return false;}
        const bool joined=WaitForSingleObject(helper,INFINITE)==WAIT_OBJECT_0;
        CloseHandle(helper);CloseHandle(thread);return joined&&change.success;
    }
public:
    struct Event {std::uint64_t afterInstruction=0,slots=0,stack=0,rcx=0,rdx=0,rdi=0,r8=0;std::array<std::uint64_t,16> stackWords{};};
    std::array<Event,512> events{};unsigned count=0;bool armed=false,busy=false,overflow=false,ownershipLost=false;
    std::array<std::uint64_t,4> hits{};
    Watch()=default;Watch(const Watch&)=delete;Watch& operator=(const Watch&)=delete;
    bool arm(std::array<std::uintptr_t,4> watched) noexcept {
        return start(watched,false);
    }
    bool armExecution(std::span<const std::uintptr_t> watched) noexcept {
        return start(watched,true);
    }
private:
    bool start(std::span<const std::uintptr_t> watched,bool execute) noexcept {
        const auto error=GetLastError();struct Preserve{DWORD error;~Preserve(){SetLastError(error);}}preserve{error};
        if(current||handler)return false;
        if(watched.empty()||watched.size()>4)return false;
        for(auto address:watched)if(!address||(!execute&&(address&3)))return false;
        execution=execute;slotMask=(1ull<<watched.size())-1;desired=0;
        for(unsigned i=0;i<watched.size();++i)desired|=(1ull<<(i*2))|(execute?0ull:15ull<<(16+i*4));
        count=0;hits={};busy=overflow=ownershipLost=false;
        std::copy(watched.begin(),watched.end(),addresses.begin());
        handler=AddVectoredExceptionHandler(1,&exception);if(!handler)return false;
        const auto tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());stackBase=reinterpret_cast<std::uintptr_t>(tib->StackBase);stackLimit=reinterpret_cast<std::uintptr_t>(tib->StackLimit);
        owner=GetCurrentThreadId();current=this;const bool requested=request(true);
        if(!requested||!armed){current=nullptr;RemoveVectoredExceptionHandler(handler);handler=nullptr;return false;}
        return true;
    }
public:
    void stop() noexcept {
        const auto error=GetLastError();struct Preserve{DWORD error;~Preserve(){SetLastError(error);}}preserve{error};
        if(handler&&owner!=GetCurrentThreadId())TerminateProcess(GetCurrentProcess(),93);
        if(handler){if(armed&&!request(false))TerminateProcess(GetCurrentProcess(),91);
            current=nullptr;RemoveVectoredExceptionHandler(handler);handler=nullptr;}
    }
    ~Watch(){stop();}
};
}
