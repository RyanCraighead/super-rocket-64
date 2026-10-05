#include "contact_n64.h"
#include "../movement/locomotion_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t bits(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return bits((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return bits((uint32_t)a*(uint32_t)b);}
static int32_t shl(int32_t a,unsigned n){return bits((uint32_t)a<<(n&31));}
static int32_t shr(int32_t a,unsigned n){n&=31;return !n?a:a>=0?(int32_t)((uint32_t)a>>n):-1-(int32_t)((uint32_t)(-1-a)>>n);}
static int16_t s16(uint16_t a){return a<=INT16_MAX?(int16_t)a:(int16_t)(-1-(int32_t)(UINT16_MAX-a));}
static float f32(float x){volatile float a=x;return a;}
static int32_t root(uint32_t a){return (int32_t)sqrtf((float)a);}
static void normalize(int32_t n[3]){
    float x=f32((float)n[0]*0x1p-12f),y=f32((float)n[1]*0x1p-12f),z=f32((float)n[2]*0x1p-12f);
    float d=sqrtf(f32(f32(f32(x*x)+f32(y*y))+f32(z*z)));
    if(d==0){n[0]=n[1]=n[2]=0;return;}
    float inv=f32(1.0f/d);
    n[0]=(int32_t)f32(f32(x*inv)*4096.0f);n[1]=(int32_t)f32(f32(y*inv)*4096.0f);n[2]=(int32_t)f32(f32(z*inv)*4096.0f);
}
static uint32_t random64(SmN64FreeContact *s){
    uint32_t v=s->random_state[0]*s->random_state[1]+s->random_state[2];
    s->random_state[0]=v;s->random_state[1]=(s->random_state[1]^v)+(uint32_t)shr(bits(v),4);
    s->random_state[2]+=0xefefeff0u+(uint32_t)shr(bits(v),3);return ((v&65535u)*64u)>>16;
}
static int query(SmN64FreeTrace trace,void *ctx,SmN64FreeRay kind,unsigned pass,
                 const int32_t start[3],const int32_t end[3],SmN64FreeHit *h,
                 SmN64ClimbQuery *retained){
    SmN64FreeQuery q;memset(&q,0,sizeof q);q.kind=kind;q.pass=pass;
    memcpy(q.ray.start,start,sizeof q.ray.start);memcpy(q.ray.end,end,sizeof q.ray.end);q.ray.arg1=q.ray.arg4=1;
    memset(h,0,sizeof *h);if(trace(ctx,&q,h)!=1)return -3;
    if(retained)*retained=q.ray;
    return 1;
}
static void slide(int32_t d[3],const SmN64ClimbHit *h,int normalized){
    int32_t n[3]={h->normal[0],0,h->normal[2]};if(normalized)normalize(n);
    int32_t dot=add(mul(shr(d[0],6),n[0]),mul(shr(d[2],6),n[2]));
    if(dot>0)return;
    dot=shr(dot,12);
    d[0]=shr(sub(d[0],shr(mul(dot,n[0]),6)),2);
    d[2]=shr(sub(d[2],shr(mul(dot,n[2]),6)),2);
}
int smn64_free_contact_run(SmN64FreeContact *state,SmN64FreeTrace trace,void *ctx,
                           uint16_t bright,uint16_t normal){
    if(!state||!trace||state->elapsed_ticks<1||state->elapsed_ticks>6||state->adhered||state->swinger_present||
       (state->state==0x40000u&&((state->animation==270&&state->frame>=13)||state->animation==271)))return -1;
    if(state->radial_constraint)return -5;
    if(state->previous_platform_present)return -4;
    SmN64FreeContact s=*state;int32_t old[3],d[3],start[3],end[3],base[3],ext[3],cross[3];
    memcpy(old,s.position,sizeof old);s.collision=0;s.d48=0;s.side.present=0;s.ceiling.present=0;s.lighting_target=-1;
    smn64_free_motion(s.velocity,s.acceleration,s.drag,s.elapsed_ticks,d);
    int32_t down=-1;
    if(s.velocity[1]>=0){down=s.velocity[1];s.velocity[1]=0;d[1]=0;}
    SmN64FreeHit h;memset(&h,0,sizeof h);
    for(unsigned pass=0;pass<2;pass++){
        int32_t x=shr(d[0],9),y=shr(d[1],9),z=shr(d[2],9);
        uint32_t horizontal=(uint32_t)mul(x,x)+(uint32_t)mul(z,z);
        int32_t length=root(horizontal+(uint32_t)mul(y,y)),flat=root(horizontal);
        if(!length||!flat)break;
        int32_t radius=s.body_radius;
        if(s.elapsed_ticks>=5)radius=add(radius,mul(shr(radius,1),s.elapsed_ticks-4));
        memcpy(base,s.position,sizeof base);base[1]=sub(base[1],64*4096);
        for(unsigned j=0;j<3;j++){
            ext[j]=mul(j==1?s.body_radius:radius,shl(d[j],3))/length;
            ext[j]=add(ext[j],shr(ext[j],3));
        }
        if(s.held_object){
            memcpy(start,base,sizeof start);start[1]=sub(start[1],128*4096);
            for(unsigned j=0;j<3;j++)end[j]=add(start[j],ext[j]);
            if(query(trace,ctx,SMN64_FREE_HELD,pass,start,end,&h,0)<0)return -3;
            if(h.hit.present)s.collision|=0x40;
        }
        for(unsigned j=0;j<3;j++){start[j]=sub(base[j],ext[j]/4);end[j]=add(base[j],ext[j]);}
        if(query(trace,ctx,SMN64_FREE_FORWARD,pass,start,end,&h,0)<0)return -3;
        if(h.hit.present){
            s.collision=(uint16_t)((s.collision|1u)&~0x40u);
            /* Source copies only marker, descriptor, XYZ and normal here. */
            s.side.present=h.hit.present;memcpy(s.side.position,h.hit.position,sizeof s.side.position);
            memcpy(s.side.normal,h.hit.normal,sizeof s.side.normal);s.side.has_surface=h.hit.has_surface;s.side.surface_flags=h.hit.surface_flags;s.side.actor_flags=h.hit.actor_flags;
            slide(d,&h.hit,1);continue;
        }
        cross[0]=mul(s.body_radius,shl(d[2],2))/length;cross[1]=0;
        cross[2]=mul(-(int32_t)s.body_radius,shl(d[0],2))/length;
        memcpy(base,s.position,sizeof base);base[1]=add(base[1],16*4096);
        ext[0]=mul(radius,shl(d[0],3))/flat;
        ext[1]=mul(s.body_radius,shl(d[1],3))/length;
        ext[2]=mul(radius,shl(d[2],3))/flat;
        for(unsigned side=0;side<2;side++){
            start[0]=sub(add(base[0],side?-(cross[0]/4):cross[0]/4),ext[0]/2);
            start[1]=base[1];start[2]=sub(add(base[2],side?-(cross[2]/4):cross[2]/4),ext[2]/2);
            end[0]=add(side?sub(base[0],cross[0]):add(base[0],cross[0]),ext[0]);
            end[1]=add(base[1],ext[1]);end[2]=add(side?sub(base[2],cross[2]):add(base[2],cross[2]),ext[2]);
            if(query(trace,ctx,side?SMN64_FREE_LEFT:SMN64_FREE_RIGHT,pass,start,end,&h,0)<0)return -3;
            if(h.hit.present)break;
        }
        if(!h.hit.present)break;
        s.collision=(uint16_t)((s.collision|1u)&~0x40u);slide(d,&h.hit,0);
    }
    for(unsigned j=0;j<3;j++)s.velocity[j]=s.elapsed_ticks>=3?shl(d[j],1)/s.elapsed_ticks:d[j];
    int32_t previous_y=s.position[1];
    if(!h.hit.present)for(unsigned j=0;j<3;j++)s.position[j]=add(s.position[j],d[j]);
    else s.position[1]=add(s.position[1],d[1]);
    if(s.state==1){
        memcpy(start,s.position,sizeof start);memcpy(end,start,sizeof end);
        end[0]=add(end[0],s.idle_probe_offset[0]);end[2]=add(end[2],s.idle_probe_offset[2]);
        if(query(trace,ctx,SMN64_FREE_IDLE,0,start,end,&h,0)<0)return -3;
        if(h.hit.present&&h.hit.normal[1]>=-2600&&h.hit.normal[1]<=3400){
            s.velocity[0]=add(s.velocity[0],shl(h.hit.normal[0],2));s.velocity[2]=add(s.velocity[2],shl(h.hit.normal[2],2));
            s.idle_probe_offset[0]=shl(-(int32_t)h.hit.normal[0],5);s.idle_probe_offset[2]=shl(-(int32_t)h.hit.normal[2],5);
        }else{
            s.idle_probe_angle=(uint16_t)(s.idle_probe_angle+256u+random64(&s));
            s.idle_probe_offset[0]=shl(smn64_locomotion_sin(s16(s.idle_probe_angle)),5);
            s.idle_probe_offset[2]=shl(smn64_locomotion_cos(s16(s.idle_probe_angle)),5);
        }
    }
    if(s.velocity[1]<0){
        memcpy(start,s.position,sizeof start);memcpy(end,start,sizeof end);
        start[1]=add(sub(start[1],d[1]),64*4096);end[1]=sub(end[1],184*4096);
        if(query(trace,ctx,SMN64_FREE_UP,0,start,end,&h,&s.ceiling_query)<0)return -3;
        /* Full persistent line query: inactive payload is not semantically read. */
        if(h.hit.present)s.ceiling=h.hit;
        else {s.ceiling.has_surface=0;s.ceiling.distance=INT32_MAX;}
        if(h.hit.present&&sub(s.position[1],120*4096)<h.hit.position[1]){
            s.position[1]=previous_y;s.velocity[1]=0;s.collision=(uint16_t)((s.collision|0x100u)&~0x40u);
        }
    }
    if(down>=0){
        int32_t fall=down;
        if(s.elapsed_ticks>=3)fall=add(fall,mul(shr(down,1),s.elapsed_ticks-2));
        memcpy(start,s.position,sizeof start);memcpy(end,start,sizeof end);start[1]=sub(start[1],40*4096);end[1]=add(start[1],add(fall,140*4096));
        if(query(trace,ctx,SMN64_FREE_DOWN,0,start,end,&h,0)<0)return -3;
        s.ground_record[8]=0; /* source123C */
        if(!h.hit.present){s.position[1]=add(s.position[1],fall);s.velocity[1]=down;}
        else{
            if(h.hit.actor_flags&0x100u)return -4;
            if(!h.hit.has_surface||!h.has_ground_record)return -6;
            memcpy(s.ground_record,h.ground_record,sizeof s.ground_record);s.ground_grace=4;
            if(s.held_object&&(h.hit.surface_flags&0x400u)){
                memcpy(s.position,old,sizeof old);s.collision|=2u;
            }else if(h.hit.normal[1]<-2600){
                h.hit.position[1]=bits((uint32_t)h.hit.position[1]&0xfffff000u);
                s.position[1]=sub(h.hit.position[1],shl(s.body_offset,12));s.velocity[1]=0;s.collision|=2u;
            }else{
                if(h.hit.distance>0){
                    s.position[1]=sub(h.hit.position[1],shl(s.body_offset,12));
                    s.position[0]=add(s.position[0],mul(shr(sub(fall,shr(fall,s.drag[0])),12),h.hit.normal[0]));
                    s.position[2]=add(s.position[2],mul(shr(sub(fall,shr(fall,s.drag[2])),12),h.hit.normal[2]));
                    s.position[1]=add(s.position[1],sub(fall,shr(fall,s.drag[1])));
                }else s.position[1]=add(s.position[1],fall);
                s.velocity[1]=down;
            }
            s.normal[0]=s.normal[2]=0;s.normal[1]=-4096;
            memcpy(s.contact_position,h.hit.position,sizeof s.contact_position);
            s.lighting_target=(h.hit.surface_flags&0x80u)?bright:normal;
        }
    }
    for(unsigned j=0;j<3;j++)s.angles[j]=(int16_t)((uint16_t)s.angles[j]&4095u);
    *state=s;return 1;
}
