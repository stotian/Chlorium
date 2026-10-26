#pragma once
#include "../R4-19/OwnedAreaOracle.hpp"
#include "NativeLayout.hpp"
#include "../../tests/SystemInputGuard.hpp"
#include <string>

class OwnedTransformOracle {
    chlorium::test::SystemInputGuard systemInput_;
    unsigned char* code_=nullptr;
    HMODULE cocos_=nullptr,crt_=nullptr;
    std::uint64_t cookie_=0;
    void (*retain_)(void*)=nullptr;
    void (*release_)(void*)=nullptr;
    float (*atan2_)(float,float)=nullptr;
    OwnedAreaOracle area_;
    static inline thread_local const OwnedTransformOracle* active_=nullptr;
    static void cookieCheck(std::uint64_t value) {if(!active_||value!=active_->cookie_)std::abort();}
    static std::uint64_t hash(std::span<const unsigned char> bytes) {
        std::uint64_t h=14695981039346656037ull;for(auto b:bytes)h=(h^b)*1099511628211ull;return h;
    }
    static std::vector<unsigned char> image(const char* path,unsigned timestamp,unsigned imageSize) {
        std::ifstream f(path,std::ios::binary);std::vector<unsigned char> raw((std::istreambuf_iterator<char>(f)),{});
        auto bad=[] {throw std::runtime_error("unsupported owned transform image");};
        IMAGE_DOS_HEADER dos{};
        if(raw.size()<sizeof(dos))bad();std::memcpy(&dos,raw.data(),sizeof(dos));
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||static_cast<std::size_t>(dos.e_lfanew)>raw.size()||sizeof(IMAGE_NT_HEADERS64)>raw.size()-dos.e_lfanew)bad();
        IMAGE_NT_HEADERS64 pe{};std::memcpy(&pe,raw.data()+dos.e_lfanew,sizeof(pe));
        if(pe.Signature!=IMAGE_NT_SIGNATURE||pe.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||pe.FileHeader.TimeDateStamp!=timestamp||
           pe.OptionalHeader.SizeOfImage!=imageSize||pe.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||pe.FileHeader.NumberOfSections>64)bad();
        const auto table=static_cast<std::size_t>(dos.e_lfanew)+sizeof(pe);
        if(pe.FileHeader.NumberOfSections>(raw.size()-table)/sizeof(IMAGE_SECTION_HEADER))bad();
        std::vector<unsigned char> mapped(imageSize);
        for(unsigned n=0;n<pe.FileHeader.NumberOfSections;++n) {
            IMAGE_SECTION_HEADER s{};std::memcpy(&s,raw.data()+table+n*sizeof(s),sizeof(s));
            if(s.PointerToRawData>raw.size()||s.SizeOfRawData>raw.size()-s.PointerToRawData||s.VirtualAddress>mapped.size()||s.SizeOfRawData>mapped.size()-s.VirtualAddress)bad();
            std::memcpy(mapped.data()+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);
        }
        return mapped;
    }
public:
    using ResetFunction=bool(*)(void*,void*,bool);
    OwnedTransformOracle(const OwnedTransformOracle&)=delete;
    OwnedTransformOracle& operator=(const OwnedTransformOracle&)=delete;
    OwnedTransformOracle() {
        using namespace chlorium::research::transform_layout;
        auto gd=image("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/GeometryDash.exe",0x69702da0,0xa3a000);
        cocos_=LoadLibraryExW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Geometry Dash\\libcocos2d.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        crt_=LoadLibraryExW(L"ucrtbase.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        try {
            if(!cocos_||!crt_)throw std::runtime_error("oracle libraries unavailable");
            chlorium::test::SystemInputGuard::assertNoGeode();
            retain_=reinterpret_cast<void(*)(void*)>(cocosExport("?retain@CCObject@cocos2d@@QEAAXXZ"));
            release_=reinterpret_cast<void(*)(void*)>(cocosExport("?release@CCObject@cocos2d@@QEAAXXZ"));
            atan2_=reinterpret_cast<float(*)(float,float)>(GetProcAddress(crt_,"atan2f"));
            if(!atan2_)throw std::runtime_error("missing native atan2f");
            auto cc=image("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/libcocos2d.dll",0x69702d99,0x1c5000);
            for(const auto& body:cocosBodies)
                if(hash({cc.data()+body.rva,body.length})!=body.hash||
                   std::memcmp(reinterpret_cast<const unsigned char*>(cocos_)+body.rva,cc.data()+body.rva,body.length))
                    throw std::runtime_error("modified Cocos helper body");
            std::array<unsigned char,16384> target{};
            for(const auto& b:bodies) {
                if(b.rva>gd.size()||b.length>gd.size()-b.rva||b.offset+b.length>0x2000||hash({gd.data()+b.rva,b.length})!=b.hash)
                    throw std::runtime_error("native transform body fingerprint");
                std::memcpy(target.data()+b.offset,gd.data()+b.rva,b.length);
            }
            std::size_t next=0x2000;
            for(const auto& p:patches) {
                if(next+16>target.size()||p.site+p.next>0x2000||p.displacement+4>p.next)throw std::runtime_error("oracle relocation bounds");
                std::uintptr_t function=0;
                if(p.kind==Data) {
                    if(p.target>gd.size()||p.size>gd.size()-p.target||p.size>16||hash({gd.data()+p.target,p.size})!=p.hash)throw std::runtime_error("oracle constant fingerprint");
                    std::memcpy(target.data()+next,gd.data()+p.target,p.size);
                    if(p.site==0x1d)std::memcpy(&cookie_,gd.data()+p.target,8);
                } else if(p.kind==Import) {
                    function=reinterpret_cast<std::uintptr_t>(GetProcAddress(cocos_,p.symbol));
                    if(!function)throw std::runtime_error(std::string("missing Cocos export ")+p.symbol);
                    std::memcpy(target.data()+next,&function,8);
                } else {
                    if(p.kind==Reset)function=reinterpret_cast<std::uintptr_t>(area_.resetEntry());
                    if(p.kind==Move)function=reinterpret_cast<std::uintptr_t>(area_.moveEntry());
                    if(p.kind==Sqrt)function=reinterpret_cast<std::uintptr_t>(GetProcAddress(crt_,"sqrtf"));
                    if(p.kind==Atan2)function=reinterpret_cast<std::uintptr_t>(GetProcAddress(crt_,"atan2f"));
                    if(p.kind==Cookie)function=reinterpret_cast<std::uintptr_t>(&cookieCheck);
                    if(!function)throw std::runtime_error("unresolved oracle direct call");
                    target[next]=0xff;target[next+1]=0x25;std::memcpy(target.data()+next+6,&function,8);
                }
                const auto delta=static_cast<std::int32_t>(next)-static_cast<std::int32_t>(p.site+p.next);
                std::memcpy(target.data()+p.site+p.displacement,&delta,4);next+=16;
            }
            code_=static_cast<unsigned char*>(VirtualAlloc(nullptr,target.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
            if(!code_)throw std::runtime_error("oracle executable storage");std::memcpy(code_,target.data(),target.size());
            DWORD old=0;if(!VirtualProtect(code_,target.size(),PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),code_,target.size()))throw std::runtime_error("oracle W then RX");
        } catch(...) {if(code_)VirtualFree(code_,0,MEM_RELEASE);if(crt_)FreeLibrary(crt_);if(cocos_)FreeLibrary(cocos_);code_=nullptr;crt_=cocos_=nullptr;throw;}
    }
    ~OwnedTransformOracle(){if(code_)VirtualFree(code_,0,MEM_RELEASE);if(crt_)FreeLibrary(crt_);if(cocos_)FreeLibrary(cocos_);}
    void* realPositionEntry()const{return code_+0x1000;}
    ResetFunction reset()const{return reinterpret_cast<ResetFunction>(area_.resetEntry());}
    void retain(void* node)const{retain_(node);}
    void release(void* node)const{release_(node);}
    auto atan2()const{return atan2_;}
    bool identityHelper(void* node)const {
        const auto table=*reinterpret_cast<const std::uintptr_t* const*>(node);
        if(reinterpret_cast<const unsigned char*>(table)!=reinterpret_cast<const unsigned char*>(cocos_)+0xe5a40)return false;
        for(const auto& body:chlorium::research::transform_layout::cocosBodies)
            if(table[body.slot/8]!=reinterpret_cast<std::uintptr_t>(cocos_)+body.rva)return false;
        return true;
    }
    void move(void* layer,void* object,float x,float y)const{area_.move(layer,object,x,y);}
    void* cocosExport(const char* name)const{auto p=GetProcAddress(cocos_,name);if(!p)throw std::runtime_error(name);return reinterpret_cast<void*>(p);}
    void transform(void* layer,void* center,void* objects,float x,float y,bool resetVisual)const {
        active_=this;
        reinterpret_cast<void(*)(void*,void*,void*,float,float,bool)>(code_)(layer,center,objects,x,y,resetVisual);
        active_=nullptr;
    }
};
