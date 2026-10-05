#include "damage_n64.h"
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int16_t s16(uint16_t v){return v<=INT16_MAX?(int16_t)v:(int16_t)(-1-(int16_t)(uint16_t)~v);}
static int32_t mul(int32_t a,int32_t b){return s32((uint32_t)a*(uint32_t)b);}
static int32_t sar12(int32_t v){return s32(((uint32_t)v>>12)|(v<0?0xfff00000u:0));}
static int run(SmN64DamageState *s,uint16_t clip,const uint16_t *c,size_t n){
    if(!c||clip>=n||!c[clip]||c[clip]>INT16_MAX)return -1;
    smn64_anim_run(&s->anim,clip,c[clip],0,-1);return 1;
}
static void release(SmN64DamageState *s,SmN64DamageEvent *e){e->release_web=s->web_graphic;s->web_graphic=0;}
static int die(SmN64DamageState *s,const uint16_t *c,size_t n,SmN64DamageEvent *e){
    uint32_t old=s->state;int body_fall=0;s->health=0;e->died=1;
    if(s->turn_lock){
        s->turn_lock=0;s->state=0x800000;if(run(s,176,c,n)<0)return -1;
        e->release_swing=s->swing_graphic;s->swing_graphic=0;e->unlock_camera=1;return 1;
    }
    if(s->surface_mode&&(s->wall||s->ceiling)){
        e->exit_aim=s->aiming;s->aiming=0;s->surface_mode=0;s->state=0x800000;
        s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;s->movement_blocked=1;
        e->align_normal=1;e->alignment_forced=1;
        if(s->wall){memcpy(e->alignment_forward,s->up,sizeof(s->up));s->wall=0;}
        else {memcpy(e->alignment_forward,s->forward,sizeof(s->forward));s->ceiling=0;}
        body_fall=1;
    }
    if(body_fall)return run(s,176,c,n);
    if(old==2||old==4){if(s->anim.animation==176)return 1;if(run(s,176,c,n)<0)return -1;s->state=4;return 1;}
    if(old==0x80)return 1;
    memset(s->velocity,0,sizeof(s->velocity));s->state=0x80;
    if(old==1||old==8||old==16||old==64||old==0x800000){
        if(s->anim.animation==176||s->anim.animation==178){if(run(s,182,c,n)<0)return -1;e->extra_sound=9;}
        else if(run(s,171,c,n)<0)return -1;
    }else if(run(s,171,c,n)<0)return -1;
    e->sound=0x24;return 1;
}
int smn64_player_die(SmN64DamageState *s,const uint16_t *c,size_t n,SmN64DamageEvent *e){
    if(!s||!e)return -1;
    memset(e,0,sizeof(*e));return die(s,c,n,e);
}
int smn64_player_damage(SmN64DamageState *s,const SmN64CombatHit *hit,uint32_t tick,
    uint32_t rng[3],const uint16_t *c,size_t n,const SmN64DamageHost *h,SmN64DamageEvent *e){
    unsigned i;int rc;uint32_t kind,flags;
    if(!s||!hit||!rng||!e)return -1;
    memset(e,0,sizeof(*e));kind=hit->kind;flags=hit->flags;
    if(s->suit==3||s->immune||s->input_disabled||s->scripted||s->invulnerable_ticks||(s->state&0x20000080u))return 0;
    if((s->state&0x800000u)&&s->anim.animation!=285)return 0;
    if((flags&2)&&kind==10&&s->state==0x800){e->sound=0x11;return 0;}
    if((s->state&0x10000000u)&&(!(flags&2)||kind!=17))return 0;
    s->display_timer=240;s->display_value=(s->combo_metric<<10)+0x2c00;
    e->stop_trails=1;s->damage_tick=tick;s->prior_damage_state=s->state;
    e->drop_actor=s->held_actor;s->held_actor=0;
    if(flags&4){
        if(s->armor_active){
            s->armor=s32((uint32_t)s->armor-hit->damage);
            if(s->armor<0){
                s->health=s16((uint16_t)((uint16_t)s->health+(uint16_t)s->armor));
                e->clear_armor_ui=1;if(s->armor_model_attached){e->restore_armor_model=1;s->armor_active=0;s->armor_model_attached=0;}s->armor=0;
            }
        }else s->health=s16((uint16_t)((uint16_t)s->health-hit->damage));
        if((flags&2)&&kind==16)return 1;
        if(s->last_voice_tick+30u<tick){e->voice=smn64_web_random(rng,3)+0x12;s->last_voice_tick=tick;}
        if(s->health<=0){e->exit_aim=s->aiming;s->aiming=0;release(s,e);return die(s,c,n,e);}
    }
    if((flags&2)&&kind==26){s->stun=1;s->stun_ticks=120;e->stun_effect=1;e->rumble_kind=2;}
    else e->rumble_kind=1;
    if((s->state&0x1000000u)&&!s->air_landing_wait)return 1;
    if(s->state&0x10043606u)return 1;
    if(run(s,s->surface_mode?181:170,c,n)<0)return -1;
    if(flags&2){
        if(!s->surface_mode&&kind==10){if(run(s,187,c,n)<0)return -1;}
        else if(kind==8&&(flags&8)){
            int32_t to[3],factor=mul(s->elapsed_ticks,48);
            if(!h||!h->line_clear)return -2;
            memcpy(to,s->position,sizeof(to));to[0]=s32((uint32_t)to[0]+(uint32_t)mul(hit->direction[0],factor));to[2]=s32((uint32_t)to[2]+(uint32_t)mul(hit->direction[2],factor));
            rc=h->line_clear(h->context,s->position,to);if(rc<0)return -2;
            if(rc){s->velocity[0]=mul(hit->direction[0],48);s->velocity[2]=mul(hit->direction[2],48);}
        }else if(kind==9||kind==14||kind==11||kind==15||kind==26){
            e->align_normal=1;
            if(s->wall||s->ceiling){
                s->wall=0;s->ceiling=0;s->surface_mode=0;s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;
                s->forward[1]=0;smn64_combat_normalize(s->forward,s->forward);memcpy(e->alignment_forward,s->forward,sizeof(s->forward));e->alignment_forced=1;
                if(run(s,176,c,n)<0)return -1;
            }else {
                for(i=0;i<3;i++)e->alignment_forward[i]=i==1?0:sar12(hit->direction[i]);
                smn64_combat_normalize(e->alignment_forward,e->alignment_forward);e->alignment_forced=1;
                if(run(s,172,c,n)<0)return -1;
                if(kind==26){s->velocity[0]=mul(hit->direction[0],48);s->velocity[2]=mul(hit->direction[2],48);}
            }
        }else if(kind==12){
            if(s->wall||s->ceiling){s->wall=0;s->ceiling=0;s->surface_mode=0;s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;e->align_normal=1;}
            s->velocity[0]=0;s->velocity[2]=0;s->velocity[1]=-524288;if(run(s,176,c,n)<0)return -1;
        }
    }
    s->state=0x800000;release(s,e);return 1;
}
int smn64_death_wait(int32_t *wait_ticks,uint8_t finished,int32_t elapsed){
    int requested;if(!wait_ticks)return -1;
    if(!finished){*wait_ticks=0;return 0;}
    requested=*wait_ticks>=120;*wait_ticks=s32((uint32_t)*wait_ticks+(uint32_t)elapsed);return requested;
}
