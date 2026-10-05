#include "spiderman_dome_host.h"
#include "spiderman_web_attack_host.h"
#include "spiderman_combat_host.h"
#include "spiderman_world.h"
#include "../pc/spiderman_dome_scene.h"
#include "../pc/spiderman_effect_scene.h"
#include <string.h>
typedef struct DomeState {
    SmN64DomeOwner owner;
    SmN64DomeShatterFragment shards[SPIDERMAN_DOME_SHARD_CAPACITY];size_t shard_count;
    SpidermanDomeShake shakes[SPIDERMAN_DOME_SHAKE_CAPACITY];size_t shake_count;
} DomeState;
typedef struct DomeHost {
    DomeState committed,pending;
    SmN64DomeOwnerPool templates[SMN64_DOME_POOL_COUNT];SmN64DomeGeometry shatter_geometry;
    SmN64CombatOwnerServices base,services;SmN64CombatOwnerFrame frame;
    uint8_t enabled,active,bound,failed,misc_complete,graphics_complete,prepared;
} DomeHost;
static DomeHost sDome;
static int fail(void){if(sDome.active)sDome.failed=1;return -2;}
static int32_t si(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int at(void *ctx,uint32_t list,size_t n,SmN64DomeActor *out){(void)ctx;return sDome.active&&!sDome.prepared?spiderman_combat_host_dome_actor_at(list,n,out):-2;}
static int mark(void *ctx,uint32_t id,uint8_t set){(void)ctx;return sDome.active&&!sDome.prepared?spiderman_combat_host_dome_mark_hit(id,set):-2;}
static int begin_pulse(void *ctx){(void)ctx;return sDome.active&&!sDome.prepared?spiderman_combat_host_dome_reset_hits():-2;}
static int apply(void *ctx,const SmN64CombatHit *hit){(void)ctx;return sDome.active&&!sDome.prepared?spiderman_combat_host_damage(hit):-2;}
static int shake(void *ctx,uint32_t kind,const int32_t pos[3]){
    (void)ctx;if(!sDome.active||sDome.prepared||kind!=1||sDome.pending.shake_count>=SPIDERMAN_DOME_SHAKE_CAPACITY)return fail();
    SpidermanDomeShake *s=&sDome.pending.shakes[sDome.pending.shake_count++];s->kind=kind;memcpy(s->position,pos,sizeof s->position);return 1;
}
static int shatter(void *ctx,const SmN64DomeBody *body,uint16_t slot,uint16_t node,const float translation[3],uint32_t rng[3],uint64_t *clock){
    (void)ctx;SmN64DomeShatterFragment born[SMN64_DOME_SHATTER_COUNT];SpidermanWorldHit hit;int32_t to[3];
    if(!sDome.active||sDome.prepared||sDome.graphics_complete||!body||slot!=249||node!=4||sDome.pending.shard_count>SPIDERMAN_DOME_SHARD_CAPACITY-SMN64_DOME_SHATTER_COUNT)return fail();
    memcpy(to,body->position,sizeof to);to[1]=si((uint32_t)to[1]+5000u*4096u);memset(&hit,0,sizeof hit);
    if(spiderman_world_trace_ex(body->position,to,1,&hit)!=1||smn64_dome_shatter_spawn(&sDome.shatter_geometry,translation,hit.present?hit.position[1]:to[1],rng,clock,born)!=1)return fail();
    memmove(sDome.pending.shards+SMN64_DOME_SHATTER_COUNT,sDome.pending.shards,sDome.pending.shard_count*sizeof born[0]);
    for(unsigned i=0;i<SMN64_DOME_SHATTER_COUNT;i++)sDome.pending.shards[i]=born[SMN64_DOME_SHATTER_COUNT-1-i];
    sDome.pending.shard_count+=SMN64_DOME_SHATTER_COUNT;return 1;
}
static SmN64DomeOwnerHost host(void){SmN64DomeOwnerHost h={NULL,at,mark,begin_pulse,apply,shatter,shake};return h;}
static SmN64DomePlayer player(const SmN64ClimbState *a){SmN64DomePlayer p;memset(&p,0,sizeof p);p.id=1;memcpy(p.position,a->position,sizeof p.position);p.offset_11a0=a->body_offset;p.frame=a->anim.frame;return p;}
static int32_t dy(const SmN64CombatOwnerFrame *f){return si((uint32_t)f->actor->position[1]-(uint32_t)f->previous_position[1]);}
static void bind(void *ctx,const SmN64CombatOwnerFrame *f){
    (void)ctx;sDome.bound=0;if(sDome.base.bind_frame)sDome.base.bind_frame(sDome.base.context,f);
    if(f&&sDome.active&&!sDome.prepared){sDome.frame=*f;sDome.bound=1;
        if(f->resource&&f->resource->web_type)sDome.services.unavailable_commands|=SMN64_COMMAND_DOME;
        else sDome.services.unavailable_commands&=~SMN64_COMMAND_DOME;
    }else memset(&sDome.frame,0,sizeof sDome.frame);
}
static int ability(void *ctx,SmN64ClimbState *a,SmN64CharacterCombat *combat,const SmN64WebAbilityEvent *e){
    (void)ctx;if(!sDome.active||!sDome.bound||sDome.prepared||sDome.misc_complete||!a||!combat||!e)return fail();
    if(e->create_dome||e->destroy_dome){
        if(e->create_dome&&e->destroy_dome)return fail();
        if(!sDome.frame.resource||sDome.frame.resource->web_type)return fail();
        SmN64DomeOwnerHost h=host();SmN64DomePlayer p=player(a);int rc=e->create_dome?
            smn64_dome_owner_create(&sDome.pending.owner,&p,0,a->random_state,sDome.frame.graphical_clock,&h):
            smn64_dome_owner_release(&sDome.pending.owner,&p,dy(&sDome.frame),a->random_state,sDome.frame.graphical_clock,&h);
        if(rc!=1)return fail();
        SmN64WebAbilityEvent rest=*e;rest.create_dome=rest.destroy_dome=0;
        if(rest.grab_target_query||rest.face_target||rest.fire_amount||rest.release_web)return sDome.base.ability_event?sDome.base.ability_event(sDome.base.context,a,combat,&rest):fail();
        return 1;
    }
    return sDome.base.ability_event?sDome.base.ability_event(sDome.base.context,a,combat,e):fail();
}
int spiderman_dome_host_enable(void){
    if(sDome.active||!spiderman_web_attack_host_services()||!spiderman_dome_scene_ready()||!spiderman_effect_scene_dome_ready())return -2;
    if(sDome.enabled)return 1;
    for(unsigned i=0;i<SMN64_DOME_POOL_COUNT;i++){SmN64DomeOwnerPool *p=&sDome.templates[i];p->slot=(uint16_t)(i==0?226:i==1?248:249);p->node=(uint16_t)(i<2?0:i-2);if(spiderman_dome_scene_copy_pool(p->slot,p->node,p->vertices,SMN64_DOME_RING_CAPACITY,&p->count)!=1)return -2;}
    if(spiderman_dome_scene_copy_geometry(249,4,&sDome.shatter_geometry)!=1||smn64_dome_owner_init(&sDome.committed.owner,sDome.templates)!=1)return -2;
    sDome.enabled=1;return 1;
}
void spiderman_dome_host_abort(void){sDome.active=sDome.bound=sDome.failed=sDome.misc_complete=sDome.graphics_complete=sDome.prepared=0;memset(&sDome.frame,0,sizeof sDome.frame);}
void spiderman_dome_host_reset(void){spiderman_dome_host_abort();memset(&sDome.committed,0,sizeof sDome.committed);if(sDome.enabled)(void)smn64_dome_owner_init(&sDome.committed.owner,sDome.templates);}
void spiderman_dome_host_disable(void){spiderman_dome_host_reset();sDome.enabled=0;memset(&sDome.base,0,sizeof sDome.base);memset(&sDome.services,0,sizeof sDome.services);}
const SmN64CombatOwnerServices *spiderman_dome_host_services(void){
    const SmN64CombatOwnerServices *base=spiderman_web_attack_host_services();if(!base){spiderman_dome_host_abort();memset(&sDome.base,0,sizeof sDome.base);memset(&sDome.services,0,sizeof sDome.services);return NULL;}if(!sDome.enabled)return base;
    sDome.base=*base;sDome.services=*base;sDome.services.unavailable_commands&=~SMN64_COMMAND_DOME;sDome.services.bind_frame=bind;sDome.services.ability_event=ability;return &sDome.services;
}
int spiderman_dome_host_begin(void){
    if(sDome.active||!sDome.enabled||!spiderman_web_attack_host_services()||!spiderman_dome_scene_ready()||!spiderman_effect_scene_dome_ready())return -2;
    sDome.pending=sDome.committed;if(smn64_dome_owner_copy(&sDome.pending.owner,&sDome.committed.owner)!=1)return -2;
    sDome.pending.shake_count=0;sDome.active=1;return 1;
}
int spiderman_dome_host_misc(SmN64CombatOwnerFrame *f,SmN64CharacterCombat *combat){
    if(!sDome.enabled)return 1;
    if(!sDome.active||sDome.prepared||sDome.failed||sDome.misc_complete||!f||!f->actor||!combat)return fail();
    bind(NULL,f);SmN64DomePlayer p=player(f->actor);SmN64DomeOwnerHost h=host();int rc=smn64_dome_owner_misc(&sDome.pending.owner,&p,1,f->actor->state!=0x20000000,dy(f),f->actor->random_state,f->graphical_clock,&h);bind(NULL,NULL);if(rc!=1)return fail();sDome.misc_complete=1;return 1;
}
int spiderman_dome_host_graphics(SmN64CombatOwnerFrame *f,SmN64CharacterCombat *combat){
    if(!sDome.enabled)return 1;
    if(!sDome.active||sDome.prepared||sDome.failed||!sDome.misc_complete||sDome.graphics_complete||!f||!f->actor||!combat)return fail();
    bind(NULL,f);SmN64DomeOwnerHost h=host();int rc=smn64_dome_owner_graphics(&sDome.pending.owner,f->actor->random_state,f->graphical_clock,&h);
    if(rc==1){size_t n=0;for(size_t i=0;i<sDome.pending.shard_count;i++){int live=smn64_dome_shatter_fragment_tick(&sDome.pending.shards[i],f->actor->random_state);if(live<0){rc=-2;break;}if(live)sDome.pending.shards[n++]=sDome.pending.shards[i];}sDome.pending.shard_count=n;}
    bind(NULL,NULL);if(rc!=1)return fail();sDome.graphics_complete=1;return 1;
}
int spiderman_dome_host_validate(void){
    if(!sDome.active||sDome.bound||sDome.failed||sDome.prepared||!sDome.misc_complete||!sDome.graphics_complete||sDome.pending.shard_count>SPIDERMAN_DOME_SHARD_CAPACITY||sDome.pending.shake_count>SPIDERMAN_DOME_SHAKE_CAPACITY||!spiderman_web_attack_host_services()||!spiderman_dome_scene_ready()||!spiderman_effect_scene_dome_ready()||smn64_dome_owner_validate(&sDome.pending.owner)!=1)return -2;
    for(size_t i=0;i<sDome.pending.shard_count;i++)if(sDome.pending.shards[i].alive!=1||!sDome.pending.shards[i].graphical_serial||(i&&sDome.pending.shards[i-1].graphical_serial<=sDome.pending.shards[i].graphical_serial))return -2;
    sDome.prepared=1;return 1;
}
void spiderman_dome_host_finalize(void){
    if(sDome.prepared&&!sDome.failed){sDome.committed=sDome.pending;(void)smn64_dome_owner_copy(&sDome.committed.owner,&sDome.pending.owner);smn64_dome_owner_render_cache(&sDome.committed.owner);}spiderman_dome_host_abort();
}
int spiderman_dome_host_snapshot(SpidermanDomeInstance *out,size_t cap,size_t *count){
    SmN64DomeOwnerInstance source[SMN64_DOME_BODY_CAPACITY];size_t n;if(!out||!count)return -1;if(!sDome.enabled){*count=0;return 1;}if(smn64_dome_owner_snapshot(&sDome.committed.owner,source,SMN64_DOME_BODY_CAPACITY,&n)!=1||n>cap)return -1;
    for(size_t i=0;i<n;i++){out[i].body=source[i].body;out[i].model_slot=source[i].model_slot;out[i].node=source[i].node;out[i].graphical_serial=source[i].graphical_serial;out[i].current_pool=source[i].current_pool;out[i].current_count=source[i].current_count;}*count=n;return 1;
}
int spiderman_dome_host_shards(SmN64DomeShatterFragment *out,size_t cap,size_t *count){if(!out||!count||sDome.committed.shard_count>cap)return -1;memcpy(out,sDome.committed.shards,sDome.committed.shard_count*sizeof *out);*count=sDome.committed.shard_count;return 1;}
int spiderman_dome_host_shakes(SpidermanDomeShake *out,size_t cap,size_t *count){if(!out||!count||sDome.committed.shake_count>cap)return -1;memcpy(out,sDome.committed.shakes,sDome.committed.shake_count*sizeof *out);*count=sDome.committed.shake_count;return 1;}
#ifdef SPIDERMAN_TESTING
int spiderman_dome_host_test_snapshot(SmN64DomeOwner *out){return sDome.enabled?smn64_dome_owner_copy(out,&sDome.committed.owner):-1;}
#endif
