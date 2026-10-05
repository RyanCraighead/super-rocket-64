#include "web_abilities_n64.h"
#include <string.h>
static int run(SmN64WebAbility *s,uint16_t clip,const uint16_t *c,size_t n) {
    if(!c||clip>=n||!c[clip]||c[clip]>INT16_MAX)return -1;
    smn64_anim_run(&s->anim,clip,c[clip],0,-1);return 1;
}
static int shot(SmN64WebAbility *s,uint32_t state,uint8_t mode,uint16_t clip,
    const uint16_t *c,size_t n,SmN64WebAbilityEvent *e) {
    if(run(s,clip,c,n)<0)return -1;
    s->state=state;s->attack_mode=mode;s->fired=0;s->aim_snapshot=s->aiming;
    if(state==0x8000)s->yank_variant=s->axis_1124>0?2:s->axis_1124<0?1:0;
    s->actor_flags&=0xfe;e->face_target=1;return 1;
}
int smn64_web_ability_plan(const SmN64WebAbility *source,const SmN64WebButtons *buttons,
    uint32_t tick,SmN64WebAbilityPlan *out) {
    SmN64WebAbility *s;
    if(!source||!buttons||!out)return -1;
    memset(out,0,sizeof(*out));out->prepared=*source;s=&out->prepared;
    if(s->latched&&!buttons->web){s->latched=0;return 0;}
    if(s->holding_object){s->latched=0;return 0;}
    if(buttons->web&&!s->latched) {
        s->latched=1;
        s->moving_latch=!(s->state&1)&&(s->axis_1123||s->axis_1124)&&s->run_ramp>=7;
    }
    if(!s->latched)return 0;
    s->blocked_directions=0;
    if(s->moving_latch) {
        if(s->axis_1123>0)s->blocked_directions|=1;
        if(s->axis_1123<0)s->blocked_directions|=2;
        if(s->axis_1124>0)s->blocked_directions|=4;
        if(s->axis_1124<0)s->blocked_directions|=8;
    }
    if(s->axis_1123>0&&!(s->blocked_directions&1)) {
        out->command=SMN64_COMMAND_YANK;out->animation=s->surface_mode?262:252;return 1;
    }
    if(s->axis_1123<0&&!(s->blocked_directions&2)) {
        out->command=SMN64_COMMAND_IMPACT;out->animation=s->surface_mode?265:255;return 1;
    }
    if(s->axis_1124<0&&!(s->blocked_directions&8)&&tick-s->last_glove_tick>=31&&!s->surface_mode) {
        out->command=SMN64_COMMAND_GLOVES;out->animation=285;return 1;
    }
    if(!s->aiming) {
        if(s->axis_1124>0&&!(s->blocked_directions&4)) {
            if(s->surface_mode)return 0;
            out->command=SMN64_COMMAND_DOME;out->animation=283;return 1;
        }
        if(!s->surface_mode&&(buttons->punch||buttons->kick)) {
            out->command=SMN64_COMMAND_GRAB;out->animation=120;return 1;
        }
    }
    if(!buttons->web||tick-s->last_glove_tick<31)return 0;
    out->command=SMN64_COMMAND_TRAP;out->animation=s->surface_mode?260:250;return 1;
}
int smn64_web_ability_request(SmN64WebAbility *s,const SmN64WebButtons *buttons,uint32_t tick,
    SmN64WebResource *resource,uint32_t rng[3],const uint16_t *counts,size_t count,SmN64WebAbilityEvent *e) {
    int rc;SmN64WebAbilityPlan plan;
    if(!s||!buttons||!e||!resource||!rng)return -1;
    memset(e,0,sizeof(*e));
    rc=smn64_web_ability_plan(s,buttons,tick,&plan);if(rc<0)return rc;
    *s=plan.prepared;if(!rc)return 0;
    switch(plan.command) {
        case SMN64_COMMAND_YANK:return shot(s,0x8000,2,plan.animation,counts,count,e);
        case SMN64_COMMAND_IMPACT:return shot(s,0x10000,4,plan.animation,counts,count,e);
        case SMN64_COMMAND_GLOVES:
            rc=smn64_web_consume(resource,1024,rng,&e->resource);if(rc<=0)return rc;
            if(run(s,plan.animation,counts,count)<0)return -1;
            s->state=0x800000;return 1;
        case SMN64_COMMAND_DOME:
            rc=smn64_web_consume(resource,3072,rng,&e->resource);if(rc<=0)return rc;
            if(run(s,plan.animation,counts,count)<0)return -1;
            s->state=0x20000000;s->dome_tick=tick;e->create_dome=1;return 1;
        case SMN64_COMMAND_GRAB:
            if(run(s,plan.animation,counts,count)<0)return -1;
            s->state=0x2000000;e->grab_target_query=1;return 1;
        case SMN64_COMMAND_TRAP:return shot(s,0x4000,1,plan.animation,counts,count,e);
        default:return -1;
    }
}
int smn64_web_gloves_tick(SmN64WebAbility *s,uint16_t completed,uint32_t tick,const uint16_t *counts,size_t count,SmN64WebAbilityEvent *e) {
    if(!s||!e||s->state!=0x800000)return -1;
    memset(e,0,sizeof(*e));
    if(s->anim.animation==285&&s->anim.frame>=7&&tick-s->last_glove_tick>=61) {
        s->glove_hits=5;s->glove_fade=0;s->last_glove_tick=tick;e->glove_sound=0x16;
    }
    if(completed==285){if(run(s,0,counts,count)<0)return -1;s->state=1;return 0;}
    return 1;
}
int smn64_web_dome_tick(SmN64WebAbility *s,const SmN64WebButtons *buttons,uint16_t completed,uint32_t tick,
    const uint16_t *counts,size_t count,SmN64WebAbilityEvent *e) {
    if(!s||!buttons||!e||s->state!=0x20000000)return -1;
    memset(e,0,sizeof(*e));
    if(completed==284){if(run(s,0,counts,count)<0)return -1;s->state=1;return 0;}
    if(s->anim.animation==284)return 1;
    if(tick-s->dome_tick>=151||buttons->punch||buttons->kick||buttons->jump) {
        if(run(s,284,counts,count)<0)return -1;
        e->destroy_dome=1;
    }
    return 1;
}
