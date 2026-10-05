/* Original THPS1 N64 0x8004a204..0x8004a490; air_animation_evidence.md. */
#include "air_animation.h"

static int32_t signed32(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v - 4294967296LL);
}
static int16_t signed16(uint32_t v) {
    uint32_t w = v & 65535u;
    return (int16_t)(w <= 32767u ? (int32_t)w : (int32_t)w - 65536);
}
static int8_t signed8(uint32_t v) {
    uint32_t w = v & 255u;
    return (int8_t)(w <= 127u ? (int32_t)w : (int32_t)w - 256);
}
static int32_t asr8(uint32_t v) {
    return signed32((v >> 8) | ((v & 0x80000000u) ? 0xff000000u : 0));
}

void thps1_anim_run(ThpsAnim *a, const ThpsAnimBank *bank, int32_t id,
                    int32_t from, int32_t to, int32_t continuation) {
    a->id = (uint16_t)id;
    if ((uint32_t)a->id >= bank->clip_count) a->id = 46;
    a->count = bank->frame_counts[a->id];
    if (from == -1) from = (int32_t)a->count - 1;
    if (to == -1) to = (int32_t)a->count - 1;
    if (from < 0) from = 0;
    if (to < 0) to = 0;
    if (from >= a->count) from = (int32_t)a->count - 1;
    if (to >= a->count) to = (int32_t)a->count - 1;
    a->mode = 0;
    a->direction = from < to ? 1 : to < from ? -1 : 0;
    a->frame = signed16((uint32_t)from);
    a->target = signed8((uint32_t)to);
    a->continuation = signed8((uint32_t)continuation);
    a->fraction = 0;
    a->finished = a->frame == a->target;
}

void thps1_anim_advance(ThpsAnim *a, const ThpsAnimBank *bank, int32_t dt8) {
    if (a->mode == 0 &&
        ((a->direction == 1 && a->frame >= a->target) ||
         (a->direction == -1 && a->frame <= a->target)) &&
        a->continuation <= 0) a->finished = 1;
    /* MULT/MFLO then SRA8, not a 64-bit Q16 multiplication. */
    int32_t step = asr8(a->rate * (uint32_t)dt8);
    uint32_t cursor = ((uint32_t)(uint16_t)a->frame << 16) | a->fraction;
    if (a->direction == 1) cursor += (uint32_t)step;
    if (a->direction == -1) cursor -= (uint32_t)step;
    a->frame = signed16(cursor >> 16);
    a->fraction = (uint16_t)cursor;
    if (a->mode == 0) {
        if ((a->direction == 1 && a->frame >= a->target) ||
            (a->direction == -1 && a->frame <= a->target)) {
            if (a->continuation > 0) {
                thps1_anim_run(a, bank, a->id, a->target, a->continuation, -1);
            } else {
                a->frame = a->target;
            }
        }
    } else if (a->mode == 1) {
        if (a->frame >= a->count) a->frame = 0;
        if (a->frame < 0) a->frame = (int16_t)a->count - 1;
    }
}
