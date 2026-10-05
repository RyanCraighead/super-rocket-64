#ifndef SMN64_PLAYER_H
#define SMN64_PLAYER_H
#include <stddef.h>
#include <stdint.h>
#include "locomotion_n64.h"

/* Revision-locked bounded controller. Original fixed12 position/velocity, Y down.
 * Native body origin is 96 world units above a flat floor. Host render scaling
 * is a separate explicit adapter and is not encoded into these physics values.
 * No wall crawl, slopes, moving platforms, attacks, scripts or fall damage yet. */
typedef struct SmN64Player {
    SmN64Locomotion motion;
    int32_t position[3];
    int32_t launch_velocity;
    int32_t jump_base_timer, jump_hold_timer;
    int32_t jump_variant;
    int32_t falling_origin_y;
    int32_t previous_velocity_y;
    uint32_t random_state[3];
    uint32_t ticks;
    uint16_t collision;
    uint8_t ground_grace;
    uint8_t drag[3];
    uint8_t awaiting_contact;
    int32_t suspended;
} SmN64Player;

typedef struct SmN64Input {
    int8_t stick_x, stick_y;
    uint8_t jump_pressed, jump_held;
    uint16_t camera_yaw; /* 4096 units per turn; native basis, not host degrees. */
    uint16_t elapsed_ticks; /* 1..6; host normal 30Hz mapping chooses 2. */
} SmN64Input;

typedef struct SmN64MotionRequest {
    int32_t from[3], displacement[3], proposed[3];
    int32_t velocity[3]; /* after original gravity/drag, before contact response */
    int32_t body_to_floor; /* original 96<<12 */
} SmN64MotionRequest;

typedef struct SmN64Contact {
    int32_t position[3]; /* host-resolved native body origin */
    uint8_t grounded, ceiling, wall;
    uint8_t unsupported; /* slopes, moving platforms or other unimplemented world */
} SmN64Contact;

/* Counts are from the verified original 300-slot animation bank. On a negative
 * result the caller must suspend this character; do not fall back to Mario. */
int smn64_player_init(SmN64Player *, const int32_t native_position[3],
                     uint16_t yaw, uint32_t seed, const uint16_t *counts, size_t count);
/* Animation/tick half only, for the full original contact owner. It does NOT
 * apply velocity/drag/gravity/displacement. After successful prepare, call one
 * recovered physics owner then finish/finish_with_hooks, never begin as well. */
/* Shared full actor owner has already advanced animation with the old rate. */
int smn64_player_prepare_after_animation(SmN64Player *,const SmN64Input *);
int smn64_player_prepare(SmN64Player *,const SmN64Input *);
int smn64_player_begin(SmN64Player *, const SmN64Input *, SmN64MotionRequest *);
/* Source-positioned extension points for other recovered mechanics. A callback
 * runs after original physics/contact, basis, input/ramp and authored successors.
 * It must be transactional/read-only outside its supplied actor/runtime copy.
 * Return0 to continue the original ground/air handler,1 when another recovered
 * action takes ownership, or negative if a required dependency is unavailable.
 * On takeover finish_routed returns2 BEFORE post-AI turn/velocity/jump-tail;
 * the owning composed controller must execute that shared tail exactly once.
 * No callback means precisely the existing bounded ground/jump path. */
typedef enum SmN64PlayerActionStage {
    SMN64_ROUTE_IDLE_BEFORE_JUMP=1, /*9095C: web,melee,wall attack,swing,zipB,zipR*/
    SMN64_ROUTE_RUN_BEFORE_JUMP=2,  /*91FB8: web*/
    SMN64_ROUTE_RUN_AFTER_JUMP=3,   /*91FD8: wall attack,melee,web,approach,swing,zip*/
    SMN64_ROUTE_RISING=4,           /*91900: ceiling,air attack,swing,zip*/
    SMN64_ROUTE_FALLING=5,          /*91AD0: wall,ceiling,air attack,swing,zip*/
    SMN64_ROUTE_LANDING_AFTER_JUMP=6, /*91E50: melee,swing,zipB,zipR*/
    SMN64_ROUTE_FALLING_LAND=7, /*original9AD3C before airborne action requests*/
    SMN64_ROUTE_CARRY_DROP=8, /*9AC98: source4*up release before falling*/
    SMN64_ROUTE_CARRY_SMASH=9 /*91F7C: source21 then held obstacle smash*/
} SmN64PlayerActionStage;
typedef int (*SmN64PlayerRoute)(void *,SmN64Player *,SmN64PlayerActionStage);
/* Optional authoritative replacement for each ORIGINAL basis update, including
 * internal hard-reversal jump rotations. The full actor owner runs the original
 * full-basis helper and copies resulting forward_x/z back into locomotion. This
 * preserves source previous-normal-cache renormalization even at delta0, which
 * the flat specialization deliberately omits. Return1 performed, negative to
 * abort. The flat helper is NOT also called. Context must be the caller's
 * transaction copy, not live host state. No hook retains the verified flat path. */
typedef int (*SmN64PlayerBasis)(void *,SmN64Locomotion *);
int smn64_player_finish_with_hooks(SmN64Player *,const SmN64Input *,const SmN64Contact *,
                       const uint16_t *counts,size_t count,
                       SmN64PlayerRoute,SmN64PlayerBasis,void *context);
int smn64_player_finish_routed(SmN64Player *, const SmN64Input *, const SmN64Contact *,
                       const uint16_t *counts, size_t count,
                       SmN64PlayerRoute route, void *context);
int smn64_player_finish(SmN64Player *, const SmN64Input *, const SmN64Contact *,
                       const uint16_t *counts, size_t count);
/* Held ordinary ground states: source jump rejection, source move/stop clips
 * and analog speed. Drop/smash callbacks must return0 after real release. */
int smn64_player_finish_carrying(SmN64Player *,const SmN64Input *,const SmN64Contact *,const uint16_t *,size_t,SmN64PlayerRoute,SmN64PlayerBasis,void *,uint32_t object_flags);
#endif
