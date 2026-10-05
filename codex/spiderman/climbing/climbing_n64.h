#ifndef SMN64_CLIMBING_H
#define SMN64_CLIMBING_H
#include "../movement/n64_behavior.h"
#include <stddef.h>
#include <stdint.h>
/* All vectors use original fixed12 units and Y-down. Matrix storage is row-major
 * s16, three columns: right, inward (negative body normal), forward. */
typedef struct SmN64ClimbBasis {
    int16_t normal[3], matrix[9], inverse[9];
    int32_t forward[3], right[3], outward[3], inward[3];
    int32_t previous_normal[3]; /* original global800F7AC8, retained by owner */
    int32_t yaw_delta;
} SmN64ClimbBasis;
typedef struct SmN64ClimbHit {
    int32_t present, position[3];
    int16_t normal[3];
    uint16_t surface_flags; /* original descriptor+0xC; mask0x4 rejects adhesion */
    uint8_t has_surface;
    int32_t distance;
    uint16_t actor_flags; /* original hit object flags; bit100 moving platform */
} SmN64ClimbHit;
typedef struct SmN64ClimbQuery {
    int32_t start[3], end[3];
    int32_t arg1, arg2, arg3, arg4; /* 8004C0B0 arguments after line pointer */
    uint8_t line_byte_88;
} SmN64ClimbQuery;
/* Return 1 for completed query (hit.present may be zero); negative unavailable.
 * No missing-query fallback is permitted. Query is read-only. */
typedef int (*SmN64ClimbTrace)(void *, const SmN64ClimbQuery *, SmN64ClimbHit *);
typedef struct SmN64ClimbState {
    SmN64ClimbBasis basis;
    SmN64Anim anim;
    int32_t position[3], velocity[3];
    uint32_t state;
    int32_t adhered, wall_class, ceiling_class;
    int32_t turn_ticks, base_timer, hold_timer, jump_variant, field1184;
    int32_t d20, d24, field65c;
    int32_t retained_forward[3]; /* CE4 */
    int32_t aiming, held_object;
    int16_t input_angle;
    int8_t analog_x, analog_y;
    uint16_t collision, body_offset;
    uint8_t approach_ticks, jump_pressed, jump_held;
    int32_t camera_reset, sound_id; /* explicit events; caller consumes/clears */
    int32_t cf8, stop_marks_cf8; /* stop_marks: source script sequence33, step!=14 */
    int32_t cf4, d48, d4c, f48, f4c;
    int32_t d00, d04, d08, d0c, d10;
    int32_t contact_position[3];
    uint8_t ground_grace;
    int32_t lighting_target;
    SmN64ClimbHit side;
    int32_t turn_target, turn_step, camera_restore;
    int16_t yaw;
    uint16_t idle_ticks;
    uint8_t aim_held, air_turn_factor;
    int32_t falling_origin_y, launch_velocity;
    int32_t input_ramp, run_ramp, control_inhibit;
    uint16_t input_base, dampen_left, dampen_total;
    int8_t previous_x, previous_y;
    uint8_t center_pressed, field331;
    int32_t acceleration[3];
    uint8_t drag[3];
    uint32_t random_state[3];
} SmN64ClimbState;
/* Basis-only 8009D258 arithmetic. Return 1 regular, 0 source degenerate
 * fallback basis; 0 requires source idle/stop transition at caller. No yaw-only
 * rendering conversion is equivalent. explicit_forward!=NULL selects a1=1. */
int smn64_climb_basis(SmN64ClimbBasis *, const int32_t explicit_forward[3]);
/* Current-normal classification only, from8008D2D8..8008D32C.
 * Caller runs basis first: explicit retained_forward for ceilings, NULL otherwise. */
void smn64_climb_classify(SmN64ClimbState *);
/* 800AA280 segment and result semantics, including source -1 sentinel. */
int smn64_climb_floor_query(const int32_t position[3], int32_t above,
                           int32_t below, int32_t include_objects,
                           SmN64ClimbTrace, void *, int32_t *height);
/* Returns 1 action performed, 0 source gates reject, -1 invalid count/input,
 * -2 unsupported degenerate-basis idle dependency, -3 unavailable world query.
 * Negative returns leave state unchanged. */
int smn64_climb_approach(SmN64ClimbState *, const SmN64ClimbHit *,
                       const uint16_t *counts, size_t count);
int smn64_climb_attach_wall(SmN64ClimbState *, const SmN64ClimbHit *,
                          SmN64ClimbTrace, void *, const uint16_t *, size_t);
int smn64_climb_attach_ceiling(SmN64ClimbState *, const SmN64ClimbHit *,
                             const uint16_t *, size_t);
int smn64_climb_jump_detach(SmN64ClimbState *, const uint16_t *, size_t);
/* Source 8009F664 heading, including arbitrary-wall frame. */
uint16_t smn64_climb_heading(const SmN64ClimbState *);
/* Source post-AI target block92718..92B74; caller supplies D1C and run ramp.
 * Shared unheld regular-camera path: supports mixed CF0/A24, including web
 * states100/200/400 after adhesion clears but wall_class is retained. The
 * original cinematic camera-mode17 actor-distance branch is outside this API.
 * Invalid ramp or held object returns-1, unchanged. */
int smn64_climb_target_velocity(SmN64ClimbState *, int32_t run_ramp, int movement_enabled, uint16_t camera_yaw);
/* Full static-surface adhered contact owner8540C..86150. The marker callback
 * evaluates original authored marker2 at the already-integrated state position.
 * It is NOT a constant body offset. Pure queries may be retried transactionally.
 * -4 requests source moving-platform handling; no mutation is committed. */
typedef int (*SmN64ClimbMarker)(void *, const SmN64ClimbState *, unsigned marker, int32_t out[3]);
int smn64_climb_physics(SmN64ClimbState *, const int32_t acceleration[3], const uint8_t drag[3],
                       SmN64ClimbMarker, SmN64ClimbTrace, void *,
                       uint16_t bright_target, uint16_t normal_target, int32_t source_level);
/* Source98140 direction bookkeeping when crossing wall/ceiling class. */
void smn64_climb_transition_input(SmN64ClimbState *, const int16_t normal[3], int commit);
int smn64_climb_turn(SmN64ClimbState *, uint16_t target, int fast);
void smn64_climb_turn_advance(SmN64ClimbState *);
/* Source989D0 input-start/turn gate for an adhered ordinary actor. */
int smn64_climb_start_move(SmN64ClimbState *, uint16_t camera_yaw, int may_run,
                          const uint16_t *counts, size_t count);
/* Original ordinary unheld stop mapping97DE4; transactional. */
int smn64_climb_stop(SmN64ClimbState *, const uint16_t *, size_t);
/* Explicit lifetime-resolved held object+10C, preserving source small/large
 * speed24/10 and analog magnitude. Ordinary APIs still reject unknown holds. */
int smn64_climb_target_velocity_carrying(SmN64ClimbState *,int32_t,int,uint16_t,uint32_t object_flags);
int smn64_climb_stop_carrying(SmN64ClimbState *,const uint16_t *,size_t,uint32_t object_flags);
#endif
