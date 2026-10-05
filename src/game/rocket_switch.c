#include "rocket_switch.h"
#include "sm64.h"
#include "rocket_adapter.h"
#include "../../codex/rocketleague/physics/switch_contact.h"
#include "pc/rocket_runtime.h"
#include "pc/network/network.h"
#include "area.h"
#include "display.h"
#include "level_update.h"
#include "mario.h"
#include "object_fields.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include "behavior_data.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "surface_terrains.h"

static struct SwitchHistory {
    RocketWhompContact contact;
    u32 sync, frame, areaSequence;
    s16 level, area;
    int networkType;
} switchHistories[OBJECT_POOL_CAPACITY];

static struct SwitchHistory *switch_history(struct Object *object) {
    uintptr_t at=(uintptr_t)object,base=(uintptr_t)gObjectPool;
    if(at<base||at-base>=sizeof(struct Object)*OBJECT_POOL_CAPACITY||(at-base)%sizeof(struct Object))return NULL;
    return &switchHistories[(at-base)/sizeof(struct Object)];
}
void rocket_switch_forget(struct Object *object) {
    struct SwitchHistory *h=switch_history(object);if(h)memset(h,0,sizeof(*h));
}

/* Derive the top from the owned collision mesh. Its actual native upward
 * triangle is checked at the impact point; no ROM vertices are embedded. */
static int switch_top(struct Object *object,RocketWhompBack *top) {
    const s16 *data=object->collisionData;
    if(!data||data[0]!=TERRAIN_LOAD_VERTICES||data[1]<3||data[1]>64||
       (s16)object->oFaceAnglePitch||(s16)object->oFaceAngleRoll)return 0;
    float low[3]={32767,32767,32767},high[3]={-32768,-32768,-32768};
    for(int i=0;i<data[1];i++)for(int k=0;k<3;k++){
        float v=data[2+3*i+k];low[k]=fminf(low[k],v);high[k]=fmaxf(high[k],v);
    }
    for(int k=0;k<3;k++)if(!isfinite(object->header.gfx.scale[k])||
        object->header.gfx.scale[k]<=0||object->header.gfx.scale[k]>4)return 0;
    memset(top,0,sizeof(*top));
    top->position[0]=object->oPosX;top->position[1]=object->oPosY;top->position[2]=object->oPosZ;
    top->right[0]=coss(object->oFaceAngleYaw);top->right[1]=-sins(object->oFaceAngleYaw);
    top->forward[0]=sins(object->oFaceAngleYaw);top->forward[1]=coss(object->oFaceAngleYaw);
    top->low[0]=low[0]*object->header.gfx.scale[0];top->high[0]=high[0]*object->header.gfx.scale[0];
    top->low[1]=low[2]*object->header.gfx.scale[2];top->high[1]=high[2]*object->header.gfx.scale[2];
    top->height=object->oPosY+high[1]*object->header.gfx.scale[1];top->eligible=1;
    return 1;
}

int rocket_switch_ground_pound(struct Object *object) {
    if(!object||object->behavior!=bhvBlueCoinSwitch)return 0;
    struct SwitchHistory *h=switch_history(object);if(!h)return 0;
    struct MarioState *m=&gMarioStates[0];
    if((!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)||!gCurrentArea||
       sCurrPlayMode!=PLAY_MODE_NORMAL||(gTimeStopState&TIME_STOP_ACTIVE)||
       !(object->activeFlags&ACTIVE_FLAG_ACTIVE)||(object->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
       object->header.gfx.activeAreaIndex!=gCurrentArea->index||object->oAction!=BLUE_COIN_SWITCH_ACT_IDLE||
       object->oSyncDeath||(object->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)||
       !m->marioObj||m->health<0x100||m->freeze||m->heldObj||m->heldByObj||m->riddenObj||
       (m->action!=ACT_IDLE&&m->action!=ACT_WALKING&&m->action!=ACT_FREEFALL)||
       (gNetworkType!=NT_NONE&&(!gNetworkPlayerLocal||!gNetworkPlayerLocal->connected||
        !gNetworkPlayerLocal->currLevelSyncValid||!gNetworkPlayerLocal->currAreaSyncValid||
        !gNetworkAreaLoaded||gNetworkAreaSyncing))) {rocket_switch_forget(object);return 0;}
    u32 areaSequence=gNetworkPlayerLocal?gNetworkPlayerLocal->currLevelAreaSeqId:0;
    if(h->sync!=object->oSyncID||h->level!=gCurrLevelNum||h->area!=gCurrentArea->index||
       h->areaSequence!=areaSequence||h->networkType!=(int)gNetworkType||(u32)(gGlobalTimer-h->frame)>1)
        memset(h,0,sizeof(*h));
    h->sync=object->oSyncID;h->level=gCurrLevelNum;h->area=gCurrentArea->index;
    h->areaSequence=areaSequence;h->networkType=gNetworkType;h->frame=gGlobalTimer;
    /* Like native Mario, only the local controller initiates this switch's
     * reliable event. Never replay remote CNET poses on multiple peers. The
     * native action/timer, late-join object sync and coin lifecycle own it. */
    RocketSnapshot car;RocketWhompBack top;
    if(!rocket_adapter_interaction_snapshot(&car)||!switch_top(object,&top)) {rocket_switch_forget(object);return 0;}
    float previous[3],point[3];rocket_whomp_lowest(&h->contact.previous,previous);
    rocket_switch_lowest(&h->contact.previous,&h->contact.back,previous);
    int kind=rocket_switch_contact(&h->contact,&car,rocket_runtime_epoch(),&top,point);
    if(!kind)return 0;
    struct Surface *floor=NULL;struct Object *saved=gCurrentObject;gCurrentObject=m->marioObj;
    float height=find_floor(point[0],top.height+24.f,point[2],&floor);gCurrentObject=saved;
    return floor&&floor->object==object&&floor->normal.y>=.99f&&
        !(floor->flags&SURFACE_FLAG_INTANGIBLE)&&fabsf(height-top.height)<=3.f&&
        rocket_adapter_whomp_path_clear(previous,point,object);
}
