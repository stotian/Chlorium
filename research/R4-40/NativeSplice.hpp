#pragma once
#include "OrderedKernel.hpp"
#include <limits>

namespace chlorium::research::maskedcorners {
class NativeSplice {
    unsigned char* page=nullptr;
    std::uintptr_t module=0;
    RUNTIME_FUNCTION function{};
    bool registered=false;
    static void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
    static std::uint64_t hash(const unsigned char* bytes,unsigned count){std::uint64_t h=14695981039346656037ull;for(unsigned i=0;i<count;++i)h=(h^bytes[i])*1099511628211ull;return h;}
public:
    static constexpr unsigned origin=0x8d66b,resume=0x8dc2a,unwindPc=0x8d665,fallback=0x80,kernelOffset=0x100,unwindOffset=0x1800;
    NativeSplice(const NativeSplice&)=delete;NativeSplice& operator=(const NativeSplice&)=delete;
    NativeSplice(const unsigned char* cocos,const OrderedKernel& kernel):module(reinterpret_cast<std::uintptr_t>(cocos)){
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(cocos);
        require(dos->e_magic==IMAGE_DOS_SIGNATURE&&dos->e_lfanew>0&&dos->e_lfanew<4096,"splice DOS identity");
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(cocos+dos->e_lfanew);
        require(pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&pe->FileHeader.TimeDateStamp==0x69702d99&&pe->OptionalHeader.SizeOfImage==0x1c5000,"splice Cocos identity");
        require(hash(cocos+0x8d570,0x719)==0x302554714c2943d2ull,"splice requires untouched native function");
        require(hash(cocos+0xc6b0,14)==0x1da34f359955329cull,"exact integer-only CCSize copy; otherwise retain native");
        DWORD64 parentBase=0;const auto parent=RtlLookupFunctionEntry(module+unwindPc,&parentBase,nullptr);
        require(parent&&parentBase==module&&parent->BeginAddress<=unwindPc&&parent->EndAddress>unwindPc,"native pre-spill unwind owner");
        // Keep all native unwind RVAs relative to the original module, including chained/EH data.
        SYSTEM_INFO system{};GetSystemInfo(&system);const auto gran=static_cast<std::uintptr_t>(system.dwAllocationGranularity);
        const auto low=(module+pe->OptionalHeader.SizeOfImage+gran-1)&~(gran-1);
        const auto high=module+0x7fff0000ull;
        for(auto address=low;address<high;){
            MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)))break;
            const auto end=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
            if(end<=address)break;
            if(info.State==MEM_FREE&&end-address>=0x2000){
                page=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(address),0x2000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
                if(page)break;
            }
            address=(end+gran-1)&~(gran-1);
        }
        require(page!=nullptr,"no nearby executable region; retain native arithmetic");
        try {
            std::array<unsigned char,0x2000> bytes;bytes.fill(0xcc);unsigned at=0;
            auto emit=[&](std::initializer_list<unsigned char> list){for(auto byte:list)bytes.at(at++)=byte;};
            auto relative=[&](std::uintptr_t target){const auto delta=static_cast<std::int64_t>(target)-static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(page)+at+4);
                require(delta>=INT32_MIN&&delta<=INT32_MAX,"bounded splice branch");const auto value=static_cast<std::int32_t>(delta);std::memcpy(bytes.data()+at,&value,4);at+=4;};
            emit({0x0f,0xae,0x5c,0x24,0x10,0x8b,0x44,0x24,0x10,0x25,0x80,0x1f,0,0,0x3d,0x80,0x1f,0,0,0x0f,0x85});relative(reinterpret_cast<std::uintptr_t>(page)+fallback);
            emit({0x80,0xbf,0x30,0x02,0,0,0,0x0f,0x85});relative(reinterpret_cast<std::uintptr_t>(page)+fallback);
            fastOffset=at;
            // Exact native CCSize copy, same two reads/writes; no call, FP work or other state exists in its gated body.
            emit({0x8b,0x87,0xb0,0x01,0,0,0x89,0x44,0x24,0x30,0x8b,0x87,0xb4,0x01,0,0,0x89,0x44,0x24,0x34});
            emit({0x48,0x8b,0x87,0xbc,0x01,0,0,0x48,0x89,0x04,0x24,0x48,0x8b,0x44,0x24,0x30,0x48,0x89,0x44,0x24,0x08});
            emit({0x48,0x8d,0x8f,0x7c,0x01,0,0,0x48,0x8d,0x14,0x24,0x44,0x8b,0x47,0x48,0x4c,0x8d,0x8f,0xcc,0x01,0,0,0xe9});relative(reinterpret_cast<std::uintptr_t>(page)+kernelOffset);
            require(at<=fallback,"splice dispatcher bound");at=fallback;
            std::memcpy(bytes.data()+at,cocos+origin,8);at+=8;emit({0xe9});relative(module+origin+8);
            std::memcpy(bytes.data()+kernelOffset,kernel.image(),4096);at=kernelOffset+kernel.returnOffset();emit({0xe9});relative(module+resume);
            const auto codeEnd=at;
            bytes[unwindOffset]=1|(UNW_FLAG_CHAININFO<<3);bytes[unwindOffset+1]=0;bytes[unwindOffset+2]=0;
            bytes[unwindOffset+3]=cocos[parent->UnwindData+3];std::memcpy(bytes.data()+unwindOffset+4,parent,sizeof(*parent));
            const auto rva=reinterpret_cast<std::uintptr_t>(page)-module;
            require(rva+bytes.size()<UINT32_MAX,"splice RVA domain");
            function={static_cast<DWORD>(rva),static_cast<DWORD>(rva+codeEnd),static_cast<DWORD>(rva+unwindOffset)};
            std::memcpy(page,bytes.data(),bytes.size());DWORD old=0;
            require(VirtualProtect(page,bytes.size(),PAGE_EXECUTE_READ,&old)&&FlushInstructionCache(GetCurrentProcess(),page,bytes.size()),"splice W then RX");
            require(RtlAddFunctionTable(&function,1,module)!=FALSE,"register native-frame splice unwind");registered=true;
            DWORD64 actualBase=0;const auto actual=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(page)+fastOffset,&actualBase,nullptr);
            require(actual&&actualBase==module&&actual->UnwindData==function.UnwindData,"verify splice unwind registration");
        }catch(...){if(registered&&!RtlDeleteFunctionTable(&function))TerminateProcess(GetCurrentProcess(),94);VirtualFree(page,0,MEM_RELEASE);page=nullptr;throw;}
    }
    unsigned fastOffset=0;
    const unsigned char* address()const noexcept{return page;}
    std::array<unsigned char,8> patchBytes()const{
        std::array<unsigned char,8> result{0xe9,0,0,0,0,0x90,0x90,0x90};const auto delta=static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(page))-static_cast<std::int64_t>(module+origin+5);
        require(delta>=INT32_MIN&&delta<=INT32_MAX,"native splice reach");const auto value=static_cast<std::int32_t>(delta);std::memcpy(result.data()+1,&value,4);return result;
    }
    ~NativeSplice(){if(registered&&!RtlDeleteFunctionTable(&function))TerminateProcess(GetCurrentProcess(),94);if(page)VirtualFree(page,0,MEM_RELEASE);}
};
}
