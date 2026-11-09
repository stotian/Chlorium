#include "../R4-33/GpuFixture.hpp"
#include "../R4-71/MixedProgram.hpp"
#include <intrin.h>
namespace mp=chlorium::research::mixedprogram;
extern "C" {void* r71PointCtor=nullptr;void* r71PointApply=nullptr;void R71Rotation(void*,const mp::Command*);void R72PointGateway();}
static void* original=nullptr;static const mp::Command* expected=nullptr;static void* expectedObject=nullptr;static unsigned observed=0;
extern "C" chlorium::affine::Point* R72Observe(chlorium::affine::Point* out,const chlorium::affine::Point* point,const chlorium::affine::Matrix* matrix,void* object,const unsigned char* frame,unsigned finish,void* layer,float angle){
    require(object==expectedObject && layer==reinterpret_cast<const unsigned char*>(expected)-0x3a0,"native RBX/RDI object/layer gateway");
    require(reinterpret_cast<const void*>(matrix)==expected->linear && finish==expected->control[2] && angle==0,"native matrix/R12B/XMM6 gateway");
    float values[4];std::memcpy(values,frame+0x58,16);require(values[0]==expected->affineCenter[2] && values[1]==expected->affineCenter[3] && values[2]==expected->move[0] && values[3]==expected->move[1],"original native stack frame gateway");
    require(reinterpret_cast<const unsigned char*>(out)==frame+0x68 && reinterpret_cast<const unsigned char*>(point)==frame+0x78,"native stack output/input ranges");++observed;
    return reinterpret_cast<chlorium::affine::Apply>(original)(out,point,matrix);
}
int wmain(int argc,wchar_t** argv)try{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==2,"owned Cocos DLL required");chlorium::test::SystemInputGuard input;const auto cocos=loadNative(argv[1],input);
    struct Module{HMODULE value;~Module(){FreeLibrary(value);}}module{cocos};r71PointCtor=reinterpret_cast<void*>(GetProcAddress(cocos,"??0CCPoint@cocos2d@@QEAA@MM@Z"));
    original=reinterpret_cast<void*>(GetProcAddress(cocos,"?__CCPointApplyAffineTransform@cocos2d@@YA?AVCCPoint@1@AEBV21@AEBUCCAffineTransform@1@@Z"));require(original && r71PointCtor,"native point exports");
    const auto saved=_mm_getcsr();std::array<unsigned char,0x600> a{},b{};mp::Command command;command.control[0]=7;command.linear[0]=1;command.linear[1]=.125f;command.linear[2]=-.25f;command.linear[3]=1;
    command.affineCenter[0]=.1f;command.affineCenter[1]=-.1f;command.affineCenter[2]=5;command.affineCenter[3]=7;command.move[0]=.5f;command.move[1]=-.25f;
    for(unsigned fp:{0x1f80u,0x1fc0u,0x9f80u,0x9fc0u})for(unsigned i=0;i<128;++i){a.fill(0);const double x=double(i)*.1,y=-double(i)*.125;std::memcpy(a.data()+0x3b8,&x,8);std::memcpy(a.data()+0x3c0,&y,8);b=a;command.control[2]=i%2;
        r71PointApply=original;_mm_setcsr(fp);SetLastError(17);R71Rotation(a.data(),&command);const auto flags=_mm_getcsr();const auto error=GetLastError();
        r71PointApply=reinterpret_cast<void*>(&R72PointGateway);expected=&command;expectedObject=b.data();_mm_setcsr(fp);SetLastError(17);R71Rotation(b.data(),&command);
        require(a==b && _mm_getcsr()==flags && GetLastError()==error,"gateway preserves complete native arithmetic bytes/FP/LastError");}
    _mm_setcsr(saved);std::cout<<"PASS "<<observed<<" native rotation gateway ABI witnesses; no GD launch\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
