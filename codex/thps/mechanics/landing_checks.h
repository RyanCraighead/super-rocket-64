#ifndef THPS1_LANDING_CHECKS_H
#define THPS1_LANDING_CHECKS_H
#include <stdint.h>
#include "air_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Ordinary already-confirmed rideable ground contact only. Source coordinates:
 * Y down; vectors/velocity Q12. This does NOT classify a host collision.
 * Basis vectors are original +8d0 (forward), +8dc (side), +8e8 (up).
 * Input vectors must fit the host's existing +/-1,000,000 Q12 safety bound;
 * unit basis/normal components must fit +/-4096. */
typedef struct Thps1LandingInput {
    int32_t velocity[3], normal[3], forward[3], side[3], up[3];
    int32_t already_bailed, source_state, stance_flags;
    int32_t trick_active, trick_interruptible;
    uint32_t trick_flags;
    int32_t flip_held, grab_held;
    int32_t rotation_active, rotation_angle, rotation_direction;
} Thps1LandingInput;
/* Diagnostic category only. Every ordinary landing bail calls source begin
 * with raw reason0; do not pass this enum as Thps1Bail.reason. */
enum Thps1LandingReason {
    THPS1_LANDING_CLEAN = 0,
    THPS1_LANDING_ALIGNMENT = 1,
    THPS1_LANDING_UPSIDE_DOWN = 2,
    THPS1_LANDING_UNFINISHED_TRICK = 3,
    THPS1_LANDING_ALREADY_BAILED = 4
};
typedef struct Thps1LandingResult {
    int32_t tangent_velocity[3], tangent_speed, alignment_q12, side_dot;
    int32_t reason;
    /* Overrides after ordinary bail begin: 5/clip46 for reverse-side
     * alignment fall, 9/clip56 for upside-down; otherwise0. */
    int32_t bail_phase_override, bail_clip_override;
    int32_t override_velocity[3];
} Thps1LandingResult;
/* Scalar helpers retain original FPU float32 roundings (not integer Q12 math). */
int32_t thps1_landing_dot(const int32_t a[3], const int32_t b[3]);
int32_t thps1_landing_mul(int32_t a, int32_t b);
int32_t thps1_landing_speed(const int32_t velocity[3]);
void thps1_landing_project(const int32_t velocity[3], const int32_t normal[3], int32_t out[3]);
int thps1_landing_trick_bails(int active, int interruptible, uint32_t flags,
    int flip_held, int grab_held, int rotation_active, int32_t angle, int32_t direction);
void thps1_landing_check(const Thps1LandingInput *, Thps1LandingResult *);
/* Original alignment-bail basis repair8004ed84..8004eea8. Positions are
 * object+16c and+2a8 (historical source actor positions), not this tick's
 * collision foot location. The source itself returns zero basis for zero delta. */
void thps1_landing_alignment_basis(const int32_t previous_q12[3],
    const int32_t earlier_q12[3], const int32_t normal_q12[3],
    int32_t forward_q12[3], int32_t side_q12[3], int32_t up_q12[3]);
/* Upside response80054334..54390: new forward=-old up;new up=old forward. */
void thps1_landing_upside_basis(const int32_t old_forward[3],const int32_t old_up[3],
    int32_t new_forward[3],int32_t new_up[3]);
/* Ground handler's next-tick stance normalization8004bd2c. Returns1 when
 * it flips axes/stance. Pass original longitudinal (opposite forward travel),
 * not a host positive-travel vector. Original matrix entries are the signed16
 * low words of these returned axes; caller rebuilds its owned basis matrix. */
int thps1_ground_stance_reorient(uint16_t clip,int32_t longitudinal[3],
    int32_t side[3],const int32_t velocity[3],uint32_t *stance_flags);
/* Deferred ordinary landing cleanup in HandleJump, before HandleTricks.
 * Caller first rejects bail.active. True means clear airborne_latch838 and
 * ramp_latch85c, start landing clip, recheck trick-bail predicate, then perform
 * narrow successful cleanup. Never clear queues via air_tricks_clear here. */
int thps1_landing_cleanup_ready(int32_t airborne_latch,int32_t previous_state,int32_t current_state);
void thps1_landing_start_animation(ThpsAnim *,const ThpsAnimBank *,int crouched);
/* Only source finished5/25 dispatch; other jump/crouch/air clips caller-owned.
 * Returns1 if a transition occurred. Caller must skip this while bailed. */
int thps1_landing_finish_animation(ThpsAnim *,const ThpsAnimBank *,int crouched);
/* Before physics dispatch: earlier=old previous;previous=current body. */
void thps1_landing_history_shift(const int32_t current[3],int32_t previous[3],int32_t earlier[3]);
/* Ordinary clean-contact subset of598b8 after air tail sets old/new normals
 * equal and918=0. Preserve original longitudinal heading/sign; normalize normal,
 * side=normalize(cross(longitudinal,normal)),longitudinal=cross(normal,side).
 * This is not the general25-tick slope-normal smoothing controller. */
void thps1_landing_clean_basis(const int32_t old_longitudinal[3],const int32_t normal[3],
    int32_t new_longitudinal[3],int32_t new_side[3],int32_t new_up[3]);
/* Original ordinary contact actor origin = hit + 30*normal. Host foot origin
 * must remain separate: this function is NOT an instruction to lift host feet. */
void thps1_landing_source_origin(const int32_t hit_q12[3], const int32_t normal_q12[3], int32_t origin_q12[3]);
#ifdef __cplusplus
}
#endif
#endif
