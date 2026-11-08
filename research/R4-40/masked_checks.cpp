#include "../R4-33/NativeCornerOracleFixture.hpp"
#include "MaskedCorners.hpp"
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==4,"Cocos DLL, V3 capture and NEW receipt");
    chlorium::test::SystemInputGuard input;const auto native=loadNative(std::filesystem::path(argv[1]),input);
    auto records=readCapture(argv[2]);const auto captured=records.size();appendArithmeticCases(records);
    constexpr std::array<std::uint32_t,16> values{0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0xffc23456u,0x7f812345u,0xff823456u,1,0x80000001u,0x007fffffu,0x00800000u,0x7f7fffffu,0xff7fffffu,0x3f800000u,0xbf800000u};
    const auto baseline=records.back();
    for(unsigned first=0;first<10;++first)for(unsigned second=0;second<10;++second)for(auto a:values)for(auto b:values){
        auto record=baseline;record.sprite.flags=512|1024;
        constexpr std::array<unsigned,6> identity{0x3f800000,0,0,0x3f800000,0,0};constexpr std::array<unsigned,4> rect{0,0,0x3f800000,0x3f800000};
        std::copy(identity.begin(),identity.end(),record.sprite.matrix);std::copy(rect.begin(),rect.end(),record.sprite.localRect);
        auto set=[&](unsigned slot,std::uint32_t bits){if(slot<6)record.sprite.matrix[slot]=bits;else record.sprite.localRect[slot-6]=bits;};
        set(first,a);set(second,b);record.sprite.z=values[(first+second)%values.size()];records.push_back(record);
    }
    std::mt19937 random(0x4405ca1u);
    for(unsigned i=0;i<4096;++i){auto record=baseline;record.sprite.flags=512|1024;
        for(auto& value:record.sprite.matrix)value=random();for(auto& value:record.sprite.localRect)value=random();record.sprite.z=random();records.push_back(record);}
    chlorium::research::maskedcorners::OrderedKernel kernel; const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    std::uint64_t comparisons=0,rejected=0;
    for(std::size_t index=0;index<records.size();++index){const auto& record=records[index];
        if(record.sprite.flags&(64|128)){auto unchanged=record.quad;_mm_setcsr(0x1fa1);
            require(!chlorium::research::maskedcorners::materialize(record.sprite,unchanged,kernel)&&std::memcmp(&unchanged,&record.quad,sizeof(unchanged))==0&&_mm_getcsr()==0x1fa1,"unsupported rejection mutates nothing");++rejected;continue;}
        for(unsigned state=0;state<32;++state){const unsigned controls=0x1f80u|((state&3)<<13)|((state&4)?0x40:0)|((state&8)?0x8000:0)|((state&16)?0x3f:0);
            const auto expected=nativeResult(record,controls,reinterpret_cast<const unsigned char*>(native));auto result=record.quad;_mm_setcsr(controls);
            require(chlorium::research::maskedcorners::materialize(record.sprite,result,kernel),"masked case rejected");const auto actualFP=_mm_getcsr();
            if(std::memcmp(&result,&expected.quad,sizeof(result))||actualFP!=expected.fpState){
                std::cerr<<"case "<<index<<" state "<<state<<" CSR native/actual "<<std::hex<<expected.fpState<<'/'<<actualFP<<std::dec<<'\n';
                for(unsigned i=0;i<4;++i)std::cerr<<"corner "<<i<<" x native/actual "<<std::hex<<std::bit_cast<unsigned>(expected.quad.corners[i].x)<<'/'<<std::bit_cast<unsigned>(result.corners[i].x)<<" y "<<std::bit_cast<unsigned>(expected.quad.corners[i].y)<<'/'<<std::bit_cast<unsigned>(result.corners[i].y)<<std::dec<<'\n';
                throw std::runtime_error("masked native arithmetic mismatch");
            }
            ++comparisons;
        }
    }
    for(unsigned mask=0;mask<6;++mask){auto result=baseline.quad;const auto before=result;const unsigned controls=0x1f80u&~(1u<<(mask+7));_mm_setcsr(controls);
        require(!chlorium::research::maskedcorners::materialize(baseline.sprite,result,kernel)&&!std::memcmp(&result,&before,sizeof(result))&&_mm_getcsr()==controls,"unmasked gate leaves bytes and controls unchanged");
    }
    _mm_setcsr(saved);
    std::ofstream output(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(output),"new masked receipt");
    output<<"{\"diagnostic_only\":true,\"native_comparisons\":"<<comparisons<<",\"captured_records\":"<<captured<<",\"rejected_records\":"<<rejected<<",\"game_fps_evidence\":false}\n";require(bool(output),"write complete masked receipt");
    std::cout<<"PASS "<<comparisons<<" masked native comparisons including paired NaNs/Inf/subnormals and raw inputs; no game gain claim\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
