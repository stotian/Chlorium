#pragma once
#include "../R4-34/QuadReadWatch.hpp"
#include <Geode/Geode.hpp>
#include <cstdlib>
#include <fstream>

extern "C" __declspec(dllexport) const char chloriumOpacityInvocationIdentity[]="CHLORIUM_NATIVE_OPACITY_INVOCATIONS_R37";
namespace chlorium::research::opacityinvocations {
class Observation {
    readwatch::Watch watch;
    std::uintptr_t base=0;
    bool enabled=false,attempted=false,stopped=false,written=false,intervalAborted=false;
    static std::uint64_t hash(const unsigned char* data,unsigned count) noexcept {
        std::uint64_t value=14695981039346656037ull;
        for(unsigned i=0;i<count;++i)value=(value^data[i])*1099511628211ull;
        return value;
    }
    bool nativeBodies()const noexcept {
        if(!base)return false;
        const auto bytes=reinterpret_cast<const unsigned char*>(base);
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return false;
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes+dos->e_lfanew);
        return pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&
            pe->FileHeader.TimeDateStamp==0x69702da0&&pe->OptionalHeader.SizeOfImage==0xa3a000&&
            hash(bytes+0x19ccb0,0x7d)==0xe7082c1fd4031c28ull&&hash(bytes+0x260b80,0xbe)==0xc75ba7aaf16ba44cull;
    }
public:
    void configure() noexcept {
        const auto flag=std::getenv("CHLORIUM_OPACITY_INVOCATIONS");enabled=flag&&std::string_view(flag)=="1";
        base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    }
    void end(unsigned frameCount,bool aborted) noexcept {
        if(!enabled)return;
        if((frameCount>=2||aborted)&&attempted){watch.stop();stopped=true;intervalAborted|=aborted;return;}
        if(frameCount==1&&!attempted&&!aborted){
            attempted=true;
            if(!nativeBodies())return;
            const std::array<std::uintptr_t,2> entries{base+0x19ccb0,base+0x260b80};
            watch.armExecution(entries);
        }
    }
    void finish(const char* output) {
        const bool wasArmed=watch.armed;watch.stop();if(!enabled||!output||written)return;written=true;
        matjson::Value result=matjson::Value::object();
        result["diagnostic_only"]=true;result["fps_evidence"]=false;result["native_functions_patched"]=false;
        result["scope"]="main thread, native group-opacity and group-value entries, one inter-frame interval; exception overhead excluded from FPS";
        result["native_bodies_verified_at_finish"]=nativeBodies();result["attempted"]=attempted;
        result["counts_complete"]=attempted&&stopped&&!wasArmed&&!intervalAborted&&!watch.busy&&!watch.ownershipLost&&nativeBodies()&&watch.hits[0]&&watch.hits[1];
        result["interval_aborted"]=intervalAborted;result["debugger_busy"]=watch.busy;result["ownership_lost"]=watch.ownershipLost;
        result["group_opacity_calls"]=watch.hits[0];result["group_value_calls"]=watch.hits[1];
        result["sample_capacity"]=watch.events.size();result["sample_truncated"]=watch.overflow;
        matjson::Value samples=matjson::Value::array();
        for(unsigned i=0;i<watch.count;++i){const auto& event=watch.events[i];matjson::Value row=matjson::Value::object();
            row["entry_rva"]=event.afterInstruction-base;row["slot"]=event.slots==1?0:1;
            row["owner_address"]=fmt::format("{:x}",event.rcx);
            if(event.slots==2)row["group_id_signed"]=static_cast<std::int32_t>(event.rdx);
            // Entry traps occur before any prologue; stack[0] is the real return PC.
            row["return_pc"]=fmt::format("{:x}",event.stackWords[0]);
            if(event.stackWords[0]>=base&&event.stackWords[0]-base<0xa3a000)row["caller_return_rva"]=event.stackWords[0]-base;
            samples.push(std::move(row));
        }
        result["samples"]=std::move(samples);
        std::ofstream stream(std::string(output)+".opacity-invocations.json",std::ios::out|std::ios::noreplace);
        if(!stream)throw std::runtime_error("refuse existing opacity invocation receipt");stream<<result.dump(2)<<'\n';
        if(!stream)throw std::runtime_error("write opacity invocation receipt");
    }
};
// Explicit frame cleanup; never suspend a thread from a DLL destructor.
inline Observation* observation=new Observation;
}
