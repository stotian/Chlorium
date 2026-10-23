#ifdef CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS
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
    std::vector<Patch*> applied;
    for (const auto& bytes : patches) {
        if (std::memcmp(base + functionRva + bytes.offset, bytes.expected.data(), bytes.length)) {
            for (const auto patch : applied) (void)patch->disable();
            log::warn("Area arithmetic experiment: patch bytes changed; rolled back"); return;
        }
        auto patch = Mod::get()->patch(base + functionRva + bytes.offset,
            ByteVector(bytes.replacement.begin(), bytes.replacement.begin() + bytes.length));
        if (!patch) {
            for (const auto previous : applied) (void)previous->disable();
            log::warn("Area arithmetic experiment: patch failed; rolled back: {}", patch.unwrapErr()); return;
        }
        applied.push_back(patch.unwrap());
    }
    const auto updateEnabled = [installed = applied](bool enabled) {
        for (const auto patch : installed) {
            if (auto toggled = patch->toggle(enabled); !toggled) {
                for (const auto previous : installed) (void)previous->disable();
                log::warn("Area arithmetic experiment: setting change failed; patches disabled");
                return;
            }
        }
    };
    updateEnabled(Mod::get()->getSettingValue<bool>("enabled"));
    listenForSettingChanges<bool>("enabled", updateEnabled);
    log::info("Area arithmetic experiment installed: repeated quotients reused, native loop retained");
}
#endif
