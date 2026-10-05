#ifndef ROCKET_VANISH_COLLISION_H
#define ROCKET_VANISH_COLLISION_H
#include "rocket_physics.h"
#include <math.h>

/* Pinned CAR_CONFIG_OCTANE chassis, including its forward/up offset. This is
 * deliberately the complete box, not Mario's capsule or the car origin. */
static inline int rocket_car_triangle_overlap(const RocketSnapshot *car,
        const float triangle[3][3], float clearance) {
    const float half[3]={120.507f*.5f*ROCKET_HOST_SCALE+clearance,
        86.6994f*.5f*ROCKET_HOST_SCALE+clearance,
        38.6591f*.5f*ROCKET_HOST_SCALE+clearance};
    const float offset[3]={13.8757f*ROCKET_HOST_SCALE,0,20.755f*ROCKET_HOST_SCALE};
    float v[3][3],edges[3][3],axes[13][3]={{1,0,0},{0,1,0},{0,0,1}};
    for(int i=0;i<3;i++)for(int a=0;a<3;a++){
        v[i][a]=-offset[a];
        for(int k=0;k<3;k++)v[i][a]+=(triangle[i][k]-car->position[k])*car->basis[a*3+k];
    }
    for(int i=0;i<3;i++)for(int a=0;a<3;a++)edges[i][a]=v[(i+1)%3][a]-v[i][a];
    for(int a=0;a<3;a++)axes[3][a]=edges[0][(a+1)%3]*edges[1][(a+2)%3]-edges[0][(a+2)%3]*edges[1][(a+1)%3];
    float area=0;for(int a=0;a<3;a++)area+=axes[3][a]*axes[3][a];
    if(area<.000001f)return 0;
    for(int i=0;i<3;i++)for(int a=0;a<3;a++)for(int k=0;k<3;k++)
        axes[4+i*3+a][k]=edges[i][(k+1)%3]*(a==(k+2)%3)-edges[i][(k+2)%3]*(a==(k+1)%3);
    for(int i=0;i<13;i++){
        float radius=0,low=0,high=0;
        for(int a=0;a<3;a++){radius+=half[a]*fabsf(axes[i][a]);low+=v[0][a]*axes[i][a];}
        high=low;
        for(int j=1;j<3;j++){
            float p=0;for(int a=0;a<3;a++)p+=v[j][a]*axes[i][a];
            low=fminf(low,p);high=fmaxf(high,p);
        }
        if(low>radius||high<-radius)return 0;
    }
    return 1;
}
#endif
