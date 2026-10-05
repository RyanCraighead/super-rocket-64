#include "targeting_n64.h"
#include <string.h>
#include "../movement/locomotion_n64.h"
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int16_t s16(uint32_t x){x&=65535;return x<=INT16_MAX?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t sar(int32_t x,unsigned n){return s32(((uint32_t)x>>n)|(x<0?(UINT32_MAX<<(32-n)):0));}
int smn64_target_transform(const int16_t matrix[9],const int32_t native[3],int32_t out[3]){
    unsigned i,j;int32_t v[3];if(!matrix||!native||!out)return -1;
    for(i=0;i<3;i++){
        uint32_t sum=0;float rounded;
        for(j=0;j<3;j++)sum+=(uint32_t)(int32_t)matrix[i*3+j]*(uint32_t)(int32_t)s16((uint32_t)native[j]);
        rounded=(float)s32(sum);rounded=rounded*(1.0f/4096.0f);v[i]=(int32_t)rounded;
    }
    memcpy(out,v,sizeof(v));return 1;
}
int smn64_target_face_heading(const SmN64Targeting *s,const int32_t target[3],uint16_t heading,uint8_t ceiling,uint16_t *desired){
    int32_t v[3],angle;unsigned i;if(!s||!target||!desired)return -1;
    for(i=0;i<3;i++)v[i]=sar(sub(target[i],s->position[i]),12);
    smn64_target_transform(s->inverse,v,v);
    angle=(1024-smn64_locomotion_atan(-v[2],-v[0]))&4095;
    *desired=(uint16_t)((ceiling?(uint32_t)heading-(uint32_t)angle:(uint32_t)heading+(uint32_t)angle)&4095);return 1;
}
int smn64_target_search(const SmN64Targeting *s,int32_t maxd,int32_t minface,int32_t dw,int32_t fw,const SmN64TargetHost *h,SmN64TargetActor *out){
    size_t i;int32_t best=0;SmN64TargetActor a;
    if(!s||!h||!h->actor_at||!h->line_clear||!out||maxd<=0)return -1;
    memset(out,0,sizeof(*out));
    for(i=0;;i++){
        int rc=h->actor_at(h->context,i,&a);int32_t score;
        if(rc<0||rc>1)return -2;
        if(!rc)break;
        if(!a.id)return -2;
        if(!a.enabled_dc||(a.flags_48&0x50)!=0x10||a.cached_distance>=maxd)continue;
        score=s32((uint32_t)sub(maxd,a.cached_distance)<<12)/maxd;
        score=sar(s32((uint32_t)score*(uint32_t)dw),12);
        if(fw){
            int32_t v[3],facing,half;unsigned j;
            for(j=0;j<3;j++)v[j]=sar(sub(a.position[j],s->position[j]),12);
            smn64_target_transform(s->inverse,v,v);v[1]=0;smn64_combat_normalize(v,v);facing=-v[2];
            if(facing<minface)continue;
            half=s32((uint32_t)facing+4096u)/2;
            score=s32((uint32_t)score+(uint32_t)sar(s32((uint32_t)half*(uint32_t)fw),12));
        }
        if(score<=best)continue;
        rc=h->line_clear(h->context,s->position,a.position);if(rc<0||rc>1)return -2;
        if(rc){best=score;*out=a;}
    }
    return 1;
}
int smn64_target_prepass(SmN64Targeting *s,const SmN64TargetHost *h){
    SmN64TargetActor a;int32_t point[3]={0,0,0};int rc;
    if(!s||!h)return -1;
    memset(s->look_angles,0,sizeof(s->look_angles));s->target_aux=0;
    if(!s->aiming){
        s->target_actor=0;rc=smn64_target_search(s,2048,2896,4096,4096,h,&a);if(rc<0)return rc;
        if(a.id){
            s->target_actor=a.id;memcpy(point,a.position,sizeof(point));point[1]=sub(point[1],s32((uint32_t)a.height_f4<<12));
            if(!h->marker)return -2;
            if(!s->marker_actor&&(h->marker(h->context,1,s,point)!=1||!s->marker_actor))return -2;
            return h->marker(h->context,2,s,point)==1?1:-2;
        }
    }
    if(s->marker_actor){if(!h->marker||h->marker(h->context,3,s,point)!=1)return -2;s->marker_actor=0;}
    return 1;
}
static void camera_direction(const SmN64Targeting *s,const SmN64TargetActor *a,int32_t out[3]){
    int32_t v[3];unsigned i;for(i=0;i<3;i++)v[i]=sub(sar(a->position[i],12),s->camera_position[i]);
    smn64_target_transform(s->camera_matrix,v,v);v[2]=0;smn64_combat_normalize(v,out);
}
static int eligible(const SmN64TargetActor *a){return a->enabled_dc&&(a->flags_0&0x8000)&&a->field_3f0&&(a->flags_48&0x50)==0x10;}
int smn64_target_awareness_update(SmN64Targeting *s,uint32_t now,const SmN64TargetHost *h){
    unsigned i;if(!s||!h||!h->lookup)return -1;
    if(!(s->awareness_tick<now-3u||now<s->awareness_tick))return 0;
    s->awareness_tick=now;
    for(i=0;i<6;i++){
        SmN64TargetActor a;SmN64TargetSlot *slot=&s->slots[i];int rc;
        if(!slot->actor)continue;
        memset(&a,0,sizeof(a));rc=h->lookup(h->context,slot->actor,slot->generation,&a);
        if(rc<0||rc>1)return -2;
        if(!rc){slot->actor=0;continue;}
        if(a.id!=slot->actor||a.generation!=slot->generation)return -2;
        if(!eligible(&a)){slot->actor=0;continue;}
        camera_direction(s,&a,slot->direction);
    }
    return 1;
}
int smn64_target_awareness_scan(SmN64Targeting *s,uint32_t now,const SmN64TargetHost *h){
    size_t i;int due;if(!s||!h||!h->actor_at)return -1;
    due=s->scan_tick<now-20u||now<s->scan_tick;
    if(due){
        s->scan_tick=now;s->combo_metric=0;s->farthest=0;s->nearest=UINT32_MAX;s->has_special=0;
        for(i=0;;i++){
            SmN64TargetActor a;unsigned j;int duplicate=0,rc=h->actor_at(h->context,i,&a);
            if(rc<0||rc>1)return -2;
        if(!rc)break;
        if(!a.id)return -2;
            if(!a.enabled_dc||!(a.flags_48&0x200))continue;
            if(a.flags_3c8&0x20){
                uint32_t d=(uint32_t)a.cached_distance;
                if(d>s->farthest)s->farthest=d;
                if(d<s->nearest)s->nearest=d;
                s->has_special=1;s->combo_metric++;
            }
            if(!eligible(&a))continue;
            /* Original compares generation tags, not pointer+tag, for duplicate slots. */
            for(j=0;j<6;j++)if(s->slots[j].actor&&s->slots[j].generation==a.generation){duplicate=1;break;}
            if(duplicate)continue;
            for(j=0;j<6;j++)if(!s->slots[j].actor)break;
            if(j==6)break; /*Original ends the entire actor scan when full.*/
            s->slots[j].actor=a.id;s->slots[j].generation=a.generation;s->slots[j].age=0;
            camera_direction(s,&a,s->slots[j].direction);
        }
    }
    if(!s->combo_metric)s->combo_active=0;
    return due;
}
