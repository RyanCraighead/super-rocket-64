#ifndef ROCKET_POLE_POSE_H
#define ROCKET_POLE_POSE_H
#include "body_contact.h"
#include <string.h>
/* Same complete Octane box, standing nose-up along the native pole. Its box
 * bottom is at feet and its horizontal center at the pole; no scale change. */
static inline void rocket_pole_pose(RocketSnapshot *pose,const float feet[3],float sine,float cosine){
    RocketSnapshot old=*pose;
    memset(pose->basis,0,sizeof pose->basis);
    pose->basis[1]=1;pose->basis[3]=cosine;pose->basis[5]=-sine;
    pose->basis[6]=-sine;pose->basis[8]=-cosine;
    for(int k=0;k<3;k++){
        pose->position[k]=feet[k]+(k==1?ROCKET_BODY_HALF_LENGTH-ROCKET_BODY_FORWARD_OFFSET:0)
            -pose->basis[6+k]*ROCKET_BODY_UP_OFFSET;
        pose->velocity[k]=pose->angular_velocity[k]=0;
    }
    for(int i=0;i<4;i++)for(int k=0;k<3;k++){
        float v=pose->position[k];
        for(int axis=0;axis<3;axis++){
            float local=0;
            for(int j=0;j<3;j++)local+=old.basis[axis*3+j]*(old.wheel_position[i][j]-old.position[j]);
            v+=pose->basis[axis*3+k]*local;
        }
        pose->wheel_position[i][k]=v;
    }
    pose->quicksand_depth=0;pose->grounded=0;
    for(int i=0;i<4;i++)pose->wheel_contacts[i]=0;
}
#endif
