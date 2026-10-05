#include "dome_pulse_n64.h"
#include "../web/resource_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t sar12(int32_t v){return s32(((uint32_t)v>>12)|(v<0?0xfff00000u:0));}
static int32_t distance(const int32_t a[3],const int32_t b[3]){uint32_t sq=0;unsigned i;for(i=0;i<3;i++){int32_t x=sar12(sub(a[i],b[i]));sq+=(uint32_t)x*(uint32_t)x;}return (int32_t)sqrtf((float)sq);}
int smn64_dome_pulse_init(SmN64DomePulse *s,const int32_t pos[3],uint32_t type,uint32_t rng[3],const SmN64DomeHost *h){
    unsigned list,i;int rc;SmN64DomeActor a;
    if(!s||!pos||!rng||!h||!h->actor_at||!h->mark_hit)return -1;
    memset(s,0,sizeof(*s));memcpy(s->position,pos,sizeof(s->position));s->position[1]=s32((uint32_t)s->position[1]+204800u);s->web_type=type;s->alive=1;
    for(i=0;i<3;i++)s->fade[i]=240;
    for(list=0;list<2;list++)for(i=0;;i++){rc=h->actor_at(h->context,list,i,&a);if(rc<0)return -2;if(!rc)break;if(!a.id||h->mark_hit(h->context,a.id,0)!=1)return -2;}
    for(i=0;i<16;i++)s->rotations[i]=(uint16_t)smn64_web_random(rng,4096);
    return 1;
}
int smn64_dome_pulse_tick(SmN64DomePulse *s,const SmN64DomeHost *h){
    unsigned list,i,j;int rc;SmN64DomeActor a;
    if(!s||!h||!h->actor_at||!h->mark_hit||!h->apply||!h->effect)return -1;
    if(!s->alive)return 0;
    s->radius=s32((uint32_t)s->radius+140u);if(s->radius>2000)s->growth=s32((uint32_t)s->growth+100u);
    for(j=0;j<3;j++)s->fade[j]=s32((uint32_t)s->fade[j]-16u);
    if(s->fade[0]<=0&&s->fade[1]<=0&&s->fade[2]<=0)s->alive=0;
    for(list=0;list<2;list++)for(i=0;;i++){
        int32_t delta_y,dir[3];SmN64CombatHit hit;
        rc=h->actor_at(h->context,list,i,&a);if(rc<0)return -2;if(!rc)break;if(!a.id)return -2;
        if((a.flags&0x110)!=0x10||a.type==0x191)continue;
        delta_y=sar12(sub(s->position[1],a.position[1]));
        if((uint32_t)delta_y+299u>=599u||distance(s->position,a.position)>s->radius)continue;
        if(s->web_type&&a.type==0x144){if(h->effect(h->context,2,a.id)!=1)return -2;}
        else if(list==0&&!h->effect_pool_unavailable){if(h->effect(h->context,1,a.id)!=1)return -2;}
        memset(&hit,0,sizeof(hit));hit.actor=a.id;hit.flags=0x1e;hit.kind=s->web_type?22:21;hit.impulse=512;hit.duration=15;
        hit.damage=s->radius>500?0:(uint16_t)(200-s32((uint32_t)s->radius*200u)/500);
        for(j=0;j<3;j++)dir[j]=j==1?0:sar12(sub(a.position[j],s->position[j]));
        smn64_combat_normalize(dir,hit.direction);memcpy(hit.position,a.position,sizeof(hit.position));
        if(h->apply(h->context,&hit)<0||h->mark_hit(h->context,a.id,1)!=1)return -2;
    }
    return s->alive?1:0;
}
