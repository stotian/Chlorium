// Written during the no-test phase; compile/run only after tests are authorized.
#define CHLORIUM_RESIDENT_FAST_R58 1
#define main residentSceneChecksR57
#include "../R4-47/scene_checks.cpp"
#undef main
#include "CanonicalIndexCache.hpp"

static void topologyChecks(){
    using chlorium::research::continuous::CanonicalIndexCache;
    std::vector<std::uint16_t> cpu(8*6);
    constexpr unsigned pattern[]{0,1,2,3,2,1};
    for(unsigned n=0;n<cpu.size();++n)cpu[n]=std::uint16_t(n/6*4+pattern[n%6]);
    auto gpu=cpu;
    CanonicalIndexCache cache;
    CanonicalIndexCache::Owner owner{1,reinterpret_cast<std::uintptr_t>(cpu.data()),2,3,4,8};
    unsigned reads=0;
    auto read=[&](std::span<unsigned char> out){++reads;require(out.size()==gpu.size()*2,"bounded index download");std::memcpy(out.data(),gpu.data(),out.size());return true;};
    require(cache.acquire(owner,cpu,2,read).size()==24 && reads==1,"first use verifies native topology");
    require(cache.acquire(owner,cpu,8,read).size()==96 && reads==1,"count growth reuses verified capacity");
    require(cache.acquire(owner,cpu,1,read).size()==12 && reads==1,"shrinking count preserves prefix");
    ++owner.uid;require(!cache.acquire(owner,cpu,8,read).empty() && reads==2,"native lifetime invalidates proof");
    ++owner.buffer;require(!cache.acquire(owner,cpu,8,read).empty() && reads==3,"buffer identity invalidates proof");
    ++owner.context;require(!cache.acquire(owner,cpu,8,read).empty() && reads==4,"context identity invalidates proof");
    cpu[0]=1;require(cache.acquire(owner,cpu,8,read).empty(),"CPU mutation rejects cached proof");
    require(cache.acquire(owner,cpu,8,read).empty(),"GPU/CPU disagreement rejects new proof");
    gpu=cpu;require(cache.acquire(owner,cpu,8,read).empty(),"matching noncanonical indices still reject");
    cpu[0]=gpu[0]=0;
    auto fail=[](std::span<unsigned char>){return false;};
    require(cache.acquire(owner,cpu,8,fail).empty(),"failed download cannot establish proof");
    require(cache.acquire(owner,cpu,9,read).empty(),"draw cannot exceed verified capacity");
    require(cache.acquire(owner,std::span<const std::uint16_t>(cpu).first(6),1,read).empty(),"short CPU snapshot rejects");
    owner.cpuIndices=0;require(cache.acquire(owner,cpu,1,read).empty(),"CPU pointer identity must match input");
    owner.cpuIndices=reinterpret_cast<std::uintptr_t>(cpu.data());owner.capacity=4;
    auto smallRead=[&](std::span<unsigned char> out){std::memcpy(out.data(),gpu.data(),out.size());return true;};
    require(cache.acquire(owner,std::span<const std::uint16_t>(cpu).first(24),4,smallRead).size()==48,"capacity change establishes a new bounded proof");
}
static void encodingChecks(){
    VerifiedEncoding cache;
    chlorium::research::pass::SpriteRecord source{};
    NativeQuadState quad{};Geometry g{};Parameters p{};
    unsigned encodings=0;
    auto encode=[&](const auto& s,const auto& q,auto& outG,auto& outP){++encodings;encodeSprite(s,q,outG,outP);};
    require(!cache.get(source,quad,0x1f80,g,p,encode),"first source is encoded");
    require(cache.get(source,quad,0x1f80,g,p,encode) && encodings==1,"identical encoding reuses exact proof");
    const auto original=source;
    for(std::size_t n=0;n<sizeof(source);++n){
        std::memcpy(&source,&original,sizeof(source));reinterpret_cast<unsigned char*>(&source)[n]^=1;
        require(!cache.get(source,quad,0x1f80,g,p,encode),"any source byte invalidates encoding proof");
        std::memcpy(&source,&original,sizeof(source));cache.get(source,quad,0x1f80,g,p,encode);
    }
    for(std::size_t n=0;n<sizeof(quad);++n){
        reinterpret_cast<unsigned char*>(&quad)[n]^=1;
        require(!cache.get(source,quad,0x1f80,g,p,encode),"any quad byte invalidates encoding proof");
        reinterpret_cast<unsigned char*>(&quad)[n]^=1;cache.get(source,quad,0x1f80,g,p,encode);
    }
    require(!cache.get(source,quad,0x3f80,g,p,encode),"FP control change invalidates encoding proof");
}
static void maintenanceFlagChecks(){
    chlorium::research::pass::SpriteRecord source{};
    source.sprite=0x1000;source.id=7;source.flags=512|1024;source.quadMatches=1;
    source.matrix[0]=source.matrix[3]=0x3f800000;source.localRect[2]=source.localRect[3]=0x3f800000;
    const auto fp=_mm_getcsr();_mm_setcsr(0x1f80);
    NativeQuadState quad{};require(chlorium::research::corners::materialize(source,quad),"render flag fixture");
    SpriteSources sources(1);MemorySink sink;
    std::array<SpriteKey,1> keys{sources.create(source,quad)};
    require(bool(keys[0]) && sources.painterOrder(keys) && sources.scene().synchronize(sink),"render flag initial seed");
    const auto revision=sources.scene().revision(),writes=sink.writes;
    for(const auto bit:{2u,4u,8u,16u,32u,256u}){
        source.flags^=bit;
        require(sources.publish(keys[0],source,quad) && sources.scene().revision()==revision,"maintenance flag cannot dirty identical render state");
        require(sources.scene().synchronize(sink) && sink.writes==writes,"maintenance flag cannot force GPU upload");
    }
    source.flags|=64u;
    require(sources.publish(keys[0],source,quad) && sources.scene().revision()!=revision,"unsupported geometry flag still changes the rendering path");
    source.flags|=0x800u;
    require(!sources.publish(keys[0],source,quad) && !sources.scene().usable(),"unknown flags still reject before normalization");
    _mm_setcsr(fp);
}
static void addressChecks(){
    AddressSet addresses;
    require(!addresses.contains(1) && !addresses.insert(1),"unprepared address scratch rejects");
    require(addresses.reset(0) && !addresses.insert(1),"empty active set remains empty");
    require(addresses.reset(2048),"bounded identity scratch");
    std::unordered_set<std::uint64_t> reference;
    for(unsigned n=0;n<2048;++n){
        const auto key=std::uint64_t(n+1)*16;
        require(addresses.insert(key)==reference.insert(key).second,"flat identity insertion agrees with reference");
        require(!addresses.insert(key) && addresses.contains(key),"duplicate is rejected without losing membership");
    }
    require(!addresses.insert(0) && !addresses.insert(UINT64_MAX),"capacity and zero identity stay guarded");
    for(std::uint64_t n=0;n<65536;++n)require(addresses.contains(n)==reference.contains(n),"membership agrees through absent and colliding values");
    for(unsigned step=0;step<64;++step){
        require(addresses.reset(37),"reuse identity scratch");
        for(unsigned n=0;n<37;++n)require(addresses.insert(UINT64_MAX-std::uint64_t(step)*64-n),"churn accepts fresh high addresses");
        require(!addresses.contains(16),"previous active identities cannot survive reset");
    }
    require(addresses.reset(65536) && !addresses.reset(65537),"maximum scratch capacity is enforced");
    for(std::uint64_t n=1;n<=65536;++n)require(addresses.insert(n),"bounded table retains room for termination");
    require(!addresses.contains(65537),"missing lookup terminates at maximum supported load");
}
int main()try{
    if(residentSceneChecksR57())return 1;
    topologyChecks();encodingChecks();maintenanceFlagChecks();addressChecks();
    std::cout<<"R58 topology and encoding controls passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
