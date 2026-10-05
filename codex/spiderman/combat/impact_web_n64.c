#include "impact_web_n64.h"
#include "../movement/locomotion_n64.h"
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int16_t s16(uint16_t v){return v<=INT16_MAX?(int16_t)v:(int16_t)(-1-(int16_t)(uint16_t)~v);}
static int32_t sar12(int32_t v){return s32(((uint32_t)v>>12)|(v<0?0xfff00000u:0));}
static int32_t mul(int32_t a,int32_t b){return s32((uint32_t)a*(uint32_t)b);}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
int smn64_impact_web_init(SmN64ImpactWeb *s,const int32_t muzzle[3],const int16_t angles[3],
    int32_t speed,uint16_t damage,uint16_t life,uint32_t special,uint32_t tick,
    uint8_t has_player,uint8_t suit,int32_t difficulty,uint32_t rng[3],const SmN64ImpactHost *h){
    SmN64ImpactWeb v;SmN64ImpactWorldHit hit;int32_t proposed[3],planar;unsigned i;int rc;
    if(!s||!muzzle||!angles||!rng||!h||!h->world_trace||speed<=0)return -1;
    memset(&v,0,sizeof(v));memset(&hit,0,sizeof(hit));memcpy(v.position,muzzle,sizeof(v.position));
    v.damage=has_player?(uint32_t)smn64_damage_scaled(damage,suit,difficulty):damage;
    v.previous_tick=tick;v.life=life;v.size=0;v.special_actor=special;v.alive=1;
    planar=sar12(mul(speed,smn64_locomotion_cos(angles[0]&4095)));
    v.velocity[0]=s32(0u-(uint32_t)mul(planar,smn64_locomotion_sin(angles[1]&4095)));
    v.velocity[1]=mul(speed,smn64_locomotion_sin(angles[0]&4095));
    v.velocity[2]=s32(0u-(uint32_t)mul(planar,smn64_locomotion_cos(angles[1]&4095)));
    for(i=0;i<3;i++)proposed[i]=add(muzzle[i],mul(v.velocity[i],life));
    rc=h->world_trace(h->context,muzzle,proposed,&hit);if(rc<0)return -2;
    if(hit.hit){
        v.world_hit=1;memcpy(v.world_position,hit.position,sizeof(v.world_position));
        memcpy(v.world_normal,hit.normal,sizeof(v.world_normal));v.life=(uint16_t)(hit.distance/speed);
    }
    if(v.life>life)v.life=life;
    v.rotation_step=smn64_web_random(rng,2)?768:-768;
    *s=v;return 1;
}
int smn64_impact_web_tick(SmN64ImpactWeb *s,uint32_t tick,const SmN64ImpactHost *h,SmN64ImpactEvent *e){
    int32_t old[3],direction[3];uint32_t elapsed,actor=0;unsigned i;int rc;
    if(!s||!e||!h||!h->actor_sweep||!h->apply||!h->special_active)return -1;
    memset(e,0,sizeof(*e));if(!s->alive)return 0;
    s->size=s16((uint16_t)((uint16_t)s->size+150u));if(s->size>500)s->size=500;
    s->rotation=s16((uint16_t)((uint16_t)s->rotation+(uint16_t)s->rotation_step));
    memcpy(old,s->position,sizeof(old));elapsed=tick-s->previous_tick;s->previous_tick=tick;
    for(i=0;i<3;i++)s->position[i]=add(s->position[i],s32((uint32_t)s->velocity[i]*elapsed));
    rc=h->actor_sweep(h->context,old,s->position,&actor);if(rc<0)return -2;
    if(rc){if(!actor)return -2;e->reason=2;e->actor=actor;}
    else {
        s->age=s16((uint16_t)((uint16_t)s->age+elapsed));
        if(s->age>=s->life)e->reason=s->world_hit?3:1;
    }
    if(!e->reason)return 1;
    memcpy(e->burst_position,old,sizeof(old));
    if(e->reason==2){
        SmN64CombatHit hit;memset(&hit,0,sizeof(hit));hit.actor=actor;hit.kind=6;hit.flags=0x1e;
        hit.damage=(uint16_t)s->damage;hit.impulse=300;hit.duration=12;
        direction[0]=sar12(s->velocity[0]);direction[1]=0;direction[2]=sar12(s->velocity[2]);
        smn64_combat_normalize(direction,hit.direction);memcpy(hit.position,s->position,sizeof(hit.position));
        rc=h->apply(h->context,&hit);if(rc<0)return -2;e->sound=0x10;
    } else if(e->reason==3){
        e->sound=0x10;e->spawn_world_decal=1;memcpy(e->decal_normal,s->world_normal,sizeof(e->decal_normal));
        for(i=0;i<3;i++)e->burst_position[i]=add(s->world_position[i],10*(int32_t)s->world_normal[i]);
    }
    if(e->reason==2||e->reason==3){
        if(s->special_actor){rc=h->special_active(h->context,s->special_actor);if(rc<0)return -2;if(rc)e->activate_special=s->special_actor;}
    }
    e->spawn_burst=1;s->alive=0;return 0;
}
