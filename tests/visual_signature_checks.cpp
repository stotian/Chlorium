#include "VisualSignature.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
int main() {
    using chlorium::test::analyzeVisual;
    constexpr int w = 128, h = 80;
    std::vector<unsigned char> pixels(w * h * 4, 0);
    auto pixel = [&](int x, int y) { return pixels.data() + (y * w + x) * 4; };
    if (analyzeVisual(nullptr,w,h).valid || analyzeVisual(pixels.data(),0,h).valid) return 1;
    auto dark = analyzeVisual(pixels.data(),w,h);
    if (!dark.valid || !dark.dark || dark.partial) return 2;
    // HUD must not hide a real black core.
    for (int y=0;y<h;++y) for(int x=0;x<w;++x)
        if(y<h/4 || y>=h*85/100) std::fill_n(pixel(x,y),3,255);
    if (!analyzeVisual(pixels.data(),w,h).dark) return 3;
    for(unsigned side=0;side<4;++side) {
        std::fill(pixels.begin(),pixels.end(),0);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            const bool bright = side==0 ? x>=w/2 : side==1 ? x<w/2 :
                side==2 ? y>=44 : y<44; // core vertical midpoint = 44
            if(bright) std::fill_n(pixel(x,y),3,160);
        }
        auto sample=analyzeVisual(pixels.data(),w,h);
        if(sample.dark || sample.partial!=(2u<<side)) return 4+side;
    }
    std::fill(pixels.begin(),pixels.end(),128);
    auto bright=analyzeVisual(pixels.data(),w,h);
    if(bright.dark || bright.partial || bright.hash==dark.hash) return 8;
    // Sampling must be confirmed across the core: this lit line occupies >1%
    // of the core and must prevent a false full-black verdict.
    std::fill(pixels.begin(),pixels.end(),0);
    for(int x=0;x<w;++x) std::fill_n(pixel(x,h/2),3,8);
    auto almost=analyzeVisual(pixels.data(),w,h);
    if(almost.dark || almost.centerMean!=8) return 9; // >1% bright pixels
    // Conversely a single lit centre row below 1% must not hide a real
    // near-black core. V1 rejected this case using its centre-row gate.
    constexpr int tall=400;
    std::vector<unsigned char> thinLine(w*tall*4,0);
    for(int x=0;x<w;++x) std::fill_n(thinLine.data()+(tall/2*w+x)*4,3,32);
    auto sparse=analyzeVisual(thinLine.data(),w,tall);
    if(!sparse.dark || sparse.centerMean!=32) return 10;
    std::fill(pixels.begin(),pixels.end(),3);
    if(!analyzeVisual(pixels.data(),w,h).dark) return 11;
    std::fill(pixels.begin(),pixels.end(),4);
    if(analyzeVisual(pixels.data(),w,h).dark) return 12;
    std::cout << "Visual diagnostics: invalid input, HUD, four partial directions, brightness and centre-row cases passed\n";
}
