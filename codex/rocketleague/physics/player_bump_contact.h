#ifndef ROCKET_PLAYER_BUMP_CONTACT_H
#define ROCKET_PLAYER_BUMP_CONTACT_H
#include "body_contact.h"
#include <string.h>
/* Equal-mass, non-damaging contacts in host units. The authority supplies raw
 * poses; presentation interpolation never authorizes an impulse. */
typedef struct RocketBumpBody {
    RocketSnapshot car;
    float center[3], axes[9], half[3], velocity[3];
    int isCar;
} RocketBumpBody;
static inline float rocket_bump_dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static inline int rocket_bump_body(RocketBumpBody *b,const RocketSnapshot *pose,int isCar){
    if(!b||!rocket_body_pose_valid(pose))return 0;
    memset(b,0,sizeof *b);b->car=*pose;b->isCar=isCar;
    for(int k=0;k<3;k++){
        if(!isfinite(pose->velocity[k])||fabsf(pose->velocity[k])>6000.f||fabsf(pose->position[k])>131072.f)return 0;
        b->velocity[k]=pose->velocity[k];
        b->center[k]=pose->position[k]+(isCar?pose->basis[k]*ROCKET_BODY_FORWARD_OFFSET+pose->basis[6+k]*ROCKET_BODY_UP_OFFSET:(k==1?80.f:0));
    }
    if(isCar){memcpy(b->axes,pose->basis,sizeof b->axes);b->half[0]=ROCKET_BODY_HALF_LENGTH;b->half[1]=ROCKET_BODY_HALF_WIDTH;b->half[2]=ROCKET_BODY_HALF_HEIGHT;}
    else{b->axes[0]=b->axes[4]=b->axes[8]=1;b->half[0]=b->half[2]=37.f;b->half[1]=80.f;}
    return 1;
}
/* OBB SAT includes all face and edge axes. The Mario corner is additionally
 * checked against its exact native cylinder. Normal points from A to B. */
static inline int rocket_bump_overlap(const RocketBumpBody *a,const RocketBumpBody *b,float normal[3],float *depth){
    float axes[15][3],delta[3];int count=0;
    for(int k=0;k<3;k++)delta[k]=b->center[k]-a->center[k];
    for(int i=0;i<3;i++){memcpy(axes[count++],a->axes+3*i,3*sizeof(float));memcpy(axes[count++],b->axes+3*i,3*sizeof(float));}
    for(int i=0;i<3;i++)for(int j=0;j<3;j++){
        const float *x=a->axes+3*i,*y=b->axes+3*j;float *axis=axes[count++];
        for(int k=0;k<3;k++)axis[k]=x[(k+1)%3]*y[(k+2)%3]-x[(k+2)%3]*y[(k+1)%3];
    }
    *depth=1e9f;
    for(int i=0;i<count;i++){
        float *axis=axes[i],length=sqrtf(rocket_bump_dot(axis,axis));if(length<.001f)continue;
        for(int k=0;k<3;k++)axis[k]/=length;
        float radius=0;for(int j=0;j<3;j++)radius+=a->half[j]*fabsf(rocket_bump_dot(axis,a->axes+j*3))+b->half[j]*fabsf(rocket_bump_dot(axis,b->axes+j*3));
        float offset=rocket_bump_dot(delta,axis),overlap=radius-fabsf(offset);if(overlap<=0)return 0;
        if(overlap<*depth){*depth=overlap;for(int k=0;k<3;k++)normal[k]=axis[k]*(offset<0?-1.f:1.f);}
    }
    if(a->isCar&&!b->isCar&&!rocket_body_overlaps_cylinder(&a->car,b->car.position,37.f,160.f))return 0;
    if(b->isCar&&!a->isCar&&!rocket_body_overlaps_cylinder(&b->car,a->car.position,37.f,160.f))return 0;
    return *depth<1e9f;
}
static inline int rocket_bump_impulse_at_speed(const RocketBumpBody *a,const RocketBumpBody *b,float delta[3],float scale){
    if(!isfinite(scale)||scale<.5f||scale>1.f)return 0;
    float normal[3],depth;if(!rocket_bump_overlap(a,b,normal,&depth))return 0;
    float relative[3];for(int k=0;k<3;k++)relative[k]=a->velocity[k]-b->velocity[k];
    float closing=rocket_bump_dot(relative,normal);
    /* No restitution at resting contacts. Small penetration uses velocity,
     * never a position teleport, so the normal world solver retains control. */
    float speed=fminf(2400.f*scale,fmaxf(0,closing)*.575f+fminf(60.f,fmaxf(0,depth-2.f)*3.f));
    if(closing< -30.f*scale||speed<scale)return 0;
    for(int k=0;k<3;k++)delta[k]=normal[k]*speed;
    return 1;
}
static inline int rocket_bump_impulse(const RocketBumpBody *a,const RocketBumpBody *b,float delta[3]) {
    return rocket_bump_impulse_at_speed(a,b,delta,1.f);
}
#endif
