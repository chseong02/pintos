#include <stdint.h>

/* 1 << 14 = 16384 */
#define FIXED_POINT_F 16384

typedef int32_t fp32;

#define FP32_TO_FP (N)          N * FIXED_POINT_F
#define FP32_TO_INT (F)         F / FIXED_POINT_F
#define FP32_TO_INT_ROUND (F)   F >= 0 ? \
    (F + FIXED_POINT_F / 2) / FIXED_POINT_F : (F - FIXED_POINT_F / 2) / FIXED_POINT_F
#define FP32_FP32_ADD (A, B)    A + B
#define FP32_FP32_SUB (A, B)    A - B
#define FP32_INT_ADD (A, B)     A + FP32_TO_FP(B)
#define FP32_INT_SUB (A, B)     A - FP32_TO_FP(B)
#define FP32_FP32_MUL (A, B)    ((int64_t) A) * B / FIXED_POINT_F
#define FP32_INT_MUL (A, B)     A * B
#define FP32_FP32_DIV (A, B)    ((int64_t) A) * FIXED_POINT_F / B
#define FP32_INT_DIV (A, B)     A / B
