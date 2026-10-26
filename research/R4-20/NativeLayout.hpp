#pragma once
// Generated relocation/identity metadata, no game instructions.
namespace chlorium::research::transform_layout {
struct Body {unsigned rva,length,offset;unsigned long long hash;};
enum Kind {Data,Import,Reset,Move,Sqrt,Atan2,Cookie};
struct Patch {unsigned site,displacement,next,kind,target,size;unsigned long long hash;const char* symbol;};
inline constexpr Body bodies[]{
    {0x229720,0x662,0x0,0xa6208a97dbf7526dull},
    {0x197b20,0x33,0x1000,0xbd7ca077b47adc27ull},
};
inline constexpr Patch patches[]{
    {0x1d,3,7,Data,0x6ba040,8,0xc602d2bb39295ca9ull,""},
    {0x3e,1,5,Reset,0x227c30,0,0x0000000000000000ull,""},
    {0xe9,2,6,Import,0x516db0,8,0x0000000000000000ull,"?retain@CCObject@cocos2d@@QEAAXXZ"},
    {0x11f,2,6,Import,0x516da8,8,0x0000000000000000ull,"?release@CCObject@cocos2d@@QEAAXXZ"},
    {0x155,2,6,Import,0x516a00,8,0x0000000000000000ull,"??0CCPoint@cocos2d@@QEAA@XZ"},
    {0x1b0,5,9,Data,0x6243f0,4,0x4d24f67f9dcd3a75ull,""},
    {0x1c2,5,9,Data,0x623200,4,0x67efb1ad94b89cc1ull,""},
    {0x242,2,6,Import,0x516a08,8,0x0000000000000000ull,"??4CCPoint@cocos2d@@QEAAAEAV01@AEBV01@@Z"},
    {0x251,1,5,Reset,0x227c30,0,0x0000000000000000ull,""},
    {0x27b,2,6,Import,0x516a08,8,0x0000000000000000ull,"??4CCPoint@cocos2d@@QEAAAEAV01@AEBV01@@Z"},
    {0x290,2,6,Import,0x515d70,8,0x0000000000000000ull,"??GCCPoint@cocos2d@@QEBA?AV01@AEBV01@@Z"},
    {0x34c,2,6,Import,0x5165c8,8,0x0000000000000000ull,"?CCAffineTransformConcat@cocos2d@@YA?AUCCAffineTransform@1@AEBU21@0@Z"},
    {0x37a,1,5,Move,0x22aab0,0,0x0000000000000000ull,""},
    {0x3b4,1,5,Sqrt,0x4d188a,0,0x0000000000000000ull,""},
    {0x3f7,1,5,Sqrt,0x4d188a,0,0x0000000000000000ull,""},
    {0x406,1,5,Atan2,0x4d1848,0,0x0000000000000000ull,""},
    {0x41b,4,8,Data,0x623294,4,0x4b2ebc7f9c23299bull,""},
    {0x42c,1,5,Atan2,0x4d1848,0,0x0000000000000000ull,""},
    {0x590,4,8,Data,0x622c24,4,0x4b72477f9c5c2f98ull,""},
    {0x640,1,5,Cookie,0x4d0750,0,0x0000000000000000ull,""},
    {0x1024,2,6,Import,0x5168c8,8,0x0000000000000000ull,"??0CCPoint@cocos2d@@QEAA@MM@Z"},
};
struct CocosBody {unsigned rva,length,slot;unsigned long long hash;};
inline constexpr CocosBody cocosBodies[]{
    {0x4d70,0x1a,0xb8,0x9e334b49abde79b2ull},
    {0x4e90,0x14,0x160,0x082a7ac626f078e7ull},
    {0x4eb0,0x14,0x170,0x7055610577a704f3ull},
    {0x4f10,0x14,0x78,0x527665047f2eb17full},
    {0x4f30,0x14,0x88,0xbc45da021d44716bull},
    {0x37f0,0x468,0x308,0x7abc292bc9db4855ull},
};
}
