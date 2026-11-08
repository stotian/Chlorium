#include "RegisterProbe.hpp"
#include <iostream>
using chlorium::research::registers::Probe;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    Probe probe;
    for(unsigned signature=0;signature<2;++signature){
        const auto native=probe.run(signature);require(Probe::preserved(native),"native target preserves sentinel registers");
        require(native.result==(signature?0x40e00000u:0x3f800000u),"native return matches signature");
        const auto poisoned=probe.run(signature,true);require(!Probe::preserved(poisoned),"direct poison must be detected");
        require(poisoned.result==(signature?0x41000000u:0x40000000u),"detour return proves argument/caller setup");
        const auto again=probe.run(signature);require(again.xmm==native.xmm&&again.result==native.result,"poison does not contaminate the next caller");
    }
    std::cout<<"PASS owned Win64 caller, two getter signatures and deliberate XMM1-3 poison; no GD/loader contract proved yet\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
