#include "entry_n64.h"
#include "../movement/locomotion_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t sar12(int32_t v){return s32(((uint32_t)v>>12)|(v<0?0xfff00000u:0));}
static int32_t distance(const int32_t a[3],const int32_t b[3]){uint32_t sq=0;unsigned i;for(i=0;i<3;i++){int32_t x=sar12(sub(a[i],b[i]));sq+=(uint32_t)x*(uint32_t)x;}return (int32_t)sqrtf((float)sq);}
static int run(SmN64CombatEntry *s,uint16_t a,const uint16_t *c,size_t n){if(!c||a>=n||!c[a]||c[a]>INT16_MAX)return -1;smn64_anim_run(&s->anim,a,c[a],0,-1);return 1;}
int smn64_combat_combo_target(const int32_t position[3],const SmN64EntryHost *h,SmN64EntryEvent *e){
    SmN64EntryActor a,target;size_t i;int rc;int32_t best;
    if(!position||!h||!h->actor_at||!h->line_clear||!e)return -1;
    e->face_actor=0;e->immediate_face=0;e->heading=0;
    best=0;memset(&target,0,sizeof(target));
    for(i=0;;i++){
        int32_t score;rc=h->actor_at(h->context,0,i,&a);if(rc<0)return -2;if(!rc)break;
        if(!a.field_dc||(a.flags_4a&0x50)!=0x10||a.cached_distance>=1024)continue;
        score=s32((uint32_t)sub(1024,a.cached_distance)<<12)/1024;
        /* Original low-word multiply4096 then shift12 retains wrap. */
        score=sar12(s32((uint32_t)score*4096u));if(score<=best)continue;
        rc=h->line_clear(h->context,position,a.position);if(rc<0)return -2;
        if(rc){best=score;target=a;}
    }
    if(target.id){int32_t dx=sar12(sub(target.position[0],position[0])),dz=sar12(sub(target.position[2],position[2]));e->face_actor=target.id;e->immediate_face=1;e->heading=(uint32_t)(1024-smn64_locomotion_atan(-dz,-dx))&4095;}
    return 1;
}
int smn64_combat_entry(SmN64CombatEntry *s,SmN64Combo *combo,const SmN64CombatBank *bank,
    SmN64CombatInput *input,uint32_t tick,const uint16_t *counts,size_t n,
    const SmN64EntryHost *h,SmN64EntryEvent *e){
    SmN64EntryActor a;size_t i;int rc,kick;int32_t best=512;uint32_t closest=0;
    if(!s||!combo||!bank||!input||!e||!h||!h->actor_at||!h->actor_by_id||!h->line_clear||!h->pickup_trace||!h->pickup_eligible||!h->interactable)return -1;
    memset(e,0,sizeof(*e));if(!(input->pressed&6)||s->surface_mode||s->aiming)return 0;
    kick=!!(input->pressed&2);input->pressed&=~6u;
    if(s->held_actor){
        if(kick)return 0;
        rc=h->actor_by_id(h->context,s->held_actor,&a);if(rc<0||a.id!=s->held_actor)return -2;
        if(run(s,(a.object_flags_10c&8)?201:195,counts,n)<0)return -1;
        s->state=0x200000;e->throw_requested=s->held_actor;e->face_actor=s->target_actor;return 1;
    }
    s->nearby_actor=0;
    for(i=0;;i++){
        int32_t d;rc=h->actor_at(h->context,0,i,&a);if(rc<0)return -2;if(!rc)break;if(!a.id)return -2;
        d=distance(s->position,a.position);
        if(d<best&&a.type!=0x131&&a.type!=0x13c){best=d;closest=a.id;}
    }
    s->nearby_actor=closest;
    if(!kick&&!closest&&!s->pickup_disabled){
        s->pickup_actor=0;
        for(i=0;;i++){
            int32_t d;uint32_t dot=0;unsigned j;SmN64PickupTrace hit;
            rc=h->actor_at(h->context,1,i,&a);if(rc<0)return -2;if(!rc)break;if(!a.id)return -2;
            if(a.type!=0x191)continue;
            d=distance(s->position,a.position);if(d>=768)continue;
            for(j=0;j<3;j++)dot+=(uint32_t)sar12(sub(s->position[j],a.position[j]))*(uint32_t)s->forward[j];
            if(s32(dot)<=0||d>=350)continue;
            memset(&hit,0,sizeof(hit));rc=h->pickup_trace(h->context,s->position,a.position,&hit);if(rc<0)return -2;
            rc=h->pickup_eligible(h->context,a.id);if(rc<0)return -2;
            if(!rc||!hit.hit||hit.distance>=257)continue;
            s->pickup_actor=a.id;
            if(run(s,(a.object_flags_10c&8)?196:190,counts,n)<0)return -1;
            s->state=0x100000;e->pickup_requested=a.id;return 1;
        }
    }
    if(!closest){uint32_t id=0;rc=h->interactable(h->context,&id);if(rc<0)return -2;s->interactable_actor=id;if(rc){if(!id)return -2;if(run(s,37,counts,n)<0)return -1;s->state=0x80000000u;s->phase=0;e->interact_requested=id;return 1;}}
    s->state=0x800;s->entry_tick=tick;
    rc=smn64_combat_combo_target(s->position,h,e);if(rc<0)return rc;
    input->pressed=0;combo->anim=s->anim;rc=smn64_combo_begin(combo,bank,(uint16_t)kick,tick,0,counts,n);if(rc<0)return rc;s->anim=combo->anim;e->began_combo=1;return 1;
}
