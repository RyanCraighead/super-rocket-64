#include "manipob_n64.h"
#include "../web/resource_n64.h"
#include <string.h>

static int32_t signed_word(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static int16_t signed_half(uint16_t v) {
    return v <= INT16_MAX ? (int16_t)v : (int16_t)(-1 - (int32_t)(uint16_t)~v);
}
int smn64_manipob_construct_scalars(SmN64ManipOb *s,uint16_t node,
    uint16_t authored_flags,int16_t hold_radius,uint16_t sound,uint32_t shadow_id) {
    if (!s) return -1;
    s->body_flags_4a |= 0x16;
    s->flags = (uint16_t)((s->flags | 0x90u) & 0xfffdu);
    s->type = 401;
    s->node = node;
    s->motion.drag[0] = 12;
    s->motion.drag[1] = 31;
    s->motion.drag[2] = 12;
    s->shadow_id = shadow_id;
    if (authored_flags & 1u) s->motion.flags_10c |= 8u;
    if (authored_flags & 2u) s->motion.flags_10c |= 0x20u;
    s->hold_radius = hold_radius;
    s->sound = sound;
    return 1;
}
static int pulse(SmN64ManipOb *s,const SmN64ManipObHost *h) {
    if (!s->pulse_sent) {
        s->pulse_sent = 1;
        if (h->pulse(h->context,s->node) != 1) return -2;
    }
    return 1;
}
int smn64_manipob_pickup(SmN64ManipOb *s,const SmN64ManipObHost *h) {
    if (!s || !h || !h->alpha || (s->shadow_id && !h->hide_shadow) ||
        (!s->pulse_sent && !h->pulse)) return -1;
    s->flags |= 0x820;
    if (h->alpha(h->context,s->motion.id,128) != 1) return -2;
    if (s->shadow_id && h->hide_shadow(h->context,s->shadow_id) != 1) return -2;
    return pulse(s,h);
}
static int release_begin(SmN64ManipOb *s,enum SmN64ManipObRelease kind,
    const int32_t velocity[3],const SmN64ManipObHost *h) {
    if (!s || kind < SMN64_MANIPOB_THROW || kind > SMN64_MANIPOB_THROW_PATH ||
        (kind != SMN64_MANIPOB_THROW_PATH && (!velocity || !h || !h->alpha))) return -1;
    s->flags &= 0xf7ff;
    if (kind != SMN64_MANIPOB_THROW_PATH &&
        h->alpha(h->context,s->motion.id,255) != 1) return -2;
    s->motion.flags_10c |= 1u;
    if (kind == SMN64_MANIPOB_DROP) s->motion.flags_10c &= ~8u;
    if (kind != SMN64_MANIPOB_THROW_PATH) {
        memcpy(s->motion.velocity,velocity,sizeof(s->motion.velocity));
        s->motion.acceleration[1] = 4096;
    }
    return 1;
}
int smn64_manipob_release(SmN64ManipOb *s,enum SmN64ManipObRelease kind,
    const int32_t velocity[3],uint32_t rng[3],const SmN64ManipObHost *h) {
    int rc;
    if (!rng) return -1;
    rc = release_begin(s,kind,velocity,h);
    if (rc != 1) return rc;
    s->motion.spin[0] = signed_half((uint16_t)(smn64_web_random(rng,32) + 64));
    return 1;
}
int smn64_manipob_release_with_spin(SmN64ManipOb *s,enum SmN64ManipObRelease kind,
    const int32_t velocity[3],uint16_t spin,const SmN64ManipObHost *h) {
    int rc = release_begin(s,kind,velocity,h);
    if (rc != 1) return rc;
    s->motion.spin[0] = signed_half(spin);
    return 1;
}
int smn64_manipob_impact_empty(SmN64ManipOb *s,const SmN64ManipObHost *h) {
    uint32_t sound;
    int32_t ground_y,position[3];
    if (!s) return -1;
    if ((s->motion.flags_10c & 0x20u) || s->debris_count) return -3;
    if (!h || !h->sound || !h->ground || !h->stimulus ||
        (!s->pulse_sent && !h->pulse)) return -1;
    sound = s->sound ? ((uint32_t)s->sound | 0x8000u) :
        (s->motion.flags_10c & 8u ? 0x1cu : 0x1bu);
    if (h->sound(h->context,sound,s->motion.position) != 1) return -2;
    if (h->ground(h->context,s->motion.position,0,700,1,&ground_y) != 1) return -2;
    memcpy(position,s->motion.position,sizeof(position));
    if (ground_y != -1) position[1] = signed_word((uint32_t)ground_y - 204800u);
    if (h->stimulus(h->context,position,2000) != 1) return -2;
    return pulse(s,h);
}
int smn64_manipob_smash_empty(SmN64ManipOb *s,const SmN64ManipObHost *h) {
    int rc;
    if (!s || !h || !h->destroy || (s->shadow_id && !h->hide_shadow)) return -1;
    if ((s->motion.flags_10c & 0x20u) || s->debris_count) return -3;
    if (!h->sound || !h->ground || !h->stimulus || (!s->pulse_sent && !h->pulse)) return -1;
    if (s->motion.destroyed) return 0;
    s->motion.velocity[0] = 0;
    s->motion.velocity[1] = 131072;
    s->motion.velocity[2] = 131072;
    rc = smn64_manipob_impact_empty(s,h);
    if (rc != 1) return rc;
    if (s->shadow_id && h->hide_shadow(h->context,s->shadow_id) != 1) return -2;
    rc = pulse(s,h);
    if (rc != 1) return rc;
    if (h->destroy(h->context,s->motion.id) != 1) return -2;
    s->motion.destroyed = 1;
    return 1;
}
