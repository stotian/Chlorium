#include "../R4-33/NativeCornerOracleFixture.hpp"
#include "../R4-34/QuadReadWatch.hpp"
#include "NativeSplice.hpp"
#include <chrono>
using chlorium::research::maskedcorners::NativeSplice;
using chlorium::research::maskedcorners::OrderedKernel;
class OwnedPatch {
    unsigned char* target;
    std::array<unsigned char,8> original{},replacement;
    bool active=false;
public:
    OwnedPatch(unsigned char* address,std::array<unsigned char,8> value):target(address),replacement(value){std::memcpy(original.data(),target,8);}
    void toggle(bool enable){
        if(enable==active)return;
        require(std::memcmp(target,(active?replacement:original).data(),8)==0,"owned patch instruction ownership");
        DWORD old=0;require(VirtualProtect(target,8,PAGE_EXECUTE_READWRITE,&old)!=FALSE,"owned patch write access");
        std::memcpy(target,(enable?replacement:original).data(),8);active=enable;
        const bool flushed=FlushInstructionCache(GetCurrentProcess(),target,8)!=FALSE;DWORD unused=0;
        const bool restored=VirtualProtect(target,8,old,&unused)!=FALSE;
        require(flushed&&restored,"owned patch cache/protection restoration");
    }
    ~OwnedPatch(){try{toggle(false);}catch(...){TerminateProcess(GetCurrentProcess(),95);}}
};
void unwindChecks(const unsigned char* base,const NativeSplice& splice){
    alignas(16) std::array<unsigned char,1024> stack{};
    const auto rsp=reinterpret_cast<DWORD64>(stack.data());
    const std::uint64_t ret=0x1234567812345678ull,rdi=0x2233445566778899ull,rbx=0x1122334455667788ull;
    std::memcpy(stack.data()+0x118,&ret,8);std::memcpy(stack.data()+0x110,&rdi,8);std::memcpy(stack.data()+0x128,&rbx,8);
    for(unsigned reg=6;reg<16;++reg)for(unsigned byte=0;byte<16;++byte)stack[0x100-(reg-6)*16+byte]=static_cast<unsigned char>(reg*16+byte);
    auto unwind=[&](std::uintptr_t pc){CONTEXT context{};context.ContextFlags=CONTEXT_ALL;context.Rip=pc;context.Rsp=rsp;context.Rdi=1;context.Rbx=2;
        std::array<unsigned char,160> live{};for(unsigned i=0;i<live.size();++i)live[i]=static_cast<unsigned char>(i^0x4f);std::memcpy(&context.Xmm6,live.data(),live.size());
        DWORD64 image=0;auto entry=RtlLookupFunctionEntry(pc,&image,nullptr);require(entry!=nullptr,"unwind entry found");
        void* handlerData=nullptr;DWORD64 establisher=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,pc,entry,&context,&handlerData,&establisher,nullptr);
        require(context.Rip==ret&&context.Rsp==rsp+0x120&&context.Rdi==rdi&&context.Rbx==rbx,"native frame unwind restoration");
        require(!std::memcmp(&context.Xmm6,live.data(),live.size()),"pre-spill nonvolatile XMM values remain live");return context;
    };
    const auto native=unwind(reinterpret_cast<std::uintptr_t>(base)+NativeSplice::unwindPc);
    for(unsigned offset:{splice.fastOffset,NativeSplice::fallback,NativeSplice::kernelOffset,NativeSplice::kernelOffset+40}){
        const auto actual=unwind(reinterpret_cast<std::uintptr_t>(splice.address())+offset);
        require(!std::memcmp(&native.Xmm6,&actual.Xmm6,sizeof(M128A)*10),"all saved native XMM registers unwind through bridge");
    }
}
void guardChecks(const unsigned char* base,const NativeSplice& splice){
    Memory object;std::array<void*,160> table{};table[0x400/8]=reinterpret_cast<void*>(&dirty);table[0x408/8]=reinterpret_cast<void*>(&setDirty);table[0x308/8]=reinterpret_cast<void*>(&transform);
    object.put(0,table.data());object.put(0x170,std::uintptr_t{1});object.put(0x11e,true);object.put(0x149,true);
    const std::array<std::uint32_t,6> identity{0x3f800000,0,0,0x3f800000,0,0};object.put(0x280,identity);object.put(0x17c,identity);
    object.put(0x1b0,0x3f800000u);object.put(0x1b4,0x3f800000u);
    const std::array<std::uintptr_t,2> watched{reinterpret_cast<std::uintptr_t>(splice.address())+splice.fastOffset,reinterpret_cast<std::uintptr_t>(splice.address())+NativeSplice::fallback};
    chlorium::research::readwatch::Watch watch;require(watch.armExecution(watched),"guard execution observer");
    for(unsigned mode=0;mode<3;++mode){
        object.put(0x178,true);object.put(0x230,mode==2);_mm_setcsr(mode==1?0x1d80u:0x1f80u);unsigned long fault=0;
        require(invoke(const_cast<unsigned char*>(base+0x8d570),&object,fault),"native fallback guard fixture");
    }
    watch.stop();require(watch.hits[0]==1&&watch.hits[1]==2&&!watch.overflow&&!watch.ownershipLost,"actual fast/unmasked/vertex-mod dispatch coverage");
}
void timings(const unsigned char* base,OwnedPatch& patch,const std::vector<Captured>& records,std::size_t count,const std::filesystem::path& path){
    std::array<void*,160> table{};table[0x400/8]=reinterpret_cast<void*>(&dirty);table[0x408/8]=reinterpret_cast<void*>(&setDirty);table[0x308/8]=reinterpret_cast<void*>(&transform);
    Memory parent;parent.put(0x17b,true);std::vector<Memory> objects(count);
    for(std::size_t i=0;i<count;++i){auto& object=objects[i];const auto& source=records[i].sprite;
        object.put(0,table.data());object.put(0x170,std::uintptr_t{1});object.put(0x11e,bool(source.flags&1024));object.put(0x149,bool(source.flags&512));
        object.put(0x22f,bool(source.flags&128));object.put(0x230,bool(source.flags&64));
        if(source.flags&1)object.put(0xd8,parent.data.data());
        object.put(0x280,source.matrix);object.put(0x17c,source.matrix);object.put(0x1bc,source.localRect[0]);object.put(0x1c0,source.localRect[1]);
        object.put(0x1b0,source.localRect[2]);object.put(0x1b4,source.localRect[3]);object.put(0x48,source.z);object.put(0x1cc,records[i].quad);
    }
    std::ofstream csv(path,std::ios::out|std::ios::noreplace);require(bool(csv),"new full-native kernel timing receipt");
    csv<<"round,variant,milliseconds,sprites,repeats,dirty_calls,transform_calls,setdirty_calls,checksum\n";
    std::uint64_t referenceHash=0;constexpr unsigned repeats=16;
    for(unsigned round=0;round<10;++round)for(unsigned order=0;order<2;++order){const unsigned variant=order^(round&1);patch.toggle(variant!=0);
        std::fill(std::begin(calls),std::end(calls),0);_mm_setcsr(0x1f80);
        auto method=reinterpret_cast<void(*)(void*)>(const_cast<unsigned char*>(base+0x8d570));const auto start=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<repeats;++repeat)for(auto& object:objects){object.put(0x178,true);method(&object);}
        const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::uint64_t checksum=14695981039346656037ull;
        for(const auto& object:objects)for(auto byte:object.data)checksum=(checksum^std::to_integer<unsigned char>(byte))*1099511628211ull;
        require(calls[0]==count*repeats&&calls[2]==count*repeats,"full native callbacks retained in timings");
        if(!round&&!variant)referenceHash=checksum;require(checksum==referenceHash,"full native timing outputs differ");
        if(round)csv<<round<<','<<variant<<','<<milliseconds<<','<<count<<','<<repeats<<','<<calls[0]<<','<<calls[1]<<','<<calls[2]<<','<<checksum<<'\n';
    }
    patch.toggle(false);require(bool(csv),"complete full-native kernel timing receipt");
}
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==4||argc==5,"Cocos DLL, V3 capture, NEW splice receipt and optional --timings");
    if(argc==5)require(std::wstring_view(argv[4])==L"--timings","explicit full-native kernel timings");
    chlorium::test::SystemInputGuard input;const auto module=loadNative(std::filesystem::path(argv[1]),input);
    const auto base=reinterpret_cast<unsigned char*>(module);OrderedKernel kernel;NativeSplice splice(base,kernel);unwindChecks(base,splice);
    OwnedPatch patch(base+NativeSplice::origin,splice.patchBytes());const auto saved=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{saved};
    patch.toggle(true);guardChecks(base,splice);patch.toggle(false);
    auto records=readCapture(argv[2]);const auto captured=records.size();appendArithmeticCases(records);
    std::mt19937 random(0x4405ca1u);
    for(unsigned i=0;i<4096;++i){auto record=records.back();record.sprite.flags=512|1024;
        for(auto& value:record.sprite.matrix)value=random();for(auto& value:record.sprite.localRect)value=random();record.sprite.z=random();records.push_back(record);}
    std::uint64_t compared=0,capturedMatch=0,capturedCompared=0;
    constexpr unsigned chunk=512,states=32;
    for(std::size_t first=0;first<records.size();first+=chunk){const auto last=std::min(records.size(),first+chunk);std::vector<NativeResult> expected;expected.reserve((last-first)*states);
        for(auto index=first;index<last;++index)for(unsigned state=0;state<states;++state){
            const auto controls=0x1f80u|((state&3)<<13)|((state&4)?0x40u:0)|((state&8)?0x8000u:0)|((state&16)?0x3fu:0);
            expected.push_back(nativeResult(records[index],controls,base));
        }
        patch.toggle(true);unsigned at=0;
        for(auto index=first;index<last;++index)for(unsigned state=0;state<states;++state){
            const auto controls=0x1f80u|((state&3)<<13)|((state&4)?0x40u:0)|((state&8)?0x8000u:0)|((state&16)?0x3fu:0);
            const auto actual=nativeResult(records[index],controls,base);const auto& reference=expected[at++];
            require(actual.fpState==reference.fpState&&!std::memcmp(&actual.quad,&reference.quad,sizeof(actual.quad)),"full native method differs with arithmetic splice");++compared;
            if(index<captured&&!state&&chlorium::research::corners::domain(records[index].sprite)){++capturedCompared;capturedMatch+=!std::memcmp(&actual.quad,&records[index].quad,sizeof(actual.quad));}
        }
        patch.toggle(false);
    }
    require(hash(base+0x8d570,0x719)==0x302554714c2943d2ull,"native body completely restored");
    if(argc==5)timings(base,patch,records,captured,std::filesystem::path(argv[3]).wstring()+L".kernel.csv");
    std::ofstream receipt(std::filesystem::path(argv[3]),std::ios::out|std::ios::noreplace);require(bool(receipt),"new splice receipt");
    receipt<<"{\"diagnostic_only\":true,\"native_method_comparisons\":"<<compared<<",\"captured_compared\":"<<capturedCompared<<",\"captured_matching\":"<<capturedMatch<<",\"native_unwind_parity\":true,\"fast_and_fallback_guards_observed\":true,\"native_body_restored\":true,\"game_fps_evidence\":false}\n";
    require(bool(receipt),"complete native splice receipt");
    std::cout<<"PASS "<<compared<<" complete native-method comparisons, guarded dispatch, native-frame unwind and body restoration; no GD process\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
