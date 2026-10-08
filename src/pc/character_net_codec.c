/* Fixed little-endian fields: never transmit native structs, pointers or paths. */
#include "character_net_codec.h"
#include <math.h>
#include <string.h>
static void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get32(const uint8_t *p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(i*8);return v;}
static int bounded(const float *v,int n,float limit){for(int i=0;i<n;i++)if(!isfinite(v[i])||fabsf(v[i])>limit)return 0;return 1;}
static int valid(const CharacterNetState *s){
    if(!s||(s->speed_percent&&!rocket_speed_valid(s->speed_percent))||s->active>CNET_PRESENTATION||s->interaction>1||(s->interaction&&s->active!=CNET_DRIVING)||(s->kind!=CNET_MARIO&&s->kind!=CNET_OCTANE)||(!s->kind&&s->active))return 0;
    if(!s->active)return 1;
    const RocketSnapshot *c=&s->car;
    if(!bounded(c->position,3,131072)||!bounded(c->velocity,3,100000)||!bounded(c->angular_velocity,3,1000)||
       !bounded(c->basis,9,1.01f)||!bounded(&c->wheel_position[0][0],12,131072)||
       !bounded(c->wheel_steer,4,7)||!bounded(c->wheel_radius,4,100)||
       !isfinite(c->boost)||c->boost<0||c->boost>100.01f||
       !isfinite(c->jump_time)||c->jump_time<0||c->jump_time>1000000||
       !isfinite(c->flip_time)||c->flip_time<0||c->flip_time>1000000||
       !isfinite(c->air_time)||c->air_time<0||c->air_time>1000000||
       !isfinite(c->quicksand_depth)||c->quicksand_depth<0||c->quicksand_depth>200||
       (s->active!=CNET_DRIVING&&c->quicksand_depth!=0))return 0;
    for(int i=0;i<3;i++)for(int j=i;j<3;j++){
        float dot=0;for(int k=0;k<3;k++)dot+=c->basis[i*3+k]*c->basis[j*3+k];
        if(fabsf(dot-(i==j?1.f:0.f))>.015f)return 0;
    }
    float cross[3];for(int k=0;k<3;k++)cross[k]=c->basis[(k+1)%3]*c->basis[3+(k+2)%3]-c->basis[(k+2)%3]*c->basis[3+(k+1)%3];
    float det=0;for(int k=0;k<3;k++)det+=cross[k]*c->basis[6+k];
    if(det<.98f)return 0;
    const int flags[]={c->grounded,c->jumped,c->double_jumped,c->flipped,c->flipping,c->wheel_contacts[0],c->wheel_contacts[1],c->wheel_contacts[2],c->wheel_contacts[3],c->boosting};
    for(int i=0;i<10;i++)if(flags[i]!=0&&flags[i]!=1)return 0;
    for(int i=0;i<4;i++){
        if(c->wheel_radius[i]<=0)return 0;
        for(int k=0;k<3;k++)if(fabsf(c->wheel_position[i][k]-c->position[k])>1000)return 0;
    }
    return 1;
}
/* List every field so layout/padding and adjacent C members never become ABI. */
#define FLOAT_FIELDS(OP) \
    OP(position,3) OP(velocity,3) OP(basis,9) OP(angular_velocity,3) \
    OP(wheel_position[0],3) OP(wheel_position[1],3) OP(wheel_position[2],3) OP(wheel_position[3],3) \
    OP(wheel_steer,4) OP(wheel_radius,4)
int character_net_encode(uint8_t *out,size_t size,const CharacterNetState *s){
    if(!out||size!=CNET_WIRE_SIZE||!valid(s)||sizeof(float)!=4)return 0;
    memset(out,0,size);memcpy(out,"CNET",4);out[4]=4;out[5]=s->interaction;out[6]=s->kind;out[7]=s->active;
    put32(out+8,s->sequence);put32(out+12,s->epoch);put32(out+16,1); /* exact pinned mesh profile */
    out[206]=(uint8_t)s->area_sequence;out[207]=(uint8_t)(s->area_sequence>>8);
    out[208]=s->speed_percent?s->speed_percent:100;put32(out+209,s->rule_revision);
    if(!s->active)return 1;
    const RocketSnapshot *c=&s->car;size_t at=20;
#define WRITE_FLOATS(field,n) for(int i=0;i<n;i++){uint32_t v;memcpy(&v,&c->field[i],4);put32(out+at,v);at+=4;}
    FLOAT_FIELDS(WRITE_FLOATS)
#undef WRITE_FLOATS
    float times[]={c->boost,c->jump_time,c->flip_time,c->air_time};
    for(int i=0;i<4;i++){uint32_t v;memcpy(&v,&times[i],4);put32(out+at,v);at+=4;}
    put32(out+at,(uint32_t)c->ticks);put32(out+at+4,(uint32_t)(c->ticks>>32));at+=8;
    unsigned flags=c->grounded|(c->jumped<<1)|(c->double_jumped<<2)|(c->flipped<<3)|(c->flipping<<4)|(c->boosting<<9);
    for(int i=0;i<4;i++)flags|=c->wheel_contacts[i]<<(5+i);
    out[at]=(uint8_t)flags;out[at+1]=(uint8_t)(flags>>8);at+=2;
    uint32_t depth;memcpy(&depth,&c->quicksand_depth,4);put32(out+at,depth);return 1;
}
int character_net_decode(CharacterNetState *out,const uint8_t *wire,size_t size){
    if(!out||!wire||size!=CNET_WIRE_SIZE||sizeof(float)!=4||memcmp(wire,"CNET",4)||wire[4]!=4||wire[5]>1||get32(wire+16)!=1)return 0;
    CharacterNetState s={0};s.kind=wire[6];s.active=wire[7];s.interaction=wire[5];s.sequence=get32(wire+8);s.epoch=get32(wire+12);
    RocketSnapshot *c=&s.car;size_t at=20;
    if(s.active){
#define READ_FLOATS(field,n) for(int i=0;i<n;i++){uint32_t v=get32(wire+at);memcpy(&c->field[i],&v,4);at+=4;}
        FLOAT_FIELDS(READ_FLOATS)
#undef READ_FLOATS
        float *times[]={&c->boost,&c->jump_time,&c->flip_time,&c->air_time};
        for(int i=0;i<4;i++){uint32_t v=get32(wire+at);memcpy(times[i],&v,4);at+=4;}
        c->ticks=get32(wire+at)|((uint64_t)get32(wire+at+4)<<32);at+=8;
        unsigned flags=wire[at]|((unsigned)wire[at+1]<<8);at+=2;if(flags&~1023u)return 0;
        c->boosting=!!(flags&512);
        c->grounded=!!(flags&1);c->jumped=!!(flags&2);c->double_jumped=!!(flags&4);c->flipped=!!(flags&8);c->flipping=!!(flags&16);
        for(int i=0;i<4;i++)c->wheel_contacts[i]=!!(flags&(1u<<(5+i)));
        uint32_t depth=get32(wire+at);memcpy(&c->quicksand_depth,&depth,4);at+=4;
    }
    for(;at<206;at++)if(wire[at])return 0;
    s.area_sequence=wire[206]|((uint16_t)wire[207]<<8);
    s.speed_percent=wire[208];s.rule_revision=get32(wire+209);
    if(!rocket_speed_valid(s.speed_percent))return 0;
    if(!valid(&s))return 0;
    *out=s;return 1;
}
static void quaternion(const float *m,float *q){
    float trace=m[0]+m[4]+m[8];
    if(trace>0){float t=sqrtf(trace+1)*2;q[3]=.25f*t;q[0]=(m[5]-m[7])/t;q[1]=(m[6]-m[2])/t;q[2]=(m[1]-m[3])/t;}
    else {int i=m[4]>m[0]?1:0;if(m[8]>m[i*3+i])i=2;int j=(i+1)%3,k=(i+2)%3;
        float t=sqrtf(1+m[i*3+i]-m[j*3+j]-m[k*3+k])*2;
        q[i]=.25f*t;q[j]=(m[i*3+j]+m[j*3+i])/t;q[k]=(m[i*3+k]+m[k*3+i])/t;q[3]=(m[j*3+k]-m[k*3+j])/t;}
}
static void rotation(const float *a,const float *b,float t,float *m){
    float p[4],q[4],r[4],dot=0;quaternion(a,p);quaternion(b,q);
    for(int i=0;i<4;i++)dot+=p[i]*q[i];
    if(dot<0){dot=-dot;for(int i=0;i<4;i++)q[i]=-q[i];}
    float x=1-t,y=t;
    if(dot<.9995f){float theta=acosf(fminf(dot,1)),s=sinf(theta);x=sinf((1-t)*theta)/s;y=sinf(t*theta)/s;}
    float n=0;for(int i=0;i<4;i++){r[i]=p[i]*x+q[i]*y;n+=r[i]*r[i];}n=sqrtf(n);for(int i=0;i<4;i++)r[i]/=n;
    x=r[0];y=r[1];float z=r[2],w=r[3];
    m[0]=1-2*(y*y+z*z);m[1]=2*(x*y+z*w);m[2]=2*(x*z-y*w);
    m[3]=2*(x*y-z*w);m[4]=1-2*(x*x+z*z);m[5]=2*(y*z+x*w);
    m[6]=2*(x*z+y*w);m[7]=2*(y*z-x*w);m[8]=1-2*(x*x+y*y);
}
int character_net_track_sample(const CharacterNetTrack *track,double now,CharacterNetState *out){
    if(!track||!out||!track->valid||!isfinite(now)||now<track->received||now-track->received>1.0)return 0;
    *out=track->latest;
    if(!out->active)return 1;
    float t=track->duration>0?(float)((now-track->received)/track->duration):1;t=fminf(fmaxf(t,0),1);
    const RocketSnapshot *a=&track->previous.car,*b=&track->latest.car;RocketSnapshot *c=&out->car;
#define BLEND(field,n) for(int i=0;i<n;i++)c->field[i]=a->field[i]+(b->field[i]-a->field[i])*t;
    FLOAT_FIELDS(BLEND)
#undef BLEND
    rotation(a->basis,b->basis,t,c->basis);
    c->quicksand_depth=a->quicksand_depth+(b->quicksand_depth-a->quicksand_depth)*t;
    /* Wheel centers rotate with the interpolated rigid body. Linear world-space
     * wheel interpolation would pull tires through the chassis during flips. */
    for(int wheel=0;wheel<4;wheel++){
        float local[3]={0};
        for(int axis=0;axis<3;axis++)for(int k=0;k<3;k++)
            local[axis]+=(1-t)*a->basis[axis*3+k]*(a->wheel_position[wheel][k]-a->position[k])+
                t*b->basis[axis*3+k]*(b->wheel_position[wheel][k]-b->position[k]);
        for(int k=0;k<3;k++){
            c->wheel_position[wheel][k]=c->position[k];
            for(int axis=0;axis<3;axis++)c->wheel_position[wheel][k]+=c->basis[axis*3+k]*local[axis];
        }
    }
    return 1;
}
int character_net_track_support(const CharacterNetTrack *track,double now,CharacterNetState *out){
    if(!track||!out||!track->valid||!isfinite(now)||now<track->received||now-track->received>.2||
       track->latest.active!=CNET_DRIVING||track->latest.kind!=CNET_OCTANE)return 0;
    *out=track->latest;return 1;
}
int character_net_track_contact(const CharacterNetTrack *track,double now,CharacterNetState *out){
    return character_net_track_support(track,now,out)&&track->latest.interaction;
}
int character_net_track_push(CharacterNetTrack *track,const CharacterNetState *s,double now){
    if(!track||!valid(s)||!isfinite(now))return 0;
    if(track->valid){uint32_t delta=s->sequence-track->latest.sequence;if(!delta||delta>=0x80000000u||now<track->received)return 0;}
    CharacterNetState from=*s;
    int blend=track->valid&&s->active&&s->active==track->latest.active&&s->kind==track->latest.kind&&s->epoch==track->latest.epoch&&s->area_sequence==track->latest.area_sequence&&s->speed_percent==track->latest.speed_percent&&s->rule_revision==track->latest.rule_revision&&character_net_track_sample(track,now,&from);
    float distance=0;for(int i=0;i<3;i++){float d=s->car.position[i]-from.car.position[i];distance+=d*d;}
    double duration=now-track->received;
    if(!blend||distance>1500.f*1500.f){from=*s;duration=0;}
    else duration=fmin(fmax(duration,1.0/30.0),.2);
    track->previous=from;track->latest=*s;track->received=now;track->duration=duration;track->valid=1;return 1;
}
