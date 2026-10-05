#ifndef ROCKET_SQUISH_VISUAL_H
#define ROCKET_SQUISH_VISUAL_H
#include "sm64.h"
#include <math.h>
/* Existing native packet fields carry the action and squish timer. Rendering
 * never changes them, damages a peer, or scales the orthonormal physics pose. */
extern u8 sSquishScaleOverTime[16];
static inline void rocket_squish_visual_scale(const struct MarioState *m,float scale[3]) {
    scale[0]=scale[1]=scale[2]=1;
    if(!m||!m->marioObj||!m->action||m->action==ACT_DISAPPEARED||m->action==ACT_BUBBLED)return;
    if(m->playerIndex==0) {
        if(m->action!=ACT_SQUISHED&&!m->squishTimer)return;
        for(int k=0;k<3;k++)if(!isfinite(m->marioObj->header.gfx.scale[k])||
            m->marioObj->header.gfx.scale[k]<.05f||m->marioObj->header.gfx.scale[k]>2.f)return;
        for(int k=0;k<3;k++)scale[k]=m->marioObj->header.gfx.scale[k];
    } else if(m->squishTimer==255) {
        /* Remote ceiling geometry need not match the owner's frame. Show its
         * reported squash without running the native damage action locally. */
        if(m->action==ACT_SQUISHED){scale[0]=scale[2]=1.8f;scale[1]=.05f;}
    } else if(m->squishTimer&&m->squishTimer<=30) {
        unsigned timer=m->squishTimer+1; // Packet follows the native decrement.
        if(timer>16){scale[0]=scale[2]=1.4f;scale[1]=.4f;}
        else {float wave=sSquishScaleOverTime[16-timer]/100.f;scale[0]=scale[2]=1.f+wave*.4f;scale[1]=1.f-wave*.6f;}
    }
}
/* Scale about the tires, including wheel centers and chassis offsets. */
static inline void rocket_squish_vertex(float position[3],float normal[3],const float pivot[3],const float scale[3]) {
    if(scale[0]==1&&scale[1]==1&&scale[2]==1)return;
    float length=0;
    for(int k=0;k<3;k++){position[k]=pivot[k]+(position[k]-pivot[k])*scale[k];normal[k]/=scale[k];length+=normal[k]*normal[k];}
    length=sqrtf(length);if(length>0)for(int k=0;k<3;k++)normal[k]/=length;
}
#endif
