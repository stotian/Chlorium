#pragma once
#include <windows.h>
#include <xmmintrin.h>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chlorium::research::movementcensus {
struct Row {
    std::uint64_t array=0,dx=0,dy=0,membersHash=14695981039346656037ull;
    unsigned frame=0,arrayId=0,stamp=0,count=0,visited=0,decorations=0,negativeSections=0,lockedX=0;
    bool complete=false,nullTerminated=false;
};
class Census {
    std::array<Row,2048> rows{};unsigned used=0;
    std::array<unsigned,2> calls{},reads{};
    std::uintptr_t layer=0;bool enabled=false,written=false,overflow=false;
    template<class Object,class Layer,class Array>
    static bool copy(Row& row,Layer* source,Array* array,unsigned budget)noexcept{
        __try {
            if(!array||!array->data||array->data->num>1000000||(!array->data->arr&&array->data->num))return false;
            row.array=reinterpret_cast<std::uintptr_t>(array);row.arrayId=array->m_uID;row.stamp=std::bit_cast<unsigned>(source->m_gameState.m_commandIndex);row.count=array->data->num;
            if(row.count>budget)return false;
            for(unsigned i=0;i<row.count;++i){auto* object=static_cast<Object*>(array->data->arr[i]);if(!object){row.nullTerminated=true;break;}
                ++row.visited;row.decorations+=object->m_isDecoration2!=0;row.negativeSections+=object->m_outerSectionIndex<0;row.lockedX+=object->m_tempOffsetXRelated!=0;
                const std::uint64_t identity[]{reinterpret_cast<std::uintptr_t>(object),object->m_uID};
                for(auto value:identity)for(unsigned k=0;k<8;++k)row.membersHash=(row.membersHash^((value>>(k*8))&255))*1099511628211ull;
            }return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
public:
    void configure(void* owner)noexcept{const auto flag=std::getenv("CHLORIUM_MOVE_COMMAND_CENSUS");enabled=flag&&std::string_view(flag)=="1";layer=reinterpret_cast<std::uintptr_t>(owner);}
    template<class Object,class Layer,class Array>
    void before(unsigned frame,Layer* source,Array* array,double dx,double dy)noexcept{
        if(!enabled||frame<1||frame>2||reinterpret_cast<std::uintptr_t>(source)!=layer)return;
        const auto saved=_mm_getcsr();const auto error=GetLastError();
        ++calls[frame-1];
        if(used<rows.size()){auto& row=rows[used++];row.frame=frame;row.dx=std::bit_cast<std::uint64_t>(dx);row.dy=std::bit_cast<std::uint64_t>(dy);row.complete=copy<Object>(row,source,array,131072-reads[frame-1]);reads[frame-1]+=row.visited;}
        else overflow=true;
        _mm_setcsr(saved);SetLastError(error);
    }
    const Row& ownedRow(unsigned index)const{return rows.at(index);}
    void finish(const char* output){
        if(!enabled||written||!output)return;written=true;
        std::ofstream stream(std::string(output)+".move-commands.json",std::ios::out|std::ios::noreplace);if(!stream)throw std::runtime_error("new movement census receipt only");
        stream<<"{\"diagnostic_only\":true,\"game_fps_evidence\":false,\"dependency_closure_proven\":false,\"overflow\":"<<(overflow?"true":"false")<<",\"calls\":["<<calls[0]<<','<<calls[1]<<"],\"rows\":[";
        for(unsigned i=0;i<used;++i){const auto& row=rows[i];if(i)stream<<',';stream<<"{\"frame\":"<<row.frame<<",\"array\":"<<row.array<<",\"array_id\":"<<row.arrayId<<",\"stamp\":"<<row.stamp<<",\"count\":"<<row.count<<",\"visited\":"<<row.visited<<",\"decoration2\":"<<row.decorations<<",\"negative_sections\":"<<row.negativeSections<<",\"locked_x\":"<<row.lockedX<<",\"dx_bits\":"<<row.dx<<",\"dy_bits\":"<<row.dy<<",\"membership_fingerprint_not_identity_proof\":"<<row.membersHash<<",\"complete\":"<<(row.complete?"true":"false")<<",\"null_terminated\":"<<(row.nullTerminated?"true":"false")<<'}';}
        stream<<"]}\n";if(!stream)throw std::runtime_error("complete movement census receipt");
    }
};
inline Census census;
}
