#include "n64_behavior.h"
#include <limits.h>

/* Explicit 32-bit wrapping and arithmetic shifts; no undefined signed overflow. */
static int32_t i32(uint32_t u) {
    return u <= INT32_MAX ? (int32_t)u : -1 - (int32_t)(UINT32_MAX - u);
}
static int16_t i16(uint16_t u) {
    return u <= INT16_MAX ? (int16_t)u : (int16_t)(-1 - (int32_t)(UINT16_MAX - u));
}
static uint32_t asr(uint32_t u, unsigned bits) {
    return (u >> bits) | ((u & UINT32_C(0x80000000)) ? (UINT32_MAX << (32u - bits)) : 0u);
}

void smn64_anim_run(SmN64Anim *s, uint16_t animation, uint16_t count, int32_t from, int32_t to) {
    s->animation = animation;
    s->frame_count = count;
    if (from == -1) from = (int32_t)count - 1;
    if (to == -1) to = (int32_t)count - 1;
    if (from < 0 || from >= (int32_t)count) from = 0;
    if (to < 0 || to >= (int32_t)count) to = 0;
    s->mode = 0;
    s->direction = (int8_t)((from < to) - (to < from));
    s->frame = i16((uint16_t)from);
    s->target = i16((uint16_t)to);
    s->fraction = 0;
    s->finished = (uint8_t)(s->frame == s->target);
}

void smn64_anim_cycle(SmN64Anim *s, uint16_t animation, uint16_t count, int8_t direction) {
    if (s->animation != animation) {
        s->frame = 0;
        s->fraction = 0;
        s->animation = animation;
        s->frame_count = count;
        s->direction = direction;
    }
    s->mode = 1;
    s->finished = 0;
}

void smn64_anim_advance(SmN64Anim *s) {
    uint32_t delta = s->rate;
    if (s->elapsed_ticks >= 3) {
        uint32_t product = delta * ((uint32_t)s->elapsed_ticks - 2u);
        /* Signed division by two, truncating towards zero, after low-word multiply. */
        delta += asr(product + (product >> 31), 1);
    }
    uint32_t phase = ((uint32_t)(uint16_t)s->frame << 16) | s->fraction;
    if (s->direction == 1) phase += delta;
    if (s->direction == -1) phase -= delta;
    s->frame = i16((uint16_t)(phase >> 16));
    s->fraction = (uint16_t)phase;
    if (s->mode == 0) {
        if ((s->direction == 1 && s->frame >= s->target) ||
            (s->direction == -1 && s->frame <= s->target)) {
            s->frame = s->target;
            s->finished = 1;
        }
    } else if (s->mode == 1) {
        /* The original adjusts by ONE count; repeated modulo would change behavior. */
        if ((int32_t)s->frame >= (int32_t)s->frame_count)
            s->frame = i16((uint16_t)((uint16_t)s->frame - s->frame_count));
        else if (s->frame < 0)
            s->frame = i16((uint16_t)((uint16_t)s->frame + s->frame_count));
    }
}

int32_t smn64_damage_scaled(int32_t damage, uint8_t suit, int32_t difficulty) {
    uint32_t d = (uint32_t)damage;
    if (suit >= 1 && suit <= 3) d <<= 1;
    if (difficulty == 2) return i32(d);
    if (difficulty == 0) return i32(asr(d << 13, 12));
    d *= 3u;
    return i32(asr(d << (difficulty == 1 ? 11 : 10), 12));
}


int smn64_ground_jump_request(SmN64Ground *s, uint16_t standing_count, uint16_t running_count) {
    if (!s->jump_pressed || s->aiming || s->holding_object) return 0;
    if (s->surface_mode || s->platform_present) return -1;
    s->jump_velocity = -245760; /* Original signed word 0xfffc4000. */
    s->field_d24 = 0;
    if (s->state & 0x10u) {
        smn64_anim_run(&s->anim, 223, running_count, 5, -1);
        s->jump_variant = 1;
    } else {
        smn64_anim_run(&s->anim, 210, standing_count, 4, -1);
        s->jump_variant = 0;
    }
    s->field_d20 = 0;
    s->state = 0x40;
    if (s->collision & 1u) s->acceleration_phase = 0;
    s->field_1184 = 0;
    s->jump_pressed = 0;
    s->copied_jump_pressed = 0;
    return 1;
}

int smn64_ground_lost(SmN64Ground *s, uint16_t fall_count) {
    if (s->holding_object || s->surface_mode) return -1;
    if (s->collision & 2u) return 0;
    if (s->ground_grace) {
        --s->ground_grace;
        if (s->ground_grace) return 0;
    }
    s->falling_origin_y = s->position_y;
    smn64_anim_run(&s->anim, 212, fall_count, 0, -1);
    s->jump_variant = 0;
    s->field_d20 = 0;
    s->field_d24 = 0;
    s->state = 4;
    return 1;
}

void smn64_free_motion(int32_t velocity[3], const int32_t acceleration[3],
                       const uint8_t drag[3], int32_t elapsed_ticks, int32_t displacement[3]) {
    for (unsigned axis = 0; axis < 3; ++axis) {
        uint32_t v = (uint32_t)velocity[axis] + (uint32_t)acceleration[axis];
        unsigned shift = drag[axis] & 31u;
        uint32_t decayed = shift ? asr(v, shift) : v;
        v -= decayed;
        if (v + 2048u < 4097u) v = 0;
        velocity[axis] = i32(v);
        uint32_t d = v;
        if (elapsed_ticks >= 3) d += asr(v, 1) * ((uint32_t)elapsed_ticks - 2u);
        displacement[axis] = i32(d);
    }
}

void smn64_jump_tail(int32_t *velocity_y, int32_t launch_velocity, int32_t *base_timer,
                    int32_t *hold_timer, uint16_t collision, int32_t elapsed_ticks) {
    if (collision & 0x100u) *hold_timer = 0;
    if (*base_timer > 0 || *hold_timer > 0) {
        uint32_t v = (uint32_t)launch_velocity;
        if (*base_timer <= 65535 && *hold_timer == 0) {
            v = asr(asr(v, 12) * (uint32_t)*base_timer, 16) << 12;
        }
        *velocity_y = i32(v);
    }
    uint32_t decrement = asr((uint32_t)elapsed_ticks << 16, 1);
    if (*hold_timer > 0) {
        int32_t next = i32((uint32_t)*hold_timer - decrement);
        *hold_timer = next < 0 ? 0 : next;
    }
    if (*base_timer > 0) {
        int32_t next = i32((uint32_t)*base_timer - decrement);
        *base_timer = next < 0 ? 0 : next;
    }
}
