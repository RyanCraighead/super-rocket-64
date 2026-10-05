#ifndef SMN64_SWINGER_H
#define SMN64_SWINGER_H
#include <stdint.h>

/* Bounded numeric Spider-Man USA 1.0 swinger, recovered from the supplied ROM.
 * This is the original phase-driven arc, not a gravity/constraint pendulum.
 * World vectors are signed fixed12, Y down. Length is in whole native units.
 * Matrices are nine row-major signed fixed12 elements (original 32-byte matrix's
 * rotation part). Graphic allocation, actor lists, camera and contact response
 * remain outside this module. Build without fast-math or FP contraction. */
typedef struct SmN64Swinger {
    uint32_t last_tick;                /* 80, original global tick at update */
    int32_t length;                    /* 108, whole native units */
    int32_t anchor[3];                 /* 10C */
    int16_t initial_basis[9];          /* rotation part of 118 */
    int16_t basis[9];                  /* rotation part of 138 */
    int32_t forward[3];                /* 158: third matrix column */
    int32_t right[3];                  /* 164: first matrix column */
    int32_t negative_up[3];            /* 170: minus second matrix column */
    int32_t phase;                     /* 180, deliberately NOT modulo 4096 */
    int32_t phase_rate;                /* 184 */
    int32_t field_188, field_18c;      /* constructor length/8, constant 9 */
    int16_t pitch, roll;               /* 10 and 14 */
} SmN64Swinger;

/* 8009B2B8 preparation: quantized position-anchor direction, normalized
 * cross products and column assembly. Returns original abs(up.y) in optional
 * vertical_abs. Degenerate directions reproduce the original zero vectors. */
void smn64_swinger_basis(const int32_t position[3], const int32_t anchor[3],
                        const int32_t target_normal[3], int16_t out[9],
                        int32_t *vertical_abs);

/* Numeric constructor. Call with the original prepared basis/target normal.
 * Like a fresh zeroed original allocation, initial roll is zero. The current
 * basis is explicitly exposed as the initial basis before the first update.
 * Does not allocate the original web graphic or an actor. */
void smn64_swinger_init(SmN64Swinger *, const int32_t anchor[3], int32_t length,
                       const int16_t initial_basis[9],
                       const int32_t target_normal[3], uint32_t now);
/* Update consumes absolute engine ticks. Unsigned tick subtraction and phase
 * arithmetic retain the original low 32 bits, including tick rollover. */
void smn64_swinger_step(SmN64Swinger *, uint32_t now);
void smn64_swinger_endpoint(const SmN64Swinger *, int32_t out[3]);
int smn64_swinger_complete(const SmN64Swinger *);
/* Original maps phase to a signed s16 player frame without clamping. Argument
 * is the last frame index, not clip count. Parent AI owns stopping/replacement. */
int16_t smn64_swinger_animation_frame(const SmN64Swinger *, int32_t last_frame);
void smn64_swinger_set_roll(SmN64Swinger *, int32_t roll);
/* Original 800B4458 helper; semantic use is not yet proven. */
int32_t smn64_swinger_phase_bias(const SmN64Swinger *);
#endif
