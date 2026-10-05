#ifndef ROCKET_SWITCH_CONTACT_H
#define ROCKET_SWITCH_CONTACT_H
#include "whomp_impact.h"

/* Lowest chassis point above the switch's actual XZ footprint. The global
 * lowest nose/corner often lies beyond a small button while another face
 * contacts its top. Clip the six box faces against the four vertical bounds;
 * no car-origin containment, radius expansion or generated collision is used.
 * The native adapter must still resolve the witness to a solid top triangle. */
static inline int rocket_switch_lowest(const RocketSnapshot *car,const RocketWhompBack *top,float out[3]) {
    if(!rocket_enemy_valid_pose(car)||!top||!out)return 0;
    const float half[]={ROCKET_ENEMY_HALF_LENGTH,ROCKET_ENEMY_HALF_WIDTH,ROCKET_ENEMY_HALF_HEIGHT};
    float vertices[8][3],best[3]={0,INFINITY,0};int found=0;
    for(int v=0;v<8;v++){
        float world[3];
        for(int k=0;k<3;k++){
            world[k]=car->position[k]+car->basis[k]*ROCKET_ENEMY_OFFSET+car->basis[6+k]*ROCKET_ENEMY_OFFSET_UP;
            for(int axis=0;axis<3;axis++)world[k]+=car->basis[3*axis+k]*half[axis]*((v&(1<<axis))?1.f:-1.f);
        }
        float x=world[0]-top->position[0],z=world[2]-top->position[2];
        vertices[v][0]=x*top->right[0]+z*top->right[1];vertices[v][1]=world[1];
        vertices[v][2]=x*top->forward[0]+z*top->forward[1];
    }
    for(int axis=0;axis<3;axis++)for(int side=0;side<2;side++){
        int a=(axis+1)%3,b=(axis+2)%3,count=4;
        const int order[]={0,1,3,2};float polygon[16][3],next[16][3];
        for(int i=0;i<4;i++)memcpy(polygon[i],vertices[(side<<axis)|((order[i]&1)<<a)|(((order[i]>>1)&1)<<b)],sizeof polygon[i]);
        for(int plane=0;plane<4&&count;plane++){
            int component=plane<2?0:2,dimension=plane/2,upper=plane&1,n=0;
            float bound=upper?top->high[dimension]-ROCKET_WHOMP_EDGE-.25f:top->low[dimension]+ROCKET_WHOMP_EDGE+.25f;
            for(int i=0;i<count;i++){
                const float *p=polygon[i],*q=polygon[(i+1)%count];
                float dp=upper?bound-p[component]:p[component]-bound;
                float dq=upper?bound-q[component]:q[component]-bound;
                if(dp>=0){if(n>=16)return 0;memcpy(next[n++],p,sizeof next[0]);}
                if((dp>=0)!=(dq>=0)){
                    if(n>=16)return 0;
                    float fraction=dp/(dp-dq);
                    for(int k=0;k<3;k++){next[n][k]=p[k]+fraction*(q[k]-p[k]);}
                    n++;
                }
            }
            count=n;memcpy(polygon,next,(size_t)count*sizeof polygon[0]);
        }
        for(int i=0;i<count;i++)if(polygon[i][1]<best[1]){memcpy(best,polygon[i],sizeof best);found=1;}
    }
    if(!found)return 0;
    out[0]=top->position[0]+best[0]*top->right[0]+best[2]*top->forward[0];
    out[1]=best[1];out[2]=top->position[2]+best[0]*top->right[1]+best[2]*top->forward[1];return 1;
}
static inline int rocket_switch_contact(RocketWhompContact *track,const RocketSnapshot *car,
        uint32_t epoch,const RocketWhompBack *top,float point[3]) {
    return rocket_whomp_contact_with_support(track,car,epoch,top,point,rocket_switch_lowest);
}
#endif
