#include "mounted_n64.h"
#include <string.h>

static int32_t s32(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)~value;
}
static int run(SmN64MountedOwner *s, uint16_t clip, const uint16_t *counts, size_t n) {
    if (!counts || clip >= n || !counts[clip] || counts[clip] > INT16_MAX) return -1;
    smn64_anim_run(&s->player.anim, clip, counts[clip], 0, -1);
    return 1;
}
static int actor(SmN64MountedOwner *s, SmN64MountedActor *out, const SmN64MountedHost *h) {
    int rc;
    memset(out, 0, sizeof(*out));
    if (!s->player.grab_actor) return 0;
    if (!h->actor) return -2;
    rc = h->actor(h->context, s->player.grab_actor, out);
    if (rc < 0 || rc > 1 || (rc == 1 && out->id != s->player.grab_actor)) return -2;
    if (!rc) {
        s->player.grab_actor = 0;
        memset(out, 0, sizeof(*out));
    }
    return rc;
}
static void display(SmN64MountedOwner *s) {
    s->display_timer = 360;
    s->display_value = (s->combo_metric << 10) + 0x2c00;
}
static int sound(SmN64MountedOwner *s, const SmN64MountedHost *h) {
    return h->sound && h->sound(h->context, 16, s->player.position) == 1 ? 1 : -2;
}
int smn64_mounted_step(SmN64MountedOwner *s, SmN64CombatInput *input,
    uint16_t completed_animation, uint32_t tick, const uint16_t *counts, size_t n,
    const SmN64MountedHost *h) {
    SmN64ComboOwner *p;
    SmN64MountedActor target;
    SmN64MountedHit hit;
    int rc;
    if (!s || !input || !h) return -1;
    p = &s->player;
    if (p->state != 0x4000000 && p->state != 0x8000000) return 0;
    p->motion_speed = 0;
    if (p->state == 0x4000000) {
        if (completed_animation == 124)
            return h->stop && h->stop(h->context, s) == 1 ? 1 : -2;
        if (p->anim.animation != 124 || p->anim.frame < 16) return 1;
        rc = actor(s, &target, h);
        if (rc < 0) return rc;
        if (!rc) return 1;
        memset(&hit, 0, sizeof(hit));
        hit.actor = target.id;
        hit.kind = 2;
        hit.flags = 0x1e;
        hit.damage = (uint16_t)smn64_damage_scaled(70, s->suit, s->difficulty);
        hit.has_knockback = 1;
        hit.direction[0] = s32(0u - (uint32_t)p->forward[0]);
        hit.direction[2] = s32(0u - (uint32_t)p->forward[2]);
        hit.impulse = 600;
        hit.duration = 16;
        if (!h->damage) return -2;
        rc = h->damage(h->context, s, &hit);
        if (rc < 0 || rc > 1) return -2;
        p->grab_actor = 0;
        display(s);
        return sound(s, h);
    }
    s->dismount_c9c = 0;
    if (p->anim.animation == 125) {
        input->pressed &= ~8u;
        s->jump_latch_311 = 0;
    }
    rc = actor(s, &target, h);
    if (rc < 0) return rc;
    if (!rc) return -2;
    if (target.health <= 0 || tick - p->grab_tick >= 301 || (input->pressed & 8)) {
        s->dismount_c9c = 1;
        p->state = 4;
        rc = run(s, 127, counts, n);
        if (rc < 0) return rc;
        p->grab_actor = 0;
        s->velocity[1] = s32((uint32_t)s->velocity[1] - 0x40000u);
        s->velocity[0] = s32((uint32_t)s->velocity[0] + (uint32_t)p->forward[0] * 24u);
        s->velocity[2] = s32((uint32_t)s->velocity[2] + (uint32_t)p->forward[2] * 24u);
    } else if (completed_animation == 126) {
        int32_t effect_position[3];
        unsigned i;
        memset(&hit, 0, sizeof(hit));
        hit.actor = target.id;
        hit.kind = 3;
        hit.flags = 6;
        hit.damage = (uint16_t)smn64_damage_scaled(20, s->suit, s->difficulty);
        if (!h->damage) return -2;
        rc = h->damage(h->context, s, &hit);
        if (rc < 0 || rc > 1) return -2;
        display(s);
        for (i = 0; i < 3; ++i)
            effect_position[i] = s32((uint32_t)p->position[i] - (uint32_t)p->forward[i] * 32u +
                                     (uint32_t)s->up[i] * 64u);
        if (!h->hit_effects || h->hit_effects(h->context, s, effect_position) != 1) return -2;
        if (sound(s, h) < 0) return -2;
    } else if (p->anim.animation == 128 && (input->pressed & 4)) {
        rc = run(s, 126, counts, n);
        if (rc < 0) return rc;
    }
    input->pressed &= ~4u;
    return 1;
}
