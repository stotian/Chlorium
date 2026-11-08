#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace chlorium::research::handoff {
// Native atlas body contains upload/layout/draw only; no program/uniform binding.
inline bool nativeAtlasBody(HMODULE module)noexcept{
    if(!module)return false;const auto base=reinterpret_cast<const unsigned char*>(module);const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return false;
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||pe->FileHeader.TimeDateStamp!=0x69702d99||pe->OptionalHeader.SizeOfImage!=0x1c5000||
       reinterpret_cast<const unsigned char*>(GetProcAddress(module,"?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"))!=base+0xa6350)return false;
    std::uint64_t hash=14695981039346656037ull;for(unsigned i=0x20;i<0x15f;++i)hash=(hash^base[0xa6350+i])*1099511628211ull;
    return hash==0xc9d0f08451f64908ull;
}
inline bool standardTexture(HMODULE module,const void* texture)noexcept{
    if(!module||!texture)return false;const auto base=reinterpret_cast<const unsigned char*>(module);
    const auto getter=GetProcAddress(module,"?getName@CCTexture2D@cocos2d@@UEAAIXZ");
    const unsigned char expected[]{0x8b,0x41,0x48,0xc3};
    return reinterpret_cast<const unsigned char*>(getter)==base+0x10760&&
        !std::memcmp(base+0x10760,expected,sizeof(expected))&&(*reinterpret_cast<void*const*const*>(texture))[0x60/8]==reinterpret_cast<void*>(getter);
}
}
