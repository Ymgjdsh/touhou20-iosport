#pragma once
#include <cstdint>
#include <cstring>
#include <limits>

// Native scalar implementations of the SSE/SSE2 operations used by gameplay.
// Compile with -fno-fast-math -ffp-contract=off and the default IEEE FP mode.
// This reproduces result bits, including x86 invalid-conversion sentinels and
// NaN selection. MXCSR exception flags and non-default DAZ/FTZ modes are not an
// API of the game and are deliberately not advertised as implemented here.
namespace th20::recovered::scalar_sse {
struct alignas(16) Float4 { float lane[4]; };
struct alignas(16) Double2 { double lane[2]; };
struct alignas(16) Int4 { std::uint32_t lane[4]; };

template<class To, class From> inline To bits(From value) noexcept {
    static_assert(sizeof(To)==sizeof(From));
    To result; std::memcpy(&result,&value,sizeof(result)); return result;
}
template<class T> struct Encoding;
template<> struct Encoding<float> {
    using UInt=std::uint32_t;
    static constexpr UInt exponent=0x7f800000u, fraction=0x007fffffu, quiet=0x00400000u, invalid=0xffc00000u;
};
template<> struct Encoding<double> {
    using UInt=std::uint64_t;
    static constexpr UInt exponent=0x7ff0000000000000ull, fraction=0x000fffffffffffffull,
                          quiet=0x0008000000000000ull, invalid=0xfff8000000000000ull;
};
template<class T> inline bool is_nan(T value) noexcept {
    using E=Encoding<T>; const auto raw=bits<typename E::UInt>(value);
    return (raw&E::exponent)==E::exponent && (raw&E::fraction)!=0;
}
template<class T> inline T quiet_nan(T value) noexcept {
    using E=Encoding<T>; return bits<T>(bits<typename E::UInt>(value)|E::quiet);
}
template<class T, class Operation> inline T arithmetic(T first,T second,Operation operation) noexcept {
    if(is_nan(first)) return quiet_nan(first);
    if(is_nan(second)) return quiet_nan(second);
    // The volatile result also enforces a binary32/binary64 rounding boundary.
    volatile T result=operation(first,second);
    if(is_nan(T(result))) return bits<T>(Encoding<T>::invalid);
    return result;
}
inline Float4 _mm_setzero_ps() noexcept { return {{0,0,0,0}}; }
inline Double2 _mm_setzero_pd() noexcept { return {{0,0}}; }
inline Float4 _mm_set_ss(float value) noexcept { return {{value,0,0,0}}; }
inline Double2 _mm_set_sd(double value) noexcept { return {{value,0}}; }
inline float _mm_cvtss_f32(Float4 value) noexcept { return value.lane[0]; }
inline double _mm_cvtsd_f64(Double2 value) noexcept { return value.lane[0]; }

#define TH20_SCALAR_BINARY(name, symbol) \
inline Float4 _mm_##name##_ss(Float4 a,Float4 b) noexcept { \
    a.lane[0]=arithmetic(a.lane[0],b.lane[0],[](float x,float y){return x symbol y;});return a; } \
inline Double2 _mm_##name##_sd(Double2 a,Double2 b) noexcept { \
    a.lane[0]=arithmetic(a.lane[0],b.lane[0],[](double x,double y){return x symbol y;});return a; }
TH20_SCALAR_BINARY(add,+)
TH20_SCALAR_BINARY(sub,-)
TH20_SCALAR_BINARY(mul,*)
TH20_SCALAR_BINARY(div,/)
#undef TH20_SCALAR_BINARY

inline Float4 _mm_cvtsi32_ss(Float4 a,std::int32_t value) noexcept {
    volatile float rounded=static_cast<float>(value);a.lane[0]=rounded;return a;
}
inline Double2 _mm_cvtsi32_sd(Double2 a,std::int32_t value) noexcept { a.lane[0]=value;return a; }
inline Float4 _mm_cvtsd_ss(Float4 a,Double2 b) noexcept {
    if(is_nan(b.lane[0])) {
        const auto raw=bits<std::uint64_t>(b.lane[0]);
        a.lane[0]=bits<float>(std::uint32_t((raw>>32)&0x80000000u)|0x7fc00000u|std::uint32_t((raw>>29)&0x007fffffu));
    } else { volatile float rounded=static_cast<float>(b.lane[0]);a.lane[0]=rounded; }
    return a;
}
template<class T> inline std::int32_t truncate(T value) noexcept {
    // Check before conversion: floating-to-integer overflow is undefined C++.
    // The lower double interval (-2147483649,-2147483648) truncates validly.
    const double wide=static_cast<double>(value);
    if(!(wide>-2147483649.0 && wide<2147483648.0)) return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
inline std::int32_t _mm_cvttss_si32(Float4 value) noexcept { return truncate(value.lane[0]); }
inline std::int32_t _mm_cvtt_ss2si(Float4 value) noexcept { return truncate(value.lane[0]); }
inline std::int32_t _mm_cvttsd_si32(Double2 value) noexcept { return truncate(value.lane[0]); }
inline Int4 _mm_set_epi32(int w,int z,int y,int x) noexcept {
    return {{static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y),static_cast<std::uint32_t>(z),static_cast<std::uint32_t>(w)}};
}
inline Float4 _mm_castsi128_ps(Int4 value) noexcept { return bits<Float4>(value); }
inline Float4 _mm_xor_ps(Float4 a,Float4 b) noexcept {
    auto x=bits<Int4>(a),y=bits<Int4>(b);for(unsigned i=0;i<4;++i)x.lane[i]^=y.lane[i];return bits<Float4>(x);
}
}
