/* Carry the native baby penguin, using native ownership and release states.
 * The mother still owns matching-baby dialogue, release requests and the star. */
#include "rocket_penguin.h"
#include "rocket_adapter.h"
#include "character_presentation.h"
#include "sm64.h"
#include "area.h"
#include "mario.h"
#include "interaction.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "behavior_data.h"
#include "level_update.h"
#include "pc/character_net.h"
#include "pc/network/network.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include <math.h>
#include <string.h>

static struct {
    struct MarioState *owner;
    struct Object *object,*marioObject;
    struct Area *area;
    s16 level;
    u32 syncId,frame;
    float roof[3],drop[3];
    int dropOverride,boostHeld,suppressBoost,hint,haveFrame;
} carry={.boostHeld=1};

static int penguin(const struct Object *o) {
    return o&&(o->behavior==bhvSmallPenguin||o->behavior==bhvPenguinBaby||o->behavior==bhvUnused20E0);
}
static int native_held(const struct MarioState *m) {
    return m&&m->playerIndex<MAX_PLAYERS&&penguin(m->heldObj)&&
        (m->heldObj->activeFlags&ACTIVE_FLAG_ACTIVE)&&
        m->heldObj->oHeldState==HELD_HELD&&m->heldObj->heldByPlayerIndex==m->playerIndex;
}
int rocket_penguin_carried(const struct MarioState *m) {
    if(!native_held(m))return 0;
    if(m->playerIndex)return character_net_is_car(m->playerIndex);
    return m==carry.owner&&m->marioObj==carry.marioObject&&m->heldObj==carry.object&&
        m->heldObj->oSyncID==carry.syncId&&rocket_adapter_car_selected();
}
static int same_world(const struct MarioState *m) {
    return m&&m->area==carry.area&&gCurrLevelNum==carry.level&&m->marioObj==carry.marioObject;
}
static void forget_carry(void) {
    carry.owner=NULL;carry.object=carry.marioObject=NULL;carry.area=NULL;carry.dropOverride=0;
}
static void roof_position(const RocketSnapshot *pose,float out[3]) {
    /* Behind the chassis origin, at the top of the pinned Octane body box.
     * Use the car basis on banks and jumps; no decorative duplicate is made. */
    for(int k=0;k<3;k++)out[k]=pose->position[k]-20.f*pose->basis[k]+80.f*pose->basis[6+k];
}
int rocket_penguin_drop_position(struct MarioState *m,struct Object *o,float out[3]) {
    if(!m||m->playerIndex||m!=carry.owner||o!=carry.object||o->oSyncID!=carry.syncId||
       o->heldByPlayerIndex!=0)return 0;
    /* This remains the old world's last roof position during death/warp cleanup. */
    memcpy(out,carry.dropOverride?carry.drop:carry.roof,sizeof carry.roof);
    return 1;
}
static void drop(struct MarioState *m,const RocketSnapshot *pose,int place) {
    if(!native_held(m)||m!=carry.owner||m->heldObj!=carry.object||m->heldObj->oSyncID!=carry.syncId){forget_carry();return;}
    if(pose&&same_world(m))roof_position(pose,carry.roof);
    carry.dropOverride=0;
    if(place&&pose&&same_world(m)) {
        float target[3];
        for(int k=0;k<3;k++)target[k]=pose->position[k]+205.f*pose->basis[k];
        target[1]=carry.roof[1];
        struct Surface *floor=NULL;
        float height=find_floor(target[0],target[1]+20.f,target[2],&floor);
        if(floor&&isfinite(height)&&height<=target[1]+20.f&&
           rocket_adapter_whomp_path_clear(carry.roof,target,carry.object)) {
            memcpy(carry.drop,target,sizeof target);carry.dropOverride=1;
        }
    }
    struct Object *o=m->heldObj;
    o->oInteractionSubtype&=~INT_SUBTYPE_DROP_IMMEDIATELY;
    /* The common drop path chooses our HOLP before its single network send. */
    mario_drop_held_object(m);
    forget_carry();
}
static int parked(const RocketSnapshot *pose,const RocketInput *input) {
    return pose&&input&&pose->grounded&&!pose->flipping&&pose->water_mode==ROCKET_WATER_DRY&&
        pose->basis[7]>.75f&&hypotf(pose->velocity[0],pose->velocity[2])<60.f&&
        fabsf(pose->velocity[1])<60.f&&fabsf(input->throttle)<.1f&&!input->jump;
}
static struct Object *pickup_target(struct MarioState *m,const RocketSnapshot *pose) {
    if(!gObjectLists)return NULL;
    int networked=!gCLIOpts.offline&&gNetworkType!=NT_NONE;
    /* Standalone keeps NT_SERVER as the native object-authority marker. */
    if(networked&&(!gCLIOpts.characterNet||!gNetworkAreaLoaded||gNetworkAreaSyncing||
       !gNetworkPlayerLocal||!gNetworkPlayerLocal->currLevelSyncValid||!gNetworkPlayerLocal->currAreaSyncValid))return NULL;
    struct Object *best=NULL;float nearest=1e9f;
    struct ObjectNode *head=&gObjectLists[OBJ_LIST_GENACTOR];
    for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next) {
        struct Object *o=(struct Object*)node;
        if(!penguin(o)||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->header.gfx.activeAreaIndex!=m->area->index||
           (o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||o->oSyncDeath||
           !(o->header.gfx.node.flags&GRAPH_RENDER_ACTIVE)||(o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)||
           o->oHeldState!=HELD_FREE||o->oIntangibleTimer||!(o->oFlags&OBJ_FLAG_HOLDABLE)||
           !(o->oInteractType&INTERACT_GRABBABLE)||(o->oInteractionSubtype&INT_SUBTYPE_NOT_GRABBABLE))continue;
        int held=0;for(int i=0;i<MAX_PLAYERS;i++)if(gMarioStates[i].heldObj==o)held=1;
        if(held||(networked&&(!o->oSyncID||!sync_object_is_initialized(o->oSyncID)||!sync_object_is_owned_locally(o->oSyncID))))continue;
        float dx=o->oPosX-pose->position[0],dy=o->oPosY-pose->position[1],dz=o->oPosZ-pose->position[2];
        float distance=dx*dx+dz*dz,forward=dx*pose->basis[0]+dz*pose->basis[2];
        if(!isfinite(distance)||!isfinite(dy)||fabsf(dy)>100.f||forward<30.f||distance>240.f*240.f||distance>=nearest)continue;
        if(!rocket_adapter_enemy_visible(pose->position,o,0))continue;
        best=o;nearest=distance;
    }
    return best;
}
void rocket_penguin_prepare(struct MarioState *m,const RocketSnapshot *pose,const RocketInput *input) {
    if(!m||m->playerIndex)return;
    if(carry.haveFrame&&carry.frame==gGlobalTimer)return;
    carry.haveFrame=1;carry.frame=gGlobalTimer;carry.hint=0;
    if(carry.object&&!rocket_penguin_carried(m)) {
        /* A native ownership update can replace heldBy before this player's
         * next action. Release our reference, never drop the new owner's actor. */
        if(carry.owner&&carry.owner->heldObj==carry.object&&!native_held(carry.owner))
            carry.owner->heldObj=NULL;
        drop(carry.owner,NULL,0);
    }
    if(rocket_penguin_carried(m)) {
        if(!same_world(m)||m->health<0x100||!m->action||m->action==ACT_DISAPPEARED||m->action==ACT_BUBBLED)drop(m,NULL,0);
        else {
            /* Native NPC dialogue returns to HOLD_IDLE. Resume the selected car;
             * the mother's existing release request is handled before driving. */
            if(m->action==ACT_HOLD_IDLE)set_mario_action(m,ACT_IDLE,0);
            if(m->heldObj->oInteractionSubtype&INT_SUBTYPE_DROP_IMMEDIATELY)drop(m,pose,1);
        }
    }
    if(!pose||!input||m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED) {
        carry.boostHeld=1;carry.suppressBoost=0;return;
    }
    int pressed=input->boost&&!carry.boostHeld;carry.boostHeld=!!input->boost;
    if(!input->boost)carry.suppressBoost=0;
    if(rocket_penguin_carried(m)) {
        carry.hint=parked(pose,input)?2:3;
        if(pressed&&carry.hint==2){drop(m,pose,1);carry.suppressBoost=1;carry.hint=0;}
        return;
    }
    if(m->heldObj||m->heldByObj||m->riddenObj||m->health<0x100||!parked(pose,input))return;
    struct Object *o=pickup_target(m,pose);if(!o)return;
    carry.hint=1;if(!pressed)return;
    m->usedObj=o;mario_grab_used_object(m);
    if(m->heldObj!=o||o->oHeldState!=HELD_HELD)return;
    carry.owner=m;carry.object=o;carry.marioObject=m->marioObj;carry.area=m->area;
    carry.level=gCurrLevelNum;carry.syncId=o->oSyncID;carry.suppressBoost=1;carry.hint=2;
    roof_position(pose,carry.roof);
    rocket_penguin_update(m,pose);
    if(o->oSyncID)network_send_object(o);
}
void rocket_penguin_filter_input(RocketInput *input) {
    if(input&&carry.suppressBoost&&carry.haveFrame&&carry.frame==gGlobalTimer)input->boost=0;
}
int rocket_penguin_hint(void) {return carry.haveFrame&&carry.frame==gGlobalTimer?carry.hint:0;}
void rocket_penguin_update(struct MarioState *m,const RocketSnapshot *pose) {
    if(!rocket_penguin_carried(m)||m->playerIndex||!pose)return;
    if(!same_world(m)||m->health<0x100){drop(m,NULL,0);return;}
    /* Ordinary jumps and all flip controls stay enabled. A flip or inversion
     * releases the real actor, which then uses its native falling behavior. */
    if(pose->flipping||pose->basis[7]<.5f||pose->water_mode!=ROCKET_WATER_DRY){drop(m,pose,0);return;}
    roof_position(pose,carry.roof);
    if(m->marioBodyState)memcpy(m->marioBodyState->heldObjLastPosition,carry.roof,sizeof carry.roof);
    m->heldObj->oPosX=carry.roof[0];m->heldObj->oPosY=carry.roof[1];m->heldObj->oPosZ=carry.roof[2];
}
void rocket_penguin_suspend(struct MarioState *m) {
    carry.hint=0;carry.boostHeld=1;carry.suppressBoost=0;carry.haveFrame=0;
    if(!m)m=carry.owner;
    if(!rocket_penguin_carried(m))return;
    if(same_world(m)&&m->health>=0x100&&(m->action==ACT_READING_NPC_DIALOG||m->action==ACT_WAITING_FOR_DIALOG))return;
    drop(m,NULL,0);
}
void rocket_penguin_forget(struct Object *o) {
    if(o==carry.object) {
        if(carry.owner&&carry.owner->heldObj==o)carry.owner->heldObj=NULL;
        forget_carry();
    } else if(o==carry.marioObject) {drop(carry.owner,NULL,0);}
}
void rocket_penguin_render_held(struct Object *o) {
    if(!o||o->heldByPlayerIndex>=MAX_PLAYERS)return;
    struct MarioState *m=&gMarioStates[o->heldByPlayerIndex];
    if(m->heldObj!=o||!rocket_penguin_carried(m))return;
    RocketSnapshot pose;float position[3];
    if(m->playerIndex) {
        if(!character_net_snapshot(m->playerIndex,&pose))return;
        roof_position(&pose,position);
    } else if(rocket_runtime_snapshot(&pose)||character_presentation_car_snapshot(&pose))roof_position(&pose,position);
    else if(same_world(m))memcpy(position,carry.roof,sizeof position);
    else return;
    for(int k=0;k<3;k++)if(!isfinite(position[k]))return;
    o->oPosX=position[0];o->oPosY=position[1];o->oPosZ=position[2];
    o->oFaceAnglePitch=o->oFaceAngleRoll=0;o->oMoveAngleYaw=m->faceAngle[1];
    o->header.gfx.node.flags=(o->header.gfx.node.flags|GRAPH_RENDER_ACTIVE)&~GRAPH_RENDER_INVISIBLE;
    if(m->marioBodyState)memcpy(m->marioBodyState->heldObjLastPosition,position,sizeof position);
}
