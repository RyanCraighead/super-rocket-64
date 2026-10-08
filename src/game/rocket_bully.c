#include "rocket_bully.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "mario.h"
#include "area.h"
#include "interaction.h"
#include "behavior_data.h"
#include "object_constants.h"
#include "pc/rocket_runtime.h"
#include "pc/network/network.h"
#include "engine/math_util.h"
#include "../../codex/rocketleague/physics/body_contact.h"
#include <math.h>

int rocket_bully_ram(struct MarioState *m,struct Object *o) {
    if(!m||m->playerIndex||!o||o->oInteractType!=INTERACT_BULLY||
       !(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->oIntangibleTimer||o->oSyncDeath||
       (o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
       (o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)||
       o->oAction==BULLY_ACT_LAVA_DEATH||o->oAction==BULLY_ACT_DEATH_PLANE_DEATH||
       !m->area||o->header.gfx.activeAreaIndex!=m->area->index)return 0;
    /* Use native ownership without replacing the Bully's lifecycle callbacks.
     * Remote Mario packets never invent another player's mapped throttle. */
    if(!gCLIOpts.offline&&gNetworkType!=NT_NONE) {
        if(!gCLIOpts.characterNet||!gNetworkAreaLoaded||gNetworkAreaSyncing||
           !gNetworkPlayerLocal||!gNetworkPlayerLocal->currLevelSyncValid||
           !gNetworkPlayerLocal->currAreaSyncValid||!o->oSyncID||
           !sync_object_is_initialized(o->oSyncID)||!sync_object_should_own(o->oSyncID))return 0;
    }
    RocketSnapshot car;RocketInput input;
    if(!rocket_adapter_body_snapshot(m->marioObj,&car)||!rocket_adapter_read_input(&input)||
       !isfinite(input.throttle)||input.throttle<=.2f||!car.grounded||car.basis[7]<.5f)return 0;
    float position[3]={o->oPosX,o->oPosY-o->hitboxDownOffset,o->oPosZ};
    if(!rocket_body_overlaps_cylinder(&car,position,o->hitboxRadius,o->hitboxHeight)||
       !rocket_adapter_enemy_visible(car.position,o,0))return 0;
    float dx=o->oPosX-car.position[0],dz=o->oPosZ-car.position[2];
    float distance=hypotf(dx,dz),heading=hypotf(car.basis[0],car.basis[2]);
    float speed=hypotf(car.velocity[0],car.velocity[2]);
    if(!isfinite(distance)||distance<1.f||!isfinite(heading)||heading<.5f||
       !isfinite(speed)||!isfinite(o->oForwardVel))return 0;
    float nx=dx/distance,nz=dz/distance,fx=car.basis[0]/heading,fz=car.basis[2]/heading;
    float forward=car.velocity[0]*fx+car.velocity[2]*fz;
    float approach=car.velocity[0]*nx+car.velocity[2]*nz;
    /* Car velocities are host units/second; native actor velocities are per
     * 30 Hz frame. Holding throttle against a wall is never an attack. */
    /* Native object_step derives motion from forwardVel/yaw, not oVelX/Z. */
    float closing=approach-30.f*o->oForwardVel*(sins(o->oMoveAngleYaw)*nx+coss(o->oMoveAngleYaw)*nz);
    return nx*fx+nz*fz>=.70710678f&&forward>=180.f&&approach>=speed*.65f&&closing>=120.f;
}
