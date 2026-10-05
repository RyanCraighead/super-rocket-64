#include "dome_owner_n64.h"
#include "../graphics/allocation_order_n64.h"
#include <string.h>
typedef struct Call {
    SmN64DomeOwner *s; const SmN64DomePlayer *player; int present;
    int32_t dy; uint32_t *rng; uint64_t *clock; const SmN64DomeOwnerHost *host;
} Call;
static int fail(SmN64DomeOwner *s){if(s)s->poisoned=1;return -2;}
static SmN64DomeOwnerPool *pool(SmN64DomeOwner *s,unsigned slot,unsigned node){for(unsigned i=0;i<SMN64_DOME_POOL_COUNT;i++)if(s->pools[i].slot==slot&&s->pools[i].node==node)return &s->pools[i];return NULL;}
static const SmN64DomeOwnerPool *cpool(const SmN64DomeOwner *s,unsigned slot,unsigned node){for(unsigned i=0;i<SMN64_DOME_POOL_COUNT;i++)if(s->pools[i].slot==slot&&s->pools[i].node==node)return &s->pools[i];return NULL;}
static SmN64DomeOwnerRecord *record(SmN64DomeOwner *s,uint32_t id){for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(s->records[i].id==id)return &s->records[i];return NULL;}
static SmN64DomeBody *body(SmN64DomeOwner *s,uint32_t id){
    for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY;i++){if(s->held[i].body.id==id&&s->held[i].body.alive)return &s->held[i].body;for(unsigned j=0;j<5;j++)if(s->held[i].pieces[j].body.id==id&&s->held[i].pieces[j].body.alive)return &s->held[i].pieces[j].body;}
    for(unsigned i=0;i<2;i++)if(s->rings[i].body.id==id&&s->rings[i].body.alive)return &s->rings[i].body;
    return NULL;
}
static SmN64HeldDome *held(SmN64DomeOwner *s){for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY;i++)if(s->held[i].body.alive&&s->held[i].phase!=3)return &s->held[i];return NULL;}
static int serial(Call *c,uint64_t *out){return c->clock&&smn64_graphical_reserve(c->clock,1,out)==1?1:fail(c->s);}
static int ready(Call *c){return c&&c->s&&!c->s->poisoned&&c->rng&&c->clock&&c->host&&c->host->actor_at&&c->host->mark_hit&&c->host->begin_pulse&&c->host->apply&&c->host->shatter&&c->host->shake;}
static int service(void *ctx,SmN64DomeBody *b,SmN64DomeCall *q){
    Call *c=ctx;SmN64DomeOwner *s=c->s;SmN64DomeOwnerRecord *r=record(s,b->id);
    if(!ready(c)||!b||!q)return fail(s);
    switch(q->operation){
    case SMN64_DOME_ALLOC_BODY:
        if(!s->next_id)return fail(s);
        for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(!s->records[i].id){r=&s->records[i];memset(r,0,sizeof *r);r->id=b->id=s->next_id++;return 1;}
    return fail(s);
    case SMN64_DOME_MAKE_HANDLE:
        if(!c->player||q->args[0]!=c->player->id||!c->present)return fail(s);
        q->args[1]=s->player_generation;return 1;
    case SMN64_DOME_INIT_ITEM:
        if(!r||!q->name)return fail(s);
        if(!strcmp(q->name,"webdome2"))r->slot=248;
        else if(!strcmp(q->name,"webdome3"))r->slot=249;
        else if(!strcmp(q->name,"ring"))r->slot=226;
        else return fail(s);
        return 1;
    case SMN64_DOME_LOOKUP_BUNDLE:return r&&q->args[0]==r->slot?1:fail(s);
    case SMN64_DOME_LOOKUP_MODEL:
        if(!r||!pool(s,r->slot,q->args[0]))return fail(s);
        r->node=(uint16_t)q->args[0];return 1;
    case SMN64_DOME_BIND_RENDER:
        if(!r||r->render_serial||!pool(s,r->slot,r->node)||serial(c,&r->render_serial)!=1)return fail(s);
        if(s->render_free_count)r->render_slot=s->render_free[--s->render_free_count];
        else if(s->render_count<SMN64_DOME_BODY_CAPACITY)r->render_slot=s->render_count++;
        else return fail(s);
        for(unsigned i=0;i<3;i++){volatile float v=(float)b->position[i];v=v*0x1p-12f;v=v*0x1p-4f;r->render_translation[i]=v;}
        b->render=(uint32_t)r->render_slot+1;b->render_bc=255;b->render_bd=0;return 1;
    case SMN64_DOME_ATTACH_MISC:
        if(!r||r->attached||s->misc_clock==UINT64_MAX||q->args[0]!=0xf646c)return fail(s);
        r->attached=1;r->misc_serial=++s->misc_clock;return 1;
    case SMN64_DOME_GET_MESH:{SmN64DomeOwnerPool *p=r?pool(s,r->slot,r->node):NULL;if(!p||r->slot!=226)return fail(s);q->mesh.vertices=p->vertices;q->mesh.count=p->count;return 1;}
    case SMN64_DOME_FADE_30:
        return r&&q->args[0]==30&&q->args[1]==30&&c->host->shatter(c->host->context,b,r->slot,r->node,r->render_translation,c->rng,c->clock)==1?1:fail(s);
    case SMN64_DOME_ALLOC_PULSE:
        if(q->args[0]!=0x98||q->args[1]!=0xf5530||!s->next_id)return fail(s);
        for(unsigned i=0;i<SMN64_DOME_PULSE_CAPACITY;i++)if(s->pulses[i].id==UINT32_MAX){s->pulses[i].id=s->next_id++;if(c->host->begin_pulse(c->host->context)!=1)return fail(s);return serial(c,&s->pulses[i].serial);}
        return fail(s);
    case SMN64_DOME_SHAKE:return c->host->shake(c->host->context,q->args[0],q->position)==1?1:fail(s);
    case SMN64_DOME_BODY_DIE:return r?1:fail(s); /* kernel sets source40 */
    case SMN64_DOME_RELEASE_RENDERS:
        if(!r||!b->render||!r->render_serial)return fail(s);
        if(s->render_free_count>=SMN64_DOME_BODY_CAPACITY)return fail(s);
        s->render_free[s->render_free_count++]=r->render_slot;b->render=0;r->render_serial=0;return 1;
    case SMN64_DOME_DETACH_MISC:
        if(!r||!r->attached)return fail(s);
        r->attached=0;return 1;
    case SMN64_DOME_BASE_DESTROY:return r&&!r->attached&&!b->render?1:fail(s);
    case SMN64_DOME_FREE_BODY:
        if(!r||r->attached||r->buffers||b->render)return fail(s);
        memset(r,0,sizeof *r);return 1;
    case SMN64_DOME_ALLOC_BUFFER:
        if(!r||r->slot!=226||q->args[1]>1||q->args[0]!=pool(s,226,0)->count*6||(r->buffers&(1u<<q->args[1])))return fail(s);
        r->buffers|=(uint8_t)(1u<<q->args[1]);return 1;
    case SMN64_DOME_FREE_BUFFER:
        if(!r||q->args[0]>1||!(r->buffers&(1u<<q->args[0])))return fail(s);
        r->buffers&=(uint8_t)~(1u<<q->args[0]);return 1;
    default:return fail(s);
    }
}
static int player(void *ctx,uint32_t id,uint32_t gen,SmN64DomePlayer *out){Call *c=ctx;if(!c->player||!out)return -1;if(!c->present||id!=c->player->id||gen!=c->s->player_generation)return 0;*out=*c->player;return 1;}
static int delta(void *ctx,int32_t *out){Call *c=ctx;if(!out)return -1;*out=c->dy;return 1;}
static SmN64DomeActorHost actors(Call *c){SmN64DomeActorHost h={c,service,player,delta};return h;}
static int at(void *ctx,uint32_t list,size_t n,SmN64DomeActor *a){Call *c=ctx;return list<2&&n<=256?c->host->actor_at(c->host->context,list,n,a):-1;}
static int mark(void *ctx,uint32_t id,uint8_t set){Call *c=ctx;return c->host->mark_hit(c->host->context,id,set);}
static int apply(void *ctx,const SmN64CombatHit *hit){Call *c=ctx;return c->host->apply(c->host->context,hit);}
static int effect(void *ctx,uint32_t kind,uint32_t id){
    Call *c=ctx;if(kind!=1)return fail(c->s);SmN64DomeActor a;int found=0;
    for(size_t i=0;i<=256;i++){int rc=at(c,0,i,&a);if(rc<0)return fail(c->s);if(!rc)break;if(a.id==id){found=1;break;}}
    if(!found||!c->s->next_id)return fail(c->s);
    for(unsigned i=0;i<SMN64_DOME_IMPACT_CAPACITY;i++)if(!c->s->impacts[i].id){SmN64DomeOwnerImpact *p=&c->s->impacts[i];p->id=c->s->next_id++;if(serial(c,&p->serial)!=1||smn64_dome_impact_init(&p->impact,a.position,c->rng)!=1)return fail(c->s);return 1;}
    return fail(c->s);
}
static SmN64DomeHost pulses(Call *c){SmN64DomeHost h={c,at,mark,apply,effect,0};return h;}
int smn64_dome_owner_init(SmN64DomeOwner *s,const SmN64DomeOwnerPool p[SMN64_DOME_POOL_COUNT]){
    if(!s||!p)return -1;
    for(unsigned i=0;i<SMN64_DOME_POOL_COUNT;i++){unsigned slot=i==0?226:i==1?248:249,node=i<2?0:i-2;if(p[i].slot!=slot||p[i].node!=node||!p[i].count||p[i].count>SMN64_DOME_RING_CAPACITY)return -1;}
    memset(s,0,sizeof *s);memcpy(s->pools,p,sizeof s->pools);s->next_id=1;s->player_generation=1;return 1;
}
int smn64_dome_owner_copy(SmN64DomeOwner *d,const SmN64DomeOwner *s){
    int ring=-1;if(!d||!s)return -1;if(s->world.ring){for(unsigned i=0;i<2;i++)if(s->world.ring==&s->rings[i])ring=(int)i;if(ring<0)return -1;}
    if(d!=s)*d=*s;
    d->world.ring=ring<0?NULL:&d->rings[ring];
    for(unsigned i=0;i<2;i++)if(d->rings[i].mesh.vertices){d->rings[i].mesh.vertices=d->pools[0].vertices;d->rings[i].mesh.count=d->pools[0].count;}
    return 1;
}
int smn64_dome_owner_create(SmN64DomeOwner *s,const SmN64DomePlayer *p,uint32_t type,uint32_t rng[3],uint64_t *clock,const SmN64DomeOwnerHost *h){
    Call c={s,p,1,0,rng,clock,h};if(!ready(&c)||!p||!p->id||type||held(s))return fail(s);
    for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY;i++)if(!s->held[i].body.alive){SmN64DomeActorHost ah=actors(&c);return smn64_dome_actor_init(&s->held[i],&s->world,p,0,&ah)==1?1:fail(s);}
    return fail(s);
}
static int release(Call *c,SmN64HeldDome *d){
    SmN64DomeOwnerGraphic *p=NULL;SmN64DomeRing *r=c->s->world.ring==&c->s->rings[0]?&c->s->rings[1]:&c->s->rings[0];
    for(unsigned i=0;i<SMN64_DOME_PULSE_CAPACITY;i++)if(!c->s->pulses[i].id){p=&c->s->pulses[i];break;}
    if(!p||!c->player||!c->s->next_id)return fail(c->s);
    p->id=UINT32_MAX; /* ALLOC_PULSE executes after synchronous shatter. */
    SmN64DomeActorHost ah=actors(c);SmN64DomeHost ph=pulses(c);
    return smn64_dome_actor_release(d,&c->s->world,r,&p->pulse,c->rng,c->player->position,&ph,&ah)==1?1:fail(c->s);
}
int smn64_dome_owner_release(SmN64DomeOwner *s,const SmN64DomePlayer *p,int32_t dy,uint32_t rng[3],uint64_t *clock,const SmN64DomeOwnerHost *h){Call c={s,p,1,dy,rng,clock,h};if(!ready(&c)||!p)return fail(s);SmN64HeldDome *d=held(s);return d?release(&c,d):fail(s);}
static int destroy(Call *c,SmN64DomeBody *b){SmN64DomeActorHost ah=actors(c);for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY;i++)if(b==&c->s->held[i].body)return smn64_dome_actor_destroy(&c->s->held[i],&c->s->world,1,&ah);for(unsigned i=0;i<2;i++)if(b==&c->s->rings[i].body)return smn64_dome_ring_destroy(&c->s->rings[i],&c->s->world,1,&ah);return -2;}
int smn64_dome_owner_misc(SmN64DomeOwner *s,const SmN64DomePlayer *p,int present,int interrupted,int32_t dy,uint32_t rng[3],uint64_t *clock,const SmN64DomeOwnerHost *h){
    Call c={s,p,present,dy,rng,clock,h};uint32_t ids[SMN64_DOME_BODY_CAPACITY];unsigned n=0;
    if(!ready(&c)||!p||present<0||present>1)return fail(s);
    if(interrupted&&held(s)&&release(&c,held(s))!=1)return fail(s);
    for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(s->records[i].id&&s->records[i].attached){unsigned j=n++;while(j&&record(s,ids[j-1])->misc_serial<s->records[i].misc_serial){ids[j]=ids[j-1];j--;}ids[j]=s->records[i].id;}
    SmN64DomeActorHost ah=actors(&c);
    for(unsigned k=0;k<n;k++){
        SmN64DomeBody *b=body(s,ids[k]);if(!b)continue;
        if(b->body_flags&0x40){if(b->body_flags&0x80){if(destroy(&c,b)!=1)return fail(s);}else b->body_flags|=0x80;continue;}
        b->body_flags&=~4u;int rc=-2,done=0;
        for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY&&!done;i++){
            if(b==&s->held[i].body){rc=smn64_dome_actor_tick(&s->held[i],rng,&ah);if(rc==2)rc=release(&c,&s->held[i]);done=1;}
            else for(unsigned j=0;j<5;j++)if(b==&s->held[i].pieces[j].body){rc=smn64_dome_piece_tick(&s->held[i].pieces[j],&ah);done=1;break;}
        }
        if(!done)for(unsigned i=0;i<2;i++)if(b==&s->rings[i].body){rc=smn64_dome_ring_tick(&s->rings[i],&ah);break;}
        if(rc!=1)return fail(s);
    }
    return 1;
}
int smn64_dome_owner_graphics(SmN64DomeOwner *s,uint32_t rng[3],uint64_t *clock,const SmN64DomeOwnerHost *h){
    Call c={s,NULL,0,0,rng,clock,h};if(!ready(&c))return fail(s);
    for(unsigned i=0;i<SMN64_DOME_IMPACT_CAPACITY;i++)if(s->impacts[i].id){int rc=smn64_dome_impact_tick(&s->impacts[i].impact);if(rc<0)return fail(s);if(!rc)memset(&s->impacts[i],0,sizeof s->impacts[i]);}
    uint32_t ids[SMN64_DOME_PULSE_CAPACITY];unsigned n=0;
    for(unsigned i=0;i<SMN64_DOME_PULSE_CAPACITY;i++)if(s->pulses[i].id){unsigned j=n++;while(j&&s->pulses[ids[j-1]].serial<s->pulses[i].serial){ids[j]=ids[j-1];j--;}ids[j]=i;}
    SmN64DomeHost ph=pulses(&c);
    for(unsigned j=0;j<n;j++){SmN64DomeOwnerGraphic *p=&s->pulses[ids[j]];int rc=smn64_dome_pulse_tick(&p->pulse,&ph);if(rc<0)return fail(s);if(!rc)memset(p,0,sizeof *p);}
    return 1;
}
int smn64_dome_owner_validate(const SmN64DomeOwner *s){
    unsigned held_count=0,ring_count=0,body_count=0,record_count=0;uint8_t slots[SMN64_DOME_BODY_CAPACITY]={0};
    if(!s||s->poisoned||!s->next_id||!s->player_generation||s->world.fire_count||s->render_count>SMN64_DOME_BODY_CAPACITY||s->render_free_count>s->render_count)return -2;
    for(unsigned i=0;i<SMN64_DOME_POOL_COUNT;i++){const SmN64DomeOwnerPool *p=&s->pools[i];if(p->slot!=(i==0?226:i==1?248:249)||p->node!=(i<2?0:i-2)||!p->count||p->count>SMN64_DOME_RING_CAPACITY)return -2;}
    for(unsigned i=0;i<SMN64_DOME_HELD_CAPACITY;i++){
        const SmN64HeldDome *d=&s->held[i];
        if(d->body.alive){if(d->body.poisoned||d->web_type||!d->body.id||!d->body.render||d->phase<0||d->phase>3)return -2;held_count++;body_count++;}
        for(unsigned j=0;j<5;j++)if(d->pieces[j].body.alive){if(!d->body.alive||d->pieces[j].body.poisoned||!d->pieces[j].body.id||!d->pieces[j].body.render)return -2;body_count++;}
    }
    for(unsigned i=0;i<2;i++){const SmN64DomeRing *r=&s->rings[i];if(r->body.alive){if(r->body.poisoned||r->web_type||!r->body.id||!r->body.render||s->world.ring!=r||r->mesh.vertices!=s->pools[0].vertices||r->mesh.count!=s->pools[0].count||r->saved_count!=r->mesh.count)return -2;ring_count++;body_count++;}}
    if(held_count!=s->world.dome_count||ring_count>1||!!s->world.ring!=!!ring_count)return -2;
    for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(s->records[i].id){
        const SmN64DomeOwnerRecord *r=&s->records[i];
        if(!r->attached||!r->render_serial||!r->misc_serial||r->misc_serial>s->misc_clock||!cpool(s,r->slot,r->node)||r->render_slot>=s->render_count||slots[r->render_slot]||r->id>=s->next_id)return -2;
        slots[r->render_slot]=1;record_count++;
        for(unsigned j=0;j<i;j++)if(r->id==s->records[j].id||r->render_serial==s->records[j].render_serial)return -2;
    }
    if(body_count!=record_count||record_count+s->render_free_count!=s->render_count)return -2;
    for(unsigned i=0;i<s->render_free_count;i++){unsigned n=s->render_free[i];if(n>=s->render_count||slots[n])return -2;slots[n]=1;}
    for(unsigned i=0;i<SMN64_DOME_PULSE_CAPACITY;i++)if(s->pulses[i].id){const SmN64DomeOwnerGraphic *p=&s->pulses[i];if(p->id>=s->next_id||!p->serial||p->pulse.alive!=1||p->pulse.web_type)return -2;}
    for(unsigned i=0;i<SMN64_DOME_IMPACT_CAPACITY;i++)if(s->impacts[i].id){const SmN64DomeOwnerImpact *p=&s->impacts[i];if(p->id>=s->next_id||!p->serial||p->impact.alive!=1)return -2;}
    return 1;
}
int smn64_dome_owner_snapshot(const SmN64DomeOwner *s,SmN64DomeOwnerInstance *out,size_t cap,size_t *count){
    size_t n=0;if(!s||!out||!count||smn64_dome_owner_validate(s)!=1)return -1;
    for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(s->records[i].id){
        const SmN64DomeOwnerRecord *r=&s->records[i];const SmN64DomeBody *b=NULL;
        for(unsigned j=0;j<SMN64_DOME_HELD_CAPACITY;j++){if(s->held[j].body.id==r->id)b=&s->held[j].body;for(unsigned k=0;k<5;k++)if(s->held[j].pieces[k].body.id==r->id)b=&s->held[j].pieces[k].body;}
        for(unsigned j=0;j<2;j++)if(s->rings[j].body.id==r->id)b=&s->rings[j].body;
        if(!b||!b->alive||b->poisoned)return -1;
        if(n>=cap)return -1;
        const SmN64DomeOwnerPool *p=cpool(s,r->slot,r->node);size_t j=n++;
        while(j&&out[j-1].body.render>b->render){out[j]=out[j-1];j--;}
        out[j].body=*b;out[j].model_slot=r->slot;out[j].node=r->node;out[j].graphical_serial=r->render_serial;out[j].current_pool=p->vertices;out[j].current_count=p->count;
    }
    *count=n;return 1;
}

void smn64_dome_owner_render_cache(SmN64DomeOwner *s){
    if(!s||s->poisoned)return;
    for(unsigned i=0;i<SMN64_DOME_BODY_CAPACITY;i++)if(s->records[i].id){
        SmN64DomeBody *b=body(s,s->records[i].id);if(!b||(b->body_flags&0x40))continue;
        for(unsigned j=0;j<3;j++){volatile float v=(float)b->position[j];v=v*0x1p-12f;v=v*0x1p-4f;s->records[i].render_translation[j]=v;}
    }
}
