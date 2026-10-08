#ifndef ROCKET_QUICKSAND_VISUAL_H
#define ROCKET_QUICKSAND_VISUAL_H
#include "rocket_physics.h"
#include <math.h>
/* Presentation copy only. Physical coordinates and orthonormal basis stay put. */
static inline void rocket_quicksand_visual_pose(RocketSnapshot *pose){
    if(!pose||!isfinite(pose->quicksand_depth)||pose->quicksand_depth<0||pose->quicksand_depth>200)return;
    pose->position[1]-=pose->quicksand_depth;
    for(int i=0;i<4;i++)pose->wheel_position[i][1]-=pose->quicksand_depth;
}
#endif
