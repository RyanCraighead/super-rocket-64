#include "throwable_n64.h"
#include "../web/resource_n64.h"
#include <string.h>

static int32_t s32(uint32_t v) {
    return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;
}
static int16_t s16(uint16_t v) {
    return v<=INT16_MAX?(int16_t)v:(int16_t)(-1-(int16_t)(uint16_t)~v);
}
static int32_t add(int32_t a,int32_t b) { return s32((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return s32((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,uint32_t b) { return s32((uint32_t)a*b); }
static int32_t sar(int32_t v,unsigned n) {
    n&=31u;
    return n?s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0)):v;
}
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
}
int smn64_throwable_types_load(SmN64ThrowableTypes *out,const void *boot,size_t bytes) {
    SmN64ThrowableTypes types;const uint8_t *p=boot;unsigned i;
    if(!out||!boot||bytes!=995056u)return -1;
    memset(&types,0,sizeof(types));
    for(i=0;i<21;i++) {
        uint32_t target=be32(p+(0x20650u-0x16ae0u)+4*i);
        if(target!=0x8007f28cu&&target!=0x8007f350u)return -1;
        types.damage[i]=(uint8_t)(target==0x8007f28cu);
    }
    types.loaded=1;*out=types;return 1;
}

int smn64_throwable_tick(SmN64Throwable *s,const SmN64ThrowableTypes *types,
    uint32_t rng[3],const SmN64ThrowableHost *h,SmN64ThrowableEvent *e) {
    int32_t old[3],delta[3];SmN64ThrowableActor actor;
    SmN64ThrowableWorldHit world;unsigned j;int32_t tick;int rc;
    if(!s||!types||types->loaded!=1||!rng||!h||!e||!s->id)return -1;
    memset(e,0,sizeof(*e));
    if(s->destroyed)return 0;
    if(!(s->flags_10c&1u))return 1;
    if(!h->world_sweep||!h->apply||!h->first_world_impact||!h->impact||
       !h->destroy||(!(s->flags_10c&16u)&&!h->actor_sweep))return -1;
    if(s->path_handle&&(!s->path||s->path_count<2||
       s->path_count>s->path_capacity||!h->release_path))return -1;
    memcpy(old,s->position,sizeof(old));
    if(s->path_handle) {
        s->path_index+=(uint32_t)s->elapsed;
        if(s->path_index<s->path_count) {
            memcpy(s->position,s->path[s->path_index],sizeof(s->position));
        } else {
            uint32_t last=s->path_count-1;
            uint32_t beyond=s->path_index-s->path_count+1;
            for(j=0;j<3;j++) {
                s->velocity[j]=sub(s->path[last][j],s->path[last-1][j]);
                s->position[j]=add(s->path[last][j],mul(s->velocity[j],beyond));
            }
            if(h->release_path(h->context,s->path_handle)!=1)return -2;
            e->path_released=s->path_handle;s->path_handle=0;s->path=NULL;
        }
    } else {
        for(tick=0;tick<s->elapsed;tick++)for(j=0;j<3;j++) {
            int32_t v=add(s->velocity[j],s->acceleration[j]);
            s->velocity[j]=sub(v,sar(v,s->drag[j]));
            s->position[j]=add(s->position[j],s->velocity[j]);
        }
    }
    /* Addition modulo 65536 is equivalent to the original positive-tick loop. */
    if(s->elapsed>0)for(j=0;j<3;j++)
        s->angles[j]=s16((uint16_t)((uint16_t)s->angles[j]+
            (uint32_t)(uint16_t)s->spin[j]*(uint32_t)s->elapsed));
    for(j=0;j<3;j++)delta[j]=sar(sub(s->position[j],old[j]),12);
    smn64_combat_normalize(delta,e->direction);
    for(j=0;j<3;j++)e->sweep_end[j]=add(s->position[j],mul(e->direction[j],128));
    memset(&actor,0,sizeof(actor));
    if(!(s->flags_10c&16u)) {
        rc=h->actor_sweep(h->context,old,e->sweep_end,6144,&actor);
        if(rc<0||rc>1||(rc==1&&!actor.id))return -2;
        if(rc==1&&actor.type>=0x130&&actor.type<=0x144&&
           types->damage[actor.type-0x130]) {
            SmN64CombatHit hit;int32_t flat[3];int large=(s->flags_10c&8u)!=0;
            memset(&hit,0,sizeof(hit));hit.actor=actor.id;hit.kind=23;hit.flags=0x1e;
            hit.damage=(uint16_t)(large?100:50);hit.impulse=(uint16_t)(large?500:350);
            hit.duration=(uint16_t)(large?15:8);
            flat[0]=e->direction[0];flat[1]=0;flat[2]=e->direction[2];
            smn64_combat_normalize(flat,hit.direction);
            rc=h->apply(h->context,&hit);if(rc<0||rc>1)return -2;
            e->contact=1;e->actor=actor.id;
            for(j=0;j<3;j++)e->normal[j]=s16((uint16_t)(0u-(uint32_t)e->direction[j]));
        }
    }
    if(!e->contact) {
        memset(&world,0,sizeof(world));
        if(h->world_sweep(h->context,s->id,old,e->sweep_end,&world)!=1)return -2;
        if(!world.hit)return 1;
        e->contact=2;memcpy(e->normal,world.normal,sizeof(e->normal));
        for(j=0;j<3;j++)s->position[j]=sub(world.position[j],mul(e->direction[j],128));
        if(s->flags_10c&8u)s->impact_count=(uint16_t)(s->impact_count+1u);
        s->spin[0]=s16((uint16_t)(smn64_web_random(rng,128)-64u));
        s->spin[1]=s16((uint16_t)(smn64_web_random(rng,(uint32_t)(int32_t)s->spin[1]*2u)-
                                     (uint16_t)s->spin[1]));
        if(s->impact_count==1&&h->first_world_impact(h->context,s)!=1)return -2;
    }
    if(h->impact(h->context,s,e->normal)!=1)return -2;
    if(h->destroy(h->context,s->id)!=1)return -2;
    s->destroyed=1;return 0;
}
