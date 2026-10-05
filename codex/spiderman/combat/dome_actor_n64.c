#include "dome_actor_n64.h"
#include "../web/resource_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int16_t s16(uint32_t v){v&=65535u;return v<=INT16_MAX?(int16_t)v:(int16_t)(-1-(int32_t)(65535u-v));}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int good(const SmN64DomeBody *b,const SmN64DomeActorHost *h){return b&&h&&h->service&&!b->poisoned;}
static int event(SmN64DomeBody *b,const SmN64DomeActorHost *h,SmN64DomeCall *q){if(h->service(h->context,b,q)!=1){b->poisoned=1;return -2;}
return 1;}
static int simple(SmN64DomeBody *b,const SmN64DomeActorHost *h,uint32_t op,uint32_t src,uint32_t a,uint32_t c){SmN64DomeCall q;memset(&q,0,sizeof(q));q.operation=op;q.source=src;q.args[0]=a;q.args[1]=c;return event(b,h,&q);}
static int base(SmN64DomeBody *b,const SmN64DomeActorHost *h,uint32_t bytes){memset(b,0,sizeof(*b));if(simple(b,h,SMN64_DOME_ALLOC_BODY,0x82fdc,bytes,0)!=1)return -2;
if(!b->id){b->poisoned=1;return -2;}b->scale[0]=b->scale[1]=b->scale[2]=4096;b->body_flags=0x16;b->alive=1;return 1;}
static int item(SmN64DomeBody *b,const SmN64DomeActorHost *h,const char *name){SmN64DomeCall q;memset(&q,0,sizeof(q));q.operation=SMN64_DOME_INIT_ITEM;q.source=0x82ed4;q.name=name;return event(b,h,&q);}
static int bind(SmN64DomeBody *b,const SmN64DomeActorHost *h,uint32_t slot){
    if(simple(b,h,SMN64_DOME_LOOKUP_BUNDLE,0x5961c,slot,0)!=1||simple(b,h,SMN64_DOME_LOOKUP_MODEL,0x596e8,b->model,0)!=1||simple(b,h,SMN64_DOME_BIND_RENDER,0x55fe4,UINT32_MAX,UINT32_MAX)!=1)return -2;

    if(!b->render){b->poisoned=1;return -2;}
return 1;
}
static int mesh(SmN64DomeBody *b,SmN64DomeMesh *m,const SmN64DomeActorHost *h){SmN64DomeCall q;memset(&q,0,sizeof(q));q.operation=SMN64_DOME_GET_MESH;q.source=0x51174;if(event(b,h,&q)!=1)return -2;
if(!q.mesh.vertices||!q.mesh.count||q.mesh.count>SMN64_DOME_RING_CAPACITY){b->poisoned=1;return -2;}*m=q.mesh;return 1;}
static void position(SmN64DomeBody *b,const SmN64DomePlayer *p){memcpy(b->position,p->position,sizeof(b->position));b->position[1]=add(b->position[1],(int32_t)((uint32_t)p->offset_11a0<<12));}
int smn64_dome_actor_init(SmN64HeldDome *d,SmN64DomeWorld *w,const SmN64DomePlayer *p,uint32_t fire,const SmN64DomeActorHost *h){
    SmN64DomeCall q;if(!d||!w||!p||!p->id||!h||!h->service)return -1;
memset(d,0,sizeof(*d));if(base(&d->body,h,0x120)!=1)return -2;

    memset(&q,0,sizeof(q));q.operation=SMN64_DOME_MAKE_HANDLE;q.source=0x7fb3c;q.args[0]=p->id;if(event(&d->body,h,&q)!=1)return -2;
d->player=q.args[0];d->generation=q.args[1];if(!d->player){d->body.poisoned=1;return -2;}
    position(&d->body,p);d->web_type=fire;
    if(item(&d->body,h,fire?"firedome":"webdome2")!=1)return -2;

    if(fire){d->body.model=1;w->fire_count++;d->body.flags|=0x200;d->body.scale[1]=0;}
    if(bind(&d->body,h,fire?165:248)!=1)return -2;
d->body.render_bc=254;d->body.render_bd=1;d->body.flags|=1;
    if(simple(&d->body,h,SMN64_DOME_ATTACH_MISC,0x82e38,0xf646c,0)!=1)return -2;
d->phase=0;w->dome_count++;return 1;
}
static int piece_init(SmN64DomePiece *p,const int32_t pos[3],unsigned model,unsigned delay,const SmN64DomeActorHost *h){
    memset(p,0,sizeof(*p));if(base(&p->body,h,0x100)!=1)return -2;
memcpy(p->body.position,pos,sizeof(p->body.position));p->delay=(int32_t)delay;
    if(item(&p->body,h,"webdome3")!=1||simple(&p->body,h,SMN64_DOME_LOOKUP_BUNDLE,0x5961c,249,0)!=1)return -2;
p->body.model=(uint16_t)model;
    if(simple(&p->body,h,SMN64_DOME_ATTACH_MISC,0x82e38,0xf646c,0)!=1)return -2;
p->body.flags|=0x400;p->body.rgb=0;p->fade_step=4;
    if(simple(&p->body,h,SMN64_DOME_LOOKUP_MODEL,0x596e8,model,0)!=1||simple(&p->body,h,SMN64_DOME_BIND_RENDER,0x55fe4,UINT32_MAX,UINT32_MAX)!=1)return -2;

    if(!p->body.render){p->body.poisoned=1;return -2;}p->body.render_bd=1;p->body.render_bc=254;p->body.body_flags|=0x20;return 1;
}
int smn64_dome_actor_tick(SmN64HeldDome *d,uint32_t rng[3],const SmN64DomeActorHost *h){
    SmN64DomePlayer p;int rc;unsigned i;static const unsigned models[5]={3,0,4,1,2};
    if(!d||!rng||!good(&d->body,h)||!h->player)return -1;
if(!d->body.alive)return 0;

    memset(&p,0,sizeof(p));rc=h->player(h->context,d->player,d->generation,&p);if(rc<0){d->body.poisoned=1;return -2;}if(!rc)return 2;
if(rc!=1||p.id!=d->player){d->body.poisoned=1;return -2;}position(&d->body,&p);
    if(d->web_type){if(d->phase==0){if(p.frame>=4)d->phase=1;return 1;}if(d->phase==1){SmN64DomeCall q;size_t n;d->body.flags&=0xfffeu;d->body.scale[1]=s16((uint32_t)(int32_t)d->body.scale[1]+600u);if(d->body.scale[1]>4096)d->body.scale[1]=4096;
        if(mesh(&d->body,&d->mesh,h)!=1)return -2;
n=smn64_web_random(rng,(uint32_t)d->mesh.count);if(n>=d->mesh.count){d->body.poisoned=1;return -2;}
        memset(&q,0,sizeof(q));q.operation=SMN64_DOME_FIRE_SCRIPT;q.source=0x8a5e8;q.name="simby";q.args[0]=5;q.args[1]=256;q.position[0]=add(d->body.position[0],s32((uint32_t)(int32_t)d->mesh.vertices[n].x<<12));q.position[1]=add(d->body.position[1],s32((uint32_t)(int32_t)d->mesh.vertices[n].y<<12));q.position[2]=add(d->body.position[2],s32((uint32_t)(int32_t)d->mesh.vertices[n].z<<12));if(event(&d->body,h,&q)!=1)return -2;}
return 1;}
    if(d->phase==0&&p.frame>=4){for(i=0;i<5;i++)if(piece_init(&d->pieces[i],d->body.position,models[i],i*2,h)!=1){d->body.poisoned=1;return -2;}d->phase=1;}
    else if(d->phase==1&&d->pieces[3].body.rgb==0x808080){d->body.flags&=0xfffeu;d->phase=2;}
return 1;
}
int smn64_dome_piece_tick(SmN64DomePiece *p,const SmN64DomeActorHost *h){int32_t dy,c;if(!p||!good(&p->body,h)||!h->vertical_delta)return -1;
if(!p->body.alive)return 0;

    if(p->delay)p->delay=add(p->delay,-1);else{c=add((int32_t)(p->body.rgb&255),p->fade_step);if(c>128)c=128;if(c<0)c=0;p->body.rgb=(uint32_t)c*0x10101u;}
    if(h->vertical_delta(h->context,&dy)!=1){p->body.poisoned=1;return -2;}p->body.position[1]=add(p->body.position[1],dy);return 1;
}
int smn64_dome_ring_capture(SmN64DomeRing *r){size_t i;if(!r||!r->mesh.vertices||!r->mesh.count||r->mesh.count>SMN64_DOME_RING_CAPACITY)return -1;

    for(i=0;i<r->mesh.count;i++){SmN64DomeVertex *v=&r->mesh.vertices[i];uint32_t q=(uint32_t)((int32_t)v->x*(int32_t)v->x)+(uint32_t)((int32_t)v->z*(int32_t)v->z);int32_t len=(int32_t)sqrtf((float)q);if(!len)return -2;

        r->saved[i][0]=v->x;r->saved[i][1]=v->rgba[3];r->saved[i][2]=v->z;r->velocity[i][0]=s16((uint32_t)(s32((uint32_t)(int32_t)v->x*(uint32_t)r->speed)/len));r->velocity[i][2]=s16((uint32_t)(s32((uint32_t)(int32_t)v->z*(uint32_t)r->speed)/len));}
    r->saved_count=r->mesh.count;return 1;
}
int smn64_dome_ring_restore(SmN64DomeRing *r){size_t i;if(!r||!r->mesh.vertices||r->saved_count!=r->mesh.count||r->saved_count>SMN64_DOME_RING_CAPACITY)return -1;
for(i=0;i<r->saved_count;i++){r->mesh.vertices[i].x=r->saved[i][0];r->mesh.vertices[i].z=r->saved[i][2];r->mesh.vertices[i].rgba[3]=(uint8_t)r->saved[i][1];}
return 1;}
int smn64_dome_ring_init(SmN64DomeRing *r,SmN64DomeWorld *w,const int32_t pos[3],uint32_t fire,const SmN64DomeActorHost *h){
    if(!r||!w||!pos||!h||!h->service||r==w->ring)return -1;
memset(r,0,sizeof(*r));if(base(&r->body,h,0x118)!=1)return -2;
if(w->ring&&smn64_dome_ring_destroy(w->ring,w,1,h)!=1){r->body.poisoned=1;return -2;}w->ring=r;r->web_type=fire;
    if(item(&r->body,h,fire?"firering":"ring")!=1||simple(&r->body,h,SMN64_DOME_ATTACH_MISC,0x82e38,0xf646c,0)!=1)return -2;
r->body.flags|=fire?0x200:0x600;r->body.rgb=0x808080;memcpy(r->body.position,pos,sizeof(r->body.position));r->body.position[1]=add(r->body.position[1],-368640);r->speed=100;r->fade_step=3;
    if(bind(&r->body,h,fire?166:226)!=1)return -2;
r->body.render_bd=1;if(mesh(&r->body,&r->mesh,h)!=1)return -2;

    if(simple(&r->body,h,SMN64_DOME_ALLOC_BUFFER,0x7fc40,(uint32_t)r->mesh.count*6u,0)!=1||simple(&r->body,h,SMN64_DOME_ALLOC_BUFFER,0x7fc40,(uint32_t)r->mesh.count*6u,1)!=1)return -2;
if(smn64_dome_ring_capture(r)!=1){r->body.poisoned=1;return -2;}
return 1;
}
int smn64_dome_ring_tick(SmN64DomeRing *r,const SmN64DomeActorHost *h){int32_t c;size_t i;if(!r||!good(&r->body,h)||r->saved_count!=r->mesh.count||!r->mesh.vertices||!r->saved_count||r->saved_count>SMN64_DOME_RING_CAPACITY)return -1;
if(!r->body.alive)return 0;

    c=add((int32_t)(r->body.rgb&255),s32(0u-(uint32_t)r->fade_step));if(c<=0){c=0;if(simple(&r->body,h,SMN64_DOME_BODY_DIE,0x829fc,0,0)!=1)return -2;
r->body.body_flags|=0x40;}
    r->body.rgb=(uint32_t)c*0x10101u;for(i=0;i<r->mesh.count;i++){r->mesh.vertices[i].x=s16((uint32_t)(int32_t)r->mesh.vertices[i].x+(uint32_t)(int32_t)r->velocity[i][0]);r->mesh.vertices[i].z=s16((uint32_t)(int32_t)r->mesh.vertices[i].z+(uint32_t)(int32_t)r->velocity[i][2]);r->mesh.vertices[i].rgba[3]=(uint8_t)(s32((uint32_t)c<<1)<256?s32((uint32_t)c<<1):255);}
return 1;
}
static int destroy_piece(SmN64DomePiece *p,const SmN64DomeActorHost *h){if(!p->body.alive)return 1;
if(simple(&p->body,h,SMN64_DOME_RELEASE_RENDERS,0x590f4,0,0)!=1||simple(&p->body,h,SMN64_DOME_DETACH_MISC,0x82dac,0xf646c,0)!=1||simple(&p->body,h,SMN64_DOME_BASE_DESTROY,0x82e5c,0,0)!=1||simple(&p->body,h,SMN64_DOME_FREE_BODY,0x82fc0,0,0)!=1)return -2;
p->body.alive=0;return 1;}
int smn64_dome_actor_destroy(SmN64HeldDome *d,SmN64DomeWorld *w,uint8_t free_storage,const SmN64DomeActorHost *h){unsigned i;if(!d||!w||!good(&d->body,h))return -1;
if(!d->body.alive)return 0;

    if(simple(&d->body,h,SMN64_DOME_DETACH_MISC,0x82dac,0xf646c,0)!=1||simple(&d->body,h,SMN64_DOME_RELEASE_RENDERS,0x590f4,0,0)!=1)return -2;
for(i=0;i<5;i++)if(destroy_piece(&d->pieces[i],h)!=1){d->body.poisoned=1;return -2;}
    if(d->web_type)w->fire_count--;
w->dome_count--;if(simple(&d->body,h,SMN64_DOME_BASE_DESTROY,0x82e5c,0,0)!=1)return -2;
if(free_storage&&simple(&d->body,h,SMN64_DOME_FREE_BODY,0x82fc0,0,0)!=1)return -2;
d->body.alive=0;return 1;
}
int smn64_dome_ring_destroy(SmN64DomeRing *r,SmN64DomeWorld *w,uint8_t free_storage,const SmN64DomeActorHost *h){if(!r||!w||!good(&r->body,h))return -1;
if(!r->body.alive)return 0;

    if(simple(&r->body,h,SMN64_DOME_RELEASE_RENDERS,0x590f4,0,0)!=1||simple(&r->body,h,SMN64_DOME_DETACH_MISC,0x82dac,0xf646c,0)!=1)return -2;
if(smn64_dome_ring_restore(r)!=1){r->body.poisoned=1;return -2;}
    if(simple(&r->body,h,SMN64_DOME_FREE_BUFFER,0x7fba4,0,0)!=1||simple(&r->body,h,SMN64_DOME_FREE_BUFFER,0x7fba4,1,0)!=1)return -2;
w->ring=NULL;if(simple(&r->body,h,SMN64_DOME_BASE_DESTROY,0x82e5c,0,0)!=1)return -2;
if(free_storage&&simple(&r->body,h,SMN64_DOME_FREE_BODY,0x82fc0,0,0)!=1)return -2;
r->body.alive=0;return 1;
}
int smn64_dome_actor_release(SmN64HeldDome *d,SmN64DomeWorld *w,SmN64DomeRing *r,SmN64DomePulse *p,uint32_t rng[3],const int32_t pos[3],const SmN64DomeHost *ph,const SmN64DomeActorHost *h){int32_t dy;SmN64DomeCall q;if(!d||!w||!r||!p||!rng||!pos||!ph||!good(&d->body,h)||!h->vertical_delta)return -1;
if(!d->body.alive)return 0;

    if(!d->web_type&&d->pieces[2].body.alive){d->pieces[2].body.flags&=0xfbffu;if(simple(&d->pieces[2].body,h,SMN64_DOME_FADE_30,0x82f04,30,30)!=1){d->body.poisoned=1;return -2;}}
    d->phase=3;if(simple(&d->body,h,SMN64_DOME_ALLOC_PULSE,0x6a94c,0x98,0xf5530)!=1)return -2;
if(smn64_dome_pulse_init(p,pos,d->web_type,rng,ph)!=1){d->body.poisoned=1;return -2;}
    memset(&q,0,sizeof(q));q.operation=SMN64_DOME_SHAKE;q.source=0x6ccb8;q.args[0]=1;memcpy(q.position,p->position,sizeof(q.position));if(event(&d->body,h,&q)!=1)return -2;

    if(smn64_dome_ring_init(r,w,d->body.position,d->web_type,h)!=1){d->body.poisoned=1;return -2;}if(simple(&d->body,h,SMN64_DOME_BODY_DIE,0x829fc,0,0)!=1)return -2;
d->body.body_flags|=0x40;
    if(h->vertical_delta(h->context,&dy)!=1){d->body.poisoned=1;return -2;}d->body.position[1]=add(d->body.position[1],dy);return 1;
}
static uint32_t fade(uint32_t c,unsigned amount){unsigned i;uint32_t out=c&0xff000000u;for(i=0;i<3;i++){uint32_t x=(c>>(8*i))&255u;out|=(x<amount?0:x-amount)<<(8*i);}
return out;}
int smn64_dome_impact_init(SmN64DomeImpact *s,const int32_t pos[3],uint32_t rng[3]){unsigned i;if(!s||!pos||!rng)return -1;
memset(s,0,sizeof(*s));memcpy(s->position,pos,sizeof(s->position));s->angle_step=409;s->alive=1;s->center_rgb=0x32000000;memset(s->gradient_rgb,128,sizeof(s->gradient_rgb));s->texture_alpha=255;
    for(i=0;i<10;i++){s->radius[i]=70+(int32_t)smn64_web_random(rng,0);s->outer_radius[i]=100;s->point_rgb[i]=0;s->outer_rgb[i]=0x3a000000;}s->bound=210.0f;return 1;}
int smn64_dome_impact_tick(SmN64DomeImpact *s){unsigned i;if(!s)return -1;
if(!s->alive)return 0;
for(i=0;i<10;i++){s->point_rgb[i]=fade(s->point_rgb[i],20);s->center_rgb=fade(s->center_rgb,2);}if(!(s->center_rgb&0xffffffu))s->alive=0;return s->alive?1:0;}
