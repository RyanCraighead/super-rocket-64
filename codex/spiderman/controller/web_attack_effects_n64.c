#include "web_attack_effects_n64.h"
#include "../graphics/allocation_order_n64.h"
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return si((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return si((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return si((uint32_t)a*(uint32_t)b);}
static int32_t sar12(int32_t x){return si(((uint32_t)x>>12)|(x<0?0xfff00000u:0));}
static int16_t sh(uint32_t x){x&=65535u;return x<=32767?(int16_t)x:(int16_t)(-1-(int32_t)(65535u-x));}
int smn64_impact_burst_init(SmN64ImpactBurst *s,const int32_t p[3]){
    if(!s||!p)return -1;
    memset(s,0,sizeof *s);memcpy(s->position,p,sizeof s->position);
    s->rgb[0]=200;s->rgb[1]=210;s->rgb[2]=255;s->alive=1;return 1;
}
int smn64_impact_burst_tick(SmN64ImpactBurst *s){
    if(!s)return -1;
    if(!s->alive)return 0;
    s->size=sh((uint16_t)s->size+316u);if(s->size>1608)s->alive=0;
    for(unsigned i=0;i<3;i++)s->rgb[i]=s->rgb[i]<23?0:(uint8_t)(s->rgb[i]-23);
    return s->alive;
}
int smn64_impact_decal_init(SmN64ImpactDecal *s,const int32_t p[3],const int16_t normal[3]){
    int32_t n[3],u[3],v[3];uint32_t mag[3];
    if(!s||!p||!normal)return -1;
    memset(s,0,sizeof *s);
    for(unsigned i=0;i<3;i++){n[i]=normal[i];mag[i]=n[i]<0?0u-(uint32_t)n[i]:(uint32_t)n[i];}
    if(mag[0]<=mag[1]&&mag[0]<=mag[2]){u[0]=0;u[1]=-n[2];u[2]=n[1];}
    else if(mag[1]<=mag[0]&&mag[1]<=mag[2]){u[0]=n[2];u[1]=0;u[2]=-n[0];}
    else{u[0]=-n[1];u[1]=n[0];u[2]=0;}
    for(unsigned i=0;i<3;i++){unsigned j=(i+1)%3,k=(i+2)%3;v[i]=sar12(sub(mul(n[j],u[k]),mul(n[k],u[j])));}
    for(unsigned i=0;i<3;i++){
        int32_t a=mul(u[i],50),b=mul(v[i],50);
        s->corners[0][i]=sub(sub(p[i],a),b);s->corners[1][i]=sub(add(p[i],a),b);
        s->corners[2][i]=add(sub(p[i],a),b);s->corners[3][i]=add(add(p[i],a),b);
        s->rgb[i]=22;
    }
    s->alive=1;return 1;
}
int smn64_impact_decal_tick(SmN64ImpactDecal *s){
    if(!s)return -1;
    if(!s->alive)return 0;
    if(!s->phase)s->phase=1;
    else if(s->phase==1){s->age=sh((uint16_t)s->age+1u);if(s->age>=31)s->phase=2;}
    else if(s->phase==2){for(unsigned i=0;i<3;i++)s->rgb[i]=s->rgb[i]<3?0:(uint8_t)(s->rgb[i]-3);if(!(s->rgb[0]|s->rgb[1]|s->rgb[2]))s->alive=0;}
    return s->alive;
}
int smn64_impact_spark_init(SmN64ImpactSpark *s,const int32_t p[3],const int32_t forward[3],uint32_t tick,uint32_t rng[3]){
    if(!s||!p||!forward||!rng)return -1;
    memset(s,0,sizeof *s);memcpy(s->position,p,sizeof s->position);memcpy(s->previous,p,sizeof s->previous);
    for(unsigned i=0;i<3;i++)s->velocity[i]=mul(si(0u-(uint32_t)forward[i]),(int32_t)smn64_web_random(rng,3)+6);
    s->previous_tick=tick;s->life=30;s->alive=1;return 1;
}
int smn64_impact_spark_tick(SmN64ImpactSpark *s,uint32_t tick){
    if(!s)return -1;
    if(!s->alive)return 0;
    uint32_t elapsed=tick-s->previous_tick;
    if(elapsed>s->life){s->alive=0;return 0;}
    s->life=(uint16_t)(s->life-elapsed);s->previous_tick=tick;
    memcpy(s->previous,s->position,sizeof s->position);s->velocity[1]=add(s->velocity[1],4096);
    for(unsigned i=0;i<3;i++)s->position[i]=add(s->position[i],si((uint32_t)s->velocity[i]*elapsed));
    return 1;
}
void smn64_web_attack_effects_init(SmN64WebAttackEffects *s){if(s){memset(s,0,sizeof *s);s->next_id=1;}}
static int alive(const SmN64WebAttackObject *o){
    switch(o->kind){case SMN64_ATTACK_PROJECTILE:return o->state.projectile.alive;
    case SMN64_ATTACK_BURST:return o->state.burst.alive;case SMN64_ATTACK_DECAL:return o->state.decal.alive;
    case SMN64_ATTACK_FRAGMENT:return o->state.fragment.alive;case SMN64_ATTACK_SPARK:return o->state.spark.alive;default:return 0;}
}
static SmN64WebAttackObject *alloc(SmN64WebAttackEffects *s,SmN64WebAttackKind kind){
    if(!s||s->count>=SMN64_WEB_ATTACK_CAPACITY||!s->next_id)return NULL;
    uint64_t serial;if(smn64_graphical_reserve(&s->graphical_clock,kind==SMN64_ATTACK_FRAGMENT?2u:1u,&serial)!=1)return NULL;
    memmove(s->objects+1,s->objects,s->count*sizeof s->objects[0]);s->count++;
    SmN64WebAttackObject *o=s->objects;memset(o,0,sizeof *o);o->id=s->next_id++;o->kind=kind;o->graphical_serial=serial;return o;
}
static SmN64WebAttackObject *lookup(SmN64WebAttackEffects *s,uint32_t id){
    for(uint32_t i=0;i<s->count;i++)if(s->objects[i].id==id)return&s->objects[i];
    return NULL;
}
int smn64_web_attack_fire(SmN64WebAttackEffects *s,const SmN64FireEvent *e,uint32_t tick,uint8_t suit,int32_t difficulty,uint32_t rng[3],const SmN64WebAttackHost *h){
    if(!s||!e||!rng||!h)return -1;
    if(e->update_graphic||e->attach_actor||e->release_graphic||e->actor_message||e->trap_samples||e->yank_mark||e->activate_special||e->special_web_type||e->rejected_yank_actor||e->impact_special)return -2;
    if(!e->spawn_impact)return e->result_flags==1?1:-2;
    if(!h->impact.world_trace||!h->impact.actor_sweep||!h->impact.apply||!h->impact.special_active||!h->fragment_floor||!h->sound)return -2;
    if(e->impact_damage!=50||e->impact_speed!=32||(e->impact_lifetime!=30&&e->impact_lifetime!=120))return -2;
    SmN64WebAttackObject *o=alloc(s,SMN64_ATTACK_PROJECTILE);if(!o)return -2;
    return smn64_impact_web_init(&o->state.projectile,e->muzzle,e->angles,32,50,(uint16_t)e->impact_lifetime,0,tick,1,suit,difficulty,rng,&h->impact);
}
int smn64_web_attack_sparks(SmN64WebAttackEffects *s,uint32_t count,const int32_t hand0[3],const int32_t hand1[3],const int32_t forward[3],uint32_t tick,uint32_t rng[3]){
    if(!s||!hand0||!hand1||!forward||!rng||(count!=2&&count!=4))return -1;
    for(unsigned hand=0;hand<(count==2?2u:1u);hand++)for(uint32_t i=0;i<count;i++){
        SmN64WebAttackObject *o=alloc(s,SMN64_ATTACK_SPARK);if(!o)return -2;
        if(smn64_impact_spark_init(&o->state.spark,hand?hand0:hand1,forward,tick,rng)!=1)return -2;
    }
    return 1;
}
static int termination(SmN64WebAttackEffects *s,const SmN64ImpactEvent *e,const int32_t sound_at[3],uint32_t rng[3],const SmN64WebAttackHost *h){
    if(e->activate_special)return -2;
    if(e->sound&&(!h->sound||h->sound(h->context,e->sound,sound_at)!=1))return -2;
    if(e->spawn_world_decal){SmN64WebAttackObject *o=alloc(s,SMN64_ATTACK_DECAL);if(!o||smn64_impact_decal_init(&o->state.decal,e->burst_position,e->decal_normal)!=1)return -2;}
    if(e->spawn_burst){SmN64WebAttackObject *o=alloc(s,SMN64_ATTACK_BURST);if(!o||smn64_impact_burst_init(&o->state.burst,e->burst_position)!=1)return -2;}
    if(e->reason==2){
        int32_t to[3],floor;SmN64ImpactWorldHit hit;memset(&hit,0,sizeof hit);memcpy(to,e->burst_position,sizeof to);to[1]=add(to[1],5000*4096);
        if(!h->fragment_floor||h->fragment_floor(h->context,e->burst_position,to,&hit)!=1)return -2;
        floor=hit.hit?hit.position[1]:to[1];
        /* Source loop creates all30 unless source global pool is full. This
         * supported host envelope requires actual room for all30 objects. */
        if(s->count>SMN64_WEB_ATTACK_CAPACITY-30u)return -2;
        for(unsigned n=0;n<30;n++){
            int32_t vertices[3][3];memcpy(vertices[0],e->burst_position,sizeof vertices[0]);
            for(unsigned j=1;j<3;j++)for(unsigned k=0;k<3;k++)vertices[j][k]=add(e->burst_position[k],((int32_t)smn64_web_random(rng,21)-10)*4096);
            SmN64WebAttackObject *o=alloc(s,SMN64_ATTACK_FRAGMENT);if(!o)return -2;
            if(smn64_web_debris_init(&o->state.fragment,(const int32_t (*)[3])vertices,e->burst_position,floor,25,1,rng)!=1)return -2;
        }
    }
    return 1;
}
static int effects_tick(SmN64WebAttackEffects *s,uint32_t tick,uint32_t rng[3],const SmN64WebAttackHost *h,SmN64WebAttackBetweenPasses between,void *context){
    uint32_t ids[SMN64_WEB_ATTACK_CAPACITY],n=0;
    if(!s||!rng||!h||s->count>SMN64_WEB_ATTACK_CAPACITY)return -1;
    for(uint32_t i=0;i<s->count;i++)if(s->objects[i].kind==SMN64_ATTACK_PROJECTILE||s->objects[i].kind==SMN64_ATTACK_BURST)ids[n++]=s->objects[i].id;
    for(uint32_t i=0;i<n;i++){
        SmN64WebAttackObject *o=lookup(s,ids[i]);if(!o||!alive(o))continue;
        if(o->kind==SMN64_ATTACK_BURST){if(smn64_impact_burst_tick(&o->state.burst)<0)return -2;}
        else{SmN64ImpactEvent e;int32_t at[3];int rc=smn64_impact_web_tick(&o->state.projectile,tick,&h->impact,&e);if(rc<0)return -2;memcpy(at,o->state.projectile.position,sizeof at);
            if(e.reason&&termination(s,&e,at,rng,h)!=1)return -2;}
    }
    if(between){uint64_t prior=s->graphical_clock;if(between(context,&s->graphical_clock)!=1||s->graphical_clock<prior)return -2;}
    for(uint32_t i=0;i<s->count;i++)if(s->objects[i].kind==SMN64_ATTACK_DECAL&&smn64_impact_decal_tick(&s->objects[i].state.decal)<0)return -2;
    for(uint32_t i=0;i<s->count;i++){
        SmN64WebAttackObject *o=&s->objects[i];
        if(o->kind==SMN64_ATTACK_FRAGMENT&&smn64_web_debris_tick(&o->state.fragment)<0)return -2;
        if(o->kind==SMN64_ATTACK_SPARK&&smn64_impact_spark_tick(&o->state.spark,tick)<0)return -2;
    }
    n=0;for(uint32_t i=0;i<s->count;i++)if(alive(&s->objects[i]))s->objects[n++]=s->objects[i];s->count=n;return 1;
}
int smn64_web_attack_effects_tick(SmN64WebAttackEffects *s,uint32_t tick,uint32_t rng[3],const SmN64WebAttackHost *h){return effects_tick(s,tick,rng,h,NULL,NULL);}
int smn64_web_attack_snapshot(const SmN64WebAttackEffects *s,SmN64WebAttackObject *out,size_t cap,size_t *count){
    if(!s||!out||!count||s->count>SMN64_WEB_ATTACK_CAPACITY||cap<s->count)return -1;
    memcpy(out,s->objects,s->count*sizeof *out);*count=s->count;return 1;
}
int smn64_web_attack_fire_ordered(SmN64WebAttackEffects *s,const SmN64FireEvent *e,uint32_t tick,uint8_t suit,int32_t difficulty,uint32_t rng[3],const SmN64WebAttackHost *h,uint64_t *clock){
    if(!s||smn64_graphical_import(&s->graphical_clock,clock)!=1)return -1;
    int rc=smn64_web_attack_fire(s,e,tick,suit,difficulty,rng,h);
    if(rc>0)*clock=s->graphical_clock;
    return rc;
}
int smn64_web_attack_sparks_ordered(SmN64WebAttackEffects *s,uint32_t count,const int32_t hand0[3],const int32_t hand1[3],const int32_t forward[3],uint32_t tick,uint32_t rng[3],uint64_t *clock){
    if(!s||smn64_graphical_import(&s->graphical_clock,clock)!=1)return -1;
    int rc=smn64_web_attack_sparks(s,count,hand0,hand1,forward,tick,rng);
    if(rc>0)*clock=s->graphical_clock;
    return rc;
}
int smn64_web_attack_effects_tick_ordered(SmN64WebAttackEffects *s,uint32_t tick,uint32_t rng[3],const SmN64WebAttackHost *h,uint64_t *clock){
    return smn64_web_attack_effects_tick_interleaved(s,tick,rng,h,clock,NULL,NULL);
}
int smn64_web_attack_effects_tick_interleaved(SmN64WebAttackEffects *s,uint32_t tick,uint32_t rng[3],const SmN64WebAttackHost *h,uint64_t *clock,SmN64WebAttackBetweenPasses between,void *context){
    if(!s||smn64_graphical_import(&s->graphical_clock,clock)!=1)return -1;
    int rc=effects_tick(s,tick,rng,h,between,context);
    if(rc>0)*clock=s->graphical_clock;
    return rc;
}
