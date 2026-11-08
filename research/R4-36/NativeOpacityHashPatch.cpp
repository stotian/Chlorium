#ifdef CHLORIUM_NATIVE_OPACITY_HASH_R36
#include "HashSubstitution.hpp"
#include <Geode/Geode.hpp>
using namespace geode::prelude;
extern "C" __declspec(dllexport) const char chloriumOpacityHashIdentity[]="CHLORIUM_NATIVE_OPACITY_HASH_R36";
#ifdef CHLORIUM_OPACITY_HASH_FACTORED
extern "C" __declspec(dllexport) const char chloriumOpacityHashFactoredIdentity[]="CHLORIUM_NATIVE_OPACITY_HASH_FACTORED_R36";
#endif
namespace {
constexpr unsigned getterRva=0x260b80,size=0xbe,offset=0x2b;
bool enabled=false;Patch* installed=nullptr;std::uint64_t expected=0;
std::uint64_t hash(const unsigned char* data,unsigned count){std::uint64_t value=14695981039346656037ull;for(unsigned i=0;i<count;++i)value=(value^data[i])*1099511628211ull;return value;}
}
extern "C" __declspec(dllexport) bool ChloriumOpacityHashActiveR36(){
    const auto base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    return enabled&&installed&&hash(base+getterRva,size)==expected;
}
$execute {
    const auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return;
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||pe->FileHeader.TimeDateStamp!=0x69702da0||pe->OptionalHeader.SizeOfImage!=0xa3a000)return;
    for(auto mod:Loader::get()->getAllMods())if(mod->isOrWillBeEnabled()&&mod->getID()!="geode.loader"&&mod->getID()!="geode.node-ids"&&mod->getID()!="local.chlorium"&&mod->getID()!="local.chlorium.audit"){
        log::warn("Opacity hash R36: isolated profile required; native getter retained");return;
    }
    if(hash(base+getterRva,size)!=0xc75ba7aaf16ba44cull){log::warn("Opacity hash R36: unknown/modified getter; native retained");return;}
    const auto kind=
#ifdef CHLORIUM_OPACITY_HASH_FACTORED
        chlorium::research::opacityhash::HashKind::factored;
#else
        chlorium::research::opacityhash::HashKind::compact;
#endif
    const auto block=chlorium::research::opacityhash::hashSubstitution(chlorium::research::opacityhash::lowHashes.data(),kind);
    std::array<unsigned char,size> body;std::memcpy(body.data(),base+getterRva,size);std::memcpy(body.data()+offset,block.data(),block.size());expected=hash(body.data(),size);
    auto result=Mod::get()->patch(base+getterRva+offset,ByteVector(block.begin(),block.end()));
    if(!result){log::warn("Opacity hash R36: patch refused; native retained");return;}installed=result.unwrap();
    const auto update=[](bool value){auto result=installed->toggle(value);enabled=value&&result.isOk();if(!result)log::warn("Opacity hash R36: toggle failed: {}",result.unwrapErr());};
    update(Mod::get()->getSettingValue<bool>("enabled"));listenForSettingChanges<bool>("enabled",update);
    log::info("Opacity hash R36: integer substitution active; live native map/value reads retained");
}
#endif
