#ifndef ROCKET_WHOMP_IMPACT_H
#define ROCKET_WHOMP_IMPACT_H
#include "enemy_impact.h"

/* Authored crossover thresholds, SM64 units/second and radians/second. */
#define ROCKET_WHOMP_FLIP_SPEED 120.f
#define ROCKET_WHOMP_DIVE_SPEED 1200.f
#define ROCKET_WHOMP_SPIN 3.f
#define ROCKET_WHOMP_MAX_SWEEP_SPIN 10.f
#define ROCKET_WHOMP_EDGE 16.f
#define ROCKET_WHOMP_SKIN 8.f
typedef struct RocketWhompBack {
    float position[3], right[2], forward[2], low[2], high[2], height;
    int eligible;
} RocketWhompBack;
typedef struct RocketWhompContact {
    RocketSnapshot previous;
    RocketWhompBack back;
    uint32_t epoch;
    int valid, armed;
} RocketWhompContact;

/* A point on the lowest chassis face/edge/corner. Zero components retain the
 * face center, so a vertical nose dive uses the center of the front bumper. */
static inline void rocket_whomp_lowest(const RocketSnapshot *c,float p[3]) {
    const float half[]={ROCKET_ENEMY_HALF_LENGTH,ROCKET_ENEMY_HALF_WIDTH,ROCKET_ENEMY_HALF_HEIGHT};
    float local[]={ROCKET_ENEMY_OFFSET,0,ROCKET_ENEMY_OFFSET_UP};
    for(int i=0;i<3;i++)if(fabsf(c->basis[3*i+1])>.001f)
        local[i]-=copysignf(half[i],c->basis[3*i+1]);
    for(int k=0;k<3;k++)p[k]=c->position[k]+local[0]*c->basis[k]+local[1]*c->basis[3+k]+local[2]*c->basis[6+k];
}
static inline int rocket_whomp_inside(const RocketWhompBack *b,const float p[3]) {
    float x=p[0]-b->position[0],z=p[2]-b->position[2];
    float at[]={x*b->right[0]+z*b->right[1],x*b->forward[0]+z*b->forward[1]};
    return at[0]>=b->low[0]+ROCKET_WHOMP_EDGE&&at[0]<=b->high[0]-ROCKET_WHOMP_EDGE&&
        at[1]>=b->low[1]+ROCKET_WHOMP_EDGE&&at[1]<=b->high[1]-ROCKET_WHOMP_EDGE;
}
/* Whomp-only passive attack: all four real tires must rest on the exposed
 * back. The host additionally resolves every witness to this object's loaded
 * upward-facing native collision. Kept separate from flip/dive policy because
 * switches and other interactions must not gain a passive attack. */
static inline int rocket_whomp_wheels(const RocketSnapshot *c,const RocketWhompBack *b,float points[4][3]) {
    if(!rocket_enemy_valid_pose(c)||!b||!b->eligible||!c->grounded||c->flipping||
       c->basis[7]<.95f||c->position[1]<=b->height||!rocket_whomp_inside(b,c->position))return 0;
    for(int i=0;i<4;i++){
        if(!c->wheel_contacts[i]||!isfinite(c->wheel_radius[i])||
           c->wheel_radius[i]<=0||c->wheel_radius[i]>64.f)return 0;
        for(int k=0;k<3;k++){
            if(!isfinite(c->wheel_position[i][k])||fabsf(c->wheel_position[i][k]-c->position[k])>200.f)return 0;
            points[i][k]=c->wheel_position[i][k];
        }
        points[i][1]-=c->wheel_radius[i];
        if(fabsf(points[i][1]-b->height)>ROCKET_WHOMP_SKIN||!rocket_whomp_inside(b,points[i]))return 0;
    }
    return 1;
}
/* Returns 1=flip, 2=boosted dive; witness is an actual chassis support point.
 * Target motion, duplicate ticks, reset epochs and overlap cannot create entry.
 * Post-contact velocity may already have been stopped by Bullet; intent and
 * speed come from the immediately preceding airborne physical sample.
 * Flips damp center velocity in RocketSim. Measure the actual descending
 * chassis support instead, so pitching/rolling into the back needs no boost. */
static inline int rocket_whomp_contact(RocketWhompContact *t,const RocketSnapshot *c,
        uint32_t epoch,const RocketWhompBack *b,float witness[3]) {
    if(!t)return 0;
    if(!rocket_enemy_valid_pose(c)||!b){memset(t,0,sizeof(*t));return 0;}
    float p[3];rocket_whomp_lowest(c,p);
    uint64_t dt=c->ticks-t->previous.ticks;
    if(t->valid&&epoch==t->epoch&&!dt)return 0;
    int hit=0,above=p[1]>b->height+ROCKET_WHOMP_SKIN+8.f;
    if(t->valid&&epoch==t->epoch&&dt>0&&dt<=12) {
        const RocketSnapshot *a=&t->previous;
        float old[3];rocket_whomp_lowest(a,old);
        float gap=old[1]-t->back.height,now=p[1]-b->height;
        float travel[3],distance=0,turn=0;
        for(int k=0;k<3;k++){travel[k]=c->position[k]-a->position[k];distance+=travel[k]*travel[k];}
        for(int k=0;k<9;k++){float d=c->basis[k]-a->basis[k];turn+=d*d;}
        float seconds=(float)dt/120.f;
        int contact=gap>ROCKET_WHOMP_SKIN&&now<=ROCKET_WHOMP_SKIN;
        int stationary=fabsf(b->height-t->back.height)<1.f;
        for(int k=0;k<3;k++)stationary=stationary&&fabsf(b->position[k]-t->back.position[k])<1.f;
        for(int k=0;k<2;k++)stationary=stationary&&fabsf(b->right[k]-t->back.right[k])<.001f&&
            fabsf(b->low[k]-t->back.low[k])<.001f&&fabsf(b->high[k]-t->back.high[k])<.001f;
        int wheels=0;for(int k=0;k<4;k++)wheels|=a->wheel_contacts[k];
        int physical=t->armed&&contact&&b->eligible&&t->back.eligible&&stationary&&
            !a->grounded&&a->air_time>=1.f/30.f&&now>=-12.f&&
            c->position[1]>b->height&&
            distance<=7500.f*7500.f*seconds*seconds&&
            /* Bound rotation by elapsed physics time, including a lost pose.
             * A fixed per-packet cap rejected valid 10 Hz flip samples. */
            turn<=2.f*ROCKET_WHOMP_MAX_SWEEP_SPIN*ROCKET_WHOMP_MAX_SWEEP_SPIN*seconds*seconds+.01f&&
            rocket_whomp_inside(b,c->position)&&rocket_whomp_inside(b,p);
        if(physical) {
            float spin2=a->angular_velocity[0]*a->angular_velocity[0]+a->angular_velocity[2]*a->angular_velocity[2];
            int flip=a->flipped&&a->flipping&&isfinite(a->flip_time)&&a->flip_time>=0&&a->flip_time<=.65f&&
                isfinite(spin2)&&spin2>=ROCKET_WHOMP_SPIN*ROCKET_WHOMP_SPIN&&turn>=.5f*seconds*seconds&&
                gap-now>=ROCKET_WHOMP_FLIP_SPEED*seconds;
            /* One/two suspension rays can reach the back before a rotating
             * chassis. RocketSim still reports airborne; these are not a
             * passive landing. Boost dives retain the original wheel gate. */
            int dive=!wheels&&travel[1]<=-100.f*seconds&&a->boosting&&c->boosting&&a->basis[1]<=-.75f&&c->basis[1]<=-.75f&&
                a->velocity[1]<=-ROCKET_WHOMP_DIVE_SPEED;
            hit=flip?1:dive?2:0;
            if(hit&&witness)memcpy(witness,p,sizeof p);
        }
        /* Failed/immune/slow entries also need a complete separation. */
        if(contact||now<=ROCKET_WHOMP_SKIN)t->armed=0;
        else if(above)t->armed=1;
    } else t->armed=above;
    t->previous=*c;t->back=*b;t->epoch=epoch;t->valid=1;
    return hit;
}
#endif
