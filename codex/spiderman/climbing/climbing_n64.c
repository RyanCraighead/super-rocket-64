#include "climbing_n64.h"
#include "../movement/locomotion_n64.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static float f32(float x) { volatile float v=x;return v; }
static int32_t bits(uint32_t x) {return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b) {return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b) {return bits((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b) {return bits((uint32_t)a*(uint32_t)b);}
static int32_t shr(int32_t a,int n) {return a>=0?a/(1<<n):-1-(int32_t)((uint32_t)(-1-a)>>n);}
static int16_t narrow(int32_t a) {uint16_t b=(uint16_t)a;return b<=INT16_MAX?(int16_t)b:(int16_t)(-1-(int32_t)(UINT16_MAX-b));}
static void cross(int32_t o[3],const int32_t a[3],const int32_t b[3]) {
    for(int i=0;i<3;i++)o[i]=shr(sub(mul(a[(i+1)%3],b[(i+2)%3]),mul(a[(i+2)%3],b[(i+1)%3])),12);
}
static void norm(int32_t v[3]) {
    float x=f32((float)v[0]*0x1p-12f),y=f32((float)v[1]*0x1p-12f),z=f32((float)v[2]*0x1p-12f);
    float n=sqrtf(f32(f32(f32(x*x)+f32(y*y))+f32(z*z)));
    if(!n) {v[0]=v[1]=v[2]=0;return;}
    float inv=f32(1.0f/n);
    v[0]=(int32_t)f32(f32(x*inv)*4096.0f);v[1]=(int32_t)f32(f32(y*inv)*4096.0f);v[2]=(int32_t)f32(f32(z*inv)*4096.0f);
}
int smn64_climb_basis(SmN64ClimbBasis *s,const int32_t explicit_forward[3]) {
    int32_t in[3],f[3],r[3];int diff=0;
    for(int i=0;i<3;i++) {in[i]=-(int32_t)s->normal[i];diff+=abs((int)s->normal[i]-s->previous_normal[i]);}
    if(explicit_forward || diff>=17 || s->yaw_delta) {
        for(int i=0;i<3;i++)f[i]=explicit_forward?explicit_forward[i]:s->matrix[i*3+2];
        if(!explicit_forward)for(int i=0;i<3;i++)s->previous_normal[i]=s->normal[i];
        cross(r,in,f);norm(r);cross(f,r,in);
        for(int i=0;i<3;i++) {s->matrix[i*3]=narrow(r[i]);s->matrix[i*3+1]=narrow(in[i]);s->matrix[i*3+2]=narrow(f[i]);}
        if(!explicit_forward && s->yaw_delta) {
            float rad=f32((float)narrow(s->yaw_delta)*0x1.921fb6p-10f);
            float si=f32((float)(int32_t)f32(sinf(rad)*4096.0f)*0x1p-12f),co=f32((float)(int32_t)f32(cosf(rad)*4096.0f)*0x1p-12f);
            for(int i=0;i<3;i++) {
                float a=f32((float)s->matrix[i*3]*0x1p-12f),b=f32((float)s->matrix[i*3+2]*0x1p-12f);
                s->matrix[i*3]=narrow((int32_t)f32(f32(f32(a*co)-f32(b*si))*4096.0f));
                s->matrix[i*3+2]=narrow((int32_t)f32(f32(f32(a*si)+f32(b*co))*4096.0f));
            }
        }
    }
    int bad=0;
    for(int i=0;i<3;i++) {
        s->inward[i]=in[i];s->forward[i]=s->matrix[i*3+2];s->right[i]=s->matrix[i*3];s->outward[i]=-(int32_t)s->matrix[i*3+1];
        for(int j=0;j<3;j++)s->inverse[i*3+j]=s->matrix[j*3+i];
    }
    for(int j=0;j<3;j++)if(abs(s->matrix[j])+abs(s->matrix[3+j])+abs(s->matrix[6+j])<2048)bad=1;
    if(bad) {
        const int16_t fallback[9]={-4096,0,0,0,4096,0,0,0,-4096};
        memcpy(s->matrix,fallback,sizeof fallback);
        for(int i=0;i<3;i++){s->forward[i]=fallback[i*3+2];s->right[i]=fallback[i*3];s->outward[i]=-fallback[i*3+1];}
        /* Original inverse was updated before fallback and remains so. */
    }
    return !bad;
}
void smn64_climb_classify(SmN64ClimbState *s) {
    s->wall_class=0;s->ceiling_class=0;
    if(s->basis.normal[1]>=3401)s->ceiling_class=1;
    else if(s->basis.normal[1]>=-2600)s->wall_class=1;
}
int smn64_climb_floor_query(const int32_t p[3],int32_t above,int32_t below,int32_t objects,SmN64ClimbTrace trace,void *ctx,int32_t *height) {
    if(!trace || !height)return -3;
    SmN64ClimbQuery q;memset(&q,0,sizeof q);
    memcpy(q.start,p,sizeof q.start);memcpy(q.end,p,sizeof q.end);
    q.start[1]=sub(p[1],mul(above,4096));q.end[1]=add(p[1],mul(below,4096));q.arg1=objects;q.arg4=1;
    SmN64ClimbHit hit;memset(&hit,0,sizeof hit);
    if(trace(ctx,&q,&hit)!=1)return -3;
    *height=hit.present?hit.position[1]:-1;return 1;
}
static int clip(SmN64ClimbState *s,uint16_t a,const uint16_t *counts,size_t n) {
    if(!counts || a>=n || !counts[a])return -1;
    smn64_anim_run(&s->anim,a,counts[a],0,-1);return 1;
}
static int32_t dot(const int32_t a[3],const int16_t b[3]) {
    int32_t v=0;for(int i=0;i<3;i++)v=add(v,shr(mul(a[i],b[i]),12));return v;
}
int smn64_climb_approach(SmN64ClimbState *s,const SmN64ClimbHit *h,const uint16_t *counts,size_t n) {
    if(!s || !h)return -1;
    if(s->held_object)return 0;
    int reset=1;
    if((s->collision&1) && h->normal[1]<=3400 && h->normal[1]>=-2600 && h->present && h->has_surface && !(h->surface_flags&4)) {
        uint8_t ticks=s->approach_ticks;
        if(dot(s->basis.forward,h->normal)>=3801){ticks=(uint8_t)(ticks+(uint8_t)s->anim.elapsed_ticks);reset=0;}
        if(ticks>=9) {
            SmN64ClimbState next=*s;
            if(clip(&next,14,counts,n)<0)return -1;
            next.approach_ticks=0;next.state=0x80000;*s=next;return 1;
        }
        s->approach_ticks=ticks;
    }
    if(reset)s->approach_ticks=0;
    return 0;
}
/* 80097DE4 ordinary unheld stop path, used by degenerate basis recovery. */
static int rest_with_object(SmN64ClimbState *s,const uint16_t *counts,size_t n,int held_flags) {
    uint16_t old=s->anim.animation,a=old;int from=0,to=-1,change=1;
    if(old==50 || old==51 || old==60 || old==63 || old==72 || old==75 || old==14)a=55;
    else if(old==52 || old==66 || old==69 || old==78 || old==81)a=56;
    else if((old==57 || old==58) && s->anim.direction==1){a=old;from=s->anim.frame;to=0;}
    else if(old==129)a=130;
    else if(old==133)a=134;
    else if(old==196)a=200;
    else if(old==190)a=194;
    else if(old==21){uint16_t f=(uint16_t)s->anim.frame;a=(f>=4 && f<=17)?((f>=10 && f<=14)?12:13):11;}
    else if(old==20 || old==130 || old==134)change=0;
    else if(s->adhered)a=19;
    else if(s->held_object){if(held_flags<0)return -1;a=(held_flags&8)?200:194;}
    else a=0;
    if(change){if(!counts || a>=n || !counts[a])return -1;smn64_anim_run(&s->anim,a,counts[a],from,to);}
    if(s->stop_marks_cf8)s->cf8=1;
    s->state=1;return 1;
}
static int rest(SmN64ClimbState *s,const uint16_t *counts,size_t n){return rest_with_object(s,counts,n,-1);}
static int attach(SmN64ClimbState *s,const SmN64ClimbHit *h,int ceiling,const uint16_t *counts,size_t n) {
    SmN64ClimbState next=*s;next.adhered=1;
    memcpy(next.basis.normal,h->normal,sizeof h->normal);
    int32_t f[3]={0,4096,0};
    if(ceiling){memcpy(f,s->basis.forward,sizeof f);memcpy(next.retained_forward,f,sizeof f);}
    if(!smn64_climb_basis(&next.basis,f) && rest(&next,counts,n)<0)return -2;
    next.turn_ticks=0;
    for(int i=0;i<3;i++){next.velocity[i]=0;next.position[i]=add(h->position[i],mul(h->normal[i],s->body_offset));}
    if(ceiling){next.base_timer=next.hold_timer=0;next.d20=next.field65c=0;if(s->state&0x300)next.camera_reset=1;}
    if(clip(&next,next.anim.animation==232?234:227,counts,n)<0)return -1;
    next.state=1;next.sound_id=9;*s=next;return 1;
}
int smn64_climb_attach_wall(SmN64ClimbState *s,const SmN64ClimbHit *h,SmN64ClimbTrace trace,void *ctx,const uint16_t *counts,size_t n) {
    if(!s || !h)return -1;
    if(!(s->state&4) || !(s->collision&1) || !h->present || h->normal[1]<-2600 || h->normal[1]>3400 || (h->surface_flags&4))return 0;
    if(!h->has_surface)return -1;
    int32_t height;int result=smn64_climb_floor_query(s->position,0,180,1,trace,ctx,&height);if(result<0)return result;
    if(height!=-1)return 0;
    return attach(s,h,0,counts,n);
}
int smn64_climb_attach_ceiling(SmN64ClimbState *s,const SmN64ClimbHit *h,const uint16_t *counts,size_t n) {
    if(!s || !h)return -1;
    if(s->velocity[1]>0 || !(s->collision&0x100) || !h->present || !s->jump_held || h->normal[1]<3401 || (h->surface_flags&4))return 0;
    if(!h->has_surface)return -1;
    return attach(s,h,1,counts,n);
}
int smn64_climb_jump_detach(SmN64ClimbState *s,const uint16_t *counts,size_t n) {
    if(!s)return -1;
    if(!s->jump_pressed || s->aiming || s->held_object || !s->adhered || (!s->wall_class && !s->ceiling_class))return 0;
    SmN64ClimbState next=*s;int32_t f[3];
    next.collision&=(uint16_t)~2;next.d24=0;next.adhered=0;next.state=4;
    next.basis.normal[0]=next.basis.normal[2]=0;next.basis.normal[1]=-4096;
    if(s->wall_class){
        next.d20=1;
        int reverse=(s->analog_x || s->analog_y) && (uint16_t)s->input_angle>=1536 && (uint16_t)s->input_angle<=2560;
        for(int i=0;i<3;i++)f[i]=reverse?-s->basis.outward[i]:s->basis.outward[i];
        if(clip(&next,216,counts,n)<0)return -1;
        next.wall_class=0;
    }else{
        next.d20=0;next.jump_variant=0;memcpy(f,s->basis.forward,sizeof f);
        if(clip(&next,212,counts,n)<0)return -1;
        next.ceiling_class=0;
    }
    if(!smn64_climb_basis(&next.basis,f) && rest(&next,counts,n)<0)return -2;
    next.field1184=0;*s=next;return 1;
}

static void transform(int32_t out[3],const int16_t m[9],const int32_t v[3]) {
    for(int i=0;i<3;i++) {
        int32_t sum=0;for(int j=0;j<3;j++)sum=add(sum,mul(m[i*3+j],narrow(v[j])));
        out[i]=(int32_t)f32((float)sum*0x1p-12f);
    }
}
uint16_t smn64_climb_heading(const SmN64ClimbState *s) {
    if(!s->wall_class)return (uint16_t)((1024-smn64_locomotion_atan(s->basis.forward[2],s->basis.forward[0]))&4095);
    const int32_t up[3]={0,-4096,0};int32_t right[3],f[3],local[3];
    cross(right,up,s->basis.outward);norm(right);cross(f,right,s->basis.outward);
    for(int i=0;i<3;i++)f[i]=-f[i];
    transform(local,s->basis.inverse,f);
    return (uint16_t)((smn64_locomotion_atan(local[2],local[0])+1024)&4095);
}
static int target_velocity(SmN64ClimbState *s,int32_t ramp,int enabled,uint16_t camera_yaw,int held_flags) {
    if(!s || (s->held_object&&held_flags<0) || ramp<0 || ramp>16)return -1;
    int32_t local[3],v[3];
    for(int i=0;i<3;i++)v[i]=shr(s->velocity[i],6);
    const int16_t identity[9]={4096,0,0,0,4096,0,0,0,4096};
    transform(local,s->wall_class?s->basis.inverse:identity,v);
    for(int i=0;i<3;i++)local[i]=mul(local[i],64);
    int write=0;
    if(!s->aiming && enabled && !(s->state&UINT32_C(0xbff7f8c1))) {
        if(s->analog_x || s->analog_y) {
            int32_t mag=64*(abs(s->analog_x)+abs(s->analog_y));if(mag>4096)mag=4096;
            if(s->anim.animation>=50 && s->anim.animation<=52){s->anim.rate=(uint32_t)((mag>>6)*1024);if(s->anim.rate<32768)s->anim.rate=32768;}
            if(!s->adhered&&!s->held_object)mag=4096;
            int speed=s->adhered?26:s->held_object?((held_flags&8)?10:24):40;
            uint16_t h=smn64_climb_heading(s);
            if(s->wall_class) {
                mag=shr(mul(mag,abs(smn64_locomotion_cos((int32_t)h-s->input_angle))),12);
                local[2]=mul(-mul(mag,speed),ramp)/16;write=1;
            }else if(!s->d20) {
                if((s->state&6) && !s->jump_variant)h=(uint16_t)((camera_yaw+s->input_angle)&4095);
                local[2]=mul(-mul(shr(mul(smn64_locomotion_cos(h),mag),12),speed),ramp)/16;
                local[0]=mul(-mul(shr(mul(smn64_locomotion_sin(h),mag),12),speed),ramp)/16;write=1;
            }
        }else if(!(s->state&UINT32_C(0x40040004))){local[0]=local[2]=0;write=1;}
    }
    if(write){
        for(int i=0;i<3;i++)v[i]=shr(local[i],6);
        transform(local,s->wall_class?s->basis.matrix:identity,v);
        for(int i=0;i<3;i++)s->velocity[i]=mul(local[i],64);
    }
    return 1;
}

void smn64_climb_transition_input(SmN64ClimbState *s,const int16_t normal[3],int commit) {
    if(s->wall_class){
        if(normal[1]>=3401){s->d08=s->d04?s->analog_x<0:s->analog_x>0;if(!s->d08 && commit)s->d00=1;s->d04=0;}
        else if(normal[1]<-2600)s->d04=0;
    }else if(s->ceiling_class && normal[1]<3401){
        s->d0c=s->d00?s->analog_x>0:s->analog_x<0;
        if(s->d0c && commit)s->d04=1;
        s->d00=0;
    }
}
static int trace_line(SmN64ClimbTrace trace,void *ctx,const int32_t a[3],const int32_t b[3],SmN64ClimbHit *h) {
    SmN64ClimbQuery q;memset(&q,0,sizeof q);memcpy(q.start,a,12);memcpy(q.end,b,12);q.arg1=q.arg4=1;
    memset(h,0,sizeof *h);if(trace(ctx,&q,h)!=1)return -3;
    if(h->present && !h->has_surface)return -1;
    return 1;
}
static void offset(int32_t o[3],const int32_t p[3],const int32_t v[3],int32_t scale) {
    for(int i=0;i<3;i++)o[i]=add(p[i],mul(v[i],scale));
}
static int32_t normal_dot(const int16_t a[3],const int16_t b[3]) {
    int32_t wide[3]={a[0],a[1],a[2]};return dot(wide,b);
}
/* Original4A204 initializes hit/surface to0 and distance toINT_MAX, but
 * retains the line's old position/normal when a subsequent query misses. */
static void side_query_result(SmN64ClimbState *s,const SmN64ClimbHit *hit) {
    if(hit->present)s->side=*hit;
    else {s->side.present=0;s->side.has_surface=0;s->side.distance=INT32_MAX;}
}
int smn64_climb_physics(SmN64ClimbState *out,const int32_t accel[3],const uint8_t drag[3],SmN64ClimbMarker marker,SmN64ClimbTrace trace,void *ctx,uint16_t bright,uint16_t ordinary,int32_t level) {
    if(!out || !accel || !drag || !out->adhered || out->anim.elapsed_ticks<1 || out->anim.elapsed_ticks>6)return -1;
    if(!marker || !trace)return -3;
    for(int i=0;i<3;i++)if(drag[i]>30)return -1;
    SmN64ClimbState c=*out,*s=&c;int32_t old[3],disp[3],anchor[3],a[3],b[3],t[3];
    memcpy(old,s->position,12);s->collision=0;s->cf4=0;s->d4c=0;
    smn64_free_motion(s->velocity,accel,drag,s->anim.elapsed_ticks,disp);
    int still=!(s->velocity[0] || s->velocity[1] || s->velocity[2]);
    if(!still)for(int i=0;i<3;i++)s->position[i]=add(s->position[i],disp[i]);
    if(marker(ctx,s,2,anchor)!=1)return -3;
    int rc;SmN64ClimbHit hit,probe;
    if(!still){
        /* Leading hand/body ray: original -(forward*96)/4 and forward*-96. */
        for(int i=0;i<3;i++){int32_t v=mul(s->basis.forward[i],-96);a[i]=sub(anchor[i],v/4);b[i]=add(anchor[i],v);}
        rc=trace_line(trace,ctx,a,b,&hit);if(rc<0)return rc;side_query_result(s,&hit);
        if(hit.present){
            if(normal_dot(s->basis.normal,hit.normal)>=3272)s->side.present=0;
            else {
                s->collision|=1;memcpy(s->position,old,12);
                if((hit.surface_flags&4) && (!s->wall_class || hit.normal[1]>=-2600))s->side.present=0;
                if(s->side.present)s->f4c=hit.distance<80?sub(88,hit.distance):0;
            }
        }else{
            offset(b,anchor,s->basis.right,16);rc=trace_line(trace,ctx,anchor,b,&probe);if(rc<0)return rc;
            if(probe.present)memcpy(s->position,old,12);
            else {offset(b,anchor,s->basis.right,-16);rc=trace_line(trace,ctx,anchor,b,&probe);if(rc<0)return rc;if(probe.present)memcpy(s->position,old,12);}
            offset(a,anchor,s->basis.forward,-96);offset(a,a,s->basis.outward,-70);offset(b,a,s->basis.forward,200);
            rc=trace_line(trace,ctx,a,b,&hit);if(rc<0)return rc;side_query_result(s,&hit);
            int back_blocked=1;
            if(hit.has_surface){
                offset(a,anchor,s->basis.forward,-150);offset(b,a,s->basis.outward,-60);
                rc=trace_line(trace,ctx,a,b,&probe);if(rc<0)return rc;
                if(probe.present)back_blocked=!!(probe.surface_flags&4);
            }
            if(!hit.present || (hit.surface_flags&4) || !back_blocked || normal_dot(s->basis.normal,hit.normal)>=3272)s->side.present=0;
            else{s->d48=1;memcpy(s->position,old,12);s->f48=hit.distance<=16?0:hit.distance-8;}
        }
    }
    offset(a,s->position,s->basis.outward,40);offset(b,s->position,s->basis.outward,-140);
    rc=trace_line(trace,ctx,a,b,&hit);if(rc<0)return rc;
    if(hit.present){
        s->ground_grace=4;s->lighting_target=(hit.surface_flags&128)?bright:ordinary;s->collision|=2;
        if(hit.surface_flags&4){if(hit.surface_flags&2048)s->d4c=1;else memcpy(s->position,old,12);}
        else {
            if((hit.normal[1]>=3401)!=(s->basis.normal[1]>=3401)){
                if(hit.normal[1]>=3401)memcpy(s->retained_forward,s->basis.forward,12);
                smn64_climb_transition_input(s,hit.normal,1);
            }
            if(hit.surface_flags&256)s->cf4=1;
            for(int i=0;i<3;i++)t[i]=add(hit.position[i],mul(hit.normal[i],s->body_offset));
            if(still){
                int32_t squared=0;
                for(int i=0;i<3;i++){int16_t v=narrow(shr(sub(t[i],s->position[i]),12));squared=add(squared,mul(v,v));}
                if(squared>=9)memcpy(s->position,t,12);
            }else memcpy(s->position,t,12);
            memcpy(s->basis.normal,hit.normal,6);memcpy(s->contact_position,hit.position,12);
            if(hit.actor_flags&256)return -4;
        }
    }else{
        s->collision|=2;offset(a,s->position,s->basis.forward,-96);offset(a,a,s->basis.outward,-70);offset(b,a,s->basis.forward,128);
        rc=trace_line(trace,ctx,a,b,&hit);if(rc<0)return rc;
        if(hit.present && !(hit.surface_flags&4)){s->d48=1;s->side=hit;}
        memcpy(s->position,old,12);
        if(level==23)return -4; /* Source scripted level-specific fall helper. */
    }
    *out=c;return 1;
}

int smn64_climb_turn(SmN64ClimbState *s,uint16_t target,int fast) {
    if(!s)return -1;
    int32_t h=smn64_climb_heading(s),t=target&4095;
    if(t==h){s->turn_ticks=0;return 1;}
    int32_t ticks=10;
    if(s->state&6){if(s->air_turn_factor!=1 && s->air_turn_factor!=2)return -1;ticks=5*s->air_turn_factor;}
    if(s->state&0x2000000)ticks*=2;
    s->turn_ticks=ticks;s->turn_target=t;
    int32_t d=t-h;if(t>h){if(d>=2048)d-=4096;}else if(-d>=2048)d+=4096;
    s->turn_step=d/ticks;int32_t cap=(s->adhered?384:512)/ticks;if(fast)cap*=2;
    if(s->turn_step>cap){s->turn_step=cap;s->turn_ticks=abs(d/cap);}
    if(s->turn_step< -cap){s->turn_step=-cap;s->turn_ticks=abs(d/cap);}
    return 1;
}
void smn64_climb_turn_advance(SmN64ClimbState *s) {
    int32_t h=smn64_climb_heading(s);int turning=s->turn_ticks!=0;s->yaw=(int16_t)h;
    if(s->turn_ticks){if(s->anim.elapsed_ticks<s->turn_ticks){s->yaw=(int16_t)((h+s->turn_step*s->anim.elapsed_ticks)&4095);s->turn_ticks-=s->anim.elapsed_ticks;}else{s->yaw=(int16_t)s->turn_target;s->turn_ticks=0;}}
    s->basis.yaw_delta=(int32_t)s->yaw-h;
    if(turning && s->basis.normal[1]>=3401){int32_t d=-s->basis.yaw_delta;int32_t v[3]={smn64_locomotion_sin(d),0,smn64_locomotion_cos(d)};transform(s->retained_forward,s->basis.matrix,v);}
}
int smn64_climb_start_move(SmN64ClimbState *out,uint16_t camera,int may_run,const uint16_t *counts,size_t n) {
    if(!out || !out->adhered)return -1;
    if((out->aim_held && (out->state&1)) || out->aiming || (!out->analog_x && !out->analog_y) || out->anim.animation==234 || out->anim.animation==227 || out->anim.animation==281)return 0;
    SmN64ClimbState c=*out,*s=&c;s->idle_ticks=0;
    uint16_t target=s->wall_class?(uint16_t)s->input_angle:(uint16_t)((camera+s->input_angle)&4095);
    uint16_t diff=(uint16_t)((target-smn64_climb_heading(s))&4095);
    if(diff>=1537 && diff<=2559 && (s->anim.animation==0 || (s->anim.animation>=11 && s->anim.animation<=13))){
        if(clip(s,31,counts,n)<0)return -1;
        s->turn_ticks=0;s->state=0x400000;*out=c;return 1;
    }
    if(diff>=128 && diff<=3968){
        uint16_t td=(uint16_t)((s->turn_target-target)&4095);
        if(!s->turn_ticks || (td>=64 && td<=4032)){if(smn64_climb_turn(s,target,1)<0)return -1;s->camera_restore=0;}
        *out=c;return 0;
    }
    if(may_run){if(clip(s,s->cf4?58:57,counts,n)<0)return -1;s->state=0x10;s->approach_ticks=0;*out=c;return 1;}
    *out=c;return 0;
}

int smn64_climb_target_velocity(SmN64ClimbState *s,int32_t ramp,int enabled,uint16_t camera){return target_velocity(s,ramp,enabled,camera,-1);}
int smn64_climb_target_velocity_carrying(SmN64ClimbState *s,int32_t ramp,int enabled,uint16_t camera,uint32_t flags){return target_velocity(s,ramp,enabled,camera,(int)(flags&8u));}
int smn64_climb_stop_carrying(SmN64ClimbState *out,const uint16_t *counts,size_t n,uint32_t flags){
    if(!out)return -1;
    SmN64ClimbState s=*out;int r=rest_with_object(&s,counts,n,(int)(flags&8u));if(r>0)*out=s;return r;
}

int smn64_climb_stop(SmN64ClimbState *out,const uint16_t *counts,size_t n) {
    if(!out)return -1;
    SmN64ClimbState s=*out;int r=rest(&s,counts,n);if(r>0)*out=s;return r;
}
