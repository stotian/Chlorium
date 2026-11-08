#include "NativeCornerOracleFixture.hpp"
#include "GpuFixture.hpp"
#include <chrono>
#include <iomanip>
#include <numeric>

namespace {
constexpr GLenum storage=0x90d2,compute=0x91b9;
using Dispatch=void(APIENTRY*)(GLuint,GLuint,GLuint);
struct alignas(16) Source {std::uint32_t matrix[8]{},rect[4]{},control[4]{};};
static_assert(sizeof(Source)==64);
void poison(NativeQuadState& quad){for(auto& vertex:quad.corners){vertex.x=std::bit_cast<float>(0x7fc00001u);vertex.y=vertex.x;vertex.z=vertex.x;}}
std::size_t mismatch(const std::vector<NativeQuadState>& a,const std::vector<NativeQuadState>& b){
    require(a.size()==b.size(),"comparison count");std::size_t count=0;
    for(std::size_t i=0;i<a.size();++i)count+=std::memcmp(&a[i],&b[i],sizeof(a[i]))!=0;
    return count;
}
}
#include "ResidentVertexChecks.hpp"
#include "ResidentReplayChecks.hpp"
#include "ResidentPixelChecks.hpp"
#include "ResidentTimingChecks.hpp"
int wmain(int argc,wchar_t** argv)try {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    require(argc==6||(argc==7&&std::wstring_view(argv[6])==L"--timings"),"Cocos DLL, V3 capture, compute shader, resident vertex shader, new receipt and optional --timings are required");
    chlorium::test::SystemInputGuard systemInput;auto native=loadNative(argv[1],systemInput);
    auto records=readCapture(argv[2]);require(!records.empty(),"nonempty source capture");
    const auto capturedCount=records.size();appendArithmeticCases(records);
    std::vector<Source> sources(records.size());std::vector<NativeQuadState> expected(records.size()),seed(records.size());
    std::size_t eligible=0,unsupported=0,capturedMatches=0;
    const auto saved=_mm_getcsr();struct Restore{unsigned value;~Restore(){_mm_setcsr(value);}}restore{saved};
    for(std::size_t i=0;i<records.size();++i){
        const auto& record=records[i];auto& source=sources[i];
        std::copy(std::begin(record.sprite.matrix),std::end(record.sprite.matrix),source.matrix);
        std::copy(std::begin(record.sprite.localRect),std::end(record.sprite.localRect),source.rect);
        source.control[0]=record.sprite.flags;source.control[1]=record.sprite.z;
        source.control[2]=chlorium::research::corners::domain(record.sprite);
        expected[i]=seed[i]=record.quad;
        if(source.control[2]){++eligible;expected[i]=nativeResult(record,0x1f80,reinterpret_cast<const unsigned char*>(native)).quad;
            if(i<capturedCount)capturedMatches+=!std::memcmp(&expected[i],&record.quad,sizeof(record.quad));poison(seed[i]);}
        else ++unsupported;
    }
    require(eligible!=0,"eligible captured corners");
    Graphics graphics;graphics.create();const auto shader=program(argv[3]);glUseProgram(shader);
    auto dispatch=reinterpret_cast<Dispatch>(wglGetProcAddress("glDispatchCompute"));require(dispatch&&glMemoryBarrier,"compute functions");
    const auto count=glGetUniformLocation(shader,"count"),mutant=glGetUniformLocation(shader,"ignoreOpacity");
    require(count>=0&&mutant>=0,"shader controls");glUniform1ui(count,static_cast<GLuint>(records.size()));
    GLuint buffers[2]{};glGenBuffers(2,buffers);
    glBindBuffer(storage,buffers[0]);glBufferData(storage,sources.size()*sizeof(Source),sources.data(),GL_STATIC_DRAW);glBindBufferBase(storage,0,buffers[0]);
    glBindBuffer(storage,buffers[1]);glBufferData(storage,seed.size()*sizeof(seed[0]),seed.data(),GL_DYNAMIC_COPY);glBindBufferBase(storage,1,buffers[1]);
    std::vector<NativeQuadState> actual(seed.size());
    auto run=[&](bool ignore){glUniform1i(mutant,ignore);dispatch(static_cast<GLuint>((records.size()+63)/64),1,1);glMemoryBarrier(0x2200);
        glBindBuffer(storage,buffers[1]);glGetBufferSubData(storage,0,actual.size()*sizeof(actual[0]),actual.data());require(glGetError()==GL_NO_ERROR,"corner dispatch/readback");return mismatch(actual,expected);};
    const auto wrong=run(false);require(wrong==0,"GPU differs from actual native Cocos corners");
    const auto mutation=run(true);require(mutation!=0,"opacity mutation must be detected");
    glBindBuffer(storage,buffers[1]);glBufferSubData(storage,0,seed.size()*sizeof(seed[0]),seed.data());require(run(false)==0,"restore correct shader branch");
    checkResidentVertices(argv[4],records,expected);
    const auto replay=checkResidentReplay(argv[4],records);
    checkReplayTransitions(argv[4]);
    const auto pixelCases=checkResidentPixels(argv[4],reinterpret_cast<const unsigned char*>(native));
    if(argc==7)residentTimings(argv[4],records,std::filesystem::path(argv[5]).wstring()+L".kernel.csv");
    std::cout<<"PASS "<<eligible<<" actual-native GPU quads (capture plus "<<records.size()-capturedCount<<" synthetic records); unsupported "<<unsupported<<"; opacity mutant rejected on "<<mutation<<" quads; GPU "<<glGetString(GL_RENDERER)<<'\n';
    std::ofstream output(std::filesystem::path(argv[5]),std::ios::out|std::ios::noreplace);require(bool(output),"refuse existing GPU receipt");
    output<<"{\"diagnostic_only\":true,\"gpu_renderer\":"<<std::quoted(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))<<",\"gl_version\":"<<std::quoted(reinterpret_cast<const char*>(glGetString(GL_VERSION)))
          <<",\"capture_records\":"<<capturedCount<<",\"synthetic_records\":"<<records.size()-capturedCount<<",\"eligible_quads\":"<<eligible<<",\"unsupported_quads\":"<<unsupported<<",\"captured_native_matches\":"<<capturedMatches
          <<",\"gpu_mismatches\":"<<wrong<<",\"resident_vertex_feedback_matches\":true,\"resident_order_indirection_matches\":true,\"resident_transition_cases\":5,\"resident_exact_pixel_cases\":"<<pixelCases<<",\"zero_mvp_black_mutant_rejected\":true,\"mutation_mismatches\":"<<mutation
          <<",\"replay_second_frame\":{\"retained\":"<<replay.retained<<",\"added\":"<<replay.added<<",\"geometry_upload_bytes\":"<<replay.geometryBytes
          <<",\"parameter_upload_bytes\":"<<replay.parameterBytes<<",\"fallback_upload_bytes\":"<<replay.fallbackBytes<<",\"order_upload_bytes\":"<<replay.orderBytes
          <<",\"full_quad_upload_bytes_reference\":"<<replay.nativeQuadBytes<<"},\"fps_gain_proven\":false,\"native_callback_removal_proven\":false}\n";
    require(bool(output),"write GPU receipt");glDeleteBuffers(2,buffers);glDeleteProgram(shader);return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
