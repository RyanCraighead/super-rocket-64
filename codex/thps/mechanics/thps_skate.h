#ifndef CODEX_THPS_SKATE_H
#define CODEX_THPS_SKATE_H
/* Original THPS1 N64 USA Rev1 reconstructed skating slice. See PROVENANCE.md.
 * Host is Y-up; yaw 0 = +Z, 0x4000 = +X. Source Q12 velocity uses Y-down.
 * Host scale is HOST UNITS per original source world unit; never gameplay tuning.
 * One begin/resolve pair is ONE source physics tick. Rate is documented below.
 * Begin copies committed state to pending and does not mutate committed state.
 * Resolve only changes pending. Commit `state = pending` only after both return 1.
 */
#include <stdint.h>
#include "air_tricks.h"
#include "air_spin.h"
#include "bail_controller.h"
#include "grind_controller.h"
#include "score_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
#define THPS_SKATE_SOURCE_HZ 30
#define THPS_SKATE_HOST_HZ 30
#define THPS_SKATE_TICKS_PER_HOST_FRAME 1

enum ThpsSkateMode { THPS_SKATE_GROUND=0, THPS_SKATE_AIR=1, THPS_SKATE_BAIL=2, THPS_SKATE_GRIND=3 };
enum ThpsSkateEvent {
 THPS_EVENT_NONE=0, THPS_EVENT_OLLIE=1u<<0, THPS_EVENT_LAND=1u<<1,
 THPS_EVENT_BAIL=1u<<2, THPS_EVENT_FLIP=1u<<3, THPS_EVENT_GRAB=1u<<4,
 THPS_EVENT_WALL=1u<<5, THPS_EVENT_TRICK_END=1u<<6,
 THPS_EVENT_GRIND=1u<<7, THPS_EVENT_GRIND_END=1u<<8,
 THPS_EVENT_RECOVER=1u<<9, THPS_EVENT_TRICK_BASE=1u<<10,
 THPS_EVENT_SCORE_BANK=1u<<11, THPS_EVENT_SPECIAL=1u<<12
};
/* Source clip IDs established by original animation calls; no invented clips. */
enum ThpsSkateAnimation {
 THPS_ANIM_ROLL=0, THPS_ANIM_PUSH_START=1, THPS_ANIM_PUSH=3, THPS_ANIM_OLLIE=4,
 THPS_ANIM_CROUCH=8, THPS_ANIM_NOLLIE=13, THPS_ANIM_FASTPLANT=45,
 THPS_ANIM_UNRESOLVED=0xffff
};
typedef struct ThpsSkateInput {
 float steer;                  /* [-1,+1], left/right in board frame */
 float forward;                /* [-1,+1], negative means brake */
 uint8_t ollie, flip, grab, grind;
 int8_t spin;                  /* -1 left shoulder, +1 right shoulder */
 uint8_t spin_left, spin_right; /* independent shoulders; legacy spin also accepted */
 /* Modern controller keeps held spin and queued 180 independent. The legacy
  * N64/keyboard shoulders above retain their existing combined behavior. */
 uint8_t spin_continuous_left, spin_continuous_right, spin_180_left, spin_180_right;
 uint8_t up, down, left, right; /* optional original digital board-relative directions */
} ThpsSkateInput;
/* Explicit read-only host geometry boundary. Candidate is queried BEFORE the
 * source main helpers. Geometry/closest/distance are host supplied; actor velocity, right axis,
 * physical state, source858 readiness and bail eligibility are wrapper owned.
 * Callback/context are used only during this begin call, never retained. */
typedef struct ThpsSkateHost {
 const Thps1GrindEntry *grind_candidate;
 Thps1GrindRailLookup rail_lookup;
 void *rail_context;
} ThpsSkateHost;
typedef struct ThpsSkateContact {
 float position[3];            /* collision-resolved host position */
 float floor_normal[3], wall_normal[3];
 uint8_t grounded, hit_wall, hit_ceiling, valid;
} ThpsSkateContact;
typedef struct ThpsSkateState {
 float position[3], velocity[3], delta[3], floor_normal[3];
 float source_to_host;          /* scale is supplied by model/world adapter */
 uint16_t yaw, animation;
 float animation_frame;
 uint32_t ticks, events;
 int32_t source_velocity[3];    /* signed Q12, original Y-down */
 int32_t source_acceleration[3]; /* retained through native collision */
 /* Rail-owned active/endpoint displacement bypasses host floor snapping.
  * Ollie and bail displacement still request ordinary host contact. */
 uint8_t step_pending, step_air, step_ground_forces, step_skip_host_collision;
 int32_t source_speed, charge_ticks, kick_ticks, air_ticks;
 int32_t spin_total, bailout_ticks, source_turn_rate;
 int32_t kick_target, kick_sound_pending;
 uint16_t clip_counts[78], anim_fraction;
 int16_t anim_frame_i;
 int8_t anim_direction, anim_target, anim_continuation;
 uint8_t anim_finished, counts_bound;
 uint32_t anim_rate;
 uint32_t score, combo_score;
 uint8_t mode, previous_ollie, previous_flip, previous_grab, input_cancelled;
 uint8_t stat_air, stat_speed, stat_ollie, stat_balance;
 uint8_t valid;
 /* Original physical state0/1/4 is independent of bail.active. mode never
  * becomes THPS_SKATE_BAIL (retained only for compatibility mode labels). */
 int32_t source_state;
 uint32_t stance_flags;
 int32_t source_forward[3], source_side[3], source_up[3];
 int16_t source_physics_basis[9]; /* original864, snapshot before physics */
 int32_t source_body_position[3], source_previous_position[3], source_earlier_position[3];
 const int16_t *pitch_rotations, *yaw_rotations; /* adapter-owned 4096x9 tables */
 float body_offset_source; /* explicit model body-origin minus foot anchor */
 uint8_t body_offset_bound, source_body_valid;
 uint8_t previous_grind, previous_spin_left, previous_spin_right;
 uint8_t previous_180_left, previous_180_right;
 uint8_t step_new_bail, landing_pending, jump_latched, trick_sequence_ready;
 int8_t previous_spin;
 ThpsSkateInput step_input;
 Thps1AirTricks tricks;
 Thps1AirSpin spin;
 Thps1Bail bail;
 Thps1GrindState grind;
 Thps1GrindRail grind_rail;
 Thps1Score scoring;
 int32_t score_boost_ticks, landing_previous_state;
 int32_t grind_score_position[3]; /* original9d0 movement-scoring anchor */
 /* Actual original trick identity/table score call and read-only compatibility totals. */
 int32_t trick_index, trick_base_points, trick_hold_base_points;
 int32_t landing_reason;
} ThpsSkateState;
int thps_skate_reset(ThpsSkateState *, float x,float y,float z,uint16_t yaw,float source_to_host);
void thps_skate_cancel_inputs(ThpsSkateState *);
int thps_skate_bind_clip_counts(ThpsSkateState *,const uint16_t *counts,uint32_t count);
/* Required before host rail candidates are used. Supply verified model anchor
 * in source units; this is geometry, never a tuning/grind-height parameter. */
int thps_skate_bind_body_offset(ThpsSkateState *,float source_units);
int thps_skate_bind_rotation_tables(ThpsSkateState *,const int16_t *pitch,
 const int16_t *yaw,uint32_t matrix_count);
int thps_skate_begin_step_with_host(const ThpsSkateState *,const ThpsSkateInput *,
 const ThpsSkateHost *,ThpsSkateState *pending);
int thps_skate_begin_step(const ThpsSkateState *committed,const ThpsSkateInput *,ThpsSkateState *pending);
int thps_skate_resolve(ThpsSkateState *pending,const ThpsSkateContact *);
/* Direct arithmetic kernels; Q12 return is positive upward impulse magnitude. */
int32_t thps1_ollie_charge_limit(int32_t air_stat,int32_t ollie_stat);
int32_t thps1_ollie_impulse(int32_t air_stat,int32_t ollie_stat,int32_t charge,int vertical_ramp);
const char *thps_skate_mode_name(uint8_t);
#ifdef __cplusplus
}
#endif
#endif
