#include "air_attack_n64.h"
#include <string.h>
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sar12(int32_t x){return s32(((uint32_t)x>>12)|(x<0?0xfff00000u:0));}
static int run(SmN64AirAttack *s,uint16_t clip,const uint16_t *c,size_t n){
    if(!c||clip>=n||!c[clip]||c[clip]>INT16_MAX)return -1;
    smn64_anim_run(&s->anim,clip,c[clip],0,-1);return 1;
}
int smn64_air_attack_request(SmN64AirAttack *s,uint32_t target_id,uint16_t type,
    const int32_t target[3],uint8_t punch,uint8_t kick,uint32_t tick,
    const uint16_t *counts,size_t n,SmN64AirEvent *e){
    int32_t d[3],norm[3];unsigned i;
    if(!s||!target||!e)return -1;
    memset(e,0,sizeof(*e));
    if(!target_id||(!punch&&!kick)||s->aiming||type==0x19c||type==0x149)return 0;
    for(i=0;i<3;i++)d[i]=sar12(sub(target[i],s->position[i]));
    smn64_combat_normalize(d,norm);if(norm[1]<=1024)return 0;
    if(run(s,punch?133:129,counts,n)<0)return -1;
    s->target_id=target_id;memcpy(s->target_position,target,sizeof(s->target_position));
    s->turn_a=0;s->turn_b=0;s->landing_wait=0;s->state=0x1000000;
    s->previous_attack_tick=s->attack_tick;s->attack_tick=tick;s->hit_done=0;
    e->detach_swing=1;e->face_target=1;e->start_trails=1;return 1;
}
int smn64_air_attack_tick(SmN64AirAttack *s,uint8_t grounded,const uint16_t *counts,size_t n,SmN64AirEvent *e){
    int32_t d[3],norm[3],back[3];unsigned i;
    if(!s||!e||s->state!=0x1000000)return -1;
    memset(e,0,sizeof(*e));
    if(s->landing_wait){
        if(!s->anim.animation){s->landing_wait=0;s->movement_enabled=1;s->movement_blocked=0;s->state=1;return 0;}
        return 1;
    }
    if((s->anim.animation==129||s->anim.animation==133)&&s->anim.finished)e->start_trails=1;
    for(i=0;i<3;i++)d[i]=sar12(sub(s->target_position[i],s->position[i]));
    if(grounded||d[1]<=0){
        s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;e->align_normal=1;e->stop_trails=1;
        memset(s->velocity,0,sizeof(s->velocity));
        if(run(s,s->anim.animation==129?130:134,counts,n)<0)return -1;
        if(s32(s->attack_tick-s->previous_attack_tick)<120)s->landing_wait=666;
        else {s->landing_wait=0;s->movement_enabled=1;s->movement_blocked=0;s->state=1;return 0;}
        return 1;
    }
    back[0]=sub(0,d[0]);back[1]=0;back[2]=sub(0,d[2]);
    smn64_combat_normalize(d,norm);smn64_combat_normalize(back,s->horizontal_back);
    for(i=0;i<3;i++)s->normal[i]=sub(0,norm[i]);
    e->align_normal=1;
    if(!s->hit_done)for(i=0;i<3;i++)s->velocity[i]=s32((uint32_t)norm[i]*96u);
    else {s->velocity[0]=0;s->velocity[1]=196608;s->velocity[2]=0;}
    return 1;
}
int smn64_air_attack_contacts(SmN64AirAttack *s,const SmN64CombatBank *bank,uint8_t suit,int32_t difficulty,const SmN64AirHost *h){
    int32_t from[3],to[3],contact[3];uint32_t skip=0,id;uint16_t type;unsigned i,queries=0;int rc;
    if(!s||!bank||!h||!h->body_sweep||!h->apply)return -1;
    if(!(s->state&0x1000000)||s->hit_done||!memcmp(s->position,s->previous_position,sizeof(s->position)))return 0;
    for(i=0;i<3;i++){from[i]=s->previous_position[i];to[i]=add(s->position[i],sub(s->position[i],s->previous_position[i]));}
    from[1]=s->position[1];
    for(;;){
        SmN64CombatHit hit;int32_t direction[3];uint8_t policy;
        rc=h->body_sweep(h->context,from,to,4096,skip,&id,&type,contact);
        if(rc<=0)return rc;
        if(!id||id==skip||++queries>65536)return -2;
        policy=type>=0x130&&type<=0x144?bank->air_types[type-0x130]:0;
        if(policy==2){memcpy(from,contact,sizeof(from));skip=id;continue;}
        if(policy!=1)return 0;
        memset(&hit,0,sizeof(hit));hit.actor=id;hit.kind=2;hit.flags=0x1e;
        hit.damage=(uint16_t)smn64_damage_scaled(20,suit,difficulty);hit.impulse=512;hit.duration=15;
        for(i=0;i<3;i++)direction[i]=i==1?0:sub(0,s->forward[i]);
        smn64_combat_normalize(direction,hit.direction);memcpy(hit.position,contact,sizeof(contact));
        rc=h->apply(h->context,&hit);if(rc<0)return -2;
        s->velocity[0]=0;s->velocity[2]=0;s->hit_done=1;return 1;
    }
}
