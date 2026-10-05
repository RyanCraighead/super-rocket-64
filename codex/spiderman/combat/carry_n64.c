#include "carry_n64.h"
#include "../web/resource_n64.h"
#include "../movement/locomotion_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return s32((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t v,unsigned n){return s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0));}
static int32_t dist(const int32_t a[3],const int32_t b[3],int planar){uint32_t sq=0;unsigned i;for(i=0;i<3;i++){int32_t v=planar&&i==1?0:sar(sub(a[i],b[i]),12);sq+=(uint32_t)v*(uint32_t)v;}return (int32_t)sqrtf((float)sq);}
int smn64_carry_position(const int32_t p1[3],const int32_t p0[3],const int32_t player[3],const int32_t forward[3],int16_t radius,uint16_t oldyaw,uint16_t delta,int32_t out[3],uint16_t *yaw){
    int32_t mid[3],dir[3];unsigned i;if(!p1||!p0||!player||!forward||!out||!yaw)return -1;
    for(i=0;i<3;i++){mid[i]=add(p1[i],sub(p0[i],p1[i])/2);dir[i]=sar(sub(mid[i],player[i]),6);}
    smn64_combat_normalize(dir,dir);for(i=0;i<3;i++)out[i]=add(add(mid[i],mul(dir[i],radius)),mul(forward[i],32));*yaw=(uint16_t)(oldyaw+delta);return 1;
}
int smn64_throw_path(const int32_t object[3],const int32_t target[3],uint32_t ticks,int32_t (*points)[3],size_t capacity){
    int32_t step[3],p[3],angle=0,angle_step;unsigned i,j;
    if(!object||!target||!points||ticks<2||ticks>2048||capacity<ticks)return -1;
    for(j=0;j<3;j++){step[j]=sub(target[j],object[j])/(int32_t)ticks;p[j]=add(object[j],step[j]);}
    angle_step=2048/((int32_t)ticks-1);
    for(i=0;i<ticks;i++){
        points[i][0]=p[0];points[i][1]=sub(p[1],s32((uint32_t)smn64_locomotion_sin(angle)<<8));points[i][2]=p[2];
        for(j=0;j<3;j++)p[j]=add(p[j],step[j]);
        angle=add(angle,angle_step);
    }
    return 1;
}
static int carry_step(SmN64Carry *s,uint32_t rng[3],const SmN64CarryHost *h,SmN64CarryEvent *e,SmN64CarryOrderedThrow ordered){
    uint16_t clip;int16_t frame;SmN64CarryObject object,target;int rc,big=0;unsigned i;
    if(!s||!rng||!h||!h->object||!h->pickup||(!h->throw_object&&!ordered)||!h->stop||!h->jump||!e)return -1;
    memset(e,0,sizeof(*e));clip=s->anim.animation;frame=s->anim.frame;
    if(s->state==0x100000){
        if(s->anim.finished||(!s->held_actor&&!clip))return h->stop(h->context,s)==1?1:-2;
        if(s->held_actor)return 1;
        if((clip!=196&&clip!=190)||frame<10)return 1;
        rc=h->object(h->context,s->pickup_actor,&object);if(rc<0)return -2;if(!rc)return 1;
        if(dist(s->position,object.position,0)>=768)return 1;
        s->held_actor=object.id;if(h->pickup(h->context,object.id)!=1)return -2;e->sound=7;return 1;
    }
    if(s->state==0x200000){
        SmN64ThrowRequest request;int active=(clip==201&&frame>=18)||(clip==195&&frame>=13);
        if(s->anim.finished)return h->stop(h->context,s)==1?1:-2;
        if(!s->held_actor){rc=h->jump(h->context,s);return rc<0?-2:1;}
        if(frame>=6)e->camera_reset=1;
        if(!active)return 1;
        big=clip==201;memset(&request,0,sizeof(request));request.actor=s->held_actor;
        if(s->target_actor){
            int32_t distance,time,vertical;
            rc=h->object(h->context,s->target_actor,&target);if(rc!=1)return -2;
            distance=dist(s->position,target.position,1);
            if(distance>=1024){request.use_path=1;request.path_ticks=(uint32_t)(distance/32);memcpy(request.target,target.position,sizeof(request.target));}
            else {
                time=s32((uint32_t)distance<<12)/(big?24:32);if(time<81920)time=81920;
                for(i=0;i<3;i++)request.velocity[i]=mul(s->forward[i],big?-24:-32);
                vertical=sub(add(target.position[1],524288),s->position[1])/time-time/2;
                request.velocity[1]=add(request.velocity[1],vertical);
            }
        }else for(i=0;i<3;i++)request.velocity[i]=add(mul(s->forward[i],big?-24:-32),mul(s->up[i],big?14:16));
        if(ordered){if(ordered(h->context,&request,rng)!=1)return -2;}
        else {
            request.spin=(uint16_t)(smn64_web_random(rng,32)+64);
            if(h->throw_object(h->context,&request)!=1)return -2;
        }
        e->sound=big?0x26:0x25;s->held_actor=0;return 1;
    }
    return 0;
}

int smn64_carry_step(SmN64Carry *s,uint32_t rng[3],const SmN64CarryHost *h,SmN64CarryEvent *e){
    return carry_step(s,rng,h,e,NULL);
}
int smn64_carry_step_ordered(SmN64Carry *s,uint32_t rng[3],const SmN64CarryHost *h,
    SmN64CarryEvent *e,SmN64CarryOrderedThrow ordered){
    if(!ordered)return -1;
    return carry_step(s,rng,h,e,ordered);
}
