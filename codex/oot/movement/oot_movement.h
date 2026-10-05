#ifndef CODEX_OOT_MOVEMENT_H
#define CODEX_OOT_MOVEMENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded NTSC 1.0 free-locomotion translation. See README.md for exclusions.
 * All action/physics functions run once per OoT 20 Hz tick, not per SM64 tick.
 * Velocity is in the source actor's units: position delta = velocity * 1.5.
 * Angles are binary turns: +Z at 0, +X at 0x4000, signed modulo 65536.
 */
#define OOT_MOTION_TICK_HZ 20
#define OOT_MOTION_UPDATE_SCALE 1.5f

typedef enum OotMoveAction {
    OOT_MOVE_IDLE,
    OOT_MOVE_RUN,
    OOT_MOVE_TURN_IN_PLACE
} OotMoveAction;

typedef struct OotMoveConfig {
    float speed_cap;
    float acceleration;
    float deceleration;
    float stop_deceleration;
    float gravity;
    float terminal_velocity;
    float autojump_threshold;
    float autojump_fast_y;
    float autojump_base_y;
    float autojump_speed_reg; /* IREG(69), divided by 1000 after multiplication */
    int16_t turn_step;
    int16_t turn_in_place_step;
} OotMoveConfig;

typedef struct OotMoveState {
    float speed_xz;
    float velocity_y;
    int16_t move_yaw;
    int16_t shape_yaw;
    OotMoveAction action;
} OotMoveState;

typedef struct OotMoveInput {
    /* Preprocessed OoT rel-stick magnitude, before curved response, [0,60].
     * Camera orientation is caller-owned; helpers below perform raw dead-zone.
     * SM64 intendedMag has a different squared response; do not feed it here. */
    float stick_magnitude;
    int16_t world_yaw;
    int16_t floor_pitch;
    /* Source unk_880 after wall-limit calculation; normally config.speed_cap. */
    float speed_cap;
    bool movement_blocked;
} OotMoveInput;

typedef struct OotMoveTarget {
    float speed;
    int16_t yaw;
    bool stick_active;
} OotMoveTarget;

typedef struct OotMoveDelta { float x, y, z; } OotMoveDelta;

typedef struct OotAutojumpContext {
    bool ground_leave;
    bool swimming;
    bool melee_active;
    bool cutscene_or_special_action;
    bool previous_floor_forbids_jump; /* OoT floor property 6 or 9 */
    float distance_to_floor;
} OotAutojumpContext;

typedef struct OotMoveClock30 { uint8_t phase; } OotMoveClock30;

typedef struct OotMoveGaitState {
    float phase;   /* source unk_868, walk-cycle phase in [0,29) */
    float startup; /* source unk_864, reset with run setup */
} OotMoveGaitState;

typedef struct OotMoveGaitSample {
    float walk_frame;
    float run_frame;
    float run_weight; /* lerp(walk, run, run_weight); ordinary flat floor */
} OotMoveGaitSample;

OotMoveConfig oot_move_config_kokiri(bool child);
void oot_move_init(OotMoveState* state, int16_t yaw);
int8_t oot_move_adjust_stick_axis(int8_t raw_axis);
float oot_move_stick_magnitude(int8_t raw_x, int8_t raw_y);
float oot_move_sin(int16_t angle);
float oot_move_cos(int16_t angle);
bool oot_move_step_angle(int16_t* angle, int16_t target, int16_t step);
OotMoveTarget oot_move_target(const OotMoveState* state, const OotMoveInput* input, bool curved);
/* Ground action + normal shape-yaw update, with no action-handler interruption. */
void oot_move_ground_tick(OotMoveState* state, const OotMoveConfig* config, const OotMoveInput* input);
/* Air-steering arithmetic only. Caller selects airborne/landing/attack states. */
void oot_move_air_tick(OotMoveState* state, const OotMoveConfig* config, const OotMoveInput* input,
                       bool melee_active);
/* Call before action tick. Includes gravity before displacement as in OoT.
 * Caller supplies collision/grounding and consumes/discards the y delta.
 * No position or collision geometry is stored by this core. */
OotMoveDelta oot_move_actor_delta(OotMoveState* state, const OotMoveConfig* config);
bool oot_move_can_autojump(const OotMoveState* state, const OotAutojumpContext* context);
float oot_move_autojump_impulse(const OotMoveState* state, const OotMoveConfig* config);
/* Rational scheduler: phase starts at zero, 30 host calls produce 20 ticks.
 * Call on physics frames only. It deliberately does not rescale action constants. */
bool oot_move_clock30_tick(OotMoveClock30* clock);
/* Linear-only conversion to a host per-30Hz velocity; NOT a state resampler. */
float oot_move_host30_velocity(float source_velocity, float world_units_per_oot_unit);
void oot_move_gait_reset(OotMoveGaitState* gait);
/* Call once at the START of a Run action, before ground_tick acceleration.
 * Run setup resets gait, but does not advance it until the next action tick.
 * Default sword+shield tracks: normal_walk and fighter_run. */
OotMoveGaitSample oot_move_gait_tick(OotMoveGaitState* gait, float speed_xz, bool child);

#ifdef __cplusplus
}
#endif
#endif
