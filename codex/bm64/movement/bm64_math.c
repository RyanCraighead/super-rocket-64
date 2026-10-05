#include "bm64_math.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static uint32_t bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float original_nan(void) {
    uint32_t raw = UINT32_C(0x7F810000);
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

/* Doubles read at 802A4798..47B0 (duplicated at 47E8..4800). */
static float polynomial(double x) {
    double square = x * x;
    double part = 0x1.5dbdf0e314bfep-19 * square;
    double result;
    part = part + -0x1.9f6ffeea56814p-13;
    part = part * square;
    part = part + 0x1.110ed3804c2a0p-7;
    part = part * square;
    part = -0x1.55554bc83656dp-3 + part;
    result = x * square;
    result = result * part;
    result = result + x;
    return (float)result;
}

float bm64_sin_degrees(float degrees) {
    float radians = 0x1.1df46ap-6f * degrees; /* 80234434 */
    uint32_t range = (bits(radians) >> 22) & 0x1FFu;
    double x = radians;
    double quotient;
    int32_t nearest;
    float result;
    /* 8029CF20: original exponent/fraction range tests. */
    if (range < 0xFFu) {
        if (range < 0xE6u) return radians;
        return polynomial(x);
    }
    if (range >= 0x136u) return isnan(radians) ? original_nan() : 0.0f;
    quotient = x * 0x1.45f306dc9c883p-2;
    nearest = (int32_t)(quotient >= 0.0 ? quotient + 0.5 : quotient - 0.5);
    x = x - (double)nearest * 0x1.921fb50000000p+1;
    x = x - (double)nearest * 0x1.110b4611a6263p-25;
    result = polynomial(x);
    return (nearest & 1) ? -result : result;
}

float bm64_cos_degrees(float degrees) {
    float radians = 0x1.1df46ap-6f * degrees; /* 80234408 */
    uint32_t range = (bits(radians) >> 22) & 0x1FFu;
    double x, quotient, shifted;
    int32_t nearest;
    float result;
    /* 8029D0E0. Cosine uses the same odd polynomial, phase shifted. */
    if (range >= 0x136u) return isnan(radians) ? original_nan() : 0.0f;
    x = radians > 0.0f ? radians : -radians;
    quotient = x * 0x1.45f306dc9c883p-2 + 0.5;
    nearest = (int32_t)(quotient >= 0.0 ? quotient + 0.5 : quotient - 0.5);
    shifted = (double)nearest - 0.5;
    x = x - shifted * 0x1.921fb50000000p+1;
    x = x - shifted * 0x1.110b4611a6263p-25;
    result = polynomial(x);
    return (nearest & 1) ? -result : result;
}
