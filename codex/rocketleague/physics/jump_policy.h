#ifndef ROCKET_JUMP_POLICY_H
#define ROCKET_JUMP_POLICY_H
#include <math.h>
/* Height preference is independent of road speed and world gravity. */
#define ROCKET_JUMP_MIN 50u
#define ROCKET_JUMP_MAX 100u
#define ROCKET_JUMP_DEFAULT 50u
static inline int rocket_jump_valid(unsigned percent) {
    return percent >= ROCKET_JUMP_MIN && percent <= ROCKET_JUMP_MAX;
}
static inline unsigned rocket_jump_preference(unsigned percent) {
    return rocket_jump_valid(percent) ? percent : ROCKET_JUMP_DEFAULT;
}
/* sqrt(height) sets ballistic energy, then small corrections account for the
 * pinned 120 Hz suspension/sticky launch and unchanged 25..200 ms hold window.
 * Constants are calibrated against actual RocketSim flat-ground apex sweeps;
 * tests cover every integer setting and all 30 Hz hold lengths. At 100% both
 * factors are exactly one. Slopes, momentum, double jumps and boost can change
 * absolute trajectory height; no body velocity, gravity or geometry is scaled. */
static inline float rocket_jump_impulse_scale(unsigned percent) {
    float q = sqrtf(rocket_jump_preference(percent) / 100.f);
    return q + .10f * q * (1.f - q);
}
static inline float rocket_jump_hold_scale(unsigned percent) {
    float q = sqrtf(rocket_jump_preference(percent) / 100.f);
    return q + .19f * q * (1.f - q);
}
#endif
