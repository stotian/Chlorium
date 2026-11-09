#include "PageBudget.hpp"
#include <cassert>
#include <limits>
#include <iostream>
using namespace chlorium::research::directvertices;
int main(){
    static_assert(pageStorage(1)==4096 && pageStorage(4096)==4096 && pageStorage(4097)==8192);
    static_assert(pageStorage(adaptiveMaxPage)==adaptiveMaxPage && !pageStorage(adaptiveMaxPage+1));
    static_assert(!pageStorage(0) && !pageStorage(std::numeric_limits<std::size_t>::max()));
    for(std::size_t bytes=1;bytes<=adaptiveMaxPage;bytes+=13){const auto storage=pageStorage(bytes);assert(storage>=bytes && storage<=adaptiveMaxPage);}
    assert(canReplacePage(adaptiveBudget,adaptiveMaxPage,adaptiveMaxPage));
    assert(!canReplacePage(adaptiveBudget,0,4096));
    assert(!canReplacePage(4096,8192,4096));
    assert(!canReplacePage(adaptiveBudget+1,4096,4096));
    assert(!canReplacePage(0,0,0));
    std::cout<<"R66 adaptive capacity and replacement budgets passed\n";
}
