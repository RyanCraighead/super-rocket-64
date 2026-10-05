#ifndef ROCKET_ENEMY_IMPACT_H
#define ROCKET_ENEMY_IMPACT_H
#include "rocket_physics.h"
#include <math.h>
#include <string.h>

/* RocketSim c2baacb8, RLConst::SUPERSONIC_START_SPEED. This policy uses the
 * entry threshold, not isSupersonic's lower-speed/time hysteresis. */
#define ROCKET_ENEMY_SUPERSONIC_SPEED (2200.f * ROCKET_HOST_SCALE)
/* Pinned Octane CarConfig: dimensions and local center offset, in host units. */
#define ROCKET_ENEMY_HALF_LENGTH (120.507f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_ENEMY_HALF_WIDTH (86.6994f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_ENEMY_HALF_HEIGHT (38.6591f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_ENEMY_OFFSET (13.8757f * ROCKET_HOST_SCALE)
#define ROCKET_ENEMY_OFFSET_UP (20.755f * ROCKET_HOST_SCALE)

typedef struct RocketEnemyTarget {
    float position[3], radius, bottom, height;
} RocketEnemyTarget;
typedef struct RocketEnemyContact {
    RocketSnapshot previous;
    RocketEnemyTarget target;
    uint32_t epoch;
    int valid;
} RocketEnemyContact;

static inline float rocket_enemy_dot(const float a[3], const float b[3]) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static inline int rocket_enemy_supersonic(const RocketSnapshot *car) {
    if (!car) return 0;
    for (int k=0;k<3;k++) if (!isfinite(car->velocity[k])) return 0;
    float speed2=rocket_enemy_dot(car->velocity,car->velocity);
    return isfinite(speed2) && speed2>=ROCKET_ENEMY_SUPERSONIC_SPEED*ROCKET_ENEMY_SUPERSONIC_SPEED;
}
static inline int rocket_enemy_valid_pose(const RocketSnapshot *car) {
    if (!car) return 0;
    for (int k=0;k<3;k++) if (!isfinite(car->position[k]) || !isfinite(car->velocity[k])) return 0;
    for (int k=0;k<9;k++) if (!isfinite(car->basis[k])) return 0;
    for (int a=0;a<3;a++) for (int b=a;b<3;b++)
        if (fabsf(rocket_enemy_dot(car->basis+3*a,car->basis+3*b)-(a==b?1.f:0.f))>.02f) return 0;
    return 1;
}
/* Swept OBB vs the bounds of the enemy's native cylinder. All 15 separating
 * axes are used, with vertical overlap at the same instant as horizontal.
 * The current chassis orientation is held over the short translation sweep;
 * rapid rotations fail closed instead of sweeping a large imaginary volume. */
static inline int rocket_enemy_axis(const float axis[3],const float p[3],const float d[3],
        const RocketSnapshot *car,const float half[3],float *enter,float *leave) {
    const float size[3]={ROCKET_ENEMY_HALF_LENGTH,ROCKET_ENEMY_HALF_WIDTH,ROCKET_ENEMY_HALF_HEIGHT};
    float radius=0;
    for(int k=0;k<3;k++) radius+=fabsf(axis[k])*half[k]+fabsf(rocket_enemy_dot(axis,car->basis+3*k))*size[k];
    float at=rocket_enemy_dot(axis,p),delta=rocket_enemy_dot(axis,d);
    if(fabsf(delta)<.00001f) return fabsf(at)<=radius;
    float a=(-radius-at)/delta,b=(radius-at)/delta;
    *enter=fmaxf(*enter,fminf(a,b));*leave=fminf(*leave,fmaxf(a,b));
    return *enter<=*leave;
}
static inline int rocket_enemy_contact(RocketEnemyContact *track,const RocketSnapshot *car,
        uint32_t epoch,const RocketEnemyTarget *target) {
    if(!track) return 0;
    if(!rocket_enemy_valid_pose(car)||!target||!isfinite(target->radius)||target->radius<=0||
       !isfinite(target->height)||target->height<=0||!isfinite(target->bottom)) {
        memset(track,0,sizeof(*track));return 0;
    }
    for(int k=0;k<3;k++) if(!isfinite(target->position[k])) {memset(track,0,sizeof(*track));return 0;}
    uint64_t delta=car->ticks-track->previous.ticks;
    if(track->valid&&epoch==track->epoch&&!delta) return 0;
    int hit=0;
    if(track->valid&&epoch==track->epoch&&delta>0&&delta<=24&&rocket_enemy_supersonic(car)) {
        const RocketSnapshot *old=&track->previous;
        float travel[3],p[3],d[3];
        float half[3]={target->radius,target->height*.5f,target->radius};
        for(int k=0;k<3;k++) {
            float offset=car->basis[k]*ROCKET_ENEMY_OFFSET+car->basis[6+k]*ROCKET_ENEMY_OFFSET_UP;
            float nowCenter=target->position[k],oldCenter=track->target.position[k];
            if(k==1){nowCenter=target->bottom+half[1];oldCenter=track->target.bottom+track->target.height*.5f;}
            travel[k]=car->position[k]-old->position[k];
            p[k]=old->position[k]+offset-oldCenter;
            d[k]=travel[k]-(nowCenter-oldCenter);
        }
        float seconds=(float)delta*ROCKET_TICK_SECONDS;
        float distance2=rocket_enemy_dot(travel,travel);
        float speed2=rocket_enemy_dot(car->velocity,car->velocity);
        /* Reject teleports / stale jumps and velocity-only fabricated contacts.
         * 2300 RL units/sec is the pinned car speed cap; 80 units covers source
         * contact correction. Entry must have actual translation toward motion. */
        float maxTravel=2300.f*ROCKET_HOST_SCALE*seconds+80.f;
        int continuous=distance2<=maxTravel*maxTravel&&
            rocket_enemy_dot(travel,car->velocity)>=speed2*seconds*.25f;
        for(int k=0;k<3;k++) if(rocket_enemy_dot(old->basis+3*k,car->basis+3*k)<.95f) continuous=0;
        float enter=0,leave=1;
        hit=continuous;
        for(int a=0;a<3&&hit;a++) {
            float axis[3]={0};axis[a]=1;
            hit=rocket_enemy_axis(axis,p,d,car,half,&enter,&leave)&&
                rocket_enemy_axis(car->basis+3*a,p,d,car,half,&enter,&leave);
            for(int b=0;b<3&&hit;b++) {
                const float *v=car->basis+3*b;
                float cross[3]={axis[1]*v[2]-axis[2]*v[1],axis[2]*v[0]-axis[0]*v[2],axis[0]*v[1]-axis[1]*v[0]};
                hit=rocket_enemy_axis(cross,p,d,car,half,&enter,&leave);
            }
        }
    }
    track->previous=*car;track->target=*target;track->epoch=epoch;track->valid=1;
    return hit;
}
#endif
