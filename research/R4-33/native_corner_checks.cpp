#include "NativeCornerOracleFixture.hpp"

int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==4,"Cocos DLL, V3 capture and new JSON output are required");
    chlorium::test::SystemInputGuard systemInput;
    const auto module=loadNative(std::filesystem::path(argv[1]),systemInput);
    const auto base=reinterpret_cast<const unsigned char*>(module);
    auto records=readCapture(argv[2]);const auto capturedCount=records.size();appendArithmeticCases(records);
    const auto saved=_mm_getcsr();
    struct Restore {unsigned state;~Restore(){_mm_setcsr(state);}} restore{saved};
    std::uint64_t compared=0,unsupported=0,capturedMatch=0,capturedCompared=0;
    {
        auto source=records.back().sprite,original=source;
        auto quad=records.back().quad,unchanged=quad;source.matrix[0]=0x2b7fffffu;
        _mm_setcsr(0x1f80);require(!chlorium::research::corners::materialize(source,quad)&&!std::memcmp(&quad,&unchanged,sizeof(quad))&&_mm_getcsr()==0x1f80,"out-of-domain rejection must not mutate output or FP state");
        source=original;_mm_setcsr(0);
        require(!chlorium::research::corners::materialize(source,quad)&&!std::memcmp(&quad,&unchanged,sizeof(quad))&&_mm_getcsr()==0,"unmasked-exception rejection must not mutate output or FP state");
        _mm_setcsr(0x1f80);require(chlorium::research::corners::materialize(source,quad)&&std::bit_cast<std::uint32_t>(quad.corners[2].x)==0x00800000u,"minimum normal intermediate bound");
    }
    for(std::size_t index=0;index<records.size();++index){
        const auto& record=records[index];
        if(!chlorium::research::corners::domain(record.sprite)){++unsupported;continue;}
        for(unsigned rounding=0;rounding<4;++rounding)for(unsigned flush=0;flush<4;++flush){
            const auto controls=0x1f80u|(rounding<<13)|((flush&1)?0x40u:0u)|((flush&2)?0x8000u:0u);
            const auto original=nativeResult(record,controls,base);
            const auto& actual=original.quad; const auto nativeFlags=original.fpState;
            auto candidate=record.quad;_mm_setcsr(controls);
            require(chlorium::research::corners::materialize(record.sprite,candidate),"eligible arithmetic rejected");
            const auto candidateFlags=_mm_getcsr();
            require(!std::memcmp(&actual,&candidate,sizeof(actual)),"native corner bits differ");
            require(nativeFlags==candidateFlags,"native corner floating-point state differs");
            ++compared;
            if(index<capturedCount&&!rounding&&!flush){++capturedCompared;capturedMatch+=!std::memcmp(&candidate,&record.quad,sizeof(candidate));}
        }
    }
    std::ofstream result(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(result),"refuse existing oracle output");
    result<<"{\"diagnostic_only\":true,\"native_arithmetic_cases\":"<<compared<<",\"unsupported_records\":"<<unsupported
          <<",\"captured_records_compared\":"<<capturedCompared<<",\"captured_records_matching\":"<<capturedMatch
          <<",\"fps_gain_proven\":false,\"whole_callback_replacement_proven\":false}\n";
    require(bool(result),"write oracle receipt");
    std::cout<<"PASS "<<compared<<" native arithmetic cases; captured default-rounding agreement "<<capturedMatch<<'/'<<capturedCompared<<"; no FPS verdict\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
