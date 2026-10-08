/* The car cannot use Mario's look-up pose. Park on the native castle beam
 * for two seconds, then let the existing F2 warp own the entire transition. */
#include "rocket_beam.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "area.h"
#include "display.h"
#include "hardcoded.h"
#include "level_update.h"
#include "save_file.h"
#include "engine/surface_collision.h"
#include "surface_terrains.h"
#include "level_table.h"
#include "object_list_processor.h"
#include <math.h>
#include <string.h>

static struct {
    struct MarioState *owner;
    struct Object *body;
    struct Area *area;
    u32 frame;
    unsigned ticks;
    int haveFrame,fired;
} dwell;

void rocket_beam_reset(void){memset(&dwell,0,sizeof dwell);}

static int parked(const RocketSnapshot *pose,const RocketInput *input){
    if(!pose->grounded||pose->flipping||pose->boosting||pose->water_mode!=ROCKET_WATER_DRY||
       !isfinite(pose->basis[7])||pose->basis[7]<.98f||input->jump||input->boost||
       !isfinite(input->throttle)||fabsf(input->throttle)>.1f||
       !isfinite(input->steer)||fabsf(input->steer)>.1f)return 0;
    for(int k=0;k<3;k++)if(!isfinite(pose->position[k])||!isfinite(pose->velocity[k])||
        fabsf(pose->velocity[k])>30.f||!isfinite(pose->angular_velocity[k])||
        fabsf(pose->angular_velocity[k])>.2f)return 0;
    for(int k=0;k<4;k++)if(!pose->wheel_contacts[k])return 0;
    return 1;
}

void rocket_beam_update(struct MarioState *m,const RocketSnapshot *pose){
    RocketInput input;
    struct Surface *floor=NULL;
    if(!m||m->playerIndex||!m->marioObj||!m->area||m->area!=gCurrentArea||
       gCurrLevelNum!=LEVEL_CASTLE||m->area->index!=1||!rocket_adapter_car_selected()||
       m->action!=ACT_IDLE||m->health<0x100||m->heldByObj||m->riddenObj||
       m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED||gTimeStopState||!pose||
       !rocket_adapter_read_input(&input)||!parked(pose,&input)){
        rocket_beam_reset();return;
    }
    float height=find_floor(pose->position[0],pose->position[1],pose->position[2],&floor);
    if(!floor||floor->type!=SURFACE_LOOK_UP_WARP||!isfinite(height)||
       pose->position[1]<height||pose->position[1]-height>70.f||
       !area_get_warp_node(WARP_NODE_F2)||
       save_file_get_total_star_count(gCurrSaveFileNum-1,COURSE_MIN-1,COURSE_MAX-1)<gLevelValues.wingCapLookUpReq){
        rocket_beam_reset();return;
    }
    if(dwell.owner!=m||dwell.body!=m->marioObj||dwell.area!=m->area)rocket_beam_reset();
    dwell.owner=m;dwell.body=m->marioObj;dwell.area=m->area;
    if(dwell.fired)return;
    if(sDelayedWarpOp!=WARP_OP_NONE||sWarpDest.type!=WARP_TYPE_NOT_WARPING||gWarpTransition.isActive){
        dwell.ticks=dwell.haveFrame=0;return;
    }
    if(dwell.haveFrame&&dwell.frame==gGlobalTimer)return;
    if(!dwell.haveFrame||(u32)(gGlobalTimer-dwell.frame)!=1)dwell.ticks=0;
    dwell.frame=gGlobalTimer;dwell.haveFrame=1;
    if(++dwell.ticks==60){
        dwell.fired=1;
        level_trigger_warp(m,WARP_OP_LOOK_UP);
    }
}
