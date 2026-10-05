#ifndef SMN64_BEHAVIOR_H
#define SMN64_BEHAVIOR_H
#include <stdint.h>
/* Bounded, independently implemented behavior from USA 1.0 machine-code observations.
 * No engine, collision, input state machine, or per-action animation mapping is implied.
 * Counts must come from the selected and bounds-checked original animation bank. */
typedef struct SmN64Anim {
    int16_t frame;
    uint16_t animation;
    uint8_t mode;
    int8_t direction;
    uint8_t finished;
    int16_t target;
    uint16_t fraction;
    uint16_t frame_count;
    uint32_t rate;
    int32_t elapsed_ticks;
} SmN64Anim;
void smn64_anim_run(SmN64Anim *, uint16_t animation, uint16_t count, int32_t from, int32_t to);
void smn64_anim_cycle(SmN64Anim *, uint16_t animation, uint16_t count, int8_t direction);
void smn64_anim_advance(SmN64Anim *);
int32_t smn64_damage_scaled(int32_t damage, uint8_t suit, int32_t difficulty);
/* Ground-only transition slice. The owning runtime must gate the call to the
 * original-compatible standing/running state and supply original clip counts.
 * -1 means unsupported held-object/surface/platform dependency; no mutation.
 * These helpers do NOT integrate velocity, resolve contacts, or implement AI. */
typedef struct SmN64Ground {
    SmN64Anim anim;
    uint32_t state;
    uint32_t aiming;
    uint32_t holding_object;
    uint32_t surface_mode;
    uint32_t platform_present;
    uint16_t collision;
    uint8_t jump_pressed;
    uint8_t copied_jump_pressed;
    uint8_t ground_grace;
    int32_t position_y;
    int32_t falling_origin_y;
    int32_t jump_velocity;
    int32_t jump_variant;
    int32_t field_d20;
    int32_t field_d24;
    int32_t field_1184;
    int32_t acceleration_phase;
} SmN64Ground;
int smn64_ground_jump_request(SmN64Ground *, uint16_t standing_count, uint16_t running_count);
int smn64_ground_lost(SmN64Ground *, uint16_t fall_count);
/* Original free-space arithmetic only; contact resolution is an explicit adapter.
 * Original default acceleration=(0,40960,0), drag=(1,4,1), native Y points down.
 * Nonpositive/downward distinctions inside contact queries do not alter these
 * no-hit outputs. The owner supplies elapsed ticks, usually 2; no wall-clock
 * cadence is inferred by this function. */
void smn64_free_motion(int32_t velocity[3], const int32_t acceleration[3],
                       const uint8_t drag[3], int32_t elapsed_ticks, int32_t displacement[3]);
void smn64_jump_tail(int32_t *velocity_y, int32_t launch_velocity, int32_t *base_timer,
                    int32_t *hold_timer, uint16_t collision, int32_t elapsed_ticks);
#endif
