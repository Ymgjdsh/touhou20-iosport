#pragma once
#if defined(__aarch64__) || defined(__arm64__)
#include "../../native_recovered/scalar_sse.hpp"
using __m128 = th20::recovered::scalar_sse::Float4;
using __m128d = th20::recovered::scalar_sse::Double2;
using __m128i = th20::recovered::scalar_sse::Int4;
using namespace th20::recovered::scalar_sse;
#else
#include_next <immintrin.h>
#endif
