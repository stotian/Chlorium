#pragma once
#include "QuadReadWatch.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>

extern "C" __declspec(dllexport) const char chloriumNativeQuadReadWatchIdentity[]="CHLORIUM_NATIVE_QUAD_READ_WATCH_DIAGNOSTIC_V1";
namespace chlorium::research::readwatch {
class Observation {
    Watch watch;
    std::uintptr_t sprite=0,root=0;
    unsigned uid=0,rootUid=0;
    bool enabled=false,attempted=false,seenAgain=false,stopped=false,written=false;
public:
    void configure() noexcept {const auto flag=std::getenv("CHLORIUM_QUAD_READ_WATCH");enabled=flag&&std::string_view(flag)=="1";}
    void consider(cocos2d::CCSprite* object,const pass::SpriteRecord& source,unsigned count,unsigned frameCount) noexcept {
        if(!enabled)return;
        if(frameCount==2&&source.sprite==sprite&&source.id==uid)seenAgain=true;
        if(frameCount!=1||attempted||count<256||source.root==source.sprite||!(source.rootFlags&0x80000000u)||
           !(source.rootFlags&1u)||!source.groupCount||(source.flags&193u)||!(source.flags&512u)||!(source.flags&1024u)||
           (object->m_pChildren&&object->m_pChildren->count()))return;
        // No retain or later pointer dereference: disappearance invalidates the observation.
        sprite=source.sprite;uid=source.id;root=source.root;rootUid=source.rootId;
    }
    void end(unsigned frameCount,bool aborted) noexcept {
        if(!enabled)return;
        if((frameCount>=2||aborted)&&attempted){watch.stop();stopped=true;return;}
        if(frameCount==1&&!attempted&&sprite&&!aborted){
            attempted=true;std::array<std::uintptr_t,4> addresses{};
            for(unsigned i=0;i<4;++i)addresses[i]=sprite+0x1cc+i*sizeof(snapshot::NativeQuadVertex);
            if(!watch.arm(addresses))stopped=true;
        }
    }
    void finish(const char* output) {
        watch.stop();if(!enabled||!output||written)return;written=true;
        matjson::Value result=matjson::Value::object();result["diagnostic_only"]=true;
        result["fps_evidence"]=false;result["all_cpu_consumers_proven"]=false;
        result["attempted"]=attempted;result["stopped_at_frame_end"]=stopped;
        result["owner_seen_again"]=seenAgain;result["overflow"]=watch.overflow;result["debugger_busy"]=watch.busy;result["ownership_lost"]=watch.ownershipLost;
        result["observation_complete"]=attempted&&stopped&&seenAgain&&watch.count&&!watch.overflow&&!watch.busy&&!watch.ownershipLost;
        result["sprite"]=fmt::format("{:x}",sprite);result["sprite_uid"]=uid;result["root"]=fmt::format("{:x}",root);result["root_uid"]=rootUid;
        result["scope"]="main thread, four X coordinates, one sprite, one inter-frame interval; no lifetime extension";
        matjson::Value events=matjson::Value::array();
        for(unsigned i=0;i<watch.count;++i){const auto& event=watch.events[i];matjson::Value row=matjson::Value::object();
            row["after_instruction"]=fmt::format("{:x}",event.afterInstruction);row["slots"]=event.slots;
            std::array<unsigned char,15> preceding{};SIZE_T copied=0;
            const bool readable=event.afterInstruction>=preceding.size()&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(event.afterInstruction-preceding.size()),preceding.data(),preceding.size(),&copied)&&copied==preceding.size();
            if(readable){std::string hex;for(auto value:preceding)hex+=fmt::format("{:02x}",value);row["preceding_bytes_at_finish"]=std::move(hex);}
            matjson::Value stack=matjson::Value::array();
            for(auto address:event.stackWords){HMODULE owner=nullptr;
                if(address&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&owner)){
                    wchar_t name[MAX_PATH]{};const auto length=GetModuleFileNameW(owner,name,MAX_PATH);
                    if(length&&length<MAX_PATH){matjson::Value item=matjson::Value::object();item["module"]=std::filesystem::path(std::wstring(name,length)).filename().string();item["rva"]=address-reinterpret_cast<std::uintptr_t>(owner);stack.push(std::move(item));}
                }
            }
            row["raw_stack_module_candidates_not_unwound"]=std::move(stack);
            HMODULE module=nullptr;
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(event.afterInstruction),&module)){
                wchar_t name[32768]{};const auto length=GetModuleFileNameW(module,name,static_cast<DWORD>(std::size(name)));
                row["module"]=length&&length<std::size(name)?std::filesystem::path(std::wstring(name,length)).filename().string():"unknown";
                if(length&&length<std::size(name))row["module_file"]=std::filesystem::path(std::wstring(name,length)).string();
                row["rva_after"]=event.afterInstruction-reinterpret_cast<std::uintptr_t>(module);
            }else row["module"]="unknown";
            events.push(std::move(row));
        }
        result["events"]=std::move(events);
        std::ofstream stream(std::string(output)+".quad-reads.json",std::ios::out|std::ios::noreplace);
        if(!stream)throw std::runtime_error("refuse existing native read observation");stream<<result.dump(2)<<'\n';
        if(!stream)throw std::runtime_error("write native read observation");
    }
};
// Process-lifetime diagnostic: explicit frame cleanup must never wait under DLL teardown.
inline Observation* observation=new Observation;
}
