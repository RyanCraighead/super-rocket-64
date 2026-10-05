#ifndef SMN64_LOCOMOTION_H
#define SMN64_LOCOMOTION_H
#include <stddef.h>
#include "n64_behavior.h"
/* USA 1.0 flat, unheld, non-crawl slice. Units are original fixed12/Y-down.
 * Caller owns contacts, buttons, animation advance, and unsupported AI actions.
 * All integers named by their player offsets below are retained state, not host
 * controller tuning. Call basis_begin after physics, input before grounded_ai,
 * then target_velocity. Do not use -ffast-math or floating-point contraction. */
typedef struct SmN64Locomotion {
    SmN64Anim anim;
    uint32_t state;                         /* 1118: 1, 10, 400000 */
    int32_t forward_x, forward_z;           /* f64, f6c; basis column */
    int32_t yaw_delta;                      /* 658, consumed next basis_begin */
    int32_t turn_target, turn_step, turn_ticks; /* 10e8, 10ec, 10f0 */
    int32_t input_ramp, run_ramp;            /* a3c, 11b4 */
    int32_t velocity_x, velocity_y, velocity_z; /* 64,68,6c */
    int16_t yaw;                            /* 12, visual orientation */
    uint16_t input_angle, input_base;        /* 1128,112a */
    uint16_t idle_ticks;                    /* 119e */
    int8_t analog_x, analog_y;              /* 1123,1124 */
    int8_t previous_x, previous_y;           /* 1125,1126 */
} SmN64Locomotion;
int32_t smn64_locomotion_sin(int32_t angle);
int32_t smn64_locomotion_cos(int32_t angle);
int32_t smn64_locomotion_atan(int32_t y, int32_t x);
uint16_t smn64_locomotion_heading(const SmN64Locomotion *);
void smn64_locomotion_input(SmN64Locomotion *, int8_t x, int8_t y);
void smn64_locomotion_ramp(SmN64Locomotion *);
void smn64_locomotion_turn(SmN64Locomotion *, uint16_t target, int fast);
/* States2/4 use player10F4:1 normally,2 in special original level0x806.
 * Returns1, or-1 without mutation for unsupported factors. */
int smn64_locomotion_turn_air(SmN64Locomotion *, uint16_t target, int fast, uint8_t turn_factor);
void smn64_locomotion_turn_advance(SmN64Locomotion *);
void smn64_locomotion_basis_begin(SmN64Locomotion *);
void smn64_locomotion_target_velocity(SmN64Locomotion *);
/* Caller-approved air/landing override; caller supplies the original heading
 * choice and gates aiming, D1C, D20 and state mask0xBFF7F8C1. */
void smn64_locomotion_velocity_at(SmN64Locomotion *, uint16_t heading);
/* Supported state/clip transitions, with counts from the verified original bank.
 * Returns 1 on success, -1 unsupported state/clip/count, -2 idle-fidget dependency.
 * -1 does not mutate; -2 commits state up to idle RNG and requires caller to
 * run clip293 for original rng(100)<50 or294 otherwise. The ceiling-hang
 * eligibility check after idle_ticks>3600 remains the contact adapter's job.
 * Analog input and ramp must have run first. Jump/lost-ground
 * checks belong BEFORE this routine and can replace its state. The slice excludes
 * held objects, crawling, aiming, cinematic camera, scripts, and button actions. */
typedef int (*SmN64BasisUpdate)(void *,SmN64Locomotion *);
/* Authoritative full-basis hook for the internal completed-reversal update.
 * Return1 performed; negative aborts. NULL retains the flat specialization. */
/* Generic original successors precede action dispatch. Returns completed clip
 * or65535. The prepared AI consumes that token without cycling a second time. */
int smn64_locomotion_grounded_successor(SmN64Locomotion *,const uint16_t *,size_t);
int smn64_locomotion_grounded_ai_prepared(SmN64Locomotion *,uint16_t,const uint16_t *,size_t,SmN64BasisUpdate,void *,uint16_t);
int smn64_locomotion_grounded_ai_with_basis(SmN64Locomotion *,uint16_t camera_yaw,
    const uint16_t *counts,size_t count,SmN64BasisUpdate,void *context);
int smn64_locomotion_grounded_ai(SmN64Locomotion *, uint16_t camera_yaw,
                                const uint16_t *counts, size_t count);
int smn64_locomotion_grounded_ai_carrying(SmN64Locomotion *,uint16_t,const uint16_t *,size_t,SmN64BasisUpdate,void *,uint16_t,uint32_t object_flags);
void smn64_locomotion_velocity_carrying(SmN64Locomotion *,uint32_t object_flags);
#endif
