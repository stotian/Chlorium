#include "../R4-33/GpuFixture.hpp"
#include "../R4-47/ResidentRenderer.hpp"
#include "../R4-33/SpriteEncoding.hpp"
#include <numeric>
using namespace chlorium::research::resident;
int wmain(int argc,wchar_t** argv)try{
    require(argc==5,"Cocos DLL, actual capture, resident.vert, NEW receipt");require(!std::filesystem::exists(argv[4]),"new oracle receipt");
    chlorium::test::SystemInputGuard input;const auto native=loadNative(argv[1],input);
    auto records=readCapture(argv[2]);const auto captured=records.size();appendArithmeticCases(records);
    const auto original=_mm_getcsr();struct Restore{unsigned fp;~Restore(){_mm_setcsr(fp);}}restore{original};
    Graphics graphics;graphics.create(true,6);
    std::ifstream shaderFile(argv[3],std::ios::binary);require(bool(shaderFile),"resident source");const std::string shader((std::istreambuf_iterator<char>(shaderFile)),{});
    std::size_t eligible=0,unsupported=0,mutants=0,batches=0;
    for(std::size_t start=0;start<records.size();start+=8192){
        const auto count=std::min<std::size_t>(8192,records.size()-start);
        std::vector<Geometry> g(count);std::vector<Parameters> p(count);std::vector<NativeQuadState> fallback(count),expected(count),actual(count);
        std::vector<std::uint32_t> order(count);std::iota(order.begin(),order.end(),0u);
        std::size_t chosen=count;
        for(std::size_t n=0;n<count;++n){const auto& r=records[start+n];
            if(chlorium::research::corners::domain(r.sprite)){
                expected[n]=nativeResult(r,0x1f80,reinterpret_cast<const unsigned char*>(native)).quad;++eligible;
                if(chosen==count && r.sprite.flags==(512|1024))chosen=n;
            }else{expected[n]=r.quad;++unsupported;}
            encodeSprite(r.sprite,expected[n],g[n],p[n]);fallback[n]=expected[n];
        }
        _mm_setcsr(0x1f80);GLuint buffers[4]{},vao=0;glGenBuffers(4,buffers);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        auto upload=[&](unsigned binding,const void* data,std::size_t bytes){glBindBuffer(GLStateSnapshot::storageTarget,buffers[binding]);glBufferData(GLStateSnapshot::storageTarget,bytes,data,GL_DYNAMIC_DRAW);glBindBufferBase(GLStateSnapshot::storageTarget,binding,buffers[binding]);};
        upload(0,g.data(),g.size()*sizeof(Geometry));upload(1,p.data(),p.size()*sizeof(Parameters));upload(2,order.data(),count*4);upload(3,fallback.data(),fallback.size()*sizeof(NativeQuadState));
        NativeVertexStream stream;require(stream.initialize(count,shader),"native slot compute initialize");
        std::vector<std::uint64_t> mask((count+63)/64);
        if(count%64){auto invalid=mask;invalid.back()=UINT64_MAX;require(!stream.publishChanges(invalid,false,false),"out-of-capacity dirty bits reject");}
        require(stream.publishChanges(mask,true,true) && stream.materialize(count) && stream.copyOwnedQuads(actual),"full native slot seed");
        require(!std::memcmp(actual.data(),expected.data(),count*sizeof(NativeQuadState)),"GPU slot compute differs from actual native Cocos quad bytes");
        require(stream.metrics.lastSlots==count,"seed accounts for all output slots");
        require(stream.materialize(count) && stream.metrics.lastSlots==0,"unchanged native slots reused");
        if(chosen<count){
            auto changed=records[start+chosen];std::fill_n(changed.sprite.matrix,4,0);
            changed.sprite.matrix[4]=std::bit_cast<std::uint32_t>(.25f);changed.sprite.matrix[5]=std::bit_cast<std::uint32_t>(.75f);
            encodeParameters(changed.sprite,changed.quad,p[chosen]);
            expected[chosen]=nativeResult(changed,0x1f80,reinterpret_cast<const unsigned char*>(native)).quad;_mm_setcsr(0x1f80);
            glBindBuffer(GLStateSnapshot::storageTarget,buffers[1]);glBufferSubData(GLStateSnapshot::storageTarget,0,p.size()*sizeof(Parameters),p.data());
            require(stream.materialize(count) && stream.copyOwnedQuads(actual),"intentionally missing dirty notification remains a cached output");
            require(std::memcmp(actual.data(),expected.data(),count*sizeof(NativeQuadState))!=0,"missing-notification mutant must be observable");++mutants;
            mask[chosen/64]=std::uint64_t{1}<<(chosen%64);
            require(stream.publishChanges(mask,false,false) && stream.materialize(count) && stream.copyOwnedQuads(actual),"single exact dirty slot repaired");
            require(stream.metrics.lastSlots==1 && !std::memcmp(actual.data(),expected.data(),count*sizeof(NativeQuadState)),"exact mask repairs only its native slot");
        }
        stream.close();glDeleteBuffers(4,buffers);glDeleteVertexArrays(1,&vao);++batches;
    }
    std::ofstream receipt(std::filesystem::path(argv[4]),std::ios::out|std::ios::noreplace);require(bool(receipt),"new native slot receipt");
    receipt<<"{\"diagnostic_only\":true,\"game_fps_evidence\":false,\"capture_records\":"<<captured<<",\"eligible\":"<<eligible<<",\"unsupported\":"<<unsupported<<",\"batches\":"<<batches<<",\"gpu_mismatches\":0,\"missing_notification_mutants_detected\":"<<mutants<<"}\n";
    require(bool(receipt),"complete native slot receipt");std::cout<<"PASS "<<records.size()<<" GPU slot quads match native bytes; "<<mutants<<" missing-notification mutants caught; NO game FPS proof\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
