#pragma once
#include "../R4-34/QuadReadWatch.hpp"
#include <Geode/Geode.hpp>
#include <chrono>
#include <fstream>

extern "C" __declspec(dllexport) const char chloriumStartupDeathWatchIdentity[]="CHLORIUM_STARTUP_DEATH_WATCH_R45";
namespace chlorium::research::startupdeath {
class Observation {
    readwatch::Watch watch;
    PlayLayer* play=nullptr;std::uintptr_t base=0;
    bool enabled=false,attempted=false,stopped=false,written=false,aborted=false;
    std::chrono::steady_clock::time_point deadline{};
    matjson::Value cause=matjson::Value::object();
    static std::uint64_t hash(const unsigned char* data,unsigned size){std::uint64_t value=14695981039346656037ull;for(unsigned i=0;i<size;++i)value=(value^data[i])*1099511628211ull;return value;}
    bool nativeBodies()const{
        const auto p=reinterpret_cast<const unsigned char*>(base);if(!p)return false;
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(p);if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return false;
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(p+dos->e_lfanew);
        return pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&pe->FileHeader.TimeDateStamp==0x69702da0&&pe->OptionalHeader.SizeOfImage==0xa3a000&&
            hash(p+0x3b39d0,0xb0)==0xf87bc643572dc49eull&&hash(p+0x3a7a20,0x50)==0xd9dcf8d773cdf1e8ull&&hash(p+0x3b8eb0,0x40)==0xf14eb0a94ec15e82ull;
    }
    void snapshotCause(){
        for(unsigned i=0;i<watch.count;++i){const auto& event=watch.events[i];if(event.slots!=1||event.rcx!=reinterpret_cast<std::uintptr_t>(play))continue;
            cause["argument_null"]=event.r8==0;cause["matched_live_object"]=false;
            if(PlayLayer::get()!=play||!play->m_objects||!play->m_objects->data||play->m_objects->data->num>1000000)return;
            cause["game_time_at_end_of_death_frame"]=play->m_gameState.m_totalTime;
            for(unsigned j=0;j<play->m_objects->data->num;++j){auto* object=static_cast<GameObject*>(play->m_objects->data->arr[j]);if(reinterpret_cast<std::uintptr_t>(object)!=event.r8||!object)continue;
                cause["matched_live_object"]=true;cause["uid"]=object->m_uID;cause["object_id"]=object->m_objectID;cause["object_type"]=static_cast<int>(object->m_objectType);
                cause["x_at_end_of_death_frame"]=object->m_positionX;cause["y_at_end_of_death_frame"]=object->m_positionY;cause["start_x"]=object->m_startPosition.x;cause["start_y"]=object->m_startPosition.y;
                return;
            }return;
        }
    }
public:
    void configure(PlayLayer* owner)noexcept{const auto flag=std::getenv("CHLORIUM_STARTUP_DEATH_WATCH");enabled=flag&&std::string_view(flag)=="1";play=owner;base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
    void end(unsigned frameCount,bool invalid)noexcept{
        if(!enabled||stopped)return;
        const auto error=GetLastError();struct Restore{DWORD saved;~Restore(){SetLastError(saved);}}restore{error};
        if(attempted){if(watch.hits[0]||invalid||std::chrono::steady_clock::now()>=deadline){watch.stop();stopped=true;aborted=invalid;try{snapshotCause();}catch(...){aborted=true;}}return;}
        if(frameCount==1&&!invalid){attempted=true;if(!nativeBodies()){stopped=true;return;}const std::array<std::uintptr_t,3> addresses{base+0x3b39d0,base+0x3a7a20,base+0x3b8eb0};if(!watch.armExecution(addresses)){stopped=true;return;}deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);}
    }
    void finish(const char* output){
        const auto stillArmed=watch.armed;watch.stop();if(!enabled||!output||written)return;written=true;
        auto result=matjson::Value::object();result["diagnostic_only"]=true;result["game_fps_evidence"]=false;result["native_functions_patched"]=false;result["attempted"]=attempted;result["stopped"]=stopped;result["stopped_before_finish"]=!stillArmed;result["aborted"]=aborted;result["native_bodies_verified"]=nativeBodies();result["debugger_busy"]=watch.busy;result["ownership_lost"]=watch.ownershipLost;result["overflow"]=watch.overflow;result["cause"]=cause;
        auto events=matjson::Value::array();for(unsigned i=0;i<watch.count;++i){const auto& event=watch.events[i];auto row=matjson::Value::object();row["entry_rva"]=event.afterInstruction-base;row["slots"]=event.slots;row["layer"]=event.rcx;row["player_or_second_argument"]=event.rdx;row["third_argument"]=event.r8;row["return_pc"]=event.stackWords[0];if(event.stackWords[0]>=base&&event.stackWords[0]-base<0xa3a000)row["caller_return_rva"]=event.stackWords[0]-base;events.push(std::move(row));}result["events"]=std::move(events);
        std::ofstream stream(std::string(output)+".startup-death.json",std::ios::out|std::ios::noreplace);if(!stream)throw std::runtime_error("new startup death receipt only");stream<<result.dump(2)<<'\n';if(!stream)throw std::runtime_error("complete startup death receipt");
    }
};
inline Observation* observation=new Observation;
}
