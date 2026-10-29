#if defined(CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS) || defined(CHLORIUM_NATIVE_CANONICAL_V10)
#include "AreaDivisionPatch.hpp"
#include <Geode/Geode.hpp>
#include <windows.h>
using namespace geode::prelude;

$execute {
    using namespace chlorium::area;
    const auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return;
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe->FileHeader.TimeDateStamp != 0x69702da0 || pe->OptionalHeader.SizeOfImage != 0xa3a000 ||
        hash({base + functionRva, functionBytes}) != functionHash) {
        log::warn("Area arithmetic experiment: unknown or modified routine; native code retained"); return;
    }
    ByteVector body(base+functionRva,base+functionRva+functionBytes);
    if(!rewriteQuotients(body)) {
        log::warn("Area arithmetic: source instructions changed; no patch installed");return;
    }
    constexpr auto begin=patches.front().offset;
    constexpr auto end=patches.back().offset+patches.back().length;
    auto patch=Mod::get()->patch(base+functionRva+begin,ByteVector(body.begin()+begin,body.begin()+end));
    if(!patch){log::warn("Area arithmetic: contiguous patch failed: {}",patch.unwrapErr());return;}
    const auto updateEnabled = [installed=patch.unwrap()](bool enabled) {
        if(auto toggled=installed->toggle(enabled);!toggled)
            log::warn("Area arithmetic: unable to change the complete patch state: {}",toggled.unwrapErr());
    };
    updateEnabled(Mod::get()->getSettingValue<bool>("enabled"));
    listenForSettingChanges<bool>("enabled", updateEnabled);
    log::info("Area arithmetic experiment installed: repeated quotients reused, native loop retained");
}
#endif
