#include "ProducerShadow.hpp"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error Producer checks require assertions
#endif
using namespace chlorium::research;
int main(){
    pass::DrawRecord draw;draw.atlas=0x2000;draw.atlasId=17;draw.count=2;
    std::array<pass::SpriteRecord,2> source{};std::array<chlorium::snapshot::NativeQuadState,2> quads{};
    for(unsigned n=0;n<2;++n){source[n].sprite=0x1000+n;source[n].id=20+n;source[n].quadMatches=1;source[n].atlasIndex=n;quads[n].corners[0].x=float(n);}
    producers::Shadow shadow;shadow.observe(1,draw,source,quads);shadow.observe(2,draw,source,quads);assert(shadow.sampledCoverageComplete());
    source[0].matrix[4]=0x3f800000;quads[0].corners[0].x=2;
    shadow.sprite(source[0],quads[0],false);shadow.atlas(draw.atlas,draw.atlasId,0,quads[0]);shadow.observe(2,draw,source,quads);assert(shadow.sampledCoverageComplete());
    auto silent=source;silent[1].localRect[0]=1;shadow.observe(2,draw,silent,quads);assert(!shadow.sampledCoverageComplete() && shadow.coverage.back().sourceDifferent==1);
    producers::Shadow missed;missed.observe(1,draw,source,quads);quads[1].corners[2].color=0x80604020;missed.observe(2,draw,source,quads);assert(!missed.sampledCoverageComplete() && missed.coverage.back().rawDifferent==1);
    producers::Shadow style;style.observe(1,draw,source,quads);source[1].flags|=6;style.sprite(source[1],quads[1],true);style.observe(2,draw,source,quads);assert(style.sampledCoverageComplete());
    auto reused=source;reused[0].id++;style.observe(2,draw,reused,quads);assert(!style.sampledCoverageComplete() && style.coverage.back().sourceMissing==1);
    producers::Shadow bounded(1);bounded.observe(1,draw,source,quads);assert(bounded.failed && !bounded.sampledCoverageComplete());
    producers::Shadow newAtlas;newAtlas.atlas(draw.atlas,draw.atlasId,0,quads[0]);newAtlas.sprite(source[0],quads[0],false);auto one=draw;one.count=1;
    newAtlas.observe(2,one,std::span(source).first(1),std::span(quads).first(1));assert(newAtlas.sampledCoverageComplete());
    std::vector<pass::SpriteRecord> replaySources;std::vector<unsigned char> replayQuads;
    const auto firstSource=std::span(source).first(1);const auto firstQuad=std::as_bytes(std::span(quads).first(1));
    assert(newAtlas.assemble(firstSource,firstQuad,replaySources,replayQuads));
    assert(replaySources.size()==1 && replaySources[0].quadMatches==1 && replayQuads.size()==96);
    assert(!std::memcmp(replayQuads.data(),quads.data(),96));
    auto bad=source;bad[0].atlasIndex++;assert(!newAtlas.assemble(std::span(bad).first(1),firstQuad,replaySources,replayQuads));
    assert(replaySources.empty() && replayQuads.empty());
    bad=source;bad[0].matrix[0]++;assert(!newAtlas.assemble(std::span(bad).first(1),firstQuad,replaySources,replayQuads));
    bad=source;bad[0].quadMatches=0;assert(!newAtlas.assemble(std::span(bad).first(1),firstQuad,replaySources,replayQuads));
    auto wrongQuad=quads;wrongQuad[0].corners[1].color++;assert(!newAtlas.assemble(firstSource,std::as_bytes(std::span(wrongQuad).first(1)),replaySources,replayQuads));
    auto repeated=source;repeated[1]=source[0];assert(!newAtlas.assemble(repeated,std::as_bytes(std::span(quads)),replaySources,replayQuads));
    producers::Shadow absent;assert(!absent.assemble(firstSource,firstQuad,replaySources,replayQuads));
    assert(!newAtlas.assemble({}, {},replaySources,replayQuads));
    std::cout<<"PASS producer seed/events, missing writer/source, style, identity reuse, budget and guarded event-packet replay; sampled data only\n";
}
