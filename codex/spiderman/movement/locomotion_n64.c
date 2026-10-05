#include "locomotion_n64.h"
#include <math.h>
#include <stdlib.h>
/* Explicit float32 rounding reproduces the original single-precision operation
 * boundaries even on hosts that otherwise retain excess expression precision. */
static float f32(float x) { volatile float r = x; return r; }
static int32_t floor64(int32_t v) {
    uint32_t bits=(uint32_t)v & UINT32_C(0xffffffc0);
    return bits <= INT32_MAX ? (int32_t)bits : -1-(int32_t)(UINT32_MAX-bits);
}
int32_t smn64_locomotion_sin(int32_t a) {
    if (!a) return 0;
    return (int32_t)f32(sinf(f32((float)(a % 4096) * 0x1.921fb4p-10f)) * 4096.0f);
}
int32_t smn64_locomotion_cos(int32_t a) {
    if (!a) return 4096;
    return (int32_t)f32(cosf(f32((float)(a % 4096) * 0x1.921fb4p-10f)) * 4096.0f);
}
/* 800538B8 -> 800BFBCC. This is the game's approximation, NOT atan2f. */
int32_t smn64_locomotion_atan(int32_t yi, int32_t xi) {
    float y=f32((float)yi * 0x1p-12f), x=f32((float)xi * 0x1p-12f);
    float norm=sqrtf(f32(f32(y*y)+f32(x*x))), angle;
    if (norm == 0.0f) return 0;
    if (norm != 1.0f) { y=f32(y/norm); x=f32(x/norm); }
    float hi=fabsf(x), lo=fabsf(y); int swap=hi<lo;
    if (swap) { float t=hi; hi=lo; lo=t; }
    float r=f32(lo/hi);
    angle=f32(f32(r*0x1.921fb6p-1f)+f32(f32(fabsf(f32(lo-hi))*0x1.3c6a7ep-2f)*r));
    if (x>=0.0f) { if(swap) angle=f32(0x1.921fb6p+0f-angle); }
    else if(swap) angle=f32(angle+0x1.921fb6p+0f);
    else angle=f32(0x1.921fb6p+1f-angle);
    if (y<0.0f) angle=-angle;
    if (angle<0.0f) angle=f32(angle+0x1.921fb6p+2f);
    return (int32_t)f32(f32(angle*0x1.45f306p-3f)*4096.0f);
}
uint16_t smn64_locomotion_heading(const SmN64Locomotion *s) {
    return (uint16_t)((1024-smn64_locomotion_atan(s->forward_z,s->forward_x))&4095);
}
void smn64_locomotion_input(SmN64Locomotion *s,int8_t x,int8_t y) {
    s->previous_x=s->analog_x; s->previous_y=s->analog_y;
    s->analog_x=x; s->analog_y=y;
    if(x || y) {
        s->input_ramp+=32; if(s->input_ramp>256) s->input_ramp=256;
        s->input_angle=(uint16_t)((s->input_base-smn64_locomotion_atan(-(int32_t)x,y)+1024)&4095);
    } else { s->input_ramp=0; s->input_angle=s->input_base; }
}
void smn64_locomotion_ramp(SmN64Locomotion *s) {
    if(s->analog_x || s->analog_y) {
        if(s->run_ramp<16) { s->run_ramp+=s->anim.elapsed_ticks; if(s->run_ramp>16)s->run_ramp=16; }
    } else if(s->run_ramp) { s->run_ramp-=s->anim.elapsed_ticks; if(s->run_ramp<0)s->run_ramp=0; }
}
static void turn_with_ticks(SmN64Locomotion *s,uint16_t target,int fast,int32_t ticks) {
    int32_t t=target&4095, h=smn64_locomotion_heading(s), d;
    if(t==h) {s->turn_ticks=0;return;}
    s->turn_ticks=ticks; s->turn_target=t;
    d=t-h;
    if(t>h) {if(d>=2048)d-=4096;} else if(-d>=2048)d+=4096;
    s->turn_step=d/ticks;
    int32_t cap=512/ticks;
    if(fast)cap*=2;
    if(s->turn_step>cap) {s->turn_step=cap;s->turn_ticks=abs(d/cap);}
    if(s->turn_step< -cap) {s->turn_step=-cap;s->turn_ticks=abs(d/cap);}
}
void smn64_locomotion_turn(SmN64Locomotion *s,uint16_t target,int fast) {
    turn_with_ticks(s,target,fast,10);
}
int smn64_locomotion_turn_air(SmN64Locomotion *s,uint16_t target,int fast,uint8_t factor) {
    if(factor!=1 && factor!=2)return -1;
    turn_with_ticks(s,target,fast,5*(int32_t)factor);
    return 1;
}
void smn64_locomotion_turn_advance(SmN64Locomotion *s) {
    int32_t h=smn64_locomotion_heading(s),dt=s->anim.elapsed_ticks;
    s->yaw=(int16_t)h;
    if(s->turn_ticks) {
        if(dt<s->turn_ticks) {s->yaw=(int16_t)((h+s->turn_step*dt)&4095);s->turn_ticks-=dt;}
        else {s->yaw=(int16_t)s->turn_target;s->turn_ticks=0;}
    }
    s->yaw_delta=(int32_t)s->yaw-h;
}
void smn64_locomotion_basis_begin(SmN64Locomotion *s) {
    /* Flat specialization of 8009D258: normalized right/up cross products,
     * then old basis times a quantized pure-Y rotation. */
    if(!s->yaw_delta) return;
    float x=f32((float)s->forward_x*0x1p-12f), z=f32((float)s->forward_z*0x1p-12f);
    float n=sqrtf(f32(f32(z*z)+f32(x*x)));
    if(!n) return;
    float inv=f32(1.0f/n);
    int32_t nx=(int32_t)f32(f32(x*inv)*4096.0f);
    int32_t nz=(int32_t)f32(f32(z*inv)*4096.0f);
    /* Rotation builder521A0 uses the adjacent, one-ulp higher angle scale. */
    float rad=f32((float)(int16_t)s->yaw_delta*0x1.921fb6p-10f);
    int32_t si=(int32_t)f32(sinf(rad)*4096.0f),co=(int32_t)f32(cosf(rad)*4096.0f);
    s->forward_x=(int32_t)f32(f32(f32(f32((float)nx*0x1p-12f)*f32((float)co*0x1p-12f))+f32(f32((float)nz*0x1p-12f)*f32((float)si*0x1p-12f))) * 4096.0f);
    s->forward_z=(int32_t)f32(f32(f32(f32((float)nz*0x1p-12f)*f32((float)co*0x1p-12f))-f32(f32((float)nx*0x1p-12f)*f32((float)si*0x1p-12f))) * 4096.0f);
}
void smn64_locomotion_velocity_at(SmN64Locomotion *s,uint16_t h) {
    if(!s->analog_x && !s->analog_y) {s->velocity_x=0;s->velocity_z=0;}
    else {
        s->velocity_x=floor64((-smn64_locomotion_sin(h)*40*s->run_ramp)/16);
        s->velocity_z=floor64((-smn64_locomotion_cos(h)*40*s->run_ramp)/16);
    }
    s->velocity_y=floor64(s->velocity_y);
}
void smn64_locomotion_target_velocity(SmN64Locomotion *s) {
    if(s->state==0x10)smn64_locomotion_velocity_at(s,smn64_locomotion_heading(s));
}
static int clip(SmN64Locomotion *s,uint16_t a,const uint16_t *counts,size_t n) {
    if(a>=n || !counts[a])return 0;
    smn64_anim_run(&s->anim,a,counts[a],0,-1);return 1;
}
static int is_supported_clip(uint16_t a) {
    return a==191 || a==192 || a==194 || a==197 || a==198 || a==200 || a==130 || a==134 || a==170 || a==174 || a==187 || a==20 || a==213 || a==0 || a==1 || a==21 || a==11 || a==12 || a==13 || a==27 || a==28 || a==31 || a==25 || a==293 || a==294;
}
int smn64_locomotion_grounded_successor(SmN64Locomotion *out,const uint16_t *counts,size_t n) {
    if(!out||!counts||(out->state!=1&&out->state!=0x10&&out->state!=0x400000)||!is_supported_clip(out->anim.animation))return -1;
    SmN64Locomotion c=*out;c.anim.rate=65536;
    uint16_t finished=65535;
    if(c.anim.finished){
        finished=c.anim.animation;uint16_t next=finished;
        if(finished==191)next=192;
        else if(finished==197)next=198;
        else if(finished==1||finished==31||finished==25)next=21;
        else if(finished==130||finished==134||finished==170||finished==174||finished==187||finished==20||finished==213||finished==11||finished==12||finished==13||finished==293||finished==294)next=0;
        if(!clip(&c,next,counts,n))return -1;
    }
    *out=c;return finished;
}
static int grounded_ai_prepared(SmN64Locomotion *out,uint16_t camera,const uint16_t *counts,size_t n,SmN64BasisUpdate basis,void *context,uint16_t finished,int holding) {
    if(!out||!counts)return -1;
    SmN64Locomotion c=*out,*s=&c;
    if((s->state!=1 && s->state!=0x10 && s->state!=0x400000) || !is_supported_clip(s->anim.animation))return -1;
    int active=s->analog_x || s->analog_y;
    uint16_t target=(uint16_t)((camera+s->input_angle)&4095);
    uint16_t h=smn64_locomotion_heading(s),diff=(uint16_t)((target-h)&4095);
    if(s->state==0x400000) {
        /* 921F0 keeps the framed turn entirely in animation space until the
         * completion transition. Partial-turn jump interruption is out of scope. */
        s->anim.rate=131072;
        if(finished!=65535) {
            s->yaw_delta=2048;
            if(basis){int rc=basis(context,s);if(rc!=1)return rc<0?rc:-5;}
            else smn64_locomotion_basis_begin(s);
            s->yaw_delta=0;
            if(active)s->state=0x10;
            else {
                uint16_t a=0;
                if(s->anim.animation==21) {
                    uint16_t f=(uint16_t)s->anim.frame;
                    a=(f>=4 && f<=17)?((f>=10 && f<=14)?12:13):11;
                }
                if(!clip(s,a,counts,n))return -1;
                s->state=1;
            }
        }
    } else if(s->state==1) {
        int started=0;
        if(active) {
            s->idle_ticks=0;
            if(diff>=1537 && diff<=2559 && (s->anim.animation==0 || s->anim.animation==11 || s->anim.animation==12 || s->anim.animation==13)) {
                if(!clip(s,31,counts,n))return -1;
                s->turn_ticks=0;s->state=0x400000;started=1;
            } else if(diff>=128 && diff<=3968) {
                uint16_t td=(uint16_t)((s->turn_target-target)&4095);
                if(!s->turn_ticks || (td>=64 && td<=4032)) smn64_locomotion_turn(s,target,1);
            } else {
                uint16_t move=s->anim.animation==200?197:s->anim.animation==194?191:1;
                s->state=0x10; if(!clip(s,move,counts,n))return -1;started=1;
            }
        }
        if(!started) {
            s->idle_ticks=(uint16_t)(s->idle_ticks+(uint16_t)s->anim.elapsed_ticks);
            if(s->turn_ticks) {
                s->idle_ticks=0;
                uint16_t a=s->turn_step>0?27:28;
                if(s->anim.animation!=194 && s->anim.animation!=200 && s->anim.animation!=a && !clip(s,a,counts,n))return -1;
            } else if(s->anim.animation==27 || s->anim.animation==28) {
                if(!clip(s,0,counts,n))return -1;
            }
            if(finished==0 && s->idle_ticks>240) {
                if(!active)s->turn_ticks=0;
                smn64_locomotion_turn_advance(s);
                *out=*s;return -2;
            }
        }
    } else {
        s->idle_ticks=0;
        if(!active) {
            uint16_t a=holding<0?0:(holding&8)?200:194;
            if(s->anim.animation==21) {
                uint16_t f=(uint16_t)s->anim.frame;
                a=(f>=4 && f<=17)?((f>=10 && f<=14)?12:13):11;
            }
            if(!clip(s,a,counts,n))return -1;
            s->state=1;
        } else if(diff>=1537 && diff<=2559 && s->anim.animation==21) {
            if(!clip(s,25,counts,n))return -1;
            s->velocity_x=s->velocity_y=s->velocity_z=0;
            s->turn_ticks=0;s->state=0x400000;
        }
    }
    if(s->state==0x10) smn64_locomotion_turn(s,target,0);
    else if(s->state==1 && !active)s->turn_ticks=0;
    smn64_locomotion_turn_advance(s);
    *out=*s;return 1;
}

int smn64_locomotion_grounded_ai_prepared(SmN64Locomotion *s,uint16_t camera,const uint16_t *counts,size_t n,SmN64BasisUpdate basis,void *ctx,uint16_t finished){return grounded_ai_prepared(s,camera,counts,n,basis,ctx,finished,-1);}
int smn64_locomotion_grounded_ai_carrying(SmN64Locomotion *s,uint16_t camera,const uint16_t *counts,size_t n,SmN64BasisUpdate basis,void *ctx,uint16_t finished,uint32_t flags){return grounded_ai_prepared(s,camera,counts,n,basis,ctx,finished,(int)(flags&8u));}
void smn64_locomotion_velocity_carrying(SmN64Locomotion *s,uint32_t flags){
    if(s->state!=0x10)return;
    if(!s->analog_x&&!s->analog_y){s->velocity_x=s->velocity_z=0;}
    else {
        int mag=64*(abs(s->analog_x)+abs(s->analog_y));if(mag>4096)mag=4096;
        int speed=(flags&8u)?10:24;uint16_t h=smn64_locomotion_heading(s);
        s->velocity_x=floor64((-((smn64_locomotion_sin(h)*mag)>>12)*speed*s->run_ramp)/16);
        s->velocity_z=floor64((-((smn64_locomotion_cos(h)*mag)>>12)*speed*s->run_ramp)/16);
    }
    s->velocity_y=floor64(s->velocity_y);
}
int smn64_locomotion_grounded_ai_with_basis(SmN64Locomotion *out,uint16_t camera,const uint16_t *counts,size_t n,SmN64BasisUpdate basis,void *context){
    if(!out)return -1;
    SmN64Locomotion s=*out;int finished=smn64_locomotion_grounded_successor(&s,counts,n);if(finished<0)return finished;
    int rc=smn64_locomotion_grounded_ai_prepared(&s,camera,counts,n,basis,context,(uint16_t)finished);
    if(rc==1||rc==-2)*out=s;
    return rc;
}
int smn64_locomotion_grounded_ai(SmN64Locomotion *s,uint16_t camera,const uint16_t *counts,size_t n){return smn64_locomotion_grounded_ai_with_basis(s,camera,counts,n,NULL,NULL);}
