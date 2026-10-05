#include "spiderman_web_attack_host.h"
#include "spiderman_combat_host.h"
#include "spiderman_world.h"
#include <string.h>
typedef struct AttackHost {
    SmN64WebAttackEffects committed,pending;
    SmN64CombatOwnerServices base,services;
    SmN64CombatOwnerFrame frame;
    SmN64CharacterCombat *combat;
    uint8_t enabled,active,bound,failed,effects_complete,prepared;
} AttackHost;
static AttackHost sAttack;
static int fail(void){if(sAttack.active)sAttack.failed=1;return -2;}
static int trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64ImpactWorldHit *out){
    int include_objects=ctx!=NULL;SpidermanWorldHit hit;
    if(!sAttack.active||!out)return -2;
    int rc=spiderman_world_trace_ex(from,to,include_objects,&hit);if(rc!=1)return rc;
    memset(out,0,sizeof *out);out->hit=(uint8_t)hit.present;out->distance=hit.distance;
    if(hit.present){memcpy(out->position,hit.position,sizeof out->position);memcpy(out->normal,hit.normal,sizeof out->normal);}return 1;
}
static int floor_trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64ImpactWorldHit *out){(void)ctx;return trace(&sAttack,from,to,out);}
static int sweep(void *ctx,const int32_t from[3],const int32_t to[3],uint32_t *actor){
    (void)ctx;
    if(!sAttack.active||!sAttack.enabled||!sAttack.bound||!actor)return -2;
    int rc=spiderman_combat_host_impact_sweep(from,to,actor);
    if(rc!=0)return rc;
    return spiderman_web_attack_dynamic_only(from,to);
}
static int apply(void *ctx,const SmN64CombatHit *hit){
    (void)ctx;if(!sAttack.active||!sAttack.combat)return -2;
    /* B2454 source display refresh precedes recipient acceptance. */
    sAttack.combat->damage.display_timer=360;
    sAttack.combat->damage.display_value=(sAttack.combat->damage.combo_metric<<10)+0x2c00u;
    return spiderman_combat_host_damage(hit);
}
static int special(void *ctx,uint32_t id){(void)ctx;return sAttack.active&&!id?0:-2;}
static int sound(void *ctx,uint32_t id,const int32_t position[3]){
    (void)ctx;SmN64CharacterCombat *c=sAttack.combat;
    if(!sAttack.active||!c||c->effect_count>=SMN64_COMBAT_EFFECT_CAPACITY)return -2;
    SmN64CombatEffect *e=&c->effects[c->effect_count++];memset(e,0,sizeof *e);e->kind=SMN64_COMBAT_SOUND;e->sound=id;memcpy(e->position,position,sizeof e->position);return 1;
}
static SmN64WebAttackHost host(void){SmN64WebAttackHost h={{NULL,trace,sweep,apply,special},NULL,floor_trace,sound};return h;}
static void bind(void *ctx,const SmN64CombatOwnerFrame *f){
    (void)ctx;sAttack.bound=0;if(sAttack.base.bind_frame)sAttack.base.bind_frame(sAttack.base.context,f);
    if(f&&sAttack.active&&!sAttack.prepared){sAttack.frame=*f;sAttack.bound=1;}else memset(&sAttack.frame,0,sizeof sAttack.frame);
}
static int fire(void *ctx,SmN64WebAction *action,const SmN64FireEvent *e,uint32_t rng[3]){
    (void)ctx;(void)action;if(!sAttack.active||!sAttack.bound||!sAttack.frame.resource||sAttack.effects_complete||sAttack.prepared)return fail();
    SmN64WebAttackHost h=host();SmN64WebResource *r=sAttack.frame.resource;
    int rc=smn64_web_attack_fire_ordered(&sAttack.pending,e,sAttack.frame.tick,r->suit,r->difficulty,rng,&h,sAttack.frame.graphical_clock);return rc==1?1:fail();
}
static int marker(unsigned id,int32_t p[3]){
    const SmN64CombatOwnerFrame *f=&sAttack.frame;
    if(!sAttack.bound||!f->actor||!f->markers||!f->retained_pose||!f->body_translation)return -2;
    return smn64_marker_world(f->markers,f->marker_count,id,f->retained_pose,18,f->actor->basis.matrix,f->body_translation,f->actor->position,0,p);
}
static int sparks(void *ctx,uint32_t count){
    (void)ctx;int32_t p[2][3]={{0}};if(!sAttack.active||!sAttack.bound||sAttack.effects_complete||sAttack.prepared||marker(1,p[1])!=1||(count==2&&marker(0,p[0])!=1))return fail();
    int rc=smn64_web_attack_sparks_ordered(&sAttack.pending,count,p[0],p[1],sAttack.frame.actor->basis.forward,sAttack.frame.tick,sAttack.frame.actor->random_state,sAttack.frame.graphical_clock);return rc==1?1:fail();
}
static void pull(SmN64ComboOwner *o,const SmN64WebAction *a){memset(o,0,sizeof *o);o->anim=a->ability.anim;o->state=a->ability.state;memcpy(o->position,a->position,sizeof o->position);memcpy(o->forward,a->forward,sizeof o->forward);o->target_actor=a->target;}
static int stop(void *ctx,SmN64WebAction *a){
    (void)ctx;if(!sAttack.active||!sAttack.bound||!sAttack.base.combo.stop)return -2;SmN64ComboOwner o;pull(&o,a);
    int rc=sAttack.base.combo.stop(sAttack.base.combo.context,&o);if(rc==1){a->ability.anim=o.anim;a->ability.state=o.state;memcpy(a->position,o.position,sizeof a->position);memcpy(a->forward,o.forward,sizeof a->forward);}return rc;
}
static int jump(void *ctx,SmN64WebAction *a){
    (void)ctx;if(!sAttack.active||!sAttack.bound||!sAttack.base.combo.jump)return -2;SmN64ComboOwner o;pull(&o,a);
    int rc=sAttack.base.combo.jump(sAttack.base.combo.context,&o);if(rc>=0){a->ability.anim=o.anim;a->ability.state=o.state;}return rc;
}
static int ability(void *ctx,SmN64ClimbState *a,SmN64CharacterCombat *c,const SmN64WebAbilityEvent *e){
    (void)ctx;if(!sAttack.active||!sAttack.bound||!a||!c||!e)return -2;
    if(e->create_dome||e->destroy_dome||e->grab_target_query||e->fire_amount||e->release_web)return sAttack.base.ability_event?sAttack.base.ability_event(sAttack.base.context,a,c,e):-2;
    if(e->face_target){
        if(a->state!=0x10000)return -2; /*trap/yank graphics still gated*/
        if(c->owner.target_actor){SmN64ComboOwner o;pull(&o,&c->web);if(!sAttack.base.combo.face_actor||sAttack.base.combo.face_actor(sAttack.base.combo.context,&o,c->owner.target_actor)!=1)return -2;memcpy(c->web.forward,o.forward,sizeof c->web.forward);}
        else a->turn_ticks=0;
    }
    return 1;
}
int spiderman_web_attack_host_enable(void){if(sAttack.active||!spiderman_combat_host_services())return -2;if(sAttack.enabled)return 1;sAttack.enabled=1;smn64_web_attack_effects_init(&sAttack.committed);return 1;}
void spiderman_web_attack_host_abort(void){sAttack.active=0;sAttack.bound=0;sAttack.failed=0;sAttack.effects_complete=0;sAttack.prepared=0;sAttack.combat=NULL;memset(&sAttack.frame,0,sizeof sAttack.frame);}
void spiderman_web_attack_host_reset(void){spiderman_web_attack_host_abort();smn64_web_attack_effects_init(&sAttack.committed);}
void spiderman_web_attack_host_disable(void){spiderman_web_attack_host_reset();sAttack.enabled=0;memset(&sAttack.base,0,sizeof sAttack.base);memset(&sAttack.services,0,sizeof sAttack.services);}
const SmN64CombatOwnerServices *spiderman_web_attack_host_services(void){
    const SmN64CombatOwnerServices *base=spiderman_combat_host_services();
    if(!base){spiderman_web_attack_host_abort();memset(&sAttack.base,0,sizeof sAttack.base);memset(&sAttack.services,0,sizeof sAttack.services);return NULL;}
    if(!sAttack.enabled)return base;
    sAttack.base=*base;sAttack.services=*base;sAttack.services.unavailable_commands&=~SMN64_COMMAND_IMPACT;sAttack.services.bind_frame=bind;sAttack.services.fire_event=fire;sAttack.services.ability_event=ability;
    sAttack.services.web.jump=jump;sAttack.services.web.stop=stop;sAttack.services.web.impact_sparks=sparks;return&sAttack.services;
}
int spiderman_web_attack_host_begin(void){if(sAttack.active||!sAttack.enabled||!spiderman_combat_host_services())return -2;sAttack.pending=sAttack.committed;sAttack.active=1;sAttack.failed=0;sAttack.prepared=0;sAttack.effects_complete=(uint8_t)!sAttack.enabled;return 1;}
int spiderman_web_attack_host_validate(void){
    if(!sAttack.active||sAttack.bound||sAttack.combat||sAttack.failed||!sAttack.effects_complete||!spiderman_combat_host_services()||sAttack.pending.count>SMN64_WEB_ATTACK_CAPACITY)return -2;
    for(uint32_t i=0;i<sAttack.pending.count;i++){
        const SmN64WebAttackObject *o=&sAttack.pending.objects[i];int live_object=0;
        if(!o->id||(sAttack.pending.next_id&&o->id>=sAttack.pending.next_id)||!o->graphical_serial||
           o->graphical_serial>sAttack.pending.graphical_clock||
           (o->kind==SMN64_ATTACK_FRAGMENT&&o->graphical_serial==sAttack.pending.graphical_clock))return -2;
        for(uint32_t j=0;j<i;j++){
            const SmN64WebAttackObject *prior=&sAttack.pending.objects[j];
            uint64_t end=o->graphical_serial+(o->kind==SMN64_ATTACK_FRAGMENT);
            uint64_t prior_end=prior->graphical_serial+(prior->kind==SMN64_ATTACK_FRAGMENT);
            if(prior->id==o->id||(o->graphical_serial<=prior_end&&prior->graphical_serial<=end))return -2;
        }
        switch(o->kind){
        case SMN64_ATTACK_PROJECTILE:live_object=o->state.projectile.alive;break;
        case SMN64_ATTACK_BURST:live_object=o->state.burst.alive;break;
        case SMN64_ATTACK_DECAL:live_object=o->state.decal.alive;break;
        case SMN64_ATTACK_FRAGMENT:live_object=o->state.fragment.alive;break;
        case SMN64_ATTACK_SPARK:live_object=o->state.spark.alive;break;
        default:return -2;
        }
        if(live_object!=1)return -2;
    }
    sAttack.prepared=1;return 1;
}
void spiderman_web_attack_host_finalize(void){
    if(sAttack.prepared)sAttack.committed=sAttack.pending;
    spiderman_web_attack_host_abort();
}
int spiderman_web_attack_host_commit(void){if(spiderman_web_attack_host_validate()!=1)return -2;spiderman_web_attack_host_finalize();return 1;}
typedef struct HostPassHook { SmN64CombatOwnerFrame *frame;SmN64WebAttackBetweenPasses hook;void *context; } HostPassHook;
static int between_passes(void *context,uint64_t *clock){
    HostPassHook *h=context;if(!h||!h->frame||!h->frame->graphical_clock||!h->hook)return -2;
    *h->frame->graphical_clock=*clock;
    int rc=h->hook(h->context,h->frame->graphical_clock);
    if(rc==1)*clock=*h->frame->graphical_clock;
    return rc;
}
int spiderman_web_attack_host_effects_interleaved(SmN64CombatOwnerFrame *f,SmN64CharacterCombat *c,SmN64WebAttackBetweenPasses hook,void *context){
    if(!sAttack.enabled)return 1;
    if(!sAttack.active||!f||!f->actor||!c||sAttack.failed||sAttack.prepared||sAttack.effects_complete)return fail();
    bind(NULL,f);sAttack.combat=c;SmN64WebAttackHost h=host();HostPassHook pass={f,hook,context};int rc=smn64_web_attack_effects_tick_interleaved(&sAttack.pending,f->tick,f->actor->random_state,&h,f->graphical_clock,hook?between_passes:NULL,&pass);sAttack.combat=NULL;bind(NULL,NULL);if(rc!=1)return fail();sAttack.effects_complete=1;return 1;
}
int spiderman_web_attack_host_effects(SmN64CombatOwnerFrame *f,SmN64CharacterCombat *c){return spiderman_web_attack_host_effects_interleaved(f,c,NULL,NULL);}
int spiderman_web_attack_host_snapshot(SmN64WebAttackObject *out,size_t cap,size_t *count){return smn64_web_attack_snapshot(&sAttack.committed,out,cap,count);}
