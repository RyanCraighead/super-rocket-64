#include "hurt_owner_n64.h"
#include "../web/resource_n64.h"
#include <string.h>
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t sar12(int32_t x){return s32(((uint32_t)x>>12)|(x<0?0xfff00000u:0));}
uint32_t smn64_hurt_invulnerability_tick(uint32_t current,int32_t elapsed){
    int32_t remaining;if(!current)return 0;remaining=s32(current-(uint32_t)elapsed);
    return remaining<0?0:(uint32_t)remaining;
}
uint16_t smn64_hurt_fall_timer_tick(uint16_t current,uint32_t state,uint16_t collision,int32_t elapsed){
    return !(state&0x800004u)||(collision&2)?0:(uint16_t)(current+(uint16_t)elapsed);
}
static int run(SmN64HurtOwner *s,uint16_t clip,const uint16_t *c,size_t n){
    if(!c||clip>=n||!c[clip]||c[clip]>INT16_MAX)return -1;
    smn64_anim_run(&s->anim,clip,c[clip],0,-1);return 1;
}
int smn64_hurt_owner_step(SmN64HurtOwner *s,uint16_t done,uint32_t tick,
    const uint16_t *c,size_t n,const SmN64HurtHost *h,SmN64HurtEvent *e){
    if(!s||!e)return -1;
    memset(e,0,sizeof(*e));if(s->state!=0x800000)return 0;
    s->motion_speed=0;
    if(s->fall_latch_664){
        if(done==175){if(run(s,177,c,n)<0)return -1;}
        else if(done==177){s->state=4;s->fall_latch_664=0;return 1;}
    }
    if(s->fall_timer_1188>=481)e->level_state=8;
    if(s->anim.animation==285&&s->anim.frame>=7&&tick-s->glove_tick>=61){
        s->glove_hits=5;s->glove_display=0;s->glove_tick=tick;e->sound=22;
    }
    if(done==170||done==187||done==181||done==180||done==174||done==285){
        if(done==180||done==174){s->invulnerable_ticks=30;s->d20=0;}
        s->cf8=1;return h&&h->stop&&h->stop(h->context,s)==1?1:-2;
    }
    if((s->anim.animation==175||s->anim.animation==176)&&(s->collision&2)){
        if(run(s,178,c,n)<0)return -1;
        if(s->health<=0)return h&&h->die&&h->die(h->context,s)==1?1:-2;
    }
    return 1;
}
int smn64_hurt_land(SmN64HurtOwner *s,uint32_t rng[3],const uint16_t *c,size_t n,
    const SmN64HurtHost *h,SmN64HurtEvent *e){
    uint16_t clip,next;int active,reset=0;
    if(!s||!rng||!e)return -1;
    memset(e,0,sizeof(*e));if(!(s->collision&2))return 0;
    e->sound=s->alternate_landing_sound?(smn64_web_random(rng,4)+80u)|0x8000u:9;
    if(s->fall_min){
        int32_t drop=sar12(sub(s->position_y,s->fall_origin_y));
        if(drop>s->fall_min){
            int32_t numerator=s32((uint32_t)sub(drop,s->fall_min)*(uint32_t)s->fall_damage_scale);
            int32_t denominator=sub(s->fall_max,s->fall_min);
            if(!denominator||(numerator==INT32_MIN&&denominator==-1))return -1;
            if(!h||!h->fall_damage||h->fall_damage(h->context,s,(uint16_t)(numerator/denominator))!=1)return -2;
            if(s->health<=0)return 1;
            goto select;
        }
    }
    e->rumble=s->rumble_enabled?1:0;
select:
    clip=s->anim.animation;active=s->axis_1123||s->axis_1124;
    if(clip==232){next=active?236:237;reset=!active;}
    else if(clip==175||clip==176)next=178;
    else if(clip==225||(s->jump_variant&&(clip==226||clip==228||clip==233||clip==235))){next=active?229:230;reset=!active;}
    else {next=213;reset=1;}
    if(run(s,next,c,n)<0)return -1;
    if(reset)s->jump_variant=0;
    s->state=8;s->d20=0;s->field_660=0;s->jump_pressed=0;s->jump_latch_311=0;return 1;
}
