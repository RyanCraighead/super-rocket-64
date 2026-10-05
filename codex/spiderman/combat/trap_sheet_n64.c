#include "trap_sheet_n64.h"
#include "../web/resource_n64.h"
#include "../movement/locomotion_n64.h"
#include <string.h>
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int8_t s8(uint32_t x){x&=255;return x<=INT8_MAX?(int8_t)x:(int8_t)(-1-(int32_t)(255-x));}
static int16_t s16(uint32_t x){x&=65535;return x<=INT16_MAX?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
int smn64_trap_sheet_init(SmN64TrapSheet *s,uint32_t actor,uint8_t mode,uint8_t type,
    const SmN64TrapActor *a,uint32_t rng[3]){
    int32_t target;
    if(!s||!actor||mode>1||!a||!rng)return -1;
    memset(s,0,sizeof(*s));s->actor=actor;s->mode=mode;s->web_type=type;s->radius=400;
    if(!mode)target=-(int32_t)((a->above>>1)+smn64_web_random(rng,a->above>>1));
    else target=-(int32_t)a->above/3;
    s->height_step=s8((uint32_t)(target/6));s->interval=6;return 1;
}
int smn64_trap_sheet_advance(SmN64TrapSheet *s,const SmN64TrapActor *a,uint32_t rng[3]){
    int32_t target;if(!s||s->mode>1||!a||!rng)return -1;
    s->height=add(s->height,s->height_step);
    if(s->interval)s->interval--;
    if(!s->interval){
        if(!s->mode)target=(int32_t)smn64_web_random(rng,(uint32_t)a->above+a->below)-(int32_t)a->above;
        else target=(int32_t)smn64_web_random(rng,2u*a->above/3)-(int32_t)(a->above/3);
        s->height_step=s8((uint32_t)(sub(target,s->height)/6));s->interval=6;
    }
    s->angle=(int32_t)(((uint32_t)s->angle+256u)&4095u);return 1;
}
static int marker(SmN64TrapSheet *s,const SmN64TrapActor *a,unsigned index,const SmN64TrapSheetHost *h){
    int32_t from[3],to[3];int rc;SmN64Marker out;
    memcpy(to,a->position,sizeof(to));to[1]=add(to[1],s32((uint32_t)s->height<<12));memcpy(from,to,sizeof(from));
    from[0]=add(from[0],s32((uint32_t)s->radius*(uint32_t)smn64_locomotion_cos(s->angle)));
    from[2]=add(from[2],s32((uint32_t)s->radius*(uint32_t)smn64_locomotion_sin(s->angle)));
    rc=h->mesh_marker(h->context,s->actor,from,to,4096,&out);if(rc<0)return -2;if(!rc)return 0;if(rc!=1)return -2;
    s->markers[index]=out;s->heights[index]=s16((uint32_t)s->height);return 1;
}
int smn64_trap_sheet_sample(SmN64TrapSheet *s,const SmN64TrapActor *a,uint32_t rng[3],const SmN64TrapSheetHost *h){
    unsigned i,best;int32_t farthest=0;int rc;
    if(!s||!a||!rng||!h||!h->mesh_marker||s->mode>1||s->segments>80||s->patch_count>20)return -1;
    if(s->segments==80)return 1;
    smn64_trap_sheet_advance(s,a,rng);
    if(!s->segments){rc=marker(s,a,0,h);if(rc<0)return rc;if(!rc)return 1;smn64_trap_sheet_advance(s,a,rng);}
    rc=marker(s,a,s->segments+1,h);if(rc<0)return rc;if(!rc)return 1;
    s->segments++;
    if(s->segments<2||smn64_web_random(rng,4)||s->patch_count>=20)return 1;
    best=s->segments-1;
    for(i=0;i<s->segments-2;i++){
        int32_t delta=(int32_t)s->heights[s->segments]-s->heights[i];if(delta<0)delta=-delta;
        if(delta>farthest){farthest=delta;best=i;}
    }
    s->patches[s->patch_count][0]=(uint8_t)s->segments;
    s->patches[s->patch_count][1]=(uint8_t)(s->segments-2);
    s->patches[s->patch_count][2]=(uint8_t)best;
    if(!h->patch||h->patch(h->context,s->actor,s->patches[s->patch_count])!=1)return -2;
    s->patch_count++;return 1;
}
int smn64_trap_sheet_feed(SmN64TrapSheet *s,const SmN64TrapActor *a,uint32_t rng[3],const SmN64TrapSheetHost *h){
    unsigned i;int rc;for(i=0;i<8;i++){rc=smn64_trap_sheet_sample(s,a,rng,h);if(rc<0)return rc;}return 1;
}

static int32_t sar(int32_t v,unsigned n){return s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0));}
static void orange(uint32_t r,uint8_t out[3]){out[0]=(uint8_t)((r*255u>>8)+63);out[1]=(uint8_t)((r*123u>>8)+30);out[2]=0;}
int smn64_trap_sheet_colors(const SmN64TrapSheet *s,SmN64TrapColorState *state,
    uint32_t actor_word0,uint8_t alpha,uint32_t rng[3],SmN64TrapSpark spark,void *context,SmN64TrapColors *out){
    unsigned i;uint32_t range=256;
    if(!s||!state||!rng||!out||s->segments>80||s->patch_count>20)return -1;
    memset(out,0,sizeof(*out));if(!s->segments)return 0;
    if(s->web_type){
        int32_t value=sar(smn64_locomotion_sin(state->phase)*100,12)+206;
        int32_t blend=state->blend,red=32-sar(blend,3),green,blue;
        if(value>255)value=255;
        green=add(32,sar(s32((uint32_t)blend*(uint32_t)(sar(value*123,8)-32)),8));
        blue=add(32,sar(s32((uint32_t)blend*(uint32_t)(value-32)),8));
        out->actor_set_flag=0x400;out->actor_color=((uint32_t)red<<16)|((uint32_t)green<<8)|(uint32_t)blue;
        state->phase=(uint16_t)(state->phase+80);state->blend=s16((uint32_t)state->blend+8);if(state->blend>256)state->blend=256;
        for(i=1;i<=s->segments;i++)orange(smn64_web_random(rng,192),out->points[i]);
        orange(smn64_web_random(rng,192),out->points[0]);
        {uint32_t index=smn64_web_random(rng,s->segments);if(!spark||spark(context,index,rng)!=1)return -2;}
        for(i=0;i<s->patch_count;i++)orange(smn64_web_random(rng,192),out->patches[i]);
    }else{
        if((actor_word0&0x04800000u)==0x04000000u){range=(uint32_t)alpha*2;if(range>256)range=256;}
        for(i=1;i<=s->segments;i++){uint8_t v=(uint8_t)smn64_web_random(rng,range);memset(out->points[i],v,3);}
        {uint8_t v=(uint8_t)smn64_web_random(rng,range);memset(out->points[0],v,3);}
        if(range)range--;
        for(i=0;i<s->patch_count;i++)memset(out->patches[i],(uint8_t)range,3);
    }
    return 1;
}
int smn64_trap_sheet_debris_random(uint32_t n,uint32_t rng[3],SmN64SheetDebrisRandom out[40]){
    unsigned i,j;if(n>40||!rng||!out)return -1;
    for(i=0;i<n;i++){
        uint32_t branch;for(j=0;j<3;j++)out[i].y_velocity_offset[j]=((int32_t)smn64_web_random(rng,21)-10)*4096;
        branch=smn64_web_random(rng,3);out[i].size_class=(uint8_t)(smn64_web_random(rng,3)+(branch?6:1));
    }
    return 1;
}
