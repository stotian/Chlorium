#pragma once
#include "../R4-42/GroupTape.hpp"
#include <windows.h>
#include <xmmintrin.h>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <cmath>

namespace chlorium::research::grouptrace {
struct alignas(16) Object {
    std::uint64_t address=0;std::uint32_t uid=0;std::int32_t section=0;
    grouptape::State state;
    std::uint32_t objectType=0,objectId=0,roles=0,groupCount=0;
};
struct Row {
    std::uint64_t array=0,dx=0,dy=0;
    std::uint32_t frame=0,arrayUid=0,stamp=0,count=0,first=0,observed=0,flags=0,mxcsr=0;
};
struct Header {
    char magic[8]{'C','H','L','G','T','R','6','3'};
    std::uint32_t version=63,frames=0,rows=0,objects=0,rowBytes=sizeof(Row),objectBytes=sizeof(Object),reasons=0,features=0;
    std::uint64_t owner=0;std::uint32_t ownerUid=0,reserved2=0;double percent=0;
};
static_assert(sizeof(Object)==64 && offsetof(Object,state)==16 && sizeof(Row)==56 && sizeof(Header)==64);
class Capture {
    static constexpr unsigned maxRows=8192,maxObjects=262144;
    std::vector<Row> rows_;
    std::vector<Object> objects_;
    unsigned limit_=maxObjects;bool active_=false;
    using Reader=void(*)(void*,Object&)noexcept;
    using Filter=bool(*)(void*)noexcept;
    template<class Layer,class Array>static void copy(Row& row,Layer* layer,Array* array,
                                Object* output,unsigned available,Reader reader,Filter filter)noexcept{
        static_assert(sizeof(layer->m_gameState.m_commandIndex)==4);
        __try {
            if(!array){row.flags|=filter?8:(1|16);return;}
            if(!array->data || array->data->num>1000000 || (array->data->num && !array->data->arr)){row.flags|=8;return;}
            row.array=reinterpret_cast<std::uintptr_t>(array);row.arrayUid=array->m_uID;
            std::memcpy(&row.stamp,&layer->m_gameState.m_commandIndex,4);row.count=array->data->num;
            if(!filter && row.count>available){row.flags|=4;return;}
            for(unsigned i=0;i<row.count;++i){
                if(!array->data->arr[i]){if(filter)continue;row.flags|=2;break;}
                if(filter && !filter(array->data->arr[i]))continue;
                if(row.observed==available){row.flags|=4;return;}
                Object value;reader(array->data->arr[i],value);
                output[row.observed++]=value;
            }
            row.flags|=1;
        }__except(EXCEPTION_EXECUTE_HANDLER){row.flags|=8;}
    }
public:
    Header header;
    explicit Capture(unsigned limit=maxObjects):limit_(limit){if(!limit || limit>maxObjects)throw std::invalid_argument("bounded group trace capacity");}
    bool start(void* owner,unsigned uid,double percent){
        if(header.owner || !owner || !uid || !std::isfinite(percent) || percent<0 || percent>100)return false;
        rows_.resize(maxRows);objects_.resize(limit_);header.owner=reinterpret_cast<std::uintptr_t>(owner);header.ownerUid=uid;header.percent=percent;return true;
    }
    bool beginFrame(void* owner,unsigned uid)noexcept{
        if(!header.owner || active_ || header.frames>=2 || reinterpret_cast<std::uintptr_t>(owner)!=header.owner || uid!=header.ownerUid)return false;
        active_=true;return true;
    }
    void endFrame()noexcept{if(active_){active_=false;++header.frames;}}
    void abort()noexcept{active_=false;header.reasons|=4;}
private:
    template<class Layer,class Array>void observe(Layer* layer,Array* array,double dx,double dy,Reader reader,Filter filter)noexcept{
        if(!active_ || reinterpret_cast<std::uintptr_t>(layer)!=header.owner)return;
        const auto fp=_mm_getcsr();const auto error=GetLastError();
        if(header.rows==maxRows)header.reasons|=1;
        else{
            auto& row=rows_[header.rows++];row.frame=header.frames+1;row.first=header.objects;row.mxcsr=fp;
            if(filter){row.flags|=32;header.features|=1;}
            row.dx=std::bit_cast<std::uint64_t>(dx);row.dy=std::bit_cast<std::uint64_t>(dy);
            copy(row,layer,array,objects_.data()+header.objects,limit_-header.objects,reader,filter);
            header.objects+=row.observed;if(!(row.flags&1))header.reasons|=2;
        }
        _mm_setcsr(fp);SetLastError(error);
    }
public:
    template<class Layer,class Array>void before(Layer* layer,Array* array,double dx,double dy,Reader reader)noexcept{
        observe(layer,array,dx,dy,reader,nullptr);
    }
    template<class Layer,class Array>void snapshot(Layer* layer,Array* array,Reader reader,Filter filter)noexcept{
        if(!filter){abort();return;}
        observe(layer,array,0,0,reader,filter);
    }
    const Row& row(unsigned index)const{return rows_.at(index);}
    const Object& object(unsigned index)const{return objects_.at(index);}
    bool write(const std::string& path)const{
        if(active_ || !header.frames)return false;
        std::ofstream output(path,std::ios::binary|std::ios::out|std::ios::noreplace);
        if(!output)return false;
        output.write(reinterpret_cast<const char*>(&header),sizeof(header));
        output.write(reinterpret_cast<const char*>(rows_.data()),header.rows*sizeof(Row));
        output.write(reinterpret_cast<const char*>(objects_.data()),header.objects*sizeof(Object));
        return bool(output);
    }
};
}
