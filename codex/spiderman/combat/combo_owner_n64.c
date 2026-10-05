#include "combo_owner_n64.h"
#include <math.h>
#include <string.h>

static int32_t s32(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)~value;
}
static int32_t sub(int32_t a, int32_t b) {
    return s32((uint32_t)a - (uint32_t)b);
}
static int32_t sar12(int32_t value) {
    return s32(((uint32_t)value >> 12) | (value < 0 ? 0xfff00000u : 0));
}
static uint32_t distance(const int32_t a[3], const int32_t b[3]) {
    uint32_t square = 0;
    unsigned i;
    for (i = 0; i < 3; ++i) {
        int32_t d = sar12(sub(a[i], b[i]));
        square += (uint32_t)d * (uint32_t)d;
    }
    return (uint32_t)sqrtf((float)square);
}
static int run(SmN64ComboOwner *s, uint16_t clip, const uint16_t *counts, size_t n) {
    if (!counts || clip >= n || !counts[clip] || counts[clip] > INT16_MAX) return -1;
    smn64_anim_run(&s->anim, clip, counts[clip], 0, -1);
    return 1;
}
static int stop(SmN64ComboOwner *s, const SmN64ComboOwnerHost *h) {
    return h->stop && h->stop(h->context, s) == 1 ? 1 : -2;
}
static int release(SmN64ComboOwner *s, uint32_t actor, const SmN64ComboOwnerHost *h) {
    if (actor && (!h->release_grab || h->release_grab(h->context, actor) != 1)) return -2;
    s->grab_actor = 0;
    return 1;
}
static int lookup(SmN64ComboOwner *s, SmN64GrabActor *actor, const SmN64ComboOwnerHost *h) {
    int rc;
    memset(actor, 0, sizeof(*actor));
    if (!s->grab_actor) return 0;
    if (!h->grab_actor) return -2;
    rc = h->grab_actor(h->context, s->grab_actor, actor);
    if (rc < 0 || rc > 1 || (rc == 1 && actor->id != s->grab_actor)) return -2;
    if (!rc) {
        s->grab_actor = 0;
        memset(actor, 0, sizeof(*actor));
    }
    return rc;
}

int smn64_combo_owner_step_admitted(SmN64ComboOwner *s, SmN64CombatInput *input,
    const SmN64WebButtons *held, uint32_t tick, const uint16_t *counts, size_t n,
    const SmN64ComboOwnerHost *h,SmN64CombatAdmission admit,void *admit_context) {
    int rc;
    int32_t status;
    uint32_t target = 0;
    if (!s || !input || !held || !h) return -1;
    if (s->state != 0x800) return 0;
    s->motion_speed = 0;
    if (!h->lost_ground) return -2;
    rc = h->lost_ground(h->context, s);
    if (rc < 0 || rc > 1) return -2;
    if (rc) return 1;
    if (!h->jump) return -2;
    rc = h->jump(h->context, s);
    if (rc < 0 || rc > 1) return -2;
    if (rc) return 1;
    if (s->combo_id < 2 && held->web && (held->punch || held->kick) &&
        tick - s->entry_tick < 5) {
        int accepted=admit?admit(admit_context,SMN64_COMMAND_GRAB):1;
        if(accepted<0||accepted>1)return -2;
        if(!accepted){input->pressed=0;goto continue_combo;}
        if (!h->select_target ||
            h->select_target(h->context, s, 190, -4096, 4096, 0, &target) != 1) return -2;
        s->grab_actor = target;
        s->grab_tick = tick;
        s->state = 0x2000000;
        return run(s, 120, counts, n);
    }
continue_combo:
    if (!h->interpret || h->interpret(h->context, s, input, tick, &status) != 1) return -2;
    if (!status) {
        s->heading_658 = 0;
        return stop(s, h);
    }
    if (status != 2 && status != 3 && status != 6) return 1;
    s->attack_mode = status == 2 ? 1 : 2;
    s->fired = 0;
    s->state = status == 2 ? 0x4000 : status == 3 ? 0x8000 : 0x10000;
    if (status == 3) s->yank_variant = s->axis_1124 > 0 ? 2 : s->axis_1124 < 0 ? 1 : 0;
    s->aim_snapshot = 0;
    if (s->target_actor) {
        if (!h->face_actor || h->face_actor(h->context, s, s->target_actor) != 1) return -2;
    } else {
        s->look_active = 0;
    }
    s->actor_flags &= 0xfe;
    return 1;
}

int smn64_grab_owner_step_admitted(SmN64ComboOwner *s, const SmN64CombatInput *input,
    uint16_t completed_animation, uint32_t tick, int32_t difficulty,
    const uint16_t *counts, size_t n, const SmN64ComboOwnerHost *h,
    SmN64CombatAdmission admit,void *admit_context) {
    SmN64GrabActor actor;
    uint32_t limit;
    int rc;
    if (!s || !input || !h) return -1;
    if (s->state != 0x2000000) return 0;
    s->motion_speed = 0;
    if (completed_animation == 121 || completed_animation == 122) return stop(s, h);
    if (completed_animation == 120) {
        rc = lookup(s, &actor, h);
        if (rc < 0) return rc;
        if (rc) {
            uint32_t dot = (uint32_t)sar12(sub(actor.position[0], s->position[0])) *
                           (uint32_t)s->forward[0] +
                           (uint32_t)sar12(sub(actor.position[2], s->position[2])) *
                           (uint32_t)s->forward[2];
            if (s32(dot) < 0 && distance(s->position, actor.position) < 191) {
                int32_t hold_position[3];
                unsigned i;
                for (i = 0; i < 3; ++i)
                    hold_position[i] = sub(s->position[i], s32((uint32_t)s->forward[i] * 32u));
                if (!h->grab_request) return -2;
                rc = h->grab_request(h->context, s, actor.id, hold_position);
                if (rc < 0 || rc > 1) return -2;
                if (rc) return 1;
            }
        }
        rc = run(s, 121, counts, n);
        if (rc < 0) return rc;
        s->grab_actor = 0;
        return 1;
    }
    if (s->anim.animation != 123) return 1;
    rc = lookup(s, &actor, h);
    if (rc < 0) return rc;
    if (!h->jump) return -2;
    rc = h->jump(h->context, s);
    if (rc < 0 || rc > 1) return -2;
    if (rc) return release(s, actor.id, h);
    if (!h->move_grab || h->move_grab(h->context, s) != 1) return -2;
    if (!s->axis_1123 && !s->axis_1124) s->look_active = 0;
    limit = !actor.id ? 0 : actor.type != 0x13a ? 300 :
            difficulty == 0 ? 420 : (difficulty == 1 || difficulty == 2) ? 120 : 60;
    if (tick - s->grab_tick > limit) {
        if (actor.id && (!h->release_grab || h->release_grab(h->context, actor.id) != 1)) return -2;
        rc = run(s, 122, counts, n);
        if (rc < 0) return rc;
        s->grab_actor = 0;
        return 1;
    }
    if (!actor.id) return -2; /* Original null dereference: unsupported fixture. */
    if ((input->pressed & 4) && actor.type != 0x13a) {
        int accepted=admit?admit(admit_context,SMN64_COMMAND_MOUNTED):1;
        if(accepted<0||accepted>1)return -2;
        if(!accepted)return 1;
        s->state = 0x8000000;
        rc = run(s, 125, counts, n);
        if (rc < 0) return rc;
        s->grab_tick = tick;
        return 1;
    }
    if (input->pressed & 2) {
        int accepted=admit?admit(admit_context,SMN64_COMMAND_MOUNTED):1;
        if(accepted<0||accepted>1)return -2;
        if(!accepted)return 1;
        s->state = 0x4000000;
        return run(s, 124, counts, n);
    }
    return 1;
}

int smn64_combo_owner_step(SmN64ComboOwner *s,SmN64CombatInput *input,
    const SmN64WebButtons *held,uint32_t tick,const uint16_t *counts,size_t n,
    const SmN64ComboOwnerHost *h) {
    return smn64_combo_owner_step_admitted(s,input,held,tick,counts,n,h,NULL,NULL);
}
int smn64_grab_owner_step(SmN64ComboOwner *s,const SmN64CombatInput *input,
    uint16_t completed,uint32_t tick,int32_t difficulty,const uint16_t *counts,size_t n,
    const SmN64ComboOwnerHost *h) {
    return smn64_grab_owner_step_admitted(s,input,completed,tick,difficulty,counts,n,h,NULL,NULL);
}
