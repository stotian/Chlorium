#ifdef CHLORIUM_EXPERIMENTAL_AFFINE_POINT
#include "NativeAffinePoint.hpp"
#include "NearCallSlot.hpp"
#include <Geode/Geode.hpp>
#include <windows.h>
#include <cstdlib>
using namespace geode::prelude;

namespace chlorium::affine {
namespace {
Apply original = nullptr;
Apply volatile* imported = nullptr;
std::uint64_t nativeEntry = 0;
Patch* callPatch = nullptr;
void* callSlot = nullptr;
thread_local Stats stats;
std::uint64_t hash(const unsigned char* bytes, std::size_t size) {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0;i<size;++i) { h^=bytes[i]; h*=1099511628211ull; } return h;
}
bool moduleMatches(const unsigned char* bytes, unsigned timestamp, unsigned size) {
    if(!bytes) return false;
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0) return false;
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes+dos->e_lfanew);
    return pe->Signature==IMAGE_NT_SIGNATURE && pe->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
        pe->FileHeader.TimeDateStamp==timestamp && pe->OptionalHeader.SizeOfImage==size;
}
Point* packedNativePoint(Point* output,const Point* point,const Matrix* matrix) {
    // The validated native call site uses three disjoint stack ranges:
    // output [rsp+0x68,+0x70), point [+0x78,+0x80), matrix [+0x88,+0xa0).
    // No per-object counters or TLS access in the measured fast path.
    return applyImportedNonAliased(output,point,matrix,imported,original,nativeEntry);
}
Point* countedNativePoint(Point* output,const Point* point,const Matrix* matrix) {
    ++stats.calls;
    // Preserve a later import replacement or entry detour by another mod.
    const auto current=*imported;
    if(current!=original || *reinterpret_cast<volatile const std::uint64_t*>(original)!=nativeEntry) {
        ++stats.fallbacks;
        return current(output,point,matrix);
    }
    return applyNonAliased(output,point,matrix,original);
}
void install() {
    const auto game=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto cocos=reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"libcocos2d.dll"));
    if(!moduleMatches(game,0x69702da0,0xa3a000) || !moduleMatches(cocos,0x69702d99,0x1c5000) ||
        hash(game+0x22b7f0+32,0x910-32)!=0x7d1426886a014a85ull ||
        hash(cocos+0x65a0,0xae)!=0x1e32bc1900bee292ull ||
        hash(cocos+0xc6d0,9)!=0x81a9cb89eb9a4289ull) {
        log::warn("Affine point experiment: unknown or modified native body; no patch");return;
    }
    const unsigned char expected[]{0xff,0x15,0x5b,0xa7,0x2e,0x00};
    if(std::memcmp(game+0x22be5f,expected,sizeof(expected))) return;
    imported=reinterpret_cast<Apply volatile*>(const_cast<unsigned char*>(game+0x5165c0));
    original=reinterpret_cast<Apply>(const_cast<unsigned char*>(cocos+0x65a0));
    if(*imported!=original) { log::warn("Affine point experiment: import owned by another mod");return; }
    std::memcpy(&nativeEntry,cocos+0x65a0,sizeof(nativeEntry));
    const std::array<std::uintptr_t,1> sites{reinterpret_cast<std::uintptr_t>(game)+0x22be5f};
    const bool countCalls=std::getenv("CHLORIUM_AFFINE_COUNT")!=nullptr;
    callSlot=chlorium::matrix::allocateNearCallSlot(sites,
        reinterpret_cast<std::uintptr_t>(countCalls?&countedNativePoint:&packedNativePoint),
        reinterpret_cast<std::uintptr_t>(game)+0xa3a000);
    if(!callSlot) { log::warn("Affine point experiment: nearby pointer allocation unavailable");return; }
    ByteVector bytes{0xff,0x15,0,0,0,0};
    const auto delta=std::int32_t(reinterpret_cast<std::uintptr_t>(callSlot)-(sites[0]+6));
    std::memcpy(bytes.data()+2,&delta,sizeof(delta));
    auto patched=Mod::get()->patch(const_cast<unsigned char*>(game+0x22be5f),std::move(bytes));
    if(!patched) {
        VirtualFree(callSlot,0,MEM_RELEASE);callSlot=nullptr;
        log::warn("Affine point experiment: call patch failed");return;
    }
    callPatch=patched.unwrap();
    // Keep the read-only pointer page for as long as Geode owns the patch.
    const auto toggle=[](bool enabled) {
        if(auto result=callPatch->toggle(enabled);!result) {
            (void)callPatch->disable();log::warn("Affine point experiment: failed to toggle patch");
        }
    };
    toggle(Mod::get()->getSettingValue<bool>("enabled"));
    listenForSettingChanges<bool>("enabled",toggle);
    log::info("Affine point v2 experiment installed at one native rotation call site; counters={}",countCalls);
}
}
}
extern "C" __declspec(dllexport) bool ChloriumReadAffineStats(chlorium::affine::Stats* output,std::size_t size) {
    using namespace chlorium::affine;
    if(!output || size!=sizeof(*output)) return false;
    *output=stats;output->installed=callPatch && callPatch->isEnabled();return true;
}
$execute { chlorium::affine::install(); }
#endif
