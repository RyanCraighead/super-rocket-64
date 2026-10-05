#ifndef ROCKET_BOSS_IMPACT_POLICY_H
#define ROCKET_BOSS_IMPACT_POLICY_H
/* Authored SM64 crossover interaction policy, not Rocket League behavior.
 * Recognize a moving front-bumper entry; native boss code owns the result. */
#include "rocket_physics.h"
#include <math.h>
#include <string.h>
#define ROCKET_BOSS_MIN_SPEED (900.f * ROCKET_HOST_SCALE)
#define ROCKET_BOSS_FRONT ((120.507f*.5f+13.8757f)*ROCKET_HOST_SCALE)
#define ROCKET_BOSS_HALF_WIDTH (84.1994f*.5f*ROCKET_HOST_SCALE)
#define ROCKET_BOSS_HALF_HEIGHT (36.1591f*.5f*ROCKET_HOST_SCALE)
typedef struct RocketBossTarget {
    float position[3], radius, bottom, height, forward[2];
    int eligible, rear_only;
} RocketBossTarget;
typedef struct RocketBossContact {
    RocketSnapshot previous;
    float boss_position[3];
    uint32_t epoch;
    int valid, armed;
} RocketBossContact;
static inline int rocket_bumper_contact(RocketBossContact *track,
        const RocketSnapshot *car, uint32_t epoch, const RocketBossTarget *boss, float min_speed) {
    if (!car || !boss) { memset(track,0,sizeof(*track)); return 0; }
    float length=hypotf(car->basis[0],car->basis[2]);
    if (!isfinite(length) || length<.5f || car->basis[7]<.5f) {
        memset(track,0,sizeof(*track)); return 0;
    }
    float fx=car->basis[0]/length, fz=car->basis[2]/length;
    float nx=car->position[0]+fx*ROCKET_BOSS_FRONT-boss->position[0];
    float nz=car->position[2]+fz*ROCKET_BOSS_FRONT-boss->position[2];
    float radius=boss->radius+ROCKET_BOSS_HALF_WIDTH;
    int outside=nx*nx+nz*nz>(radius+30.f)*(radius+30.f);
    int hit=0;
    uint64_t delta=car->ticks-track->previous.ticks;
    if (track->valid && track->epoch==epoch && !delta) return 0;
    if (track->valid && track->epoch==epoch && delta>0 && delta<=24) {
        const RocketSnapshot *previous=&track->previous;
        float oldLength=hypotf(previous->basis[0],previous->basis[2]);
        float oldX=previous->position[0]+previous->basis[0]/oldLength*ROCKET_BOSS_FRONT-track->boss_position[0];
        float oldZ=previous->position[2]+previous->basis[2]/oldLength*ROCKET_BOSS_FRONT-track->boss_position[2];
        float dx=nx-oldX,dz=nz-oldZ,denom=dx*dx+dz*dz;
        float enter=0,leave=1;
        float c=oldX*oldX+oldZ*oldZ-radius*radius;
        if(denom>.000001f) {
            float b=2.f*(oldX*dx+oldZ*dz),discriminant=b*b-4.f*denom*c;
            if(discriminant<0)leave=-1;
            else {
                float root=sqrtf(discriminant);
                enter=fmaxf(enter,(-b-root)/(2.f*denom));
                leave=fminf(leave,(-b+root)/(2.f*denom));
            }
        } else if(c>0)leave=-1;
        float travelX=car->position[0]-previous->position[0];
        float travelZ=car->position[2]-previous->position[2];
        float y=previous->position[1]-track->boss_position[1]+boss->position[1];
        float dy=car->position[1]-y;
        float low=boss->bottom-ROCKET_BOSS_HALF_HEIGHT,high=boss->bottom+boss->height+ROCKET_BOSS_HALF_HEIGHT;
        if(fabsf(dy)>.000001f) {
            float a=(low-y)/dy,b=(high-y)/dy;
            enter=fmaxf(enter,fminf(a,b));leave=fminf(leave,fmaxf(a,b));
        } else if(y<low||y>high)leave=-1;
        float behindX=previous->position[0]-track->boss_position[0];
        float behindZ=previous->position[2]-track->boss_position[2];
        int touching=enter<=leave;
        int rear=!boss->rear_only || behindX*boss->forward[0]+behindZ*boss->forward[1]<=-.5f*hypotf(behindX,behindZ);
        float actualSpeed=(travelX*fx+travelZ*fz)*120.f/(float)delta;
        hit=track->armed && touching && rear && boss->eligible &&
            car->velocity[0]*fx+car->velocity[2]*fz>=min_speed &&
            actualSpeed>=min_speed*.5f &&
            travelX*travelX+travelZ*travelZ<=1000.f*1000.f &&
            behindX*fx+behindZ*fz<0;
        // Even an immune/slow contact consumes the entry. Separate before retry.
        if (touching) track->armed=outside;
        else if(outside) track->armed=1;
    } else track->armed=outside;
    track->previous=*car;
    memcpy(track->boss_position,boss->position,sizeof(track->boss_position));
    track->epoch=epoch;track->valid=1;
    return hit;
}
static inline int rocket_boss_contact(RocketBossContact *track,
        const RocketSnapshot *car, uint32_t epoch, const RocketBossTarget *boss) {
    return rocket_bumper_contact(track,car,epoch,boss,ROCKET_BOSS_MIN_SPEED);
}
#endif
