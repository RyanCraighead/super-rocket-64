#include "owner_n64.h"
#include <string.h>
int smn64_web_owner_after_animation(SmN64WebRuntime *s,SmN64WebOwner *o,int32_t dt){
    if(!s||!o||dt<1||dt>6)return -1;
    s->player.anim.elapsed_ticks=dt;
    smn64_web_resource_tick(&s->player.resource,o->health,dt,s->now,&o->refill_tick);
    if(s->player.swinger_present)smn64_swinger_step(&s->swinger,s->now);
    s->player.anim.rate=65536;o->previous_velocity_y=s->player.velocity[1];
    s->collision=0;s->side.hit=0;s->platform_present=0;
    return 1;
}
int smn64_web_owner_begin(SmN64WebRuntime *s,SmN64WebOwner *o,int32_t dt){
    if(!s||!o||dt<1||dt>6)return -1;
    s->player.anim.elapsed_ticks=dt;smn64_anim_advance(&s->player.anim);
    return smn64_web_owner_after_animation(s,o,dt);
}
int smn64_web_owner_basis_drag(SmN64WebRuntime *out,SmN64WebOwner *owner){
    if(!out||!owner)return -1;
    SmN64WebRuntime s=*out;SmN64WebPlayer *p=&s.player;
    memcpy(s.basis.normal,p->normal,sizeof(p->normal));
    memcpy(s.basis.forward,p->forward,sizeof(p->forward));memcpy(s.basis.right,p->right,sizeof(p->right));memcpy(s.basis.outward,p->outward,sizeof(p->outward));
    const int32_t *f=p->normal[1]>=3401?s.retained_forward:NULL;
    if(!smn64_climb_basis(&s.basis,f))return -5;
    memcpy(p->forward,s.basis.forward,sizeof(p->forward));memcpy(p->right,s.basis.right,sizeof(p->right));memcpy(p->outward,s.basis.outward,sizeof(p->outward));
    p->ceiling_orientation=p->normal[1]>=3401;p->wall_orientation=!p->ceiling_orientation&&p->normal[1]>=-2600;
    uint8_t d;
    if(p->adhered)d=1;
    else if(p->state&0x1000000u)d=31;
    else if(p->state&6u)d=4;
    else if(p->state&0x40000u)d=1;
    else{owner->drag[0]=owner->drag[2]=1;owner->drag[1]=4;*out=s;return 1;}
    owner->drag[0]=owner->drag[1]=owner->drag[2]=d;*out=s;return 1;
}
int smn64_web_owner_successor(SmN64WebRuntime *s,const uint16_t *c,size_t n){
    if(!s||!c)return -1;
    if(!s->player.anim.finished)return 65535;
    uint16_t old=s->player.anim.animation,next;
    switch(old){case 270:next=271;break;case 276:case 278:next=226;break;case 281:next=19;break;default:return 65535;}
    if(next>=n||!c[next])return -1;
    smn64_anim_run(&s->player.anim,next,c[next],0,-1);return old;
}
void smn64_web_owner_acceleration(const SmN64WebRuntime *s,SmN64WebOwner *o){
    o->acceleration[0]=o->acceleration[2]=0;
    o->acceleration[1]=(s->player.adhered||s->player.anim.animation==277)?0:40960;
}
