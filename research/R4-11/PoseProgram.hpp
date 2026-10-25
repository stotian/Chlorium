#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <emmintrin.h>

namespace chlorium::research::pose {
struct Matrix { float a,b,c,d,tx,ty; };
static_assert(sizeof(Matrix)==24);
using Concat = Matrix* (*)(Matrix*,const Matrix*,const Matrix*);
inline float mul(float a,float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a),_mm_set_ss(b))); }
inline float add(float a,float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a),_mm_set_ss(b))); }
inline float sub(float a,float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a),_mm_set_ss(b))); }
// Actual Cocos 2.2081 operation/operand order, not an algebraic reassociation.
inline Matrix scalar(const Matrix& l,const Matrix& r) {
    return {add(mul(r.a,l.a),mul(r.c,l.b)),add(mul(r.b,l.a),mul(r.d,l.b)),
            add(mul(l.c,r.a),mul(l.d,r.c)),add(mul(l.c,r.b),mul(l.d,r.d)),
            add(add(mul(l.tx,r.a),mul(l.ty,r.c)),r.tx),
            add(add(mul(l.tx,r.b),mul(l.ty,r.d)),r.ty)};
}
inline std::array<float,4> packedLinear(const Matrix& l,const Matrix& r) {
    const auto x=_mm_mul_ps(_mm_setr_ps(r.a,r.b,l.c,l.c),_mm_setr_ps(l.a,l.a,r.a,r.b));
    const auto y=_mm_mul_ps(_mm_setr_ps(r.c,r.d,l.d,l.d),_mm_setr_ps(l.b,l.b,r.c,r.d));
    std::array<float,4> result;
    _mm_storeu_ps(result.data(),_mm_add_ps(x,y));
#ifdef CHLORIUM_PRIVATE_POSE_MUTANT
    result[0]=0.f;
#endif
    return result;
}
template<std::size_t Words,std::size_t Capacity=4096> class Recipes {
    static_assert(Capacity && !(Capacity&(Capacity-1)));
public:
    using Key=std::array<std::uint32_t,Words>;
    struct Result { bool hit; bool stored; std::size_t slot; };
    void begin() noexcept {
        if (++generation_==0) { for(auto& entry:entries_) entry.generation=0; generation_=1; }
        unique_=overflow_=0;
    }
    Result intern(const Key& key) noexcept {
        if (!generation_) return {false,false,0};
        std::uint64_t hash=0xcbf29ce484222325ull;
        for(auto word:key) hash=(hash^word)*0x100000001b3ull;
        auto slot=static_cast<std::size_t>((hash^(hash>>32))&(Capacity-1));
        for(std::size_t probe=0;probe<64 && probe<Capacity;++probe,slot=(slot+1)&(Capacity-1)) {
            auto& entry=entries_[slot];
            if(entry.generation==generation_ && entry.key==key) return {true,true,slot};
            if(entry.generation!=generation_) {
                entry={key,generation_}; ++unique_; return {false,true,slot};
            }
        }
        ++overflow_; return {false,false,0}; // Always calculate the native result on overflow.
    }
    std::size_t unique() const { return unique_; }
    std::size_t overflow() const { return overflow_; }
private:
    struct Entry { Key key{}; std::uint64_t generation=0; };
    std::array<Entry,Capacity> entries_{};
    std::uint64_t generation_=0;
    std::size_t unique_=0,overflow_=0;
};
struct Work { std::size_t inputs=0,recipes=0,reused=0,fallback=0,stoppedAt=0; };
struct LinearPose { float scaleX,scaleY,rotationX,rotationY; };
struct Pose { Matrix matrix; LinearPose linear; };
using Atan2=float(*)(float,float);
using Decompose=LinearPose(*)(const Matrix&);
using Trig=float(*)(float);
struct RawPose { float rx,ry,sx,sy,x,y; };
inline Matrix localMatrix(const RawPose& p,Trig cosine,Trig sine) {
    float cx=1,cy=1,sx=0,sy=0;
    const auto negate=[](float v) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(v)^0x80000000u); };
    if(p.rx!=0.f || p.ry!=0.f) {
        const auto radians=negate(mul(p.rx,std::bit_cast<float>(0x3c8efa35u)));
        cx=cosine(radians);sx=sine(radians);
        if(p.rx==p.ry) {cy=cx;sy=sx;}
        else {
            const auto other=negate(mul(p.ry,std::bit_cast<float>(0x3c8efa35u)));
            cy=cosine(other);sy=sine(other);
        }
    }
    return {mul(p.sx,cy),mul(p.sx,sy),mul(negate(sx),p.sy),mul(p.sy,cx),p.x,p.y};
}
inline LinearPose decompose(const Matrix& m,Atan2 atan2) {
    const auto sx=_mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(add(mul(m.a,m.a),mul(m.b,m.b)))));
    const auto sy=_mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(add(mul(m.d,m.d),mul(m.c,m.c)))));
    const auto degrees=std::bit_cast<float>(0x42652ee1u);
    const auto negate=[](float value) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value)^0x80000000u); };
    const auto rx=negate(sub(mul(atan2(m.d,m.c),degrees),90.f));
    const auto ry=negate(mul(atan2(m.b,m.a),degrees));
    // The native block subtracts the object's source rotations afterwards.
    // The zero source rotations used here retain that final subtraction.
    return {sx,sy,sub(rx,0.f),sub(ry,0.f)};
}
// Second architecture: fuse the temporary helper-node graph into a pure pose
// pipeline, with NO recipe lookup and no dependence on repeated poses.
// Only the zero-anchor/skew/additional-transform-free helper contract is
// represented. No real GameObject callback or write is bypassed by this file.
inline Pose fused(const RawPose& raw,const Matrix& parent,Trig cosine,Trig sine,Atan2 atan2) {
    const auto local=localMatrix(raw,cosine,sine);
    const auto linear=packedLinear(local,parent);
    Matrix matrix{linear[0],linear[1],linear[2],linear[3],
        add(add(mul(local.tx,parent.a),mul(local.ty,parent.c)),parent.tx),
        add(add(mul(local.tx,parent.b),mul(local.ty,parent.d)),parent.ty)};
    return {matrix,decompose(matrix,atan2)};
}
// A phase-local snapshot program. It never reads/writes GameObject or runs
// callbacks. The producer must supply an immutable, causally independent phase.
// This is NOT authorization to defer/gather real GD callbacks.
template<std::size_t Capacity=4096> class Program {
public:
    Work evaluate(std::span<const Matrix> inputs,const Matrix& parent,std::span<Pose> output,Concat native,Decompose nativePose,Atan2 atan2) {
        if(inputs.size()!=output.size() || !native || !nativePose || !atan2) throw std::invalid_argument("pose cardinality/oracle");
        if(!inputs.empty()) {
            auto a=reinterpret_cast<std::uintptr_t>(inputs.data()),b=reinterpret_cast<std::uintptr_t>(output.data());
            if(a<=b ? b-a<inputs.size_bytes() : a-b<output.size_bytes()) throw std::invalid_argument("pose snapshots alias");
        }
        recipes_.begin(); Work work;
        const auto controls=_mm_getcsr()&0xffc0u;
        for(std::size_t n=0;n<inputs.size();++n) {
            if(n==1024 && work.reused*4<n) {
                // Dense unique phases cannot amortize compilation. Stop the
                // entire lookup loop, not just individual recipe insertions.
                work.stoppedAt=n;
                for(;n<inputs.size();++n) {
                    native(&output[n].matrix,&inputs[n],&parent);
                    output[n].linear=nativePose(output[n].matrix);
                    ++work.inputs; ++work.fallback;
                }
                break;
            }
            const auto& l=inputs[n]; ++work.inputs;
            // Restrict cached execution to masked finite arithmetic. Unusual
            // values stay native; exception-status observers are not modelled.
            bool eligible=(controls&0x1f80u)==0x1f80u;
            const std::array<float,8> values{l.a,l.b,l.c,l.d,parent.a,parent.b,parent.c,parent.d};
            typename Recipes<9,Capacity>::Key key{};
            for(std::size_t i=0;i<8;++i) {
                key[i]=std::bit_cast<std::uint32_t>(values[i]);
                eligible &= (key[i]&0x7f800000u)!=0x7f800000u;
            }
            key[8]=controls;
            const auto fallback=[&] { native(&output[n].matrix,&l,&parent); output[n].linear=nativePose(output[n].matrix); ++work.fallback; };
            if(!eligible) { fallback(); continue; }
            const auto recipe=recipes_.intern(key);
            if(!recipe.stored) { fallback(); continue; }
            if(!recipe.hit) {
                linear_[recipe.slot]=packedLinear(l,parent);
                const auto& v=linear_[recipe.slot];
                poses_[recipe.slot]=decompose({v[0],v[1],v[2],v[3],0,0},atan2); ++work.recipes;
            }
            else ++work.reused;
            const auto& v=linear_[recipe.slot];
            output[n].matrix={v[0],v[1],v[2],v[3],
                add(add(mul(l.tx,parent.a),mul(l.ty,parent.c)),parent.tx),
                add(add(mul(l.tx,parent.b),mul(l.ty,parent.d)),parent.ty)};
            output[n].linear=poses_[recipe.slot];
        }
        return work;
    }
private:
    Recipes<9,Capacity> recipes_;
    std::array<std::array<float,4>,Capacity> linear_{};
    std::array<LinearPose,Capacity> poses_{};
};
}
