#pragma once
#include <windows.h>
#include <stdexcept>
#include <string>
#include <cwchar>

namespace chlorium::test {
// Cocos imports XINPUT1_4. Pin Windows' library before loading the owned Cocos
// DLL, otherwise its sibling Geode proxy can start a loader in the fixture.
class SystemInputGuard {
    HMODULE input_=nullptr;
public:
    SystemInputGuard(const SystemInputGuard&)=delete;
    SystemInputGuard& operator=(const SystemInputGuard&)=delete;
    static void assertNoGeode() {
        if(GetModuleHandleW(L"Geode.dll"))throw std::runtime_error("Geode loaded inside an isolated native fixture");
    }
    SystemInputGuard() {
        assertNoGeode();
        wchar_t system[MAX_PATH]{},actual[MAX_PATH]{};
        const auto n=GetSystemDirectoryW(system,MAX_PATH);
        if(!n||n>=MAX_PATH)throw std::runtime_error("Windows system directory unavailable");
        const std::wstring expected=std::wstring(system)+L"\\XINPUT1_4.dll";
        if(const auto existing=GetModuleHandleW(L"XINPUT1_4.dll")) {
            const auto length=GetModuleFileNameW(existing,actual,MAX_PATH);
            if(!length||length>=MAX_PATH||_wcsicmp(actual,expected.c_str()))
                throw std::runtime_error("Foreign XInput already loaded before fixture isolation");
        }
        input_=LoadLibraryExW(expected.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!input_)throw std::runtime_error("Windows XInput unavailable for fixture isolation");
        const auto length=GetModuleFileNameW(input_,actual,MAX_PATH);
        if(!length||length>=MAX_PATH||_wcsicmp(actual,expected.c_str())||GetModuleHandleW(L"XINPUT1_4.dll")!=input_) {
            FreeLibrary(input_);input_=nullptr;throw std::runtime_error("Fixture resolved a foreign XInput library");
        }
        try {assertNoGeode();}catch(...){FreeLibrary(input_);input_=nullptr;throw;}
    }
    ~SystemInputGuard(){if(input_)FreeLibrary(input_);}
};
}
