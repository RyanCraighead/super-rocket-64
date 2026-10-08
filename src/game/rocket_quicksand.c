#include "rocket_quicksand.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "mario.h"
#include "mario_step.h"
#include "area.h"
#include "level_update.h"
#include "display.h"
#include "engine/surface_collision.h"
#include "../../codex/rocketleague/physics/body_contact.h"
#include <math.h>

static struct MarioState *owner;
static struct Object *object;
static struct Area *area;
static s16 level;
static u32 sinkFrame,inputFrame;
static int haveSinkFrame,haveInputFrame,sinkSeverity,touching,jumpHeld=1,consumedJump,escapeTimer;

static int severity(const struct Surface *s) {
    if(!s)return 0;
    switch(s->type){
        case SURFACE_INSTANT_QUICKSAND:case SURFACE_INSTANT_MOVING_QUICKSAND:return 5;
        case SURFACE_DEEP_QUICKSAND:case SURFACE_DEEP_MOVING_QUICKSAND:return 4;
        case SURFACE_QUICKSAND:case SURFACE_MOVING_QUICKSAND:return 3;
        case SURFACE_SHALLOW_MOVING_QUICKSAND:return 2;
        case SURFACE_SHALLOW_QUICKSAND:return 1;
        default:return 0;
    }
}
static struct Surface *sand_below(const float point[3],float radius) {
    if(!isfinite(radius)||radius<0||radius>64)return NULL;
    for(int k=0;k<3;k++)if(!isfinite(point[k]))return NULL;
    struct Surface *floor=NULL;float h=find_floor(point[0],point[1]+8,point[2],&floor);
    return severity(floor)&&floor->normal.y>.01f&&isfinite(h)&&fabsf(point[1]-radius-h)<8?floor:NULL;
}
static struct Surface *sand_contact(const RocketSnapshot *pose) {
    if(!rocket_body_pose_valid(pose)||pose->water_mode!=ROCKET_WATER_DRY)return NULL;
    struct Surface *best=NULL,*s;
    for(int i=0;i<4;i++)if(pose->wheel_contacts[i]&&pose->wheel_radius[i]>0){
        s=sand_below(pose->wheel_position[i],pose->wheel_radius[i]);
        if(severity(s)>severity(best))best=s;
    }
    // A roof-down car must not gain immunity to a fatal pit. Keep the real
    // oriented body contact, without using its higher chassis origin as feet.
    const float half[]={ROCKET_BODY_HALF_LENGTH,ROCKET_BODY_HALF_WIDTH,ROCKET_BODY_HALF_HEIGHT};
    for(int v=0;v<8;v++){
        float p[3];for(int k=0;k<3;k++){
            p[k]=pose->position[k]+pose->basis[k]*ROCKET_BODY_FORWARD_OFFSET+pose->basis[6+k]*ROCKET_BODY_UP_OFFSET;
            for(int axis=0;axis<3;axis++)p[k]+=pose->basis[axis*3+k]*half[axis]*((v&(1<<axis))?1.f:-1.f);
        }
        s=sand_below(p,0);if(severity(s)>severity(best))best=s;
    }
    return best;
}
void rocket_quicksand_suspend(void){
    owner=NULL;object=NULL;area=NULL;touching=0;escapeTimer=0;
    haveSinkFrame=haveInputFrame=0;jumpHeld=1;consumedJump=0;
    // Native death/presentation still owns the Mario depth: do not erase it.
}
float rocket_quicksand_depth(void){
    if(!owner||owner->playerIndex||!rocket_adapter_car_selected()||!touching||!isfinite(owner->quicksandDepth))return 0;
    return fminf(200.f,fmaxf(0,owner->quicksandDepth));
}
int rocket_quicksand_update(struct MarioState *m,const RocketSnapshot *pose){
    if(!m||m->playerIndex||!m->marioObj||!m->area||!rocket_adapter_car_selected())return 0;
    if(owner!=m||object!=m->marioObj||area!=m->area||level!=gCurrLevelNum){
        rocket_quicksand_suspend();owner=m;object=m->marioObj;area=m->area;level=gCurrLevelNum;
    }
    struct Surface *sand=sand_contact(pose);touching=sand!=NULL;
    if(!touching){m->quicksandDepth=0;escapeTimer=0;return 0;}
    if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED)return 0;
    int already=haveSinkFrame&&sinkFrame==gGlobalTimer;
    if(already&&severity(sand)<=sinkSeverity)return m->action==ACT_QUICKSAND_DEATH;
    sinkFrame=gGlobalTimer;haveSinkFrame=1;
    sinkSeverity=severity(sand);
    // The real native function owns the hazard hook, caps, deep threshold and
    // instant death. Temporarily use the actual contacted triangle for the call.
    struct Surface *saved=m->floor;m->floor=sand;
    float sink=escapeTimer||hypotf(pose->velocity[0],pose->velocity[2])>3.f?.25f:.5f;
    if(already)sink=0; // A newly contacted fatal pit still acts this frame.
    if(!isfinite(m->quicksandDepth)||m->quicksandDepth<0)m->quicksandDepth=0;
    int changed=mario_update_quicksand(m,sink);m->floor=saved;
    if(m->quicksandDepth==0)escapeTimer=0; // Native hazard veto.
    return changed;
}
void rocket_quicksand_filter_input(RocketInput *input,int blocked){
    if(!input||!owner||owner->playerIndex||!rocket_adapter_car_selected())return;
    blocked=blocked||owner->freeze||sCurrPlayMode==PLAY_MODE_PAUSED;
    int held=!!input->jump,fresh=held&&!jumpHeld;
    if(blocked){jumpHeld=1;fresh=0;consumedJump=1;}else jumpHeld=held;
    if(!held&&!blocked)consumedJump=0;
    if(haveInputFrame&&inputFrame==gGlobalTimer){
        if(consumedJump||escapeTimer||blocked||(touching&&owner->quicksandDepth>=11.f))input->jump=0;
        return;
    }
    inputFrame=gGlobalTimer;haveInputFrame=1;
    if(blocked){input->jump=0;return;} // Pause the extraction interval with the simulation.
    if(touching&&fresh&&owner->quicksandDepth>=11.f&&!escapeTimer){escapeTimer=1;consumedJump=1;}
    if(escapeTimer){
        // Native quicksand_jump_land_action's six extraction steps followed
        // by its landing interval. Do not run a second native ground movement
        // on top of RocketSim or grant a jump while the car is still buried.
        if(escapeTimer<=6){owner->quicksandDepth-=(7-escapeTimer)*.8f;
            if(owner->quicksandDepth<1.f)owner->quicksandDepth=1.1f;}
        if(++escapeTimer>13)escapeTimer=0;
        input->jump=0;
    }
    if(consumedJump||blocked||(touching&&owner->quicksandDepth>=11.f))input->jump=0;
}
