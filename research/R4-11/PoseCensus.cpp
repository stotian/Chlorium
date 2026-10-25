// Passive research adapter: every native function/callback still runs.
#ifndef CHLORIUM_POSE_CENSUS
#error Explicit isolated pose census build required
#endif
#include "PoseProgram.hpp"
#include "renderer/NearCallSlot.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
using namespace geode::prelude;
namespace {
namespace pose=chlorium::research::pose;
struct Sample {
    unsigned frame=0;
    double percent=0,ms=0;
    std::uint64_t phases=0,calls=0,measured=0,skipped=0,inputUnique=0,inputReuse=0,inputOverflow=0,
        resultUnique=0,resultReuse=0,resultOverflow=0,nested=0,missing=0;
};
std::array<Sample,32> samples{};
unsigned used=0;
bool written=false,installed=false;
thread_local Sample* active=nullptr;
thread_local pose::Recipes<9,8192> inputRecipes;
thread_local pose::Recipes<5,8192> resultRecipes;
pose::Concat volatile* imported=nullptr;
void* callSlot=nullptr;
std::uint64_t fnv(const unsigned char* bytes,std::size_t count) {
    std::uint64_t value=14695981039346656037ull;
    for(std::size_t n=0;n<count;++n) value=(value^bytes[n])*1099511628211ull;
    return value;
}
pose::Matrix* observedConcat(pose::Matrix* output,const pose::Matrix* left,const pose::Matrix* right) {
    // Read the live import on every call, preserving other mods' forwarding.
    const auto native=*imported;
    if(!active) return native(output,left,right);
    ++active->calls;
    if(active->measured>=4096) { ++active->skipped;return native(output,left,right); }
    ++active->measured;
    const auto controls=_mm_getcsr()&0xffc0u;
    pose::Recipes<9,8192>::Key input{
        std::bit_cast<std::uint32_t>(left->a),std::bit_cast<std::uint32_t>(left->b),
        std::bit_cast<std::uint32_t>(left->c),std::bit_cast<std::uint32_t>(left->d),
        std::bit_cast<std::uint32_t>(right->a),std::bit_cast<std::uint32_t>(right->b),
        std::bit_cast<std::uint32_t>(right->c),std::bit_cast<std::uint32_t>(right->d),controls};
    const auto result=native(output,left,right);
    const auto recipe=inputRecipes.intern(input);
    if(!recipe.stored) ++active->inputOverflow;
    else if(recipe.hit) ++active->inputReuse; else ++active->inputUnique;
    if(!result) { ++active->missing;return result; }
    const pose::Recipes<5,8192>::Key key{
        std::bit_cast<std::uint32_t>(result->a),std::bit_cast<std::uint32_t>(result->b),
        std::bit_cast<std::uint32_t>(result->c),std::bit_cast<std::uint32_t>(result->d),controls};
    const auto repeat=resultRecipes.intern(key);
    if(!repeat.stored) ++active->resultOverflow;
    else if(repeat.hit) ++active->resultReuse; else ++active->resultUnique;
    return result;
}
void write() {
    if(written||!used) return;
    const auto path=std::getenv("CHLORIUM_AUDIT_OUTPUT");if(!path) return;
    written=true;std::ofstream out(std::string(path)+".pose-work.jsonl",std::ios::trunc);
    for(unsigned n=0;n<used;++n) {
        const auto& s=samples[n];
        out<<matjson::makeObject({{"schema",1},{"director_frame",s.frame},{"percent",s.percent},
            {"phases",s.phases},{"actual_concat_calls",s.calls},{"measured_calls",s.measured},
            {"budget_skipped_calls",s.skipped},{"per_frame_budget",4096},{"input_recipes",s.inputUnique},
            {"input_reused",s.inputReuse},{"input_overflow",s.inputOverflow},
            {"result_recipes",s.resultUnique},{"result_reused",s.resultReuse},
            {"result_overflow",s.resultOverflow},{"nested_barriers",s.nested},{"missing_results",s.missing},
            {"inclusive_diagnostic_ms",s.ms},{"native_callbacks_preserved",true},
            {"diagnostic_only",true},{"phase_callback_purity_proven",false}}).dump(matjson::NO_INDENTATION)<<'\n';
    }
    out.flush();if(!out) log::error("Pose census output incomplete");
}
void install() {
    const auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0) return;
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp!=0x69702da0 ||
       pe->OptionalHeader.SizeOfImage!=0xa3a000 || fnv(base+0x229740,0x642)!=0x6680410ab39143d4ull) {
        log::warn("Pose census: unknown Area routine, no patch");return;
    }
    const unsigned char expected[]{0xff,0x15,0x56,0xcb,0x2e,0x00};
    if(std::memcmp(base+0x229a6c,expected,sizeof(expected))) return;
    imported=reinterpret_cast<pose::Concat volatile*>(base+0x5165c8);
    if(!*imported) return;
    const std::array<std::uintptr_t,1> sites{reinterpret_cast<std::uintptr_t>(base+0x229a6c)};
    callSlot=chlorium::matrix::allocateNearCallSlot(sites,reinterpret_cast<std::uintptr_t>(&observedConcat),
        reinterpret_cast<std::uintptr_t>(base)+0xa3a000);
    if(!callSlot) return;
    ByteVector replacement{0xff,0x15,0,0,0,0};
    const auto displacement=std::int32_t(reinterpret_cast<std::uintptr_t>(callSlot)-(sites[0]+6));
    std::memcpy(replacement.data()+2,&displacement,4);
    const auto patch=Mod::get()->patch(base+0x229a6c,std::move(replacement));
    if(!patch) { VirtualFree(callSlot,0,MEM_RELEASE);callSlot=nullptr;return; }
    installed=true;
    log::info("Passive phase-local pose census installed; native concat/callbacks retained");
}
}
extern "C" __declspec(dllexport) void ChloriumFlushAreaWorkCensus() { write(); }
$execute { install(); }
class $modify(ChloriumPoseCensus, GJBaseGameLayer) {
    void transformAreaObjects(GameObject* object,CCArray* objects,float sx,float sy,bool reset) {
        auto play=PlayLayer::get();
        if(!installed||written||!play||static_cast<GJBaseGameLayer*>(play)!=this || play->getCurrentPercent()<96.f) {
            GJBaseGameLayer::transformAreaObjects(object,objects,sx,sy,reset);return;
        }
        if(active) {
            auto previous=active;++previous->nested;active=nullptr;
            GJBaseGameLayer::transformAreaObjects(object,objects,sx,sy,reset);active=previous;return;
        }
        if(play->getCurrentPercent()>=97.f) { write();GJBaseGameLayer::transformAreaObjects(object,objects,sx,sy,reset);return; }
        const auto frame=CCDirector::get()->getTotalFrames();
        if(!used||samples[used-1].frame!=frame) {
            if(used==samples.size()) { write();GJBaseGameLayer::transformAreaObjects(object,objects,sx,sy,reset);return; }
            samples[used].frame=frame;samples[used].percent=play->getCurrentPercent();++used;
        }
        auto& sample=samples[used-1];++sample.phases;
        inputRecipes.begin();resultRecipes.begin();active=&sample;
        const auto begin=std::chrono::steady_clock::now();
        struct Scope { ~Scope(){active=nullptr;} } scope;
        GJBaseGameLayer::transformAreaObjects(object,objects,sx,sy,reset);
        sample.ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    }
};
