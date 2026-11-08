#include "../R4-33/NativeCornerOracleFixture.hpp"
#include "PackedCorners.hpp"
#include <chrono>
using chlorium::research::packedcorners::materialize;
__declspec(noinline) bool scalar(const SpriteRecord& source,NativeQuadState& target){return chlorium::research::corners::materialize(source,target);}
__declspec(noinline) bool packed(const SpriteRecord& source,NativeQuadState& target){return materialize(source,target);}
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==4||argc==5,"Cocos DLL, complete V3 capture, NEW receipt and optional --timings");
    if(argc==5)require(std::wstring_view(argv[4])==L"--timings","explicit kernel timings switch");
    chlorium::test::SystemInputGuard input;
    const auto native=loadNative(std::filesystem::path(argv[1]),input);
    auto records=readCapture(argv[2]);const auto capturedCount=records.size();appendArithmeticCases(records);
    const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    std::uint64_t comparisons=0,unsupported=0,capturedCompared=0,capturedMatch=0;
    for(unsigned index=0;index<records.size();++index){const auto& record=records[index];
        if(!chlorium::research::corners::domain(record.sprite)){
            auto unchanged=record.quad;_mm_setcsr(0x1fa1);
            require(!materialize(record.sprite,unchanged)&&std::memcmp(&unchanged,&record.quad,sizeof(unchanged))==0&&_mm_getcsr()==0x1fa1,"domain rejection mutates nothing");++unsupported;continue;
        }
        for(unsigned controlsCase=0;controlsCase<32;++controlsCase){
            const unsigned controls=0x1f80u|((controlsCase&3)<<13)|((controlsCase&4)?0x40u:0)|((controlsCase&8)?0x8000u:0)|((controlsCase&16)?0x21u:0);
            const auto expected=nativeResult(record,controls,reinterpret_cast<const unsigned char*>(native));
            auto result=record.quad;_mm_setcsr(controls);require(materialize(record.sprite,result),"eligible packed arithmetic rejected");
            const auto flags=_mm_getcsr();
            require(std::memcmp(&expected.quad,&result,sizeof(result))==0,"packed positions/Z/color/UV differ from native Cocos");
            require(flags==expected.fpState,"packed MXCSR differs from native Cocos");++comparisons;
            if(index<capturedCount&&controlsCase==0){++capturedCompared;capturedMatch+=std::memcmp(&result,&record.quad,sizeof(result))==0;}
        }
    }
    for(unsigned mask=0;mask<6;++mask){auto unchanged=records.back().quad;const auto original=unchanged;
        const auto controls=0x1f80u&~(1u<<(mask+7));_mm_setcsr(controls);
        require(!materialize(records.back().sprite,unchanged)&&std::memcmp(&unchanged,&original,sizeof(original))==0&&_mm_getcsr()==controls,"unmasked control rejection mutates nothing");
    }
    _mm_setcsr(saved);
    if(argc==5){
        std::ofstream csv(std::filesystem::path(argv[3]).wstring()+L".kernel.csv",std::ios::out|std::ios::noreplace);require(bool(csv),"new kernel timing receipt");
        std::vector<NativeQuadState> output(records.size());std::uint64_t sink=0;
        csv<<"round,variant,milliseconds,accepted,checksum\n";
        for(unsigned round=0;round<10;++round)for(unsigned order=0;order<2;++order){
            const unsigned variant=order^(round&1);const auto operation=variant?&packed:&scalar;std::uint64_t accepted=0;
            const auto start=std::chrono::steady_clock::now();
            for(unsigned repeat=0;repeat<32;++repeat)for(unsigned i=0;i<records.size();++i)accepted+=operation(records[i].sprite,output[i]);
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            // Consume the complete output after each batch; neither kernel may disappear.
            std::uint64_t checksum=14695981039346656037ull;
            for(const auto& quad:output)for(auto byte:std::as_bytes(std::span(&quad,1)))checksum=(checksum^std::to_integer<unsigned char>(byte))*1099511628211ull;
            sink^=checksum;if(round)csv<<round<<','<<variant<<','<<ms<<','<<accepted<<','<<checksum<<'\n';
        }
        require(bool(csv),"complete kernel timing receipt");std::cout<<"kernel output sink "<<sink<<'\n';
    }
    std::ofstream result(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(result),"new native packed receipt");
    result<<"{\"diagnostic_only\":true,\"native_arithmetic_cases\":"<<comparisons<<",\"unsupported_records\":"<<unsupported<<",\"captured_records_compared\":"<<capturedCompared<<",\"captured_records_matching\":"<<capturedMatch<<",\"fps_gain_proven\":false,\"whole_callback_replacement_proven\":false}\n";
    require(bool(result),"write packed receipt");
    std::cout<<"PASS "<<comparisons<<" packed/native arithmetic comparisons; captured bits "<<capturedMatch<<'/'<<capturedCompared<<"; no GD process or FPS verdict\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
