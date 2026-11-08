#include "PassCapture.hpp"
#include "../../tests/MeasurementHookPolicy.hpp"
#include <array>
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#error Capture checks require assertions.
#endif
using namespace chlorium::research::pass;
int main() {
    for(unsigned char raw:std::array<unsigned char,5>{0,1,2,3,255}) assert(nativeFlag(&raw)==unsigned(raw!=0));
    for(bool compiled : {false,true}) for(bool native : {false,true}) for(bool production : {false,true}) {
        assert(chlorium::test::nativePassObserverAllowed(compiled,native,production,
            "cocos2d::CCTextureAtlas::drawNumberOfQuads") == (compiled && native && !production));
        assert(!chlorium::test::nativePassObserverAllowed(compiled,native,production,"Other::draw"));
        for(auto name:{"cocos2d::CCSprite::updateTransform","cocos2d::CCSprite::updateColor","cocos2d::CCTextureAtlas::updateQuad"}){
            assert(!chlorium::test::nativePassObserverAllowed(compiled,native,production,name));
            assert(chlorium::test::nativePassObserverAllowed(compiled,native,production,name,true)==(compiled&&native&&!production));
        }
        assert(!chlorium::test::nativePassObserverAllowed(compiled,native,production,"cocos2d::CCTextureAtlas::updateQuad"));
        assert(chlorium::test::nativePassObserverAllowed(compiled,native,production,"cocos2d::CCTextureAtlas::updateQuad",true)==(compiled&&native&&!production));
    }
    assert(validRange(10, 10, 2, 8));
    assert(!validRange(10, 9, 0, 1));
    assert(!validRange(10, 10, UINT32_MAX, 2));
    assert(!validRange(UINT32_MAX, UINT32_MAX, UINT32_MAX, 1));
    assert(validRange(0, 0, 0, 0));
    Frame frame;
    DrawRecord draw; draw.count = draw.total = draw.capacity = 1;
    std::array<std::byte,96> raw{};
    raw.fill(std::byte{0x5a});
    SpriteRecord owner; owner.sprite = 123; owner.quadMatches = 1;
    assert(!frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.draws.empty() && frame.quads.empty());
    frame.header.reasons = 0; frame.reserve();
    assert(frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.header.draws == 1 && frame.header.quads == 1 && frame.header.sprites == 1);
    assert(frame.draws[0].mapped == 1 && !frame.draws[0].reasons);
    assert(!std::memcmp(frame.quads.data(), raw.data(), raw.size()));
    draw.first = UINT32_MAX;
    assert(!frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.draws.size() == 1 && frame.quads.size() == 1);
    draw.first = 0;
    assert(!frame.append(draw, std::span(raw).first(95), std::span(&owner,1)));
    owner.sprite = 0;
    assert(frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.draws.back().mapped == 0 && (frame.draws.back().reasons & missingSprites));
    owner.sprite = 123; owner.quadMatches = 0;
    assert(frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.draws.back().reasons & unmatchedQuad);
    while(frame.draws.size() < maxDraws) assert(frame.append(draw, raw, std::span(&owner,1)));
    const auto oldCount = frame.quads.size();
    assert(!frame.append(draw, raw, std::span(&owner,1)));
    assert(frame.quads.size() == oldCount && (frame.header.reasons & budget));
    Frame empty; empty.reserve();
    assert(empty.append({}, {}, {}));
    assert(empty.quads.empty() && empty.draws.size() == 1);
    std::cout << "PASS native capture bounds, no-allocation append, raw byte copy and explicit unknown ownership\n";
}
