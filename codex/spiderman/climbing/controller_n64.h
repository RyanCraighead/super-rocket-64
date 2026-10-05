#ifndef SMN64_CLIMB_CONTROLLER_H
#define SMN64_CLIMB_CONTROLLER_H
#include "transitions_n64.h"
typedef struct SmN64ClimbInput {
    int8_t stick_x, stick_y;
    uint8_t jump_pressed, jump_held, aim_held;
    uint16_t camera_yaw, elapsed_ticks;
    uint32_t other_actions; /* nonzero requires env.action; otherwise return-6 */
} SmN64ClimbInput;
/* Original input after logical stick/d-pad selection; preserves crawl mirrors. */
int smn64_climb_input(SmN64ClimbState *, int8_t logical_x, int8_t logical_y);
/* Seed original RNG with explicit host seed/history, never global host rand(). */
void smn64_climb_seed(SmN64ClimbState *, uint32_t seed);
/* After physics/input and original animation advance. Owns successor, ordinary
 * crawl idle/move/reverse, jump detach and authored transition dispatch. */
int smn64_climb_ai(SmN64ClimbState *, uint16_t camera_yaw,
                   const SmN64ClimbTransitionEnv *, SmN64ClimbTransitionEvent *,
                   const uint16_t *counts, size_t count);
/* Composed native adhered tick. Performs original animation advance, retained
 * physics (except authored states80000/1000/2000), basis/classification, input,
 * ramp, AI, turn and velocity/gravity/jump-tail. This ends before world actor
 * interactions/camera simulation. Parent owns ground/air after CF0 clears.
 * On error no state/event is committed. Missing callback=-3; external action=-6.
 * source_level23 physics remains an explicit scripted-world dependency. */
int smn64_climb_tick(SmN64ClimbState *, const SmN64ClimbInput *,
                     const SmN64ClimbTransitionEnv *, SmN64ClimbTransitionEvent *,
                     const uint16_t *, size_t, uint16_t bright_target,
                     uint16_t normal_target, int32_t source_level);
/* Same composed owner after the common actor prepass advanced animation.
 * Production calls this OR climb_tick, never both. */
int smn64_climb_tick_after_animation(SmN64ClimbState *,const SmN64ClimbInput *,
    const SmN64ClimbTransitionEnv *,SmN64ClimbTransitionEvent *,const uint16_t *,
    size_t,uint16_t,uint16_t,int32_t);
/* Shared actor target selection belongs after basis/drag and before input. */
typedef int (*SmN64ClimbPreInput)(void *,SmN64ClimbState *);
int smn64_climb_tick_after_animation_hooked(SmN64ClimbState *,const SmN64ClimbInput *,
    const SmN64ClimbTransitionEnv *,SmN64ClimbTransitionEvent *,const uint16_t *,size_t,
    uint16_t,uint16_t,int32_t,SmN64ClimbPreInput,void *,const uint32_t *movement_enabled);
/* Action handoff tail9251C..92D84: turn request/advance, next acceleration,
 * regular-camera unheld target velocity, then jump-tail. Does NOT advance
 * animation, physics, input, ramp or state AI. Supports nonadhered web states.
 * Camera simulation and the intervening moving-platform owner are excluded.
 * Do not call after climb_ai/climb_tick: those already execute their tail. */
int smn64_climb_post_ai_tail(SmN64ClimbState *, uint16_t camera_yaw,
                            int movement_enabled);
int smn64_climb_post_ai_tail_carrying(SmN64ClimbState *,uint16_t,int,uint32_t object_flags);
#endif
