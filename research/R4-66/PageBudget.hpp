#pragma once
#include <cstddef>
namespace chlorium::research::directvertices {
inline constexpr std::size_t adaptivePageCount=128,adaptiveMaxPage=16384u*96u,adaptiveBudget=24u*1024u*1024u;
constexpr std::size_t pageStorage(std::size_t bytes)noexcept{
    if(!bytes || bytes>adaptiveMaxPage)return 0;
    std::size_t result=4096;while(result<bytes)result*=2;
    return result>adaptiveMaxPage?adaptiveMaxPage:result;
}
constexpr bool canReplacePage(std::size_t allocated,std::size_t previous,std::size_t desired)noexcept{
    return allocated<=adaptiveBudget && previous<=allocated && desired && desired<=adaptiveMaxPage && desired<=adaptiveBudget-(allocated-previous);
}
}
