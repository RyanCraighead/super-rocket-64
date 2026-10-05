/* Independently expressed original N64 marker arithmetic. No ROM/code bytes. */
#include "markers_n64.h"
#include <float.h>
#include <limits.h>

#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error "Marker evaluation requires IEEE binary32 float"
#endif

/* Volatile stores force each original mul.s/add.s boundary to binary32, even
 * on excess-precision hosts, and prevent multiply-add contraction. */
static float fadd(float a, float b) { volatile float v = a + b; return v; }
static float fmul(float a, float b) { volatile float v = a * b; return v; }
static float fi32(int32_t a) { volatile float v = (float)a; return v; }
static int32_t signed32(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v);
}
static int16_t negate16(int16_t x) {
    uint16_t v = (uint16_t)(0u - (uint16_t)x);
    return v <= INT16_MAX ? (int16_t)v : (int16_t)(-1 - (int32_t)(UINT16_MAX - v));
}
static int32_t sar4(int32_t v) {
    /* Avoid implementation-defined signed shifts and INT_MIN negation. */
    return v >= 0 ? v / 16 : (int32_t)(-((-((int64_t)v) + 15) / 16));
}
static int32_t trunc_s32(float x) {
    /* Source checks are strict c.lt.s at +/-2^31. The observed original-MIPS
     * oracle saturates trunc.w.s(+2^31) to INT_MAX at that exact endpoint;
     * source code explicitly substitutes INT_MIN outside the closed interval.
     * This endpoint policy is function-oracle evidence, not N64 FPU hardware
     * certification (normal game-range inputs never reach it). */
    if (x == 2147483648.0f) return INT32_MAX;
    if (!(x >= -2147483648.0f && x < 2147483648.0f)) return INT32_MIN;
    return (int32_t)x;
}
static float transform_row(const float xyz[3], const int16_t matrix[9],
                           unsigned row, float translation, int mirrored) {
    unsigned p = row * 3;
    int16_t x = mirrored ? negate16(matrix[p]) : matrix[p];
    float v = fmul(xyz[0], fmul(fi32(x), 0x1p-12f));
    v = fadd(v, fmul(xyz[1], fmul(fi32(matrix[p+1]), 0x1p-12f)));
    v = fadd(v, fmul(xyz[2], fmul(fi32(matrix[p+2]), 0x1p-12f)));
    return fadd(v, translation);
}
int smn64_marker_world(const SmN64Marker *markers, size_t marker_count,
                      size_t marker_index, const int16_t *poses,
                      size_t pose_count, const int16_t body_matrix[9],
                      const int32_t body_translation[3],
                      const int32_t position_fixed12[3], int mirror_x,
                      int32_t out[3]) {
    const SmN64Marker *marker;
    const int16_t *pose;
    float xyz[3], joint[3];
    int32_t result[3];
    unsigned row;
    if (!markers || !poses || !body_matrix || !body_translation ||
        !position_fixed12 || !out || marker_index >= marker_count ||
        marker_count > SIZE_MAX / sizeof(*markers) ||
        !pose_count || pose_count > SIZE_MAX / (12 * sizeof(int16_t))) return 0;
    marker = markers + marker_index;
    if (marker->joint >= pose_count) return 0;
    pose = poses + (size_t)marker->joint * 12;
    for (row = 0; row < 3; ++row) xyz[row] = fi32(marker->xyz[row]);
    for (row = 0; row < 3; ++row)
        joint[row] = transform_row(xyz, pose, row, fi32(pose[9+row]), 0);
    for (row = 0; row < 3; ++row) {
        int32_t transformed = trunc_s32(transform_row(joint, body_matrix, row,
                                                     fi32(body_translation[row]), mirror_x));
        uint32_t shifted = (uint32_t)transformed << 12;
        int32_t delta = signed32(shifted - (uint32_t)position_fixed12[row]);
        result[row] = signed32((uint32_t)position_fixed12[row] + (uint32_t)sar4(delta));
    }
    for (row = 0; row < 3; ++row) out[row] = result[row];
    return 1;
}
