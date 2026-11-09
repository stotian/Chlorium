#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>
#include <algorithm>
#include <cstdlib>

namespace chlorium::research::fieldwatch {
// Diagnostic only: four read/write data breakpoints on this process's calling thread.
class NativeFieldWatch {
public:
    struct Row {std::uint64_t rip=0,exception=0,ticks=0;std::uint32_t mask=0,frame=0;};
    static constexpr unsigned capacity=32768;
private:
    struct HostState {unsigned fp=_mm_getcsr();DWORD error=GetLastError();~HostState(){_mm_setcsr(fp);SetLastError(error);}};
    inline static std::atomic<NativeFieldWatch*> active_{nullptr};
    std::unique_ptr<Row[]> rows_;
    std::array<std::uintptr_t,4> addresses_{};
    CONTEXT saved_{};
    DWORD64 installedControl_=0;
    HANDLE thread_=nullptr,ready_=nullptr,clear_=nullptr;
    PVOID handler_=nullptr;
    std::thread worker_;
    DWORD owner_=0;
    unsigned count_=0;
    std::atomic<unsigned> written_{0},frame_{0};
    std::atomic<bool> recording_{false},armed_{false},overflow_{false};
    std::atomic<DWORD> failure_{0};
    static bool allowedProbe()noexcept{
        wchar_t path[32768]{};const auto size=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));
        return size && size<std::size(path) &&
            (!_wcsicmp(path,L"C:\\Users\\stoti\\Chlorium\\test-game\\GeometryDash.exe") ||
             !_wcsicmp(path,L"C:\\Users\\stoti\\Chlorium-v5-research\\build-research\\native-field-watch-r69\\watch_checks.exe"));
    }
    [[noreturn]] static void failedRecovery()noexcept{
        // A suspended owned probe must not survive with dangling exception-handler state.
        TerminateProcess(GetCurrentProcess(),0x6901);std::abort();
    }
    static LONG CALLBACK exception(PEXCEPTION_POINTERS info)noexcept{
        const auto oldError=GetLastError();const auto fp=_mm_getcsr();
        auto* self=active_.load(std::memory_order_acquire);LONG result=EXCEPTION_CONTINUE_SEARCH;
        if(self && info && info->ExceptionRecord && info->ContextRecord &&
           info->ExceptionRecord->ExceptionCode==EXCEPTION_SINGLE_STEP && GetCurrentThreadId()==self->owner_){
            auto* context=info->ContextRecord;const unsigned mask=unsigned(context->Dr6)&((1u<<self->count_)-1);
            if(mask && ((context->Dr6&15)!=mask || (context->Dr6&0xe000))){self->failure_=ERROR_BUSY;self->recording_=false;}
            else if(mask && self->armed_.load(std::memory_order_acquire)){
                if(self->recording_.load(std::memory_order_relaxed)){
                    const auto index=self->written_.fetch_add(1,std::memory_order_relaxed);
                    if(index<capacity)self->rows_[index]={context->Rip,reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress),__rdtsc(),mask,self->frame_.load(std::memory_order_relaxed)};
                    else{self->overflow_.store(true,std::memory_order_relaxed);self->recording_.store(false,std::memory_order_relaxed);}
                }
                context->Dr6&=~DWORD64(mask);context->EFlags|=0x10000;result=EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        _mm_setcsr(fp);SetLastError(oldError);return result;
    }
    bool change(bool install)noexcept{
        const auto suspended=SuspendThread(thread_);
        if(suspended==DWORD(-1)){failure_=GetLastError();return false;}
        bool result=false;
        if(!suspended){
            CONTEXT context{};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
            if(GetThreadContext(thread_,&context)){
                if(install){
                    if(!(context.Dr7&0xff) && !context.Dr0 && !context.Dr1 && !context.Dr2 && !context.Dr3){
                        saved_=context;
                        DWORD64* registers[]{&context.Dr0,&context.Dr1,&context.Dr2,&context.Dr3};
                        for(unsigned i=0;i<count_;++i){*registers[i]=addresses_[i];context.Dr7|=DWORD64(1)<<(i*2);context.Dr7&=~(DWORD64(15)<<(16+i*4));context.Dr7|=DWORD64(11)<<(16+i*4);}
                        installedControl_=context.Dr7;
                        context.Dr6&=~DWORD64(15);result=SetThreadContext(thread_,&context)!=0;
                        if(!result){failure_=GetLastError();if(!SetThreadContext(thread_,&saved_))failedRecovery();}
                    }else failure_=ERROR_BUSY;
                }else{
                    const DWORD64 registers[]{context.Dr0,context.Dr1,context.Dr2,context.Dr3};bool owns=true;
                    for(unsigned i=0;i<4;++i)owns&=registers[i]==(i<count_?addresses_[i]:0);
                    owns&=((context.Dr7^installedControl_)&0xffff00ffull)==0;
                    if(!owns || !SetThreadContext(thread_,&saved_))failedRecovery();
                    CONTEXT verify{};verify.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                    if(!GetThreadContext(thread_,&verify) || verify.Dr0!=saved_.Dr0 || verify.Dr1!=saved_.Dr1 || verify.Dr2!=saved_.Dr2 || verify.Dr3!=saved_.Dr3 ||
                       ((verify.Dr7^saved_.Dr7)&0xffff00ffull))failedRecovery();
                    result=true;
                }
            }else failure_=GetLastError();
        }else failure_=ERROR_BUSY;
        if(result)armed_.store(install,std::memory_order_release);
        if(ResumeThread(thread_)==DWORD(-1))failedRecovery();
        return result;
    }
    void release()noexcept{
        if(handler_){if(!RemoveVectoredExceptionHandler(handler_))failedRecovery();handler_=nullptr;}
        NativeFieldWatch* expected=this;active_.compare_exchange_strong(expected,nullptr);
        for(auto* handle:{&thread_,&ready_,&clear_})if(*handle){CloseHandle(*handle);*handle=nullptr;}
    }
public:
    NativeFieldWatch()=default;NativeFieldWatch(const NativeFieldWatch&)=delete;NativeFieldWatch& operator=(const NativeFieldWatch&)=delete;
    ~NativeFieldWatch(){if(!stop())failedRecovery();}
    void frame(unsigned value)noexcept{frame_.store(value,std::memory_order_relaxed);}
    bool start(std::span<const std::uintptr_t> addresses){
        HostState preserve;
        if(armed_ || worker_.joinable() || !allowedProbe() || IsDebuggerPresent() || addresses.empty() || addresses.size()>4)return false;
        for(unsigned i=0;i<addresses.size();++i){const auto address=addresses[i];MEMORY_BASIC_INFORMATION region{};
            if(!address || (address&7) || VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))!=sizeof(region) || region.State!=MEM_COMMIT ||
               (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || region.RegionSize<8 || address-reinterpret_cast<std::uintptr_t>(region.BaseAddress)>region.RegionSize-8 ||
               std::find(addresses.begin(),addresses.begin()+i,address)!=addresses.begin()+i)return false;
        }
        rows_=std::make_unique<Row[]>(capacity);written_=0;overflow_=false;failure_=0;count_=unsigned(addresses.size());owner_=GetCurrentThreadId();
        std::copy(addresses.begin(),addresses.end(),addresses_.begin());
        thread_=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,owner_);
        ready_=CreateEventW(nullptr,FALSE,FALSE,nullptr);clear_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!thread_ || GetProcessIdOfThread(thread_)!=GetCurrentProcessId() || !ready_ || !clear_){release();return false;}
        NativeFieldWatch* empty=nullptr;if(!active_.compare_exchange_strong(empty,this)){release();return false;}
        handler_=AddVectoredExceptionHandler(1,&exception);if(!handler_){release();return false;}
        recording_=true;
        try{worker_=std::thread([this]{const bool installed=change(true);SetEvent(ready_);if(installed){WaitForSingleObject(clear_,INFINITE);if(!change(false))failedRecovery();}});}
        catch(...){recording_=false;release();throw;}
        if(WaitForSingleObject(ready_,INFINITE)!=WAIT_OBJECT_0)failedRecovery();
        if(!armed_){worker_.join();release();return false;}return true;
    }
    bool stop()noexcept{
        HostState preserve;
        if(worker_.joinable()){
            if(GetCurrentThreadId()!=owner_)return false;
            recording_=false;if(!SetEvent(clear_))failedRecovery();worker_.join();
        }
        release();return !armed_;
    }
    std::span<const Row> rows()const noexcept{return armed_?std::span<const Row>{}:std::span<const Row>{rows_.get(),std::min(written_.load(),capacity)};}
    bool overflow()const noexcept{return overflow_.load();}
    DWORD failure()const noexcept{return failure_.load();}
};
static_assert(sizeof(NativeFieldWatch::Row)==32);
}
