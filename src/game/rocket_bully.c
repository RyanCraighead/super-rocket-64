#include "rocket_bully.h"
#include "rocket_adapter.h"
#include "character_presentation.h"
#include "sm64.h"
#include "mario.h"
#include "area.h"
#include "interaction.h"
#include "behavior_data.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include "display.h"
#include "level_update.h"
#include "engine/surface_collision.h"
#include "pc/rocket_runtime.h"
#include "pc/network/network.h"
#include "engine/math_util.h"
#include "../../codex/rocketleague/physics/body_contact.h"
#include <math.h>
#include <string.h>

/* The native knockback and bounded correction can open a small gap for one
 * frame. Require one correction step of clearance before arming another hit. */
#define CONTACT_RELEASE_MARGIN 64.f

static struct BullyContact {
    struct Object *object,*player;
    const BehaviorScript *behavior;
    struct Area *area;
    u32 sync_id,frame,observed;
    s16 level;
    int outgoing,overlapping;
    float normal[2];
} contacts[OBJECT_POOL_CAPACITY];

static int eligible(struct MarioState *m,struct Object *o) {
    return m&&!m->playerIndex&&m->marioObj&&m->area&&o&&o->oInteractType==INTERACT_BULLY&&
        m->health>=0x100&&(o->activeFlags&ACTIVE_FLAG_ACTIVE)&&!o->oIntangibleTimer&&!o->oSyncDeath&&
        !(o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))&&
        !(o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)&&
        o->oAction!=BULLY_ACT_LAVA_DEATH&&o->oAction!=BULLY_ACT_DEATH_PLANE_DEATH&&
        o->header.gfx.activeAreaIndex==m->area->index;
}
static int car_pose(struct MarioState *m,RocketSnapshot *car) {
    return m&&!m->playerIndex&&rocket_adapter_car_selected()&&
        (rocket_adapter_body_snapshot(m->marioObj,car)||character_presentation_car_snapshot(car));
}
static int owns(struct Object *o) {
    if(gCLIOpts.offline||gNetworkType==NT_NONE)return 1;
    return gCLIOpts.characterNet&&gNetworkAreaLoaded&&!gNetworkAreaSyncing&&gNetworkPlayerLocal&&
        gNetworkPlayerLocal->currLevelSyncValid&&gNetworkPlayerLocal->currAreaSyncValid&&o->oSyncID&&
        sync_object_is_initialized(o->oSyncID)&&sync_object_should_own(o->oSyncID);
}
static int overlaps(const RocketSnapshot *car,struct Object *o,float margin) {
    float bottom[3]={o->oPosX,o->oPosY-o->hitboxDownOffset-margin,o->oPosZ};
    return rocket_body_overlaps_cylinder(car,bottom,o->hitboxRadius+margin,o->hitboxHeight+2*margin);
}
static struct BullyContact *history(struct Object *o) {
    for(unsigned i=0;i<OBJECT_POOL_CAPACITY;i++)if(contacts[i].object==o)return &contacts[i];
    return NULL;
}
void rocket_bully_forget(struct Object *o) {
    struct BullyContact *h=history(o);if(h)memset(h,0,sizeof *h);
}
static int current(struct BullyContact *h,struct MarioState *m) {
    return h&&eligible(m,h->object)&&h->player==m->marioObj&&h->behavior==h->object->behavior&&
        h->sync_id==h->object->oSyncID&&h->area==m->area&&h->level==gCurrLevelNum&&rocket_adapter_car_selected();
}
void rocket_bully_prepare(void) {
    struct MarioState *m=&gMarioStates[0];RocketSnapshot car;int have=car_pose(m,&car);
    for(unsigned i=0;i<OBJECT_POOL_CAPACITY;i++) {
        struct BullyContact *h=&contacts[i];if(!h->object)continue;
        if(!current(h,m)){memset(h,0,sizeof *h);continue;}
        /* Native injury/pause may temporarily suspend the physics snapshot.
         * Only a measured gap rearms a live pair; missing data is not a gap. */
        if(have){h->overlapping=overlaps(&car,h->object,CONTACT_RELEASE_MARGIN);h->observed=gGlobalTimer;
            if(!h->overlapping&&h->frame!=gGlobalTimer)memset(h,0,sizeof *h);}
    }
}
int rocket_bully_repeat(struct MarioState *m,struct Object *o) {
    struct BullyContact *h=history(o);if(!current(h,m)){if(h)memset(h,0,sizeof *h);return 0;}
    if(h->frame==gGlobalTimer)return 1;
    RocketSnapshot car;
    if(car_pose(m,&car)){h->observed=gGlobalTimer;h->overlapping=overlaps(&car,o,CONTACT_RELEASE_MARGIN);}
    if(h->observed!=gGlobalTimer)return 0;
    if(!h->overlapping){memset(h,0,sizeof *h);return 0;}
    return 1;
}
int rocket_bully_car_contact(struct MarioState *m,struct Object *o) {
    RocketSnapshot car;return eligible(m,o)&&rocket_adapter_body_snapshot(m->marioObj,&car)&&overlaps(&car,o,0);
}
void rocket_bully_record(struct MarioState *m,struct Object *o,int outgoing) {
    if(!rocket_bully_car_contact(m,o))return;
    struct BullyContact *h=history(o);
    if(!h)for(unsigned i=0;i<OBJECT_POOL_CAPACITY;i++)if(!contacts[i].object){h=&contacts[i];break;}
    if(!h)return;
    *h=(struct BullyContact){.object=o,.player=m->marioObj,.behavior=o->behavior,.area=m->area,
        .sync_id=o->oSyncID,.frame=gGlobalTimer,.observed=gGlobalTimer,.level=gCurrLevelNum,.outgoing=outgoing,.overlapping=1};
    RocketSnapshot car;if(!rocket_adapter_body_snapshot(m->marioObj,&car))return;
    float dx=o->oPosX-car.position[0]-car.basis[0]*ROCKET_BODY_FORWARD_OFFSET-car.basis[6]*ROCKET_BODY_UP_OFFSET;
    float dz=o->oPosZ-car.position[2]-car.basis[2]*ROCKET_BODY_FORWARD_OFFSET-car.basis[8]*ROCKET_BODY_UP_OFFSET;
    float length=hypotf(dx,dz);
    if(length<1.f){dx=car.basis[0];dz=car.basis[2];length=hypotf(dx,dz);}
    if(length>.5f){h->normal[0]=dx/length;h->normal[1]=dz/length;}
}

static void limit_closing(struct BullyContact *h,const RocketSnapshot *car,int blocked) {
    float nx=h->normal[0],nz=h->normal[1];struct Object *o=h->object;
    float target=blocked?0:30.f*o->oForwardVel*(sins(o->oMoveAngleYaw)*nx+coss(o->oMoveAngleYaw)*nz);
    float closing=car->velocity[0]*nx+car->velocity[2]*nz-fmaxf(0,target);
    if(!isfinite(closing)||closing<=0)return;
    /* Ordinary bounded collision impulse: preserve tangent/vertical motion,
     * position, rotation, fuel and suspension. No recovery/reset/teleport. */
    float impulse=fminf(2400.f,closing),delta[3]={-nx*impulse,0,-nz*impulse};
    rocket_runtime_bump(delta);
}
void rocket_bully_response(struct MarioState *m,struct Object *o) {
    struct BullyContact *h=history(o);RocketSnapshot car;
    if(current(h,m)&&h->outgoing&&owns(o)&&rocket_adapter_body_snapshot(m->marioObj,&car)&&overlaps(&car,o,0))
        limit_closing(h,&car,0);
}

void rocket_bully_separate(struct Object *o) {
    struct BullyContact *h=history(o);struct MarioState *m=&gMarioStates[0];RocketSnapshot car;
    if(!current(h,m)||!h->outgoing||!owns(o)||sCurrPlayMode==PLAY_MODE_PAUSED||(gTimeStopState&TIME_STOP_ACTIVE)||
       !rocket_adapter_body_snapshot(m->marioObj,&car)||car.basis[7]<.5f||!overlaps(&car,o,0))return;
    /* Native knockback moves a small capsule away from a much longer car.
     * Clear remaining horizontal penetration before native object_step;
     * never lift the Bully onto the chassis or override floor/death stepping. */
    float center[3];for(int k=0;k<3;k++)center[k]=car.position[k]+car.basis[k]*ROCKET_BODY_FORWARD_OFFSET+car.basis[6+k]*ROCKET_BODY_UP_OFFSET;
    if(o->oPosY-o->hitboxDownOffset>center[1])return; // Above-car falls retain native incoming rules.
    // Retain the impact side even if one fast physics step crosses the center.
    float nx=h->normal[0],nz=h->normal[1];if(hypotf(nx,nz)<.5f)return;
    const float half[3]={ROCKET_BODY_HALF_LENGTH,ROCKET_BODY_HALF_WIDTH,ROCKET_BODY_HALF_HEIGHT};
    float extent=0;for(int axis=0;axis<3;axis++)extent+=fabsf(car.basis[axis*3]*nx+car.basis[axis*3+2]*nz)*half[axis];
    float remaining=extent+o->hitboxRadius+4.f-((o->oPosX-center[0])*nx+(o->oPosZ-center[2])*nz);
    remaining=fminf(64.f,fmaxf(0.f,remaining));
    int blocked=0;
    while(remaining>.01f) {
        float step=fminf(8.f,remaining),x=o->oPosX+nx*step,y=o->oPosY,z=o->oPosZ+nz*step;
        f32_find_wall_collision(&x,&y,&z,o->hitboxHeight*.5f,o->hitboxRadius);
        struct Surface *floor=NULL;float height=find_floor(x,o->oPosY,z,&floor);
        if(!floor||!isfinite(height)||height>o->oPosY+8.f||
           (floor->normal.y<.5f&&height>o->oPosY)||!isfinite(x)||!isfinite(z)){blocked=1;break;}
        float advance=(x-o->oPosX)*nx+(z-o->oPosZ)*nz;
        if(advance<=.01f||hypotf(x-o->oPosX,z-o->oPosZ)>step+1.f){blocked=1;break;}
        o->oPosX=x;o->oPosZ=z;remaining-=step;
        if(advance<step-.1f){blocked=1;break;}
    }
    limit_closing(h,&car,blocked);
}

int rocket_bully_ram(struct MarioState *m,struct Object *o) {
    if(!m||m->playerIndex||!o||o->oInteractType!=INTERACT_BULLY||
       !(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->oIntangibleTimer||o->oSyncDeath||
       (o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
       (o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)||
       o->oAction==BULLY_ACT_LAVA_DEATH||o->oAction==BULLY_ACT_DEATH_PLANE_DEATH||
       !m->area||o->header.gfx.activeAreaIndex!=m->area->index)return 0;
    /* Use native ownership without replacing the Bully's lifecycle callbacks.
     * Remote Mario packets never invent another player's mapped throttle. */
    if(!owns(o))return 0;
    RocketSnapshot car;RocketInput input;
    if(!rocket_adapter_body_snapshot(m->marioObj,&car)||!rocket_adapter_read_input(&input)||
       !isfinite(input.throttle)||input.throttle<=.2f||!car.grounded||car.basis[7]<.5f)return 0;
    float position[3]={o->oPosX,o->oPosY-o->hitboxDownOffset,o->oPosZ};
    if(!rocket_body_overlaps_cylinder(&car,position,o->hitboxRadius,o->hitboxHeight)||
       !rocket_adapter_enemy_visible(car.position,o,0))return 0;
    float centerY=car.position[1]+car.basis[1]*ROCKET_BODY_FORWARD_OFFSET+car.basis[7]*ROCKET_BODY_UP_OFFSET;
    if(position[1]>centerY)return 0; // A Bully falling onto the roof is not a frontal ram.
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
