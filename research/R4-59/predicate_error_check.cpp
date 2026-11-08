#include "../R4-33/GpuFixture.hpp"
#include "../R4-58/NativeDrawSuppression.hpp"
#include <iostream>
int main()try{
    Graphics graphics;graphics.create(true,6);
    glViewport(0,0,64,64);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    chlorium::research::continuous::NativeDrawSuppression suppress;
    require(suppress.begin() && suppress.end(),"seed the reusable zero predicate");
    glEnable(0); // Leave INVALID_ENUM pending before the next begin.
    require(!suppress.begin(),"pending driver error must reject suppression");
    glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);
    std::array<unsigned char,4> pixel{};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    require(pixel[0]==0 && pixel[1]==255,"rejected suppression must not mask subsequent rendering");
    require(glGetError()==GL_NO_ERROR,"pending error was handled before changing conditional state");
    std::cout<<"pending-error predicate recovery passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
