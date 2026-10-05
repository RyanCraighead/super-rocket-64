#ifndef THPS1_GRIND_CONTROLLER_H
#define THPS1_GRIND_CONTROLLER_H
#include <stdint.h>
#include "air_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* USA Rev1 source-Q12, Y DOWN. position is ORIGINAL ACTOR/BODY origin:
 * entry position = closest point on authored rail, then Y -= 25*4096.
 * Do not pass the SM64 foot origin without the documented adapter conversion.
 * Rail lookup/closest collision and yaw matrix application are host boundaries.
 * No original asset bytes, pointers, scoring tables, or THPS2 mechanics. */
#define THPS1_GRIND_NO_RAIL (-1)
enum Thps1GrindEvent { THPS1_GRIND_ENTER=1, THPS1_GRIND_END=2,
 THPS1_GRIND_BAIL=4, THPS1_GRIND_OLLIE=8, THPS1_GRIND_LINK=16 };
typedef struct Thps1GrindRail {
 int32_t id, chain_id, previous_id, next_id;
 int32_t a[3], b[3];
} Thps1GrindRail;
typedef int (*Thps1GrindRailLookup)(void *,int32_t id,Thps1GrindRail *);
typedef struct Thps1GrindInput {
 int8_t x, y; /* source analog: left< -40, right>40, up< -40, down>40 */
 uint8_t left, right, up, down, grind, ollie;
} Thps1GrindInput;
typedef struct Thps1GrindRng { uint32_t a,b,c; } Thps1GrindRng;
typedef struct Thps1GrindState {
 int32_t position[3], velocity[3], acceleration[3];
 int32_t travel_direction[3], facing[3]; /* Q12 unit vectors */
 int32_t balance, balance_velocity, balance_ticks;
 int32_t rail_id, chain_id, last_chain_id, cooldown;
 int32_t turn_q12, dismount_turn_q12, tilt_q12;
 int32_t charge_ticks, speed_stat, balance_stat, ollie_stat, air_stat;
 int32_t profile_id; /* original profile+0xc: Tony=0 */
 uint32_t events;
 uint8_t active, balance_enabled, stance, trick, previous_ollie;
 Thps1GrindRng rng;
 ThpsAnim animation;
} Thps1GrindState;
typedef struct Thps1GrindEntry {
 Thps1GrindRail rail;
 int32_t closest[3], actor_position[3], velocity[3], right[3];
 int32_t distance; /* source integer distance returned by rail collision query */
 int32_t source_state; /* original0/2=ground variants;1=air;4=grind */
 uint8_t bailed, input_locked, candidate_valid, ready, switch_stance;
} Thps1GrindEntry;
/* Build source884 columns(side,up,facing), exact original cross/normalize.
 * Then apply original yaw(turn_q12) and tilt (roll for stance0, pitch otherwise)
 * with the shared source matrix multiplier, without pivot compensation.
 * `roll_from_pitch` remaps a verified original22598 single-axis pitch matrix.
 * It does not synthesize trigonometry or use host angles. */
int thps1_grind_basis_q12(int16_t basis[9],const int32_t facing[3]);
void thps1_grind_roll_from_pitch_q12(int16_t roll[9],const int16_t pitch[9]);
/* Exact source58594 velocity turn; call on PRE-OLLIE-IMPULSE velocity.
 * previous is source864, prior physics basis; yaw is original22598 delta.
 * Caller resolves the source collision-normal equality gate before calling.
 * To use post-impulse OLLIE output: add back the known impulse to Y, call this,
 * then subtract the same impulse. Also yaw-rotate the rebuilt physical basis. */
void thps1_grind_turn_velocity_q12(int32_t velocity[3],const int16_t previous[9],const int16_t yaw[9]);
/* reset selects exact original RNG algorithm; seed is an explicit host choice,
 * not claimed equal to the original game's shared RNG state. Stats are totals,
 * so callers include original equipment/special contributions when known. */
void thps1_grind_reset(Thps1GrindState *,uint32_t seed);
void thps1_grind_ground_reset(Thps1GrindState *);
void thps1_grind_rng_seed(Thps1GrindRng *,uint32_t seed);
int32_t thps1_grind_random(Thps1GrindRng *,int32_t bound);
int thps1_grind_normalize(int32_t out[3],const int32_t in[3]);
int32_t thps1_grind_dot(const int32_t a[3],const int32_t b[3]);
int32_t thps1_grind_multiply(int32_t,int32_t);
int32_t thps1_grind_low_speed(int32_t speed,int32_t rail_y);
int32_t thps1_grind_balance_position(int32_t pos,int32_t drift,int32_t ticks,int32_t stat);
/* Deterministic scalar/source operations, also used by the independent oracle. */
void thps1_grind_balance_step(Thps1GrindState *,const Thps1GrindInput *);
int thps1_grind_select_trick(int32_t rail_dot_right,int switch_stance,
 const Thps1GrindInput *,int32_t *turn);
void thps1_grind_animation_start(Thps1GrindState *,const ThpsAnimBank *);
void thps1_grind_animation_continue(Thps1GrindState *,const ThpsAnimBank *,int32_t source_frame);
/* 1 accepted,0 ineligible,-1 invalid host input. Reject is atomic except the
 * source's held-input cooldown counter. Full lip/special-grind paths excluded. */
/* Invoke after geometric/general entry eligibility but before commit. Exact
 * old-trick landing window and original automatic-rotation818/81c/820 gate.
 * landing_frames is source trick record+0x18 (only read with flags&0x6000).
 * If true, shared bail0 replaces entry; preserve the original queue state. */
int thps1_grind_entry_bail(int32_t active,int32_t interruptible,int32_t frame,
 int32_t count,uint32_t flags,int32_t landing_frames,int32_t rotation_active,
 int32_t rotation_angle,int32_t rotation_direction);
int thps1_grind_enter(Thps1GrindState *,const Thps1GrindEntry *,
 const Thps1GrindInput *,const ThpsAnimBank *);
/* One original nominal30Hz tick,dtQ8=256. C-up release does not dismount;
 * C-down charge/release requests an ollie. Source yaw turn on dismount is
 * returned in dismount_turn_q12 (-200/+200,4096=turn); caller MUST apply it
 * to the returned rail-projected velocity before ordinary air integration.
 * lookup may be NULL for finite independent rails. Caller advances the shared
 * original animation once, centrally; this routine handles finished-clip gates.
 * 1 success,0 invalid/unsafe imported state (destination unchanged). */
/* Exact owner ordering: source Jump5a438 precedes Tricks5a4c4, whereas rail
 * entry5ad4c and state4 physics5ae90 occur later. Integrated wrapper calls
 * handle_jump on an already-active grind BEFORE HandleTricks. Returns1 on
 * release,0 remaining on rail,-1 invalid (unchanged). It does not increment
 * balance age, advance animation, or move position. On release, change shared
 * state to air1 and apply basis/velocity yaw before HandleTricks this tick.
 * Then call motion_pre_final only if still grinding. That function never
 * charges/releases jump and increments balance age once before rail physics.
 * Handle BAIL through common recovery, then common final velocity integration.
 * step_pre_final and step remain isolated convenience compositions; do not
 * combine handle_jump with them or jump charge would advance twice. */
int thps1_grind_handle_jump(Thps1GrindState *,const Thps1GrindInput *,const ThpsAnimBank *);
int thps1_grind_motion_pre_final(Thps1GrindState *,const Thps1GrindRail *,
 const Thps1GrindInput *,const ThpsAnimBank *,int32_t source_frame,
 Thps1GrindRailLookup,void *);
int thps1_grind_step_pre_final(Thps1GrindState *,const Thps1GrindRail *,
 const Thps1GrindInput *,const ThpsAnimBank *,int32_t source_frame,
 Thps1GrindRailLookup,void *);
void thps1_grind_finish_velocity(Thps1GrindState *);
int thps1_grind_step(Thps1GrindState *,const Thps1GrindRail *,
 const Thps1GrindInput *,const ThpsAnimBank *,int32_t source_frame,
 Thps1GrindRailLookup,void *);
#ifdef __cplusplus
}
#endif
#endif
