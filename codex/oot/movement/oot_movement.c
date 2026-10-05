#include "oot_movement.h"

#include <math.h>
#include <stdlib.h>

/* Source IDs refer to the pinned line links in sources.json, not an evolving branch. */
static const int16_t kQuarterSine[1024] = {
#include "oot_sine_table.inc"
};

/* Explicit wrap avoids implementation-defined unsigned-to-signed conversion. */
static int16_t wrap_angle(int32_t angle) {
    uint32_t u = (uint32_t) angle & 0xffffu;
    return (int16_t) (u >= 0x8000u ? (int32_t) u - 65536 : (int32_t) u);
}

static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static bool step_float(float* value, float target, float increase, float decrease) {
    float step = target >= *value ? increase : -decrease;
    if (step == 0.0f) return *value == target;
    *value += step;
    if ((*value - target) * step >= 0.0f) {
        *value = target;
        return true;
    }
    return false;
}

/* [boot-data] Only ordinary outdoor Kokiri boots; no implicit age/gear heuristics. */
OotMoveConfig oot_move_config_kokiri(bool child) {
    OotMoveConfig config = {
        child ? 5.5f : 6.0f, 2.0f, 1.5f, 8.0f, -1.0f, -20.0f,
        child ? 5.4f : 5.9f, 7.5f, 1.25f, child ? 400.0f : 200.0f,
        2000, 1200
    };
    return config;
}

void oot_move_init(OotMoveState* state, int16_t yaw) {
    state->speed_xz = state->velocity_y = 0.0f;
    state->move_yaw = state->shape_yaw = yaw;
    state->action = OOT_MOVE_IDLE;
}

/* [raw-stick] Native N64 input, not already-deadzoned SM64 controller axes. */
int8_t oot_move_adjust_stick_axis(int8_t raw_axis) {
    if (raw_axis > 7) return (int8_t) ((raw_axis < 67 ? raw_axis : 67) - 7);
    if (raw_axis < -7) return (int8_t) ((raw_axis > -67 ? raw_axis : -67) + 7);
    return 0;
}

float oot_move_stick_magnitude(int8_t raw_x, int8_t raw_y) {
    float x = oot_move_adjust_stick_axis(raw_x);
    float y = oot_move_adjust_stick_axis(raw_y);
    return fminf(sqrtf(x * x + y * y), 60.0f);
}

/* [binang-sine] Quarter-table endpoints and 4-bit truncation are significant. */
float oot_move_sin(int16_t angle) {
    unsigned index = ((uint16_t) angle) >> 4;
    unsigned quarter_index = index & 1023u;
    if (index & 1024u) quarter_index = 1023u - quarter_index;
    int value = kQuarterSine[quarter_index];
    if (index & 2048u) value = -value;
    return (float) value * (1.0f / 32767.0f);
}

float oot_move_cos(int16_t angle) {
    return oot_move_sin(wrap_angle((int32_t) angle + 0x4000));
}

/* [angle-step] Mirrors source's signed subtraction and 1.5 update scaling,
 * including the source direction at the exact 180-degree tie. */
bool oot_move_step_angle(int16_t* angle, int16_t target, int16_t step) {
    int signed_step = step;
    if (signed_step == 0) return *angle == target;
    if (wrap_angle((int32_t) *angle - target) > 0) signed_step = -signed_step;
    *angle = wrap_angle((int32_t) *angle + (int) (signed_step * OOT_MOTION_UPDATE_SCALE));
    if ((int32_t) wrap_angle((int32_t) *angle - target) * signed_step >= 0) {
        *angle = target;
        return true;
    }
    return false;
}

/* [stick-target] Input adapter owns Lib_GetControlStickData and camera yaw.
 * No bog/sand depth modifier is included in this ordinary-floor slice. */
OotMoveTarget oot_move_target(const OotMoveState* state, const OotMoveInput* input, bool curved) {
    OotMoveTarget target = {0.0f, state->shape_yaw, false};
    if (input->movement_blocked || !isfinite(input->stick_magnitude) ||
        !isfinite(input->speed_cap)) return target;
    float magnitude = clampf(input->stick_magnitude, 0.0f, 60.0f);
    if (magnitude == 0.0f) return target;
    float value = magnitude;
    if (curved) {
        value -= 20.0f;
        if (value < 0.0f) value = 0.0f;
        else {
            float bend = 1.0f - oot_move_cos((int16_t) (value * 450.0f));
            value = bend * bend * 30.0f + 7.0f;
        }
    } else value *= 0.8f;
    float incline = clampf(oot_move_sin(input->floor_pitch), 0.0f, 0.6f);
    target.speed = clampf(value * 0.14f - 8.0f * incline * incline,
                          0.0f, fmaxf(0.0f, input->speed_cap));
    target.yaw = input->world_yaw;
    target.stick_active = true;
    return target;
}

/* [idle-action], [run-action], [reverse-brake], [ground-steer], [turn-action].
 * Handler lists, upper-body actions, animation/root motion and Z-targeting are
 * intentionally outside this bounded arithmetic/state implementation. */
void oot_move_ground_tick(OotMoveState* state, const OotMoveConfig* config, const OotMoveInput* input) {
    OotMoveTarget target = oot_move_target(state, input, true);
    bool follow_shape = state->action == OOT_MOVE_RUN;
    switch (state->action) {
        case OOT_MOVE_IDLE:
            step_float(&state->speed_xz, 0.0f, config->stop_deceleration, config->stop_deceleration);
            if (target.speed != 0.0f) {
                state->move_yaw = state->shape_yaw = target.yaw;
                state->action = OOT_MOVE_RUN;
            } else if (abs(wrap_angle((int32_t) target.yaw - state->shape_yaw)) > 800) {
                state->move_yaw = target.yaw;
                state->action = OOT_MOVE_TURN_IN_PLACE;
            } else {
                oot_move_step_angle(&state->shape_yaw, target.yaw, config->turn_in_place_step);
                state->move_yaw = state->shape_yaw;
            }
            break;
        case OOT_MOVE_TURN_IN_PLACE:
            /* Preserve original ESS behavior: turn action does not decelerate. */
            if (target.speed != 0.0f) {
                state->shape_yaw = target.yaw;
                state->action = OOT_MOVE_RUN;
            } else if (oot_move_step_angle(&state->shape_yaw, target.yaw, config->turn_in_place_step)) {
                state->action = OOT_MOVE_IDLE;
            }
            state->move_yaw = state->shape_yaw;
            break;
        case OOT_MOVE_RUN:
            if (abs(wrap_angle((int32_t) state->move_yaw - target.yaw)) > 0x6000) {
                if (!step_float(&state->speed_xz, 0.0f, config->stop_deceleration,
                                config->stop_deceleration)) break;
                target.speed = 0.0f;
                target.yaw = state->move_yaw;
            }
            step_float(&state->speed_xz, target.speed, config->acceleration, config->deceleration);
            oot_move_step_angle(&state->move_yaw, target.yaw, config->turn_step);
            if (state->speed_xz == 0.0f && target.speed == 0.0f) {
                state->action = OOT_MOVE_IDLE;
                state->move_yaw = state->shape_yaw; /* walk-end setup */
            }
            break;
    }
    /* [shape-yaw] Run sets STATE2_5 even when it subsequently changes action. */
    if (follow_shape) oot_move_step_angle(&state->shape_yaw, state->move_yaw, 2000);
}

/* [air-steer] Only the non-attacking, non-special airborne steering branch. */
void oot_move_air_tick(OotMoveState* state, const OotMoveConfig* config, const OotMoveInput* input,
                       bool melee_active) {
    OotMoveTarget target = oot_move_target(state, input, false);
    if (!melee_active) state->speed_xz = clampf(state->speed_xz, -config->speed_cap, config->speed_cap);
    if (abs(wrap_angle((int32_t) state->move_yaw - target.yaw)) > 0x6000) {
        if (step_float(&state->speed_xz, 0.0f, 1.0f, 1.0f)) state->move_yaw = target.yaw;
    } else {
        step_float(&state->speed_xz, target.speed, 0.05f, 0.1f);
        oot_move_step_angle(&state->move_yaw, target.yaw, 200);
    }
}

/* [actor-physics] Do not apply source gravity a second time in the host engine. */
OotMoveDelta oot_move_actor_delta(OotMoveState* state, const OotMoveConfig* config) {
    state->velocity_y = fmaxf(state->velocity_y + config->gravity, config->terminal_velocity);
    OotMoveDelta delta = {
        (state->speed_xz * oot_move_sin(state->move_yaw)) * OOT_MOTION_UPDATE_SCALE,
        state->velocity_y * OOT_MOTION_UPDATE_SCALE,
        (state->speed_xz * oot_move_cos(state->move_yaw)) * OOT_MOTION_UPDATE_SCALE
    };
    return delta;
}

/* [autojump-gate] Regular dry-ground branch only; excludes diving/ledge grabs. */
bool oot_move_can_autojump(const OotMoveState* state, const OotAutojumpContext* context) {
    return context->ground_leave && !context->swimming && !context->melee_active &&
           !context->cutscene_or_special_action && !context->previous_floor_forbids_jump &&
           context->distance_to_floor > 20.0f && state->speed_xz > 3.0f &&
           abs(wrap_angle((int32_t) state->move_yaw - state->shape_yaw)) < 0x2000;
}

/* [autojump-impulse] There is intentionally no new jump-button mechanic. */
float oot_move_autojump_impulse(const OotMoveState* state, const OotMoveConfig* config) {
    return state->speed_xz > config->autojump_threshold ? config->autojump_fast_y :
        config->autojump_base_y + (config->autojump_speed_reg * state->speed_xz) / 1000.0f;
}

bool oot_move_clock30_tick(OotMoveClock30* clock) {
    clock->phase += 2;
    if (clock->phase < 3) return false;
    clock->phase -= 3;
    return true;
}

float oot_move_host30_velocity(float source_velocity, float world_units_per_oot_unit) {
    return source_velocity * world_units_per_oot_unit;
}

void oot_move_gait_reset(OotMoveGaitState* gait) {
    gait->phase = gait->startup = 0.0f;
}

/* [animation-blend], [animation-phase], [boot-data]. Flat-ground projection of
 * func_80841EE4: caller blends extracted skeletal poses, not world positions.
 * Slope/climbing-pose overlay and six-frame action morph are separate concerns. */
OotMoveGaitSample oot_move_gait_tick(OotMoveGaitState* gait, float speed_xz, bool child) {
    float base_rate = child ? 0.5f : 0.55f; /* REG(35) / 1000 */
    float walk_rate = child ? 0.4f : 0.27f; /* REG(36) / 1000 */
    float blend_rate = child ? 0.8f : 0.6f; /* REG(37) / 1000 */
    float run_rate = child ? 0.4f : 0.35f;  /* REG(38) / 1000 */
    float run_weight = 0.0f;
    float rate;
    if (gait->startup < 1.0f) {
        rate = base_rate;
        gait->startup = fminf(gait->startup + OOT_MOTION_UPDATE_SCALE, 1.0f);
    } else {
        float excess = speed_xz - 3.7f; /* REG(48) / 100 */
        if (excess < 0.0f) rate = base_rate + walk_rate * speed_xz;
        else {
            run_weight = blend_rate * excess;
            if (run_weight < 1.0f) rate = base_rate + walk_rate * speed_xz;
            else {
                run_weight = 1.0f;
                rate = 1.2f + run_rate * excess;
            }
        }
    }
    gait->phase += clampf(rate * OOT_MOTION_UPDATE_SCALE, -7.25f, 7.25f);
    if (gait->phase < 0.0f) gait->phase += 29.0f;
    else if (gait->phase >= 29.0f) gait->phase -= 29.0f;
    OotMoveGaitSample sample = {gait->phase, gait->phase * (20.0f / 29.0f), run_weight};
    return sample;
}
