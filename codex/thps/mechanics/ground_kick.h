#ifndef THPS1_GROUND_KICK_H
#define THPS1_GROUND_KICK_H
#include <stdint.h>
/* Ground-only control flow from0x80057eac..0x80058594.
 * Animation advance is external, with original clip counts. Call AFTER clearing
 * the frame's acceleration and BEFORE HandleJump, as main0x8005a424..5a438 does.
 * Original helper4d2d4 handles pending orientation switches at each play event;
 * caller must apply those if it represents switched stance. */
typedef struct Thps1GroundKick {
 int32_t state;                 /* object0x82c; only0 runs */
 int32_t timer;                 /*0x938, decremented once per call */
 int32_t target_speed;          /*0x430 */
 int32_t animation, frame, finished, animation_rate;
 int32_t sound_pending;         /*0x80c */
 int32_t speed;                 /* original sqrt(dot(v,v))<<6 */
 int32_t slope_dot;             /* dot(plane-projected v, inplane gravity) */
 int32_t auto_kick, push_button, crouched, down_and_turning;
 int32_t effective_speed_stat;  /* stat+equipment+special, or override */
 int32_t acceleration_scale;    /* OUTPUT: 0,4,8; acceleration=-basis*scale */
 int32_t animation_changed;     /* OUTPUT: consumer restarts returned clip */
 int32_t kick_sound;            /* OUTPUT: play original SFX34 */
} Thps1GroundKick;
void thps1_ground_kick(Thps1GroundKick *);
#endif
