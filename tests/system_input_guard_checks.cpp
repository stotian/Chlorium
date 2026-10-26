#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef CHLORIUM_TEST_FOREIGN_XINPUT
// Harmless child-process fixture, built only under build-tests. No Geode entry.
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
#else
#include "SystemInputGuard.hpp"
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv) try {
    if(argc!=3)throw std::runtime_error("pass mode and fixture library path");
    const bool foreign=std::wstring(argv[1])==L"--reject-foreign";
    if(foreign) {
        const auto input=LoadLibraryExW(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!input||GetModuleHandleW(L"XINPUT1_4.dll")!=input)throw std::runtime_error("foreign fixture failed to load");
        bool rejected=false;
        try {chlorium::test::SystemInputGuard guard;}
        catch(const std::runtime_error& error) {rejected=std::string(error.what())=="Foreign XInput already loaded before fixture isolation";}
        chlorium::test::SystemInputGuard::assertNoGeode();FreeLibrary(input);
        if(!rejected)throw std::runtime_error("foreign XInput was not rejected by the intended preflight");
        std::cout<<"PASS foreign XInput rejected in child; no Geode loaded\n";return 0;
    }
    if(std::wstring(argv[1])!=L"--load-cocos")throw std::runtime_error("unknown loader test mode");
    chlorium::test::SystemInputGuard guard;
    const auto pinned=GetModuleHandleW(L"XINPUT1_4.dll");
    const auto cocos=LoadLibraryExW(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!cocos)throw std::runtime_error("owned Cocos failed to load");
    chlorium::test::SystemInputGuard::assertNoGeode();
    const bool same=GetModuleHandleW(L"XINPUT1_4.dll")==pinned;
    FreeLibrary(cocos);
    chlorium::test::SystemInputGuard::assertNoGeode();
    if(!same||!pinned||GetModuleHandleW(L"XINPUT1_4.dll")!=pinned)throw std::runtime_error("system XInput pin lost across Cocos load/unload");
    std::cout<<"PASS owned Cocos load/unload retains system XInput; no Geode loaded\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#endif
