#include "transitions_n64.h"
#include "../movement/locomotion_n64.h"
#include <string.h>
static int32_t bits(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return bits((uint32_t)a*(uint32_t)b);}
static int32_t shr12(int32_t a){return a>=0?a/4096:-1-(int32_t)((uint32_t)(-1-a)>>12);}
static int32_t dot(const int32_t a[3],const int16_t b[3]){int32_t sum=0;for(int i=0;i<3;i++)sum=add(sum,shr12(mul(a[i],b[i])));return sum;}
static int run(SmN64ClimbState *s,unsigned clip,const uint16_t *counts,size_t n){if(!counts || clip>=n || !counts[clip])return -1;smn64_anim_run(&s->anim,(uint16_t)clip,counts[clip],0,-1);return 1;}
static int orient(SmN64ClimbState *s,const int32_t f[3],const uint16_t *counts,size_t n){if(!smn64_climb_basis(&s->basis,f))return smn64_climb_stop(s,counts,n);return 1;}
static int endpoint(SmN64ClimbState *s,unsigned clip,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *ev,const uint16_t *counts,size_t n){
    if(!env || !env->marker)return -3;
    if(run(s,clip,counts,n)<0)return -1;
    s->anim.frame=(int16_t)(counts[clip]-1);
    if(env->marker(env->context,s,2,1,ev->camera_target)!=1)return -3;
    ev->camera_flags|=1;ev->camera_ticks=(uint16_t)(counts[clip]*2);return 1;
}
static void yaw_event(SmN64ClimbTransitionEvent *e,const SmN64ClimbHit *h){e->camera_flags|=2;e->camera_yaw=(uint16_t)((1024-smn64_locomotion_atan(h->normal[2],h->normal[0]))&4095);}
static void profile_event(SmN64ClimbTransitionEvent *e,int profile){e->camera_flags|=4;e->camera_profile=profile;}
int smn64_climb_corner_begin(SmN64ClimbState *out,int outer,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n){
    if(!out || !event || !env)return -1;
    if(!out->side.present || (!!out->d48)!=!!outer || !out->adhered || (!outer && out->side.distance<0))return 0;
    if(!out->side.has_surface)return -1;
    SmN64ClimbState c=*out,*s=&c;SmN64ClimbTransitionEvent e;memset(&e,0,sizeof e);
    int32_t normal[3]={s->basis.normal[0],s->basis.normal[1],s->basis.normal[2]};
    int aligned=dot(normal,s->side.normal)>=1567,special=s->cf4 || (s->side.surface_flags&256);
    int to_floor=0,to_wall=0,to_ceiling=0;unsigned clip;
    if(!outer){
        clip=aligned?72:60;if(special)clip=aligned?78:66;
        if(s->wall_class){smn64_climb_transition_input(s,s->side.normal,0);if(s->side.normal[1]>=3401)to_ceiling=1;else if(s->side.normal[1]<-2600)to_floor=1;}
        else if(s->ceiling_class){smn64_climb_transition_input(s,s->side.normal,0);if(s->side.normal[1]<3401)to_wall=1;}
        else if(s->side.normal[1]>=-2600 && s->side.normal[1]<=3400)to_wall=1;
        if(to_floor){const int32_t up[3]={0,-4096,0};if(orient(s,up,counts,n)<0)return -2;clip=s->cf4?88:87;}
        else if(!aligned){int32_t left[3];for(int i=0;i<3;i++)left[i]=-s->basis.right[i];int32_t d=dot(left,s->side.normal);if(d>2048)clip=special?68:62;else if(d< -2048)clip=special?67:61;}
        s->cf8=0;
        if(!env->camera_special && !to_floor && !to_wall && !to_ceiling)yaw_event(&e,&s->side);
    }else{
        clip=aligned?75:63;if(special)clip=aligned?81:69;
        if(s->wall_class && s->side.normal[1]<-2600){const int32_t down[3]={0,4096,0};if(orient(s,down,counts,n)<0)return -2;clip=special?86:59;}
        else{int32_t left[3];for(int i=0;i<3;i++)left[i]=-s->basis.right[i];int32_t d=dot(left,s->side.normal);if(d>2048)clip=special?70:64;else if(d< -2048)clip=special?71:65;}
    }
    int r=endpoint(s,clip,env,&e,counts,n);if(r<0)return r;
    if(outer){
        if(s->wall_class){
            if(s->side.normal[1]>=3401){s->d08=s->d04?s->analog_x<0:s->analog_x>0;s->d04=0;to_ceiling=1;}
            else if(s->side.normal[1]<-2600){s->d04=0;to_floor=1;}
            else{s->cf8=0;if(!env->camera_special)yaw_event(&e,&s->side);}
        }else if(s->ceiling_class){
            if(s->side.normal[1]<3401){s->cf8=0;to_wall=1;s->d10=s->d00?s->analog_x<0:s->analog_x>0;s->d00=0;if(!env->camera_special)yaw_event(&e,&s->side);}
        }else if(s->side.normal[1]<3401){to_wall=1;s->cf8=0;if(!env->camera_special)yaw_event(&e,&s->side);}
    }
    if(to_floor)profile_event(&e,0);else if(to_wall)profile_event(&e,1);else if(to_ceiling)profile_event(&e,2);
    s->state=outer?0x2000:0x1000;if(run(s,clip,counts,n)<0)return -1;s->turn_ticks=0;s->d20=0;
    if(outer)memset(s->velocity,0,sizeof s->velocity);
    *out=c;*event=e;return 1;
}
int smn64_climb_ledge_begin(SmN64ClimbState *out,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n){
    if(!out || !event || !env)return -1;
    if(!out->d4c || !out->wall_class || !out->adhered)return 0;
    SmN64ClimbState c=*out,*s=&c;SmN64ClimbTransitionEvent e;memset(&e,0,sizeof e);
    const int32_t down[3]={0,4096,0};memcpy(s->retained_forward,down,12);if(orient(s,down,counts,n)<0)return -2;
    int r=endpoint(s,93,env,&e,counts,n);if(r<0)return r;
    s->d04=0;profile_event(&e,0);s->side.normal[0]=s->side.normal[2]=0;s->side.normal[1]=-4096;
    s->state=0x2000;if(run(s,93,counts,n)<0)return -1;s->turn_ticks=0;s->d20=0;*out=c;*event=e;return 1;
}
static void displace(int32_t p[3],const int32_t v[3],int amount){for(int i=0;i<3;i++)p[i]=add(p[i],mul(v[i],amount));}
int smn64_climb_transition_step(SmN64ClimbState *out,uint16_t finished,const SmN64ClimbTransitionEnv *env,const uint16_t *counts,size_t n){
    if(!out || !env)return -1;
    if(out->state!=0x1000 && out->state!=0x2000 && out->state!=0x80000)return -1;
    SmN64ClimbState c=*out,*s=&c;
    if(s->state==0x80000){
        s->idle_ticks=0;if(!s->anim.finished){*out=c;return 1;}
        if(!env->marker)return -3;
        s->adhered=1;if(env->marker(env->context,s,2,0,s->position)!=1)return -3;
        const int32_t down[3]={0,4096,0};memcpy(s->retained_forward,down,12);memcpy(s->basis.normal,s->side.normal,6);
        if(orient(s,down,counts,n)<0)return -2;
        s->turn_ticks=0;if(smn64_climb_stop(s,counts,n)<0)return -1;*out=c;return 1;
    }
    s->idle_ticks=0;
    if(finished==65535){int32_t *remaining=s->state==0x1000?&s->f4c:&s->f48;if(*remaining){int amount=*remaining<8?*remaining:8;displace(s->position,s->basis.forward,amount);*remaining-=amount;}*out=c;return 1;}
    if(!env->marker)return -3;
    int inner=s->state==0x1000,offset=finished==(inner?60:63)?66:55;
    if(inner && finished==87)offset=12;else if(inner && finished==88)offset=16;
    s->cf8=1;
    if(inner){
        memcpy(s->basis.normal,s->side.normal,6);
        if(s->basis.normal[1]<-2600){s->ceiling_class=s->wall_class=s->adhered=0;if(smn64_climb_stop(s,counts,n)<0)return -1;if(finished!=87 && finished!=88){if(run(s,20,counts,n)<0)return -1;s->state=1;}}
        else{s->state=0x10;if(s->basis.normal[1]>=3401){if(!s->d08)s->d00=1;s->wall_class=0;s->ceiling_class=1;}else{if(!s->wall_class && s->d0c)s->d04=1;s->wall_class=1;s->ceiling_class=0;}}
        if(env->marker(env->context,s,2,0,s->position)!=1)return -3;
        int32_t f[3];for(int i=0;i<3;i++)f[i]=-s->basis.outward[i];memcpy(s->retained_forward,f,12);
        if(orient(s,f,counts,n)<0)return -2;
        s->turn_ticks=0;displace(s->position,s->basis.outward,offset);
    }else{
        if(env->marker(env->context,s,2,0,s->position)!=1)return -3;
        int32_t f[3];memcpy(f,s->basis.outward,12);memcpy(s->retained_forward,f,12);s->turn_ticks=0;
        if(s->side.normal[1]<-2600){
            s->ceiling_class=s->wall_class=0;s->basis.normal[0]=s->basis.normal[2]=0;s->basis.normal[1]=-4096;
            if(orient(s,f,counts,n)<0)return -2;
            s->adhered=0;
            if(finished==59 || finished==86 || finished==93){s->sound_id=9;offset=0;if(smn64_climb_stop(s,counts,n)<0)return -1;}else{if(run(s,20,counts,n)<0)return -1;s->state=1;}
            if(offset){displace(s->position,s->basis.outward,offset);offset=0;}
            if(!env->clearance || !env->trace)return -3;
            int r=env->clearance(env->context,s->position,&s->basis,8);if(r!=1)return r<0?r:-3;
            int32_t y;r=smn64_climb_floor_query(s->position,0,200,1,env->trace,env->context,&y);if(r<0)return r;
            if(y!=-1)s->position[1]=add(y,-mul(s->body_offset,4096));
            else{int step=8;for(int i=0;i<18;i++){int32_t p[3];memcpy(p,s->position,12);displace(p,s->basis.right,(i&1)?step:-step);r=smn64_climb_floor_query(p,0,200,1,env->trace,env->context,&y);if(r<0)return r;if(y!=-1){memcpy(s->position,p,12);s->position[1]=add(y,-mul(s->body_offset,4096));break;}if(i&1)step*=2;}}
        }else{
            memcpy(s->basis.normal,s->side.normal,6);if(orient(s,f,counts,n)<0)return -2;s->state=0x10;
            if(s->basis.normal[1]>=3401){if(s->wall_class && s->d08)s->d00=1;s->wall_class=0;s->ceiling_class=1;}
            else{if(s->ceiling_class && s->d10)s->d04=1;s->wall_class=1;s->ceiling_class=0;}
            if(offset){displace(s->position,s->basis.outward,offset);offset=0;}
            if(!env->clearance)return -3;
            int r=env->clearance(env->context,s->position,&s->basis,16);if(r!=1)return r<0?r:-3;
        }
        if(offset)displace(s->position,s->basis.outward,offset);
    }
    *out=c;return 1;
}
