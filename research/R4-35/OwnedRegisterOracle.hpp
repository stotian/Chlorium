#pragma once
#include "RegisterProbe.hpp"
#include <Geode/Geode.hpp>
#include <fstream>
#include <cstdlib>

extern "C" __declspec(dllexport) const char chloriumOwnedRegisterOracleIdentity[]="CHLORIUM_OWNED_REGISTER_ORACLE_V1";
namespace chlorium::research::registers {
inline void runOwnedRegisterOracle() noexcept {
    static bool attempted=false;
    const auto flag=std::getenv("CHLORIUM_OWNED_REGISTER_ORACLE"),output=std::getenv("CHLORIUM_AUDIT_OUTPUT");
    if(attempted||!flag||std::string_view(flag)!="1"||!output)return;attempted=true;
    const auto fp=_mm_getcsr();const auto lastError=GetLastError();
    struct Restore{unsigned fp;DWORD error;~Restore(){_mm_setcsr(fp);SetLastError(error);}}restore{fp,lastError};
    matjson::Value result=matjson::Value::object();result["diagnostic_only"]=true;
    result["fps_evidence"]=false;result["native_getters_hooked"]=false;result["native_getter_equivalence_proven"]=false;
    result["status"]="failed";
    try {
        Probe probe;matjson::Value cases=matjson::Value::array();bool passed=true;
        // No game address is patched. The loader owns only our one-page fixture.
        probe.retainForLoader();
        for(unsigned signature=0;signature<2;++signature){
            const auto original=probe.run(signature),poisoned=probe.run(signature,true);
            if(!Probe::preserved(original)||Probe::preserved(poisoned))throw std::runtime_error("register oracle controls failed");
            using Getter=float(*)(void*);using GroupGetter=float(*)(void*,int);
            auto created=signature?
                geode::Mod::get()->hook(probe.target(signature),reinterpret_cast<GroupGetter>(probe.detour(signature)),"ChloriumOwnedRegisterOracle",tulip::hook::TulipConvention::Thiscall):
                geode::Mod::get()->hook(probe.target(signature),reinterpret_cast<Getter>(probe.detour(signature)),"ChloriumOwnedRegisterOracle",tulip::hook::TulipConvention::Thiscall);
            if(created.isErr())throw std::runtime_error(created.unwrapErr());
            auto hook=created.unwrap();bool detached=false;Output hooked{};
            {
                struct Detach {geode::Hook* hook;bool& success;~Detach(){success=geode::Mod::get()->disownHook(hook).isOk();}}detach{hook,detached};
                if(!hook->isEnabled())throw std::runtime_error("owned probe hook not enabled");
                hooked=probe.run(signature);
            }
            const auto after=probe.run(signature);
            const bool registers=Probe::preserved(hooked),returnChanged=hooked.result==poisoned.result;
            const bool restored=detached&&after.result==original.result&&after.xmm==original.xmm;
            matjson::Value row=matjson::Value::object();row["arguments"]=signature?2:1;
            row["direct_poison_detected"]=true;row["detour_return_observed"]=returnChanged;
            row["xmm1_2_3_preserved"]=registers;row["hook_detached_and_native_return_restored"]=restored;
            row["observed_return_bits"]=hooked.result;
            matjson::Value words=matjson::Value::array();for(auto value:hooked.xmm)words.push(value);row["observed_xmm_words"]=std::move(words);
            cases.push(std::move(row));passed&=registers&&returnChanged&&restored;
            if(!restored)break;
        }
        result["cases"]=std::move(cases);result["status"]=passed?"owned_hook_register_contract_passed":"owned_hook_register_contract_failed";
        result["scope"]="Two owned float getter signatures using installed Geode Thiscall hooks; not native call-site parity";
    }catch(const std::exception& error){result["error"]=error.what();}
    try {
        std::ofstream stream(std::string(output)+".owned-register-oracle.json",std::ios::out|std::ios::noreplace);
        if(stream)stream<<result.dump(2)<<'\n';
    }catch(...){} // Missing receipt is failure, never evidence of success.
}
}
