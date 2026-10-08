#ifndef ROCKET_JUMP_POLICY_H
#define ROCKET_JUMP_POLICY_H
#include <math.h>
/* Height preference is independent of road speed and world gravity. */
#define ROCKET_JUMP_MIN 30u
#define ROCKET_JUMP_MAX 100u
#define ROCKET_JUMP_DEFAULT 100u
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
    percent=rocket_jump_preference(percent);
    float q = sqrtf(percent / 100.f);
    float original=q + .10f * q * (1.f - q);
    /* Extend the measured curve below 50 without changing any old setting. */
    return percent<50 ? original + .11f*q*((50.f-percent)/100.f) : original;
}
static inline float rocket_jump_hold_scale(unsigned percent) {
    percent=rocket_jump_preference(percent);
    float q = sqrtf(percent / 100.f);
    float original=q + .19f * q * (1.f - q);
    return percent<50 ? original + .15f*q*((50.f-percent)/100.f) : original;
}
#endif
