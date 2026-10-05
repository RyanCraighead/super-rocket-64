#include "oot_sword.h"
#include <math.h>
#include <string.h>

#define TRACK(name) "gPlayerAnim_link_" name
static const OotSwordAttack attacks[] = {
    {0, TRACK("fighter_normal_kiru"), TRACK("fighter_normal_kiru_end"), TRACK("fighter_normal_kiru_endR"), 1, 4},
    {2, TRACK("fighter_normal_kiru_finsh"), TRACK("fighter_normal_kiru_finsh_end"), TRACK("anchor_normal_kiru_finsh_endR"), 0, 5},
    {4, TRACK("fighter_Lside_kiru"), TRACK("fighter_Lside_kiru_end"), TRACK("anchor_Lside_kiru_endR"), 1, 4},
    {6, TRACK("fighter_Lside_kiru_finsh"), TRACK("fighter_Lside_kiru_finsh_end"), TRACK("anchor_Lside_kiru_finsh_endR"), 2, 8},
    {8, TRACK("fighter_Rside_kiru"), TRACK("fighter_Rside_kiru_end"), TRACK("anchor_Rside_kiru_endR"), 0, 4},
    {10, TRACK("fighter_Rside_kiru_finsh"), TRACK("fighter_Rside_kiru_finsh_end"), TRACK("anchor_Rside_kiru_finsh_endR"), 0, 6},
    {12, TRACK("fighter_pierce_kiru"), TRACK("fighter_pierce_kiru_end"), TRACK("anchor_pierce_kiru_endR"), 0, 3},
    {14, TRACK("fighter_pierce_kiru_finsh"), TRACK("fighter_pierce_kiru_finsh_end"), TRACK("anchor_pierce_kiru_finsh_endR"), 1, 9},
    {24, TRACK("fighter_rolling_kiru"), TRACK("fighter_rolling_kiru_end"), TRACK("anchor_rolling_kiru_endR"), 0, 12},
};
#undef TRACK

static int8_t wrap8(int value) {
    unsigned u = (unsigned)value & 255u;
    return (int8_t)(u < 128 ? (int)u : (int)u - 256);
}
static int16_t wrap16(int value) {
    unsigned u = (unsigned)value & 65535u;
    return (int16_t)(u < 32768 ? (int)u : (int)u - 65536);
}

void oot_sword_stick_reset(OotSwordStickHistory *h) {
    if (!h) return;
    memset(h, 0, sizeof(*h));
    for (int i = 0; i < 4; ++i) h->spin_angles[i] = h->directions[i] = -1;
}

void oot_sword_stick_push(OotSwordStickHistory *h, float magnitude,
                          int16_t stick_angle, int16_t world_yaw, int16_t facing_yaw) {
    if (!h) return;
    h->index = (h->index + 1) % 4;
    if (!isfinite(magnitude) || magnitude < 55.0f) {
        h->directions[h->index] = -1;
        h->spin_angles[h->index] = -1;
    } else {
        h->spin_angles[h->index] = (uint16_t)(stick_angle + 0x2000) >> 9;
        h->directions[h->index] = (uint16_t)(wrap16(world_yaw - facing_yaw) + 0x2000) >> 14;
    }
}

bool oot_sword_can_quick_spin(const OotSwordStickHistory *h) {
    if (!h) return false;
    int8_t a[4];
    for (int i = 0; i < 4; ++i) {
        if (h->spin_angles[i] < 0) return false;
        a[i] = wrap8(h->spin_angles[i] * 2);
    }
    int first = wrap8(a[0] - a[1]);
    if (first > -10 && first < 10) return false;
    for (int i = 1; i < 3; ++i) {
        int next = wrap8(a[i] - a[i + 1]);
        if ((next > -10 && next < 10) || next * first < 0) return false;
    }
    return true;
}

int oot_sword_select_attack(const OotSwordStickHistory *h, bool z_targeting) {
    if (!h || h->index >= 4) return -1;
    if (oot_sword_can_quick_spin(h)) return OOT_SWORD_SPIN;
    int direction = h->directions[h->index];
    if (direction <= -1) return z_targeting ? OOT_SWORD_FORWARD : OOT_SWORD_RIGHT;
    static const int choices[4] = {OOT_SWORD_STAB, OOT_SWORD_RIGHT, OOT_SWORD_RIGHT, OOT_SWORD_LEFT};
    if (direction > 3) return -1;
    int selected = choices[direction];
    return selected == OOT_SWORD_STAB && !z_targeting ? OOT_SWORD_FORWARD : selected;
}

void oot_sword_combo_reset(OotSwordCombo *c) {
    if (!c) return;
    c->animation = -1; c->repeat_count = 0; c->hold_timer = 0;
}
void oot_sword_combo_pre_tick(OotSwordCombo *c) {
    if (!c) return;
    if (c->hold_timer == 0) c->repeat_count = 0;
    else if (c->hold_timer < 0) ++c->hold_timer;
    else --c->hold_timer;
}
void oot_sword_combo_release(OotSwordCombo *c, bool held) {
    if (c && c->hold_timer > 0 && !held) c->hold_timer = -c->hold_timer;
}

int oot_sword_combo_start(OotSwordCombo *c, int requested) {
    if (!c || !oot_sword_attack_info(requested)) return -1;
    /* Initial one-handed selector yields only base attacks or quick-spin. */
    if (requested != 0 && requested != 4 && requested != 8 && requested != 12 && requested != 24) return -1;
    c->hold_timer = 8;
    if (requested != c->animation || c->repeat_count >= 3) c->repeat_count = 0;
    ++c->repeat_count;
    int actual = requested + (c->repeat_count >= 3 ? 2 : 0);
    /* Big-spin id 26 requires the magic/effect controller, deliberately outside
       the bounded first slice. Never silently substitute an invented attack. */
    if (!oot_sword_attack_info(actual)) {
        c->repeat_count = 0; c->hold_timer = 0; c->animation = -1;
        return -1;
    }
    c->animation = actual;
    return actual;
}

const OotSwordAttack *oot_sword_attack_info(int id) {
    for (unsigned i = 0; i < sizeof(attacks) / sizeof(attacks[0]); ++i)
        if (attacks[i].animation == id) return &attacks[i];
    return 0;
}

int oot_sword_weapon_state(int animation, float frame) {
    const OotSwordAttack *a = oot_sword_attack_info(animation);
    if (!a || !isfinite(frame) || frame < 0 || frame > a->active_end) return 0;
    return frame >= a->active_start ? 1 : -1;
}

bool oot_sword_animation_once(float *frame, float end_frame, float speed) {
    if (!frame || !isfinite(*frame) || *frame < 0 || !isfinite(end_frame) || end_frame < 0 || !isfinite(speed) || speed <= 0) return true;
    /* Original function reports completion on the update AFTER reaching the
       final frame, not on the update that first clamps to it. */
    if (*frame == end_frame) return true;
    *frame += speed * 1.5f;  /* R_UPDATE_RATE=3, LinkAnimation_Once */
    if (*frame > end_frame) *frame = end_frame;
    return false;
}

void oot_sword_local_edges(float length, int *repeat, OotSwordVec3 tip[3], OotSwordVec3 base[3]) {
    if (!tip || !base || !repeat) return;
    if (!isfinite(length) || length < 0 || length > 5500) length = 0;
    if (*repeat < 0 || *repeat > 127) *repeat = 0; /* source field is s8 */
    float collision_length = length;
    if (*repeat >= 3) {
        /* Original Player_CalcMeleeWeaponTipPositions updates this counter once
           per source draw/gameplay tick. Host interpolated draws MUST NOT call
           this stateful function more frequently. */
        *repeat = wrap8(*repeat + 1);
        collision_length *= 1.0f + ((9 - *repeat) * 0.1f);
    }
    collision_length += 1200.0f;
    tip[0] = (OotSwordVec3){length, 400, 0};
    tip[1] = (OotSwordVec3){collision_length, -400, 1000};
    tip[2] = (OotSwordVec3){collision_length, 1400, -1000};
    base[0] = (OotSwordVec3){0, 400, 0};
    base[1] = (OotSwordVec3){0, 1400, -1000};
    base[2] = (OotSwordVec3){0, -400, 1000};
}

static bool same(OotSwordVec3 a, OotSwordVec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
static bool finite3(OotSwordVec3 a) { return isfinite(a.x) && isfinite(a.y) && isfinite(a.z); }
bool oot_sword_sweep(OotSwordEdgeHistory *h, OotSwordVec3 tip, OotSwordVec3 base, OotSwordQuad *q) {
    if (!h || !q) return false;
    q->valid = false;
    if (!finite3(tip) || !finite3(base)) { h->active = false; return false; }
    if (!h->active) {
        h->tip = tip; h->base = base; h->active = true;
        return true; /* Original initializes the trail but submits NO quad. */
    }
    if (same(h->tip, tip) && same(h->base, base)) return false;
    q->v[0] = base; q->v[1] = tip; q->v[2] = h->base; q->v[3] = h->tip;
    q->valid = true;
    h->base = base; h->tip = tip;
    return true;
}
