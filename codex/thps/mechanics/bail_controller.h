#ifndef THPS1_BAIL_CONTROLLER_H
#define THPS1_BAIL_CONTROLLER_H
#include "air_animation.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original +450,+344,+82c,+294,+814,+834,+938, motion and animation.
 * Source_state0 means grounded,1 airborne,2 ramp air. Caller supplies actual
 * contact; animation advancement occurs separately before this controller. */
typedef struct Thps1Bail {
    int32_t active, phase, source_state, reason, impact_done, crouched, kick_timer;
    int32_t velocity[3], acceleration[3];
    ThpsAnim animation;
    uint32_t hidden_joint_mask; /* object+28c; Tony board bits0..2, other bits preserved */
    uint32_t events;
} Thps1Bail;
enum Thps1BailEvents {
    THPS1_BAIL_HIDE_BOARD=1U,
    THPS1_BAIL_SHOW_BOARD=2U,
    THPS1_BAIL_IMPACT=4U,
    THPS1_BAIL_RECOVERED=8U,
    THPS1_BAIL_ANIMATION_CHANGED=16U
};
/* Begin preserves existing velocity and acceleration. Subsequent step clears
 * X/Z acceleration; only phase8 frame<18 additionally clears Y acceleration.
 * Caller clears its trick/rotation/combo state on begin. Source clears those
 * fields in8004d398; score, audio, particles and basis repair are not in API. */
void thps1_bail_begin(Thps1Bail *,const ThpsAnimBank *,int32_t reason);
/* After begin, use landing result's overrides where nonzero. */
void thps1_bail_override(Thps1Bail *,const ThpsAnimBank *,int32_t phase,int32_t clip);
/* flip_held is genuinely physical N64 C-left (mask0x0002,controller+10),
 * even though this source branch selects crouch clip8. Do not pass ollie. */
void thps1_bail_step(Thps1Bail *,const ThpsAnimBank *,int32_t dt_q8,int flip_held);
#ifdef __cplusplus
}
#endif
#endif
