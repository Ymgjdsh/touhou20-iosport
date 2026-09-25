#pragma once
#if defined(__aarch64__) || defined(__arm64__)
#include "immintrin.h"
#else
#include_next <xmmintrin.h>
#endif
