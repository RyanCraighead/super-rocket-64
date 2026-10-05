#include "bm64_movement.h"
#include <math.h>

static int trim_axis(int axis) {
    /* 802395A8/80239714; default 3 written at 8023A350. */
    if (axis > 3) return axis - 3;
    if (axis < -3) return axis + 3;
    return 0;
}

static float wrap_once(float angle) {
    if (angle >= 360.0f) return angle - 360.0f;
    if (angle < 0.0f) return angle + 360.0f;
    return angle;
}

/* Original continued fraction at 8029CB40..8029CCFC. Keep single-precision
 * rounding between multiplies; compile without fast-math/FMA contraction. */
static float original_atan(float value) {
    int reciprocal = 0;
    float part = 0.0f;
    float numerator;
    int k;
    if (value > 1.0f) { value = 1.0f / value; reciprocal = 1; }
    else if (value < -1.0f) { value = 1.0f / value; reciprocal = -1; }
    for (k = 6; k >= 1; --k) {
        numerator = (float)(k * k) * value;
        numerator = numerator * value;
        part = numerator / ((float)(2 * k + 1) + part);
    }
    part = value / (1.0f + part);
    if (reciprocal > 0) return (float)(1.5707963267948966 - (double)part);
    if (reciprocal < 0) return (float)(-1.5707963267948966 - (double)part);
    return part;
}

Bm64PolarInput bm64_controller_polar(int8_t stick_x, int8_t stick_y) {
    Bm64PolarInput out = { 0.0f, 0.0f };
    float x = (float)-trim_axis(stick_x);
    float z = (float)trim_axis(stick_y);
    float angle;
    /* 80239880 invokes 8022FF58 with {-trim(X),0,trim(Y)}, adds 180. */
    out.magnitude = sqrtf(x * x + z * z);
    if (out.magnitude <= 0.001f) return out;
    /* Original computes atan(x/z), then converts in double and adjusts Z<0.
     * Avoid a host divide-by-zero exception while preserving axis directions. */
    angle = (float)((double)original_atan(z == 0.0f ? copysignf(INFINITY, x)
                                                        : x / z) * 57.29577951308232);
    if (z < 0.0f) angle -= 180.0f;
    angle = wrap_once(angle);
    out.angle_degrees = wrap_once(angle + 180.0f);
    return out;
}

Bm64MotionPlan bm64_plan_normal_motion(float magnitude, float world_angle,
                                     uint32_t player_flags) {
    static const uint32_t directions[8] = {
        0x80u, 0x60u, 0x40u, 0x20u, 0x00u, 0xE0u, 0xC0u, 0xA0u
    };
    static const int8_t xs[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int8_t zs[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    Bm64MotionPlan out = { 0.0f, 0.0f, 0.0f, 0.0f, BM64_DIRECTION_NONE };
    unsigned sector;
    float component;
    /* Fail-closed host boundary, not a new original-game mechanic. */
    if (!isfinite(magnitude) || !isfinite(world_angle) || magnitude < 0.0f ||
        world_angle < 0.0f || world_angle >= 360.0f ||
        (player_flags & BM64_PLAYER_INPUT_BLOCK_MASK)) return out;
    /* 8024E97C..8024EA24. There is no analog acceleration or inertia. */
    if (magnitude > 20.0f && magnitude < 35.0f) out.speed = 2.0f;
    else if (magnitude >= 35.0f && magnitude < 60.0f) out.speed = 4.0f;
    else if (magnitude >= 60.0f) out.speed = 8.5f;
    else return out;
    /* 8024EA3C..8024EC98. Explicit comparisons preserve half-open edges. */
    if (world_angle < 22.5f || world_angle >= 337.5f) sector = 0;
    else if (world_angle < 67.5f) sector = 1;
    else if (world_angle < 112.5f) sector = 2;
    else if (world_angle < 157.5f) sector = 3;
    else if (world_angle < 202.5f) sector = 4;
    else if (world_angle < 247.5f) sector = 5;
    else if (world_angle < 292.5f) sector = 6;
    else sector = 7;
    out.direction = directions[sector];
    out.facing_target = (float)(sector * 45u);
    /* 8029197C..A4 uses a DOUBLE 0.707, not float or sqrt(1/2). */
    component = (out.direction & 0x20u) ? (float)((double)out.speed * 0.707)
                                        : out.speed;
    /* 8024F4B4..DC permits turning while translation is locked. */
    if (!(player_flags & BM64_PLAYER_TRANSLATION_BLOCK_MASK)) {
        out.dx = (float)xs[sector] * component;
        out.dz = (float)zs[sector] * component;
    }
    return out;
}

void bm64_facing_init(Bm64Facing *state, float angle) {
    if (!state) return;
    if (!isfinite(angle)) angle = 0.0f;
    angle = fmodf(angle, 360.0f);
    if (angle < 0.0f) angle += 360.0f;
    state->angle = state->target_unwrapped = state->angle_unwrapped = angle;
    state->step = 0.0f;
    state->turning = 0;
}

void bm64_facing_tick(Bm64Facing *state, const Bm64MotionPlan *plan) {
    float target, lo, hi;
    if (!state || !plan) return;
    /* 8024EC9C..BC checks turning flag before calling 8024A6B4. */
    if (!state->turning && plan->direction != BM64_DIRECTION_NONE &&
        state->angle != plan->facing_target) {
        target = plan->facing_target;
        lo = fminf(state->angle, target);
        hi = fmaxf(state->angle, target);
        if ((lo + 360.0f) - hi < hi - lo) {
            target += lo == target ? 360.0f : -360.0f;
            if (target == -360.0f) target = 360.0f;
        }
        state->target_unwrapped = target;
        state->step = state->angle < target ? 20.0f : -20.0f;
        state->angle_unwrapped = state->angle;
        state->turning = 1;
    }
    /* 8024BD8C..BED0. Mid-turn input cannot retarget the current turn. */
    if (!state->turning) return;
    state->angle_unwrapped += state->step;
    state->angle = state->angle_unwrapped;
    if (state->angle_unwrapped > 360.0f) state->angle -= 360.0f;
    if (state->angle_unwrapped < 0.0f) state->angle += 360.0f;
    if ((state->step > 0.0f && state->angle_unwrapped >= state->target_unwrapped) ||
        (state->step < 0.0f && state->angle_unwrapped <= state->target_unwrapped)) {
        state->angle = wrap_once(state->target_unwrapped);
        state->turning = 0;
    }
}

unsigned bm64_vertical_tick(Bm64Vertical *state,
                            int has_floor, float floor_y,
                            int has_ceiling, float ceiling_y) {
    unsigned contacts = BM64_CONTACT_NONE;
    float projected;
    if (!state) return contacts;
    /* 80230948..84: floor prediction uses (y-v)+extra, in that order. */
    projected = (state->y - state->fall_velocity) + state->extra_y;
    if (has_floor && projected <= floor_y) {
        state->fall_velocity = 0.0f;
        state->extra_y = 0.0f;
        state->y = floor_y;
        contacts |= BM64_CONTACT_FLOOR;
    } else {
        /* 80230988..EC: actual integration instead uses (y+extra)-v. */
        state->y = (state->y + state->extra_y) - state->fall_velocity;
        if (state->fall_velocity <= BM64_PLAYER_TERMINAL_FALL)
            state->fall_velocity += BM64_PLAYER_GRAVITY;
        else state->fall_velocity = BM64_PLAYER_TERMINAL_FALL;
    }
    /* 80230A28..8C: exact boundary is strict; preserve the unusual velocity
     * rule. A host collision adapter may require additional resolution. */
    if (has_ceiling && (ceiling_y < state->y ||
                       ceiling_y < state->y + BM64_PLAYER_TOP_OFFSET)) {
        if (state->extra_y != 0.0f) state->fall_velocity = 0.0f;
        state->extra_y = 0.0f;
        state->y = ceiling_y - BM64_PLAYER_TOP_OFFSET;
        contacts |= BM64_CONTACT_CEILING;
    }
    return contacts;
}
