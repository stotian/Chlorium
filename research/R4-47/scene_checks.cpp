#include "RetainedScene.hpp"
#include "NativeSpriteSources.hpp"
#include <array>
#include <iostream>
#include <functional>
using namespace chlorium::research::resident;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

struct MemorySink {
    std::uint64_t id=nextIdentity(),owner=0,revision=0;
    std::size_t capacity=0,count=0,writes=0;
    std::array<std::vector<std::byte>,4> bytes;
    std::array<std::size_t,4> sent{};
    std::vector<std::uint64_t> quadMask;
    bool seeded=false,reordered=false;
    bool fail=false,failCommit=false;
    std::function<void()> onWrite;
    static constexpr std::array<std::size_t,4> stride{64,48,4,96};
    std::uint64_t identity() const{return id;}
    bool belongsTo(std::uint64_t who,std::size_t size) const{return owner==who && owner && capacity==size;}
    bool matches(std::uint64_t who,std::uint64_t version) const{return owner==who && owner && revision==version;}
    bool begin(std::uint64_t,std::size_t size){
        if(capacity!=size){capacity=size;for(unsigned n=0;n<4;++n)bytes[n].resize(size*stride[n]);}
        owner=revision=0;count=0;sent={};writes=0;return true;
    }
    bool write(Lane lane,std::size_t first,std::span<const std::byte> source){
        const auto n=static_cast<unsigned>(lane);++writes;
        if(onWrite){auto callback=std::move(onWrite);onWrite={};callback();}
        if(fail){fail=false;return false;}
        require(n<4 && first*stride[n]+source.size()<=bytes[n].size(),"bounded sink write");
        std::copy(source.begin(),source.end(),bytes[n].begin()+first*stride[n]);sent[n]+=source.size();return true;
    }
    bool commit(std::uint64_t who,std::uint64_t version,std::size_t size,std::span<const std::uint64_t> mask,bool seed,bool orderChanged){
        if(failCommit){failCommit=false;return false;}
        quadMask.assign(mask.begin(),mask.end());seeded=seed;reordered=orderChanged;owner=who;revision=version;count=size;return true;
    }
    template<class T>T read(unsigned lane,std::size_t at)const{
        T result{};std::memcpy(&result,bytes[lane].data()+at*sizeof(T),sizeof(T));return result;
    }
};
static Geometry geometry(){Geometry g{};g.control[0]=1;g.rect[2]=g.rect[3]=0x3f800000;return g;}
static Parameters parameters(){Parameters p{};p.matrix[0]=p.matrix[3]=0x3f800000;p.translationZFlags[3]=512|1024;return p;}

static void snapshotChecks(){
    const auto original=_mm_getcsr();_mm_setcsr(0x1f80);
    Scene scene(4);const auto g=geometry();auto p=parameters();
    for(unsigned c=0;c<4;++c)p.colors[c]=0x80604020+c;
    auto h=scene.create(g,p);require(bool(h),"snapshot source");
    auto first=scene.prepareNative(h);require(first && scene.current(*first),"owned native preparation");
    require(first->quad.corners[0].x==0 && first->quad.corners[0].y==1 && first->quad.corners[3].x==1,"native corner order");
    auto newStyle=first->quad;for(auto& corner:newStyle.corners){corner.x=999;corner.color=0;}
    _mm_setcsr(0x3f80);
    require(scene.style(h,newStyle),"style cannot requalify a previously accepted pose under new rounding");
    require(!scene.current(*first),"style invalidates an earlier prepared publication");
    for(unsigned control:{0x1f80u,0x3f80u,0x5f80u,0x7f80u,0x9fc0u,0x0f80u}){
        _mm_setcsr(control);const auto before=_mm_getcsr();
        auto snapshot=scene.prepareNative(h);
        require(snapshot && scene.current(*snapshot) && _mm_getcsr()==before,"native preparation preserves reader FP state");
        for(unsigned c=0;c<4;++c){
            require(!std::memcmp(&snapshot->quad.corners[c].x,&first->quad.corners[c].x,12),"native preparation uses accepted nearest pose");
            require(snapshot->quad.corners[c].color==0,"native preparation includes latest style");
        }
    }
    _mm_setcsr(0x1f80);auto prepared=scene.prepareNative(h);require(bool(prepared),"prepare before mutation");
    p.translationZFlags[0]=0x40000000;require(scene.parameters(h,p) && !scene.current(*prepared),"new pose invalidates prepared CPU copy");
    require(scene.erase(h) && !scene.current(*prepared),"destruction invalidates prepared CPU copy");
    auto reused=scene.create(g,p);require(reused.slot==h.slot && reused.generation!=h.generation && !scene.current(*prepared),"reused slot cannot revive a prepared CPU copy");
    Scene invalid(1);_mm_setcsr(0x3f80);
    require(!invalid.create(g,p) && !invalid.usable(),"unsupported producer rounding rejects computed geometry");
    _mm_setcsr(0x1f80);
    SpriteSources sources(4);chlorium::research::pass::SpriteRecord record{};
    record.sprite=0x2000;record.id=1;record.flags=512|1024;record.quadMatches=1;
    record.localRect[2]=0x3e99999a;record.localRect[3]=0x3f800000;
    record.matrix[0]=0x3dcccccd;record.matrix[3]=0x3f800000;
    NativeQuadState native{};_mm_setcsr(0x3f80);
    require(chlorium::research::corners::materialize(record,native),"downward native producer fixture");
    const auto key=sources.create(record,native);require(bool(key),"non-nearest producer retains native quad");
    _mm_setcsr(0x1f80);MemorySink sink;std::array<SpriteKey,1> order{key};
    require(sources.painterOrder(order) && sources.scene().synchronize(sink),"draw-time rounding differs from producer");
    const auto slot=sink.read<std::uint32_t>(2,0);
    require(sink.read<Geometry>(0,slot).control[0]==0,"producer rounding cannot be forgotten at nearest draw time");
    auto snapshot=sources.prepareNative(key);
    require(snapshot && !std::memcmp(&snapshot->quad,&native,sizeof(native)),"native fallback retains producer-rounded positions exactly");
    _mm_setcsr(original);
}

static void sourceChecks(){
    chlorium::research::pass::SpriteRecord source{};
    source.sprite=0x1000;source.id=7;source.flags=512|1024;source.quadMatches=1;
    source.matrix[0]=source.matrix[3]=0x3f800000;source.localRect[2]=source.localRect[3]=0x3f800000;
    NativeQuadState style{};for(auto& corner:style.corners)corner.color=0x80604020;
    auto nativePositions=[&]{
        const auto previous=_mm_getcsr();_mm_setcsr(0x1f80);
        const bool ok=chlorium::research::corners::materialize(source,style);_mm_setcsr(previous);
        require(ok,"closed native source arithmetic control");
    };
    nativePositions();
    SpriteSources sources(8);MemorySink sink;std::array<SpriteKey,1> order{sources.create(source,style)};
    require(bool(order[0]) && sources.painterOrder(order) && sources.scene().synchronize(sink),"native source initial seed");
    const auto firstRevision=sources.scene().revision();const auto firstWrites=sink.writes;
    require(sources.publish(order[0],source,style) && sources.scene().revision()==firstRevision,"unchanged source notification retains revision");
    require(sources.scene().synchronize(sink) && sink.writes==firstWrites,"unchanged source notification retains GPU acknowledgement");
    const auto styleSlot=sink.read<std::uint32_t>(2,0);
    auto colorOnly=style;for(auto& corner:colorOnly.corners){corner.color=0;corner.x=999;}
    const auto poseFlags=sink.read<Parameters>(1,styleSlot).translationZFlags[3];
    require(sources.publishStyle(order[0],colorOnly) && sources.scene().synchronize(sink),"classified color event ignores deferred CPU positions");
    require(sink.sent[0]==0 && sink.sent[3]==0 && sink.read<Parameters>(1,styleSlot).translationZFlags[3]==poseFlags,"color event cannot change pose/visibility flags");
    require(sink.read<Geometry>(0,styleSlot).control[0]==1,"classified color event retains owned reconstruction");
    require(sources.publishStyle(order[0],style) && sources.scene().synchronize(sink),"restore classified style");
    Geometry beforeG{},afterG{};Parameters beforeP{},afterP{};encodeSprite(source,style,beforeG,beforeP);
    for(auto& corner:style.corners)corner.x=std::bit_cast<float>(0x7fc01234u);
    encodeSprite(source,style,afterG,afterP);
    require(std::memcmp(&beforeG,&afterG,sizeof(beforeG))==0 && std::memcmp(&beforeP,&afterP,sizeof(beforeP))==0,"supported encoding never consumes native positions");
    nativePositions();source.matrix[4]=0x40000000;nativePositions();
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink),"native pose publication");
    require(sink.sent[0]==0 && sink.sent[1]==8*48 && sink.sent[2]==0 && sink.sent[3]==0,"native pose changes only parameter lane");
    style.corners[2].color=0x70402010;
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink) && sink.sent[0]==0 && sink.sent[1]==8*48,"color event retains geometry");
    style.corners[1].u=.5f;
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink) && sink.sent[0]==8*64 && sink.sent[1]==0,"UV event retains pose");
    source.localRect[2]=0x40000000;nativePositions();
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink) && sink.sent[0]==8*64 && sink.sent[1]==0,"rectangle event retains pose");
    source.flags|=64;for(auto& corner:style.corners)corner.x=std::bit_cast<float>(0x7fc01234u);
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink),"vertex modifier selects full native quad");
    const auto slot=sink.read<std::uint32_t>(2,0);
    require(sink.read<Geometry>(0,slot).control[0]==0 &&
            std::bit_cast<std::uint32_t>(sink.read<NativeQuadState>(3,slot).corners[0].x)==0x7fc01234u,"unsupported source preserves complete raw quad");
    source.flags&=~64u;nativePositions();require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink),"native source recovers supported arithmetic");
    const auto preservedState=_mm_getcsr();
    style.corners[0].x+=.125f;const auto entryState=_mm_getcsr();
    require(sources.publish(order[0],source,style) && _mm_getcsr()==entryState && sources.scene().synchronize(sink),"shadow comparison preserves incoming floating state");
    require(sink.read<Geometry>(0,slot).control[0]==0 && sink.read<NativeQuadState>(3,slot).corners[0].x==style.corners[0].x,"unexplained native geometry selects exact fallback");
    nativePositions();_mm_setcsr(preservedState);source.quadMatches=0;
    require(sources.publish(order[0],source,style) && sources.scene().synchronize(sink) && sink.read<Geometry>(0,slot).control[0]==0,"unmatched atlas source cannot become reconstructed geometry");
    source.quadMatches=1;
    require(sources.erase(order[0]) && !sources.scene().usable(),"native destruction invalidates current order");
    ++source.id;order[0]=sources.create(source,style);
    require(bool(order[0]) && sources.painterOrder(order) && sources.scene().synchronize(sink),"address reuse can precede order repair");
    auto stale=source;--stale.id;
    require(!sources.publish(order[0],stale,style) && !sources.scene().usable(),"stale native ID cannot mutate reused address");
    SpriteSources duplicate(2);require(duplicate.create(source,style) && !duplicate.create(source,style) && !duplicate.scene().usable(),"missing lifetime boundary cannot register twice");
    SpriteSources unknown(2);source.flags|=0x800;
    require(!unknown.create(source,style) && !unknown.scene().usable(),"unknown source flag rejects replacement ownership");
    SpriteSources absent(2);source.flags&=~0x800u;
    require(!absent.publish(order[0],source,style) && !absent.scene().usable(),"publication before registration rejects");
    SpriteSources reused(2);const auto old=reused.create(source,style);
    require(bool(old) && reused.erase(old),"retire original source lifetime");
    const auto fresh=reused.create(source,style);
    require(bool(fresh) && fresh.lifetime!=old.lifetime,"same address AND native ID receive a new lifetime");
    require(!reused.publish(old,source,style) && !reused.scene().usable(),"old lifetime cannot update identical reused identity");
}

int main()try{
    const auto g=geometry();auto p=parameters();
    Scene scene(256);MemorySink a,b;
    std::vector<Handle> handles;
    for(unsigned n=0;n<200;++n){auto h=scene.create(g,p);require(bool(h),"create supported");handles.push_back(h);}
    require(scene.painterOrder(handles) && scene.synchronize(a),"first complete seed");
    require(a.sent==std::array<std::size_t,4>{256*64,256*48,200*4,256*96},"exact initial seed volume");
    auto oldWrites=a.writes;require(scene.synchronize(a) && a.writes==oldWrites,"unchanged frame performs no uploads");
    const auto unchangedRevision=scene.revision();
    require(scene.painterOrder(handles) && scene.revision()==unchangedRevision && scene.synchronize(a) && a.writes==oldWrites,"unchanged order preserves revision and uploads");
    p.translationZFlags[0]=0x40000000;
    require(scene.parameters(handles[130],p) && scene.synchronize(a),"notification-only update");
    require(a.sent[0]==0 && a.sent[1]==64*48 && a.sent[2]==0 && a.sent[3]==0,"only dirty parameter block uploaded");
    require(!a.seeded && !a.reordered && a.quadMask==std::vector<std::uint64_t>({0,0,4,0}),"GPU dirtiness names only the changed slot, not the broad CPU upload block");
    require(a.read<Parameters>(1,handles[130].slot).translationZFlags[0]==p.translationZFlags[0],"authoritative new pose");
    std::reverse(handles.begin(),handles.end());
    require(scene.painterOrder(handles) && scene.synchronize(a),"painter reorder");
    require(a.sent[0]==0 && a.sent[1]==0 && a.sent[2]==800 && a.sent[3]==0,"reorder leaves object storage resident");
    require(a.reordered && a.quadMask==std::vector<std::uint64_t>(4,0),"order-only publication carries no dirty quads");
    require(scene.parameters(handles[1],p),"queue notification before rejected acknowledgement");a.failCommit=true;
    require(!scene.synchronize(a) && !a.matches(scene.identity(),scene.revision()),"failed commit cannot authorize output");
    require(scene.synchronize(a) && a.seeded && std::any_of(a.quadMask.begin(),a.quadMask.end(),[](auto bits){return bits!=0;}),"rejected commit retains exact changes and retries a full seed");
    require(a.read<std::uint32_t>(2,0)==handles.front().slot,"exact painter order");
    require(scene.parameters(handles[1],p),"queue retry pose");a.fail=true;
    require(!scene.synchronize(a) && !a.matches(scene.identity(),scene.revision()),"partial upload cannot authorize draw");
    require(scene.synchronize(a) && a.sent[0]==256*64,"failed baseline is completely reseeded");
    require(scene.synchronize(b) && b.sent[0]==256*64,"second consumer receives full seed");
    p.translationZFlags[1]=0x40400000;require(scene.parameters(handles[1],p) && scene.synchronize(b),"second consumer update");
    require(scene.synchronize(a) && a.sent[0]==256*64,"returning consumer cannot miss cleared dirties");
    Scene other(256);auto h=other.create(g,p);std::array<Handle,1> one{h};
    require(other.painterOrder(one) && other.synchronize(a),"same storage used by another scene");
    require(scene.synchronize(a) && a.sent[0]==256*64 && a.count==200,"returning scene receives full seed");
    std::array<Handle,3> repeated{handles[0],handles[1],handles[0]};
    require(scene.painterOrder(repeated) && scene.synchronize(a) && a.count==3,"repeated draw occurrences retain order");
    require(a.read<std::uint32_t>(2,0)==a.read<std::uint32_t>(2,2),"same sprite may occur twice");
    require(scene.painterOrder(handles),"restore complete order");

    Scene fallback(2);Geometry unsupported{};NativeQuadState native{};
    for(unsigned c=0;c<4;++c){native.corners[c].color=0x10203040+c;native.corners[c].u=float(c);native.corners[c].v=float(c+1);}
    auto f=fallback.create(unsupported,p,&native);std::array<Handle,1> fo{f};MemorySink fs;
    require(bool(f) && fallback.painterOrder(fo) && fallback.synchronize(fs),"complete fallback record");
    require(fs.read<Parameters>(1,f.slot).colors[3]==native.corners[3].color,"fallback cannot inherit old colors");
    require(fs.read<Geometry>(0,f.slot).uv[6]==std::bit_cast<std::uint32_t>(native.corners[3].u),"fallback cannot inherit old UV");
    require(fallback.replace(f,g,p) && fallback.synchronize(fs),"fallback becomes supported");
    native.corners[0].x=3;require(fallback.replace(f,unsupported,p,&native) && fallback.synchronize(fs),"supported becomes fallback");
    require(fs.read<NativeQuadState>(3,f.slot).corners[0].x==3,"fallback positions refreshed");
    require(!fallback.parameters(f,p) && !fallback.usable(),"partial fallback mutation invalidates scene");
    Scene fallbackStyleOnly(1);auto sf=fallbackStyleOnly.create(unsupported,p,&native);
    auto nativeStyle=native;nativeStyle.corners[0].color=0xff102030;
    require(fallbackStyleOnly.style(sf,nativeStyle),"fallback style keeps native positions");
    nativeStyle.corners[0].x+=1;
    require(!fallbackStyleOnly.style(sf,nativeStyle) && !fallbackStyleOnly.usable(),"fallback position mutation cannot masquerade as style");

    Scene deletion(2);auto d=deletion.create(g,p);std::array<Handle,1> order{d};
    require(deletion.painterOrder(order) && deletion.erase(d) && !deletion.usable(),"deleting ordered sprite blocks submission");
    auto replacement=deletion.create(g,p);require(replacement.slot==d.slot && replacement.generation!=d.generation,"slot reuse advances generation");
    order[0]=replacement;require(deletion.painterOrder(order) && deletion.usable(),"fresh order repairs deletion barrier");
    require(!deletion.parameters(d,p) && !deletion.usable(),"stale producer cannot edit reused slot");
    Scene unreferenced(3);auto visible=unreferenced.create(g,p),hidden=unreferenced.create(g,p);
    std::array<Handle,1> visibleOrder{visible};
    require(unreferenced.painterOrder(visibleOrder) && unreferenced.erase(hidden) && unreferenced.usable(),"unreferenced retirement preserves current order");
    Scene invalid(1);auto bad=g;bad.rect[0]=0x7f800000;
    require(!invalid.create(bad,p) && !invalid.usable(),"unsupported arithmetic cannot silently retain native ownership");
    Scene missing(1);require(!missing.create(unsupported,p) && !missing.usable(),"fallback needs complete native quad");
    Scene foreign(1);require(!foreign.parameters(h,p) && !foreign.usable(),"foreign scene handle rejected");
    Scene full(1);require(bool(full.create(g,p)) && !full.create(g,p) && !full.usable(),"capacity exhaustion selects native");
    Scene recursive(2);auto r=recursive.create(g,p);MemorySink rs;
    rs.onWrite=[&]{require(!recursive.parameters(r,p),"reentrant producer rejected");};
    require(!recursive.synchronize(rs) && !rs.matches(recursive.identity(),recursive.revision()),"reentrancy cannot commit stale snapshot");
    Scene thrown(1);auto t=thrown.create(g,p);require(bool(t),"exception fixture");MemorySink ts;
    ts.onWrite=[] {throw std::runtime_error("owned upload fault");};
    bool caught=false;try{thrown.synchronize(ts);}catch(const std::runtime_error&){caught=true;}
    require(caught && thrown.synchronize(ts),"upload exception unlocks scene and retains complete work");
    sourceChecks();
    snapshotChecks();
    std::cout<<"retained scene contracts passed; no gameplay or FPS claim\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
