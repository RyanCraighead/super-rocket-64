#include "traversal_n64.h"
#include "../movement/locomotion_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return si((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return si((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return si((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t x,unsigned n){uint32_t u=(uint32_t)x;return si((u>>n)|((u&0x80000000u)?(UINT32_MAX<<(32-n)):0u));}
static int32_t shl(int32_t x,unsigned n){return si((uint32_t)x<<n);}
static float f32(float x){volatile float r=x;return r;}
static void copy(int32_t d[3],const int32_t s[3]){memcpy(d,s,3*sizeof(*d));}
static void norm(int32_t v[3]){
    float x=f32((float)v[0]*0x1p-12f),y=f32((float)v[1]*0x1p-12f),z=f32((float)v[2]*0x1p-12f);
    float len=sqrtf(f32(f32(f32(x*x)+f32(y*y))+f32(z*z)));
    if(len==0){memset(v,0,3*sizeof(*v));return;}
    float inv=f32(1.0f/len);
    v[0]=(int32_t)f32(f32(x*inv)*4096.0f);
    v[1]=(int32_t)f32(f32(y*inv)*4096.0f);
    v[2]=(int32_t)f32(f32(z*inv)*4096.0f);
}
static int32_t length(const int32_t v[3]){
    int32_t x=sar(v[0],12),y=sar(v[1],12),z=sar(v[2],12);
    uint32_t square=(uint32_t)x*(uint32_t)x+(uint32_t)y*(uint32_t)y+(uint32_t)z*(uint32_t)z;
    return (int32_t)sqrtf((float)square);
}
int smn64_web_zip_eligible(const SmN64WebPlayer *s,const SmN64WebLine *h,int32_t max){
    if(!s||!h)return -1;
    if(h->distance<=(s->state==4?8:16)||h->distance>=max||!h->surface||!h->hit||(h->surface_flags&4))return 0;
    float d[3];
    for(int i=0;i<3;i++)d[i]=f32((float)h->position[i]-f32((float)s->position[i]-f32((float)s->outward[i]*(float)s->body_offset)));
    float len=sqrtf(f32(f32(f32(d[0]*d[0])+f32(d[1]*d[1]))+f32(d[2]*d[2])));
    return s->state==4 || (int32_t)f32(len*0x1p-12f)>=65;
}
int smn64_web_swing_prepare(SmN64WebPlayer *s,const SmN64WebLine *h){
    if(!s||!h)return -1;
    if(h->normal[1]>=3401||h->distance<=512||h->distance>=4096||!h->surface||!h->hit||(h->surface_flags&4)||s->ceiling_orientation||!(s->state&0x4000fu))return 0;
    copy(s->target,h->position);
    int far=h->distance>3072;int32_t base[3],d[3],unit[3],n[3],q[3];copy(base,s->position);
    if(s->state&1u){
        base[1]=sub(base[1],0x244000);
        q[0]=sar(sub(s->target[0],s->position[0]),12);q[1]=0;q[2]=sar(sub(s->target[2],s->position[2]),12);norm(q);
        for(int i=0;i<3;i++)base[i]=add(base[i],mul(q[i],320));
    }
    for(int i=0;i<3;i++)d[i]=sub(s->target[i],base[i]);
    if(h->normal[1]>=-2600 && h->normal[1]<=3400)for(int i=0;i<3;i++)d[i]=add(d[i],d[i]/32);
    int32_t len=length(d);if(!len)return -2;
    for(int i=0;i<3;i++){unit[i]=d[i]/len;d[i]/=2;}
    len/=2;
    if(unit[1]<=-2896||unit[1]>=2896)return 0;
    for(int i=0;i<3;i++)q[i]=sar(d[i],8);
    n[0]=sar(sub(0,mul(q[1],sub(0,q[0]))),8);
    n[1]=sar(sub(mul(q[0],sub(0,q[0])),mul(q[2],q[2])),8);
    n[2]=sar(mul(q[1],q[2]),8);
    int32_t nl=length(n);if(!nl)return -2;
    for(int i=0;i<3;i++)n[i]/=nl;
    copy(s->target_normal,n);len=shl(len,1);
    for(int i=0;i<3;i++)s->anchor[i]=add(add(base[i],far?d[i]/2:d[i]),mul(n[i],len));
    if(far)for(int i=0;i<3;i++)s->second_anchor[i]=add(s->anchor[i],d[i]);
    s->second_web=far;return 1;
}
static int event(SmN64WebEvents *e,SmN64WebEventKind k,int32_t v,int32_t x){
    if(e->count>=16)return -3;
    e->events[e->count++]=(SmN64WebEvent){k,v,x};return 1;
}
static void play(SmN64WebPlayer *s,uint16_t clip,int from,const uint16_t *c){smn64_anim_run(&s->anim,clip,c[clip],from,-1);}
static int check(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,const uint16_t *c,size_t n,SmN64WebEvents *e){
    if(!s||!in||!ray||!c||n<300||!e||e->count>8)return -1;
    const int clips[]={250,260,270,273,282};for(unsigned i=0;i<sizeof(clips)/sizeof(*clips);i++)if(!c[clips[i]])return -1;
    return 1;
}
static int swing_button(const SmN64WebPlayer *s,const SmN64WebInput *in){
    if(in->swing_held&&(!in->kid_mode||s->resource.player_2cc))return 1;
    return in->kid_mode&&(s->state&6u)&&s->kid_jump_gate&&in->jump_held;
}
static int zip_finish(SmN64WebPlayer *s,const SmN64WebLine *h,int is_b,const uint16_t *c,SmN64WebEvents *e,SmN64WebLifecycle visual,void *ctx){
    copy(s->target,h->position);for(int i=0;i<3;i++)s->target_normal[i]=h->normal[i];copy(s->zip_origin,s->position);
    s->auxiliary_a38=0;s->turn_ticks=0;
    if(s->state&1u)play(s,s->adhered?260:250,0,c);
    else{
        if(is_b&&s->swinger_present){if(visual&&visual(ctx,SMN64_WEB_DELETE_SWINGER,s,NULL)!=1)return -6;s->swinger_present=0;s->airborne_owner=0;event(e,SMN64_WEB_DELETE_SWINGER,0,0);event(e,SMN64_WEB_CAMERA,-1,0);}
        s->zip_graphic=1;s->web_mode=8;if(visual&&visual(ctx,SMN64_WEB_CREATE_ZIP,s,NULL)!=1)return -6;event(e,SMN64_WEB_CREATE_ZIP,s->resource.web_type,0);
        s->resource.allow_empty=1;SmN64WebResourceEvent re;
        smn64_web_consume(&s->resource,128,s->random_state,&re);
        if(re.sound)event(e,SMN64_WEB_REFILL_SOUND,re.sound,0);
        if(re.voice_group)event(e,SMN64_WEB_EMPTY_VOICE,re.voice_group,re.voice_variant);
        if(visual&&visual(ctx,SMN64_WEB_FIRE_ZIP,s,NULL)!=1)return -6;
        s->resource.allow_empty=0;
        event(e,SMN64_WEB_FIRE_ZIP,128,1);event(e,SMN64_WEB_SOUND_POSITION,21,0);play(s,270,13,c);
    }
    s->state=0x40000;return 1;
}
static int trace_zip(SmN64WebPlayer *s,SmN64WebRay ray,void *ctx,const int32_t to[3],int max,int is_b,const uint16_t *c,SmN64WebEvents *e,SmN64WebLifecycle visual){
    SmN64WebLine h;memset(&h,0,sizeof(h));int rc=ray(ctx,is_b?SMN64_WEB_ZIP_B:SMN64_WEB_ZIP_R,s->position,to,&h);if(rc!=1)return -3;
    if(!h.hit||!h.surface)return 0;
    if((h.surface_flags&4)&&h.normal[1]>=-2600)h.hit=0;
    rc=smn64_web_zip_eligible(s,&h,max);if(rc<=0)return rc;
    return zip_finish(s,&h,is_b,c,e,visual,ctx);
}
static int try_zip_b_impl(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e,SmN64WebLifecycle visual){
    if(check(s,in,ray,c,n,e)<0)return -1;
    if(in->stable_updates<5||s->aiming||s->holding||s->lock||!in->zip_held)return 0;
    int32_t to[3];for(int i=0;i<3;i++)to[i]=add(s->position[i],mul(s->outward[i],3072));
    return trace_zip(s,ray,ctx,to,3072,1,c,e,visual);
}
static int try_zip_r_impl(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e,SmN64WebLifecycle visual){
    if(check(s,in,ray,c,n,e)<0)return -1;
    if(s->aiming||s->holding||s->lock||!swing_button(s,in))return 0;
    int32_t offset=(s->state&6u)?256-(int32_t)smn64_web_random(s->random_state,512):512;
    int32_t to[3];for(int i=0;i<3;i++)to[i]=add(sub(s->position[i],mul(s->forward[i],2048)),mul(s->outward[i],offset));
    return trace_zip(s,ray,ctx,to,2048,0,c,e,visual);
}
static int swing_ray(SmN64WebPlayer *s,SmN64WebRay ray,void *ctx,SmN64WebQueryKind kind,const int32_t from[3],const int32_t to[3],SmN64WebLine *h){
    memset(h,0,sizeof(*h));int rc=ray(ctx,kind,from,to,h);if(rc!=1)return -3;
    if((h->surface_flags&4)&&h->normal[1]>=-2600)h->hit=0;
    return h->hit?smn64_web_swing_prepare(s,h):0;
}
static void angle_ray(const SmN64WebPlayer *s,const int32_t basis[3],int angle,int32_t to[3]){
    int32_t sn=smn64_locomotion_sin(angle),cs=smn64_locomotion_cos(angle);
    for(int i=0;i<3;i++)to[i]=sub(add(s->position[i],shl(sar(mul(basis[i],sn),12),12)),shl(sar(mul(s->forward[i],cs),12),12));
    to[1]=sub(to[1],0x40000);
}
static int try_swing_impl(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e){
    if(check(s,in,ray,c,n,e)<0)return -1;
    if(!swing_button(s,in)||s->aiming||s->holding||s->lock)return 0;
    if(s->swing_scan_vertical<0||s->swing_scan_vertical>=6||s->swing_scan_lateral<0||s->swing_scan_lateral>=6)return -1;
    int32_t from[3],to[3];copy(from,s->position);from[1]=sub(from[1],0x40000);
    for(int i=0;i<3;i++)to[i]=sub(s->position[i],mul(s->forward[i],4096));
    SmN64WebLine h;int rc=swing_ray(s,ray,ctx,SMN64_WEB_SWING_FIRST,from,to,&h);if(rc<0)return rc;
    if(!rc&&(s->state&0x40006u)){
        /* Tables800F7AB0 and800F7A98 are revision-locked authored ray angles. */
        static const int16_t vertical[6]={-171,171,-341,341,-512,512};
        static const int32_t lateral[6]={57,-57,114,-114,171,-171};
        int angle=vertical[s->swing_scan_vertical];s->swing_scan_vertical=(s->swing_scan_vertical+1)%6;
        angle_ray(s,s->outward,angle,to);rc=swing_ray(s,ray,ctx,SMN64_WEB_SWING_VERTICAL,from,to,&h);if(rc<0)return rc;
        if(rc){
            while(angle<-56){
                angle+=57;angle_ray(s,s->outward,angle,to);
                rc=swing_ray(s,ray,ctx,SMN64_WEB_SWING_REFINE,from,to,&h);if(rc<0)return rc;
                if(!rc){rc=1;break;} /* original keeps prior prepared geometry but final ray hit position */
            }
        }
        if(!rc){
            angle=lateral[s->swing_scan_lateral];s->swing_scan_lateral=(s->swing_scan_lateral+1)%6;
            angle_ray(s,s->right,angle,to);rc=swing_ray(s,ray,ctx,SMN64_WEB_SWING_LATERAL,from,to,&h);if(rc<0)return rc;
        }
    }
    if(!rc)return 0;
    copy(s->target,h.position);copy(s->swing_target,h.position);s->auxiliary_a38=0;s->adhered=0;s->turn_ticks=0;
    if(s->state&9u){s->state=0x100;play(s,273,0,c);}
    else{s->airborne_owner=1;s->state=0x200;s->substate=0;play(s,282,0,c);if(s->velocity[1]>0)s->velocity[1]=0;}
    s->aim_pressed=1;s->aim_copied=1;return 1;
}


#define ZIP_WRAPPER(NAME) \
int smn64_web_try_##NAME##_visual(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e,SmN64WebLifecycle visual){ \
    if(!s||!e||!visual)return -1; \
    SmN64WebPlayer work=*s;SmN64WebEvents pending=*e; \
    int rc=try_##NAME##_impl(&work,in,ray,ctx,c,n,&pending,visual); \
    if(rc>=0){*s=work;*e=pending;}return rc; \
} \
int smn64_web_try_##NAME(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e){ \
    if(!s||!e)return -1; \
    SmN64WebPlayer work=*s;SmN64WebEvents pending=*e; \
    int rc=try_##NAME##_impl(&work,in,ray,ctx,c,n,&pending,NULL); \
    if(rc>=0){*s=work;*e=pending;}return rc; \
}
ZIP_WRAPPER(zip_b)
ZIP_WRAPPER(zip_r)
#undef ZIP_WRAPPER
int smn64_web_try_swing(SmN64WebPlayer *s,const SmN64WebInput *in,SmN64WebRay ray,void *ctx,const uint16_t *c,size_t n,SmN64WebEvents *e){
    if(!s||!e)return -1;
    SmN64WebPlayer work=*s;SmN64WebEvents pending=*e;
    int rc=try_swing_impl(&work,in,ray,ctx,c,n,&pending);
    if(rc>=0){*s=work;*e=pending;}return rc;
}
