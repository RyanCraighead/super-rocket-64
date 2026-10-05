#ifndef THPS1_AIR_TRICKS_H
#define THPS1_AIR_TRICKS_H
#include "air_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source HandleTricks0x8004f13c. Ordinary and original special air tricks
 * for Tony profile type0. Scoring is emitted for the separate scoring owner.
 * Tick/direction queues retain original signed32 modular arithmetic. */
typedef struct Thps1TrickRecord {
 int32_t clip, rate, hold_frame, interrupt_frames, landing_frames, base_points;
 const char *name;
} Thps1TrickRecord;
typedef struct Thps1AirTricks {
 int32_t delay, rotation_lock, blocked, airborne, source_state;
 int32_t active, interruptible, scored;
 uint32_t trick_flags;
 int32_t read_index,write_index,release_debounce;
 int32_t directions[10],buttons[10],direction_ticks[10],button_ticks[10];
 /* Original directional-history fields354,358,35c,360,364,368,36c,370,374,378.
  * A prior released direction is packed above the current direction nibble. */
 int32_t history[10];
 int32_t trick_count,started_tick,end_block;
 /* Per-call output; consume before next call. No combo/score claim is made. */
 int32_t started,ended,score_index,score_base,hold_score_base;
 /* Shared source state. Host synchronizes special_enabled from score+34c,
  * queued180/stance_flags from spin/orientation before HandleTricks, then
  * copies locks, queue and stance back after it. No meter threshold here. */
 int32_t special_enabled, input_lock, count_lock, hold_disabled;
 int32_t pending_stance_toggle, pending_basis_flip;
 int32_t rotation_active, rotation_direction, queued180;
 uint32_t stance_flags;
 /* Per-call original4d2d4 effects, XOR-composed: value1 toggles stance (already
  * reflected in stance_flags), value2 negates side and longitudinal columns.
  * Apply value2 to the owner's basis once after this call. */
 uint32_t orientation_effects;
} Thps1AirTricks;
typedef struct Thps1TrickInput {
 int32_t tick;
 int8_t stick_x,stick_y; /* Original normalized axes: negativeX left,negativeY up. */
 uint8_t up,down,left,right,grab,flip;
} Thps1TrickInput;
const Thps1TrickRecord *thps1_trick_record(unsigned index);
/* Valid states require queue indices0..9 and Tony trick index0..35.
 * Call after the owner's anim_advance, once per original30Hz tick. Direction is board-
 * relative input, never camera-relative movement. No implicit default direction. */
void thps1_air_tricks(Thps1AirTricks *,ThpsAnim *,const ThpsAnimBank *,const Thps1TrickInput *);
/* Original5c444, called by begin-sequence4c9e8. Narrow queue reset preserves
 * the current direction if its start is fewer than9 signed modular ticks ago;
 * other ring slots, timestamps, debounce and pending orientation are untouched. */
void thps1_air_tricks_begin_sequence(Thps1AirTricks *,int32_t tick);
/* Original orientation helper4d2d4 before an owner-triggered Anim::Run.
 * Returns this invocation's effects and accumulates them in orientation_effects.
 * Does not change spin locks or queued180. Apply returned effects exactly once;
 * do not apply accumulated effects again for a separately owned transition. */
uint32_t thps1_air_tricks_finish_orientation(Thps1AirTricks *);
/* Apply effect2 to original884 Q12 row-major matrix. Effect1 is represented by
 * stance_flags already; pending basis flip keeps the up column unchanged. */
void thps1_air_tricks_apply_orientation(int16_t basis[9],uint32_t effects);
/* Explicit host ownership-cancellation boundary, not original gameplay code. */
void thps1_air_tricks_clear(Thps1AirTricks *);
#ifdef __cplusplus
}
#endif
#endif
