#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <processsnapshot.h>
#include <psapi.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstddef>

// Diagnostic only: Windows snapshots, no injected code or explicit thread suspension.
// https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-psscapturesnapshot
namespace {
using Clock=std::chrono::steady_clock;
unsigned cleanupFailures=0;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string hashBytes(const unsigned char* data,ULONG size) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"open SHA256 provider");
    struct Close{BCRYPT_ALG_HANDLE handle;~Close(){if(BCryptCloseAlgorithmProvider(handle,0)<0)++cleanupFailures;}} close{algorithm};
    std::array<unsigned char,32> digest{};
    require(BCryptHash(algorithm,nullptr,0,const_cast<unsigned char*>(data),size,digest.data(),static_cast<ULONG>(digest.size()))>=0,"hash source bytes");
    constexpr char hex[]="0123456789abcdef";std::string result(64,'0');
    for(unsigned n=0;n<digest.size();++n){result[n*2]=hex[digest[n]>>4];result[n*2+1]=hex[digest[n]&15];}
    return result;
}
std::string hashFile(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);require(size<=64*1024*1024,"fingerprint file exceeds budget");
    std::ifstream file(path,std::ios::binary);require(bool(file),"read fingerprint source");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    require(bool(file),"complete fingerprint read");return hashBytes(bytes.data(),static_cast<ULONG>(bytes.size()));
}
std::uint64_t ticks(FILETIME value){return (std::uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
struct Thread {DWORD id;std::uint64_t created,cpu,rip;WORD suspended;};
struct Snapshot {
    HPSS handle=nullptr;HPSSWALK marker=nullptr;
    explicit Snapshot(HANDLE process) {
        const auto flags=static_cast<PSS_CAPTURE_FLAGS>(PSS_CAPTURE_THREADS|PSS_CAPTURE_THREAD_CONTEXT);
        const auto error=PssCaptureSnapshot(process,flags,CONTEXT_CONTROL|CONTEXT_INTEGER,&handle);
        if(error)throw std::runtime_error("PssCaptureSnapshot error "+std::to_string(error));
        const auto walkError=PssWalkMarkerCreate(nullptr,&marker);
        if(walkError){PssFreeSnapshot(GetCurrentProcess(),handle);handle=nullptr;throw std::runtime_error("PssWalkMarkerCreate error "+std::to_string(walkError));}
    }
    Snapshot(const Snapshot&)=delete;
    ~Snapshot(){if(marker&&PssWalkMarkerFree(marker))++cleanupFailures;if(handle&&PssFreeSnapshot(GetCurrentProcess(),handle))++cleanupFailures;}
    std::vector<Thread> threads(DWORD pid) {
        std::vector<Thread> result;PSS_THREAD_ENTRY entry{};DWORD error=0;
        while((error=PssWalkSnapshot(handle,PSS_WALK_THREADS,marker,&entry,sizeof(entry)))==ERROR_SUCCESS) {
            if(entry.ProcessId!=pid||entry.ExitStatus!=STILL_ACTIVE)continue;
            if(!entry.ContextRecord||entry.SizeOfContextRecord<offsetof(CONTEXT,Rip)+sizeof(DWORD64))continue;
            result.push_back({entry.ThreadId,ticks(entry.CreateTime),ticks(entry.UserTime)+ticks(entry.KernelTime),entry.ContextRecord->Rip,entry.SuspendCount});
        }
        require(error==ERROR_NO_MORE_ITEMS,"snapshot walk failed");return result;
    }
};
bool fixture(HANDLE process) {
    std::array<wchar_t,32768> path{};DWORD size=static_cast<DWORD>(path.size());
    require(QueryFullProcessImageNameW(process,0,path.data(),&size)!=0,"query target executable");
    return _wcsicmp(path.data(),L"C:\\Users\\stoti\\Chlorium\\test-game\\GeometryDash.exe")==0;
}
struct Module {std::uintptr_t base;DWORD size;std::string name;};
template<class T>T mappedValue(HANDLE process,const Module& module,std::size_t rva) {
    require(rva<=module.size&&sizeof(T)<=module.size-rva,"mapped export is outside module bounds");
    T value{};SIZE_T read=0;
    require(ReadProcessMemory(process,reinterpret_cast<const void*>(module.base+rva),&value,sizeof(value),&read)&&read==sizeof(value),"read mapped diagnostic export");
    return value;
}
DWORD samplingGate(HANDLE process,const Module& module) {
    const auto dos=mappedValue<IMAGE_DOS_HEADER>(process,module,0);
    require(dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0,"diagnostic module DOS header");
    const auto pe=mappedValue<IMAGE_NT_HEADERS64>(process,module,static_cast<std::size_t>(dos.e_lfanew));
    require(pe.Signature==IMAGE_NT_SIGNATURE&&pe.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&pe.FileHeader.SizeOfOptionalHeader==sizeof(IMAGE_OPTIONAL_HEADER64)&&pe.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC&&pe.OptionalHeader.SizeOfImage==module.size&&pe.OptionalHeader.NumberOfRvaAndSizes>IMAGE_DIRECTORY_ENTRY_EXPORT,"diagnostic module PE identity");
    const auto directory=pe.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    require(directory.VirtualAddress&&directory.Size>=sizeof(IMAGE_EXPORT_DIRECTORY)&&directory.VirtualAddress<=module.size&&directory.Size<=module.size-directory.VirtualAddress,"diagnostic export directory bounds");
    const auto exports=mappedValue<IMAGE_EXPORT_DIRECTORY>(process,module,directory.VirtualAddress);
    require(exports.AddressOfNames&&exports.AddressOfNameOrdinals&&exports.AddressOfFunctions&&exports.NumberOfNames<=4096&&exports.NumberOfFunctions<=4096,"diagnostic export count budget");
    constexpr char expected[]="chloriumExternalSamplingSessionMode";
    for(DWORD n=0;n<exports.NumberOfNames;++n) {
        const auto at=mappedValue<DWORD>(process,module,std::size_t(exports.AddressOfNames)+n*sizeof(DWORD));
        if(at>module.size||sizeof(expected)>module.size-at)continue;
        const auto name=mappedValue<std::array<char,sizeof(expected)>>(process,module,at);
        if(std::memcmp(name.data(),expected,sizeof(expected)))continue;
        const auto ordinal=mappedValue<WORD>(process,module,std::size_t(exports.AddressOfNameOrdinals)+n*sizeof(WORD));
        require(ordinal<exports.NumberOfFunctions,"diagnostic export ordinal");
        const auto rva=mappedValue<DWORD>(process,module,std::size_t(exports.AddressOfFunctions)+ordinal*sizeof(DWORD));
        require(!(rva>=directory.VirtualAddress&&rva-directory.VirtualAddress<directory.Size),"forwarded diagnostic data export rejected");
        require(mappedValue<unsigned>(process,module,rva)==0x53414d50u,"target is not an active sampling diagnostic; refusing normal FPS runs");
        return rva;
    }
    throw std::runtime_error("target has no sampling diagnostic session export");
}
std::vector<Module> modules(HANDLE process) {
    std::array<HMODULE,1024> handles{};DWORD bytes=0;
    require(EnumProcessModulesEx(process,handles.data(),sizeof(handles),&bytes,LIST_MODULES_64BIT)&&bytes<=sizeof(handles),"enumerate target modules");
    std::vector<Module> result;
    for(unsigned n=0;n<bytes/sizeof(HMODULE);++n) {
        MODULEINFO info{};std::array<char,MAX_PATH> name{};
        require(GetModuleInformation(process,handles[n],&info,sizeof(info))&&GetModuleBaseNameA(process,handles[n],name.data(),static_cast<DWORD>(name.size())),"query module mapping");
        result.push_back({reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll),info.SizeOfImage,name.data()});
    }
    return result;
}
void selfTest() {
    require(hashBytes(reinterpret_cast<const unsigned char*>("abc"),3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 known vector");
    std::atomic<bool> stop=false;std::atomic<std::uint64_t> work=0;
    std::jthread worker([&]{while(!stop.load(std::memory_order_relaxed))work.fetch_add(1,std::memory_order_relaxed);});
    struct Stop {std::atomic<bool>& flag;~Stop(){flag=true;}} cleanup{stop};
    while(!work.load())Sleep(1);
    const auto workerId=GetThreadId(worker.native_handle());
    {Snapshot warm(GetCurrentProcess());const auto rows=warm.threads(GetCurrentProcessId());require(!rows.empty(),"self snapshot has threads");
        require(std::min_element(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.created<b.created;})->id==GetCurrentThreadId(),"earliest thread selects the real main thread");}
    DWORD before=0,after=0;require(GetProcessHandleCount(GetCurrentProcess(),&before)!=0,"handle counter");
    for(unsigned n=0;n<20;++n) {
        Snapshot shot(GetCurrentProcess());const auto rows=shot.threads(GetCurrentProcessId());
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const Thread& t){return t.id==workerId;});
        require(found!=rows.end()&&found->rip&&found->suspended==0,"worker context copied and not left suspended");
    }
    require(GetProcessHandleCount(GetCurrentProcess(),&after)&&before==after&&cleanupFailures==0,"all snapshot/marker handles released");
    const auto previous=work.load();Sleep(2);require(work.load()>previous,"worker keeps running after snapshots");
    require(!fixture(GetCurrentProcess()),"own process cannot be mistaken for GD fixture");
    stop=true;worker.join();std::cout<<"PASS 20 thread-context snapshots; exact handle balance; worker continues; non-fixture rejected\n";
}
void sample(DWORD pid,unsigned duration,unsigned interval,const std::filesystem::path& output,const std::string& expectedAuditHash) {
    require(duration>=100&&duration<=20000&&interval>=5&&interval<=100,"bounded sampling duration/interval");
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_DUP_HANDLE,FALSE,pid);
    require(process!=nullptr,"open dedicated test process");
    struct Close{HANDLE handle;~Close(){CloseHandle(handle);}} close{process};
    require(fixture(process),"refusing every process except the exact GD test fixture");
    const auto map=modules(process);DWORD mainThread=0;std::uint64_t mainCreated=0;
    const auto audit=std::find_if(map.begin(),map.end(),[](const Module& module){return _stricmp(module.name.c_str(),"local.chlorium.audit.dll")==0;});
    require(audit!=map.end(),"no mapped diagnostic auditor");
    const auto gate=samplingGate(process,*audit);
    struct Image {std::string name,path,sha256;};std::vector<Image> images;
    for(const auto& module:map)if(_stricmp(module.name.c_str(),"GeometryDash.exe")==0||_stricmp(module.name.c_str(),"libcocos2d.dll")==0||module.base==audit->base) {
        std::array<wchar_t,32768> actual{};
        const auto length=GetModuleFileNameExW(process,reinterpret_cast<HMODULE>(module.base),actual.data(),static_cast<DWORD>(actual.size()));
        require(length&&length<actual.size(),"query complete mapped source image path");
        const auto expected=std::filesystem::path(L"C:\\Users\\stoti\\Chlorium\\test-game")/module.name;
        if(module.base!=audit->base)require(_wcsicmp(actual.data(),expected.c_str())==0,"source image is outside the exact test fixture");
        const auto path=std::filesystem::path(actual.data());const auto digest=hashFile(path);
        if(module.base==audit->base)require(digest==expectedAuditHash,"loaded auditor disk fingerprint differs from the selected build");
        images.push_back({module.name,path.generic_string(),digest});
    }
    require(images.size()==3,"game, Cocos and auditor source identities required");
    {Snapshot shot(process);const auto rows=shot.threads(pid);require(!rows.empty(),"no live thread contexts");const auto first=std::min_element(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.created<b.created;});mainThread=first->id;mainCreated=first->created;}
    require(!std::filesystem::exists(output),"refusing an existing sampler output");
    const auto statsPath=std::filesystem::path(output.wstring()+L".stats.json");
    require(!std::filesystem::exists(statsPath),"refusing existing sampler statistics");
    std::ofstream csv(output);require(bool(csv),"open sample output");
    csv<<"sample,elapsed_ms,snapshot_ms,thread_cpu_delta_ms,module,rva,suspend_count\n";
    const auto start=Clock::now();unsigned count=0,missing=0;std::uint64_t previousCpu=0;double overhead=0,worst=0;const char* stopReason="duration";
    while(Clock::now()-start<std::chrono::milliseconds(duration)) {
        DWORD exitCode=0;require(GetExitCodeProcess(process,&exitCode)!=0,"query target exit");if(exitCode!=STILL_ACTIVE){stopReason="process_exited";break;}
        const auto begin=Clock::now();std::vector<Thread> rows;
        try {
            require(mappedValue<unsigned>(process,*audit,gate)==0x53414d50u,"sampling session changed; no further captures permitted");
            Snapshot shot(process);rows=shot.threads(pid);
        }
        catch(...) {
            // The authorized run can finish between the exit query and capture.
            DWORD current=STILL_ACTIVE;
            if(GetExitCodeProcess(process,&current)&&current!=STILL_ACTIVE){stopReason="process_exited";break;}
            throw;
        }
        const auto end=Clock::now();const auto cost=std::chrono::duration<double,std::milli>(end-begin).count();overhead+=cost;worst=std::max(worst,cost);
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const Thread& t){return t.id==mainThread&&t.created==mainCreated;});
        if(found==rows.end()){++missing;stopReason="main_thread_missing";break;}
        const auto module=std::find_if(map.begin(),map.end(),[&](const Module& m){return found->rip>=m.base&&found->rip-m.base<m.size;});
        csv<<count++<<','<<std::chrono::duration<double,std::milli>(end-start).count()<<','<<cost<<','<<(previousCpu?double(found->cpu-previousCpu)/10000:0)<<',';
        if(module!=map.end())csv<<module->name<<",0x"<<std::hex<<found->rip-module->base<<std::dec;else csv<<"unmapped,0x"<<std::hex<<found->rip<<std::dec;
        csv<<','<<found->suspended<<'\n';previousCpu=found->cpu;
        // Bound observer cost; this run is diagnostic, never a performance comparison.
        if(count>=10&&overhead>std::chrono::duration<double,std::milli>(end-start).count()*.25){stopReason="observer_budget";break;}
        std::this_thread::sleep_until(begin+std::chrono::milliseconds(interval));
    }
    csv.flush();require(bool(csv)&&count&&cleanupFailures==0,"sampling incomplete or cleanup failed");
    csv.close();require(bool(csv),"close sample output");
    std::ofstream stats(statsPath);require(bool(stats),"open sampler statistics");
    stats<<"{\"diagnostic_only\":true,\"auditor_session_verified\":true,\"pid\":"<<pid<<",\"main_thread\":"<<mainThread
        <<",\"samples\":"<<count<<",\"missing\":"<<missing<<",\"snapshot_total_ms\":"<<overhead
        <<",\"snapshot_worst_ms\":"<<worst<<",\"requested_duration_ms\":"<<duration
        <<",\"interval_ms\":"<<interval<<",\"cleanup_failures\":"<<cleanupFailures
        <<",\"elapsed_ms\":"<<std::chrono::duration<double,std::milli>(Clock::now()-start).count()
        <<",\"stop_reason\":\""<<stopReason<<"\",\"sample_file_sha256\":\""<<hashFile(output)<<"\",\"images\":{";
    for(std::size_t n=0;n<images.size();++n){if(n)stats<<',';const auto& image=images[n];stats<<'"'<<image.name<<"\":{\"path\":\""<<image.path<<"\",\"sha256\":\""<<image.sha256<<"\"}";}
    stats<<"}}\n";
    stats.flush();require(bool(stats),"write sampler statistics");
    std::cout<<"DIAGNOSTIC samples="<<count<<" missing="<<missing<<" capture_ms="<<overhead<<" worst_capture_ms="<<worst<<"; leaf addresses, not call stacks or an FPS result\n";
}
}
int wmain(int argc,wchar_t** argv)try {
    if(argc==2&&std::wstring(argv[1])==L"--self-test"){selfTest();return 0;}
    require(argc==6,"PID DURATION_MS INTERVAL_MS OUTPUT.csv EXPECTED_AUDITOR_DLL_SHA256 or --self-test");
    const auto parse=[](const wchar_t* text){std::size_t used=0;const auto value=std::stoull(text,&used);require(text[used]==L'\0',"numeric argument contains extra text");return value;};
    const auto pid=parse(argv[1]),duration=parse(argv[2]),interval=parse(argv[3]);
    require(pid&&pid<=MAXDWORD&&duration<=20000&&interval<=100,"numeric input bounds");
    const std::wstring hash=argv[5];require(hash.size()==64,"expected auditor SHA256 length");std::string expected;
    for(const auto c:hash){require((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f')||(c>=L'A'&&c<=L'F'),"expected auditor SHA256 syntax");expected+=static_cast<char>(c>=L'A'&&c<=L'F'?c+32:c);}
    sample(static_cast<DWORD>(pid),static_cast<unsigned>(duration),static_cast<unsigned>(interval),argv[4],expected);return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
