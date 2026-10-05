#include "zip_physics_n64.h"
#include <limits.h>

static int32_t signed32(uint32_t x) {
    return x <= INT32_MAX ? (int32_t)x : -1 - (int32_t)(UINT32_MAX - x);
}
static int32_t arithmetic_right(int32_t value, unsigned shift) {
    uint32_t bits = (uint32_t)value;
    shift &= 31u;
    if (!shift) return value;
    return signed32((bits >> shift) |
                    ((bits & 0x80000000u) ? UINT32_MAX << (32u - shift) : 0u));
}
int smn64_zip_physics_active(uint32_t state, uint16_t clip, int16_t frame) {
    return state == 0x40000u &&
           ((clip == 270u && frame >= 13) || clip == 271u);
}
void smn64_zip_physics_step(const int32_t velocity[3],
                           const int32_t acceleration[3],
                           const uint8_t drag[3], int32_t dt,
                           SmN64ZipPhysics *out) {
    SmN64ZipPhysics result;
    uint32_t multiplier = dt >= 3 ? (uint32_t)dt - 1u : 1u;
    for (unsigned i = 0; i < 3; ++i) {
        int32_t value = signed32((uint32_t)velocity[i] +
                                 (uint32_t)acceleration[i]);
        value = signed32((uint32_t)value -
                         (uint32_t)arithmetic_right(value, drag[i]));
        if (value >= -2048 && value <= 2048) value = 0;
        result.velocity[i] = value;
        result.displacement[i] = signed32((uint32_t)value * multiplier);
    }
    *out = result;
}
