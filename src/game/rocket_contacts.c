#include "rocket_contacts.h"
#include "rocket_enemy.h"
#include "rocket_bobomb.h"
#include "rocket_bully.h"
#include "sm64.h"
#include "area.h"
#include "display.h"
#include "interaction.h"
#include "object_list_processor.h"
#include "object_fields.h"
#include "object_constants.h"
#include "behavior_data.h"
#include "pc/cliopts.h"
#include <stdint.h>
#include <string.h>

static struct StagedBump { u32 frame,sync_id; s16 yaw; int valid; } bumps[OBJECT_POOL_CAPACITY];
static u32 preparedFrame;
static s16 preparedLevel;
static struct Area *preparedArea;
static int prepared;
static struct StagedBump *bump_slot(struct Object *object) {
    uintptr_t address=(uintptr_t)object,base=(uintptr_t)gObjectPool;
    if(address<base||address-base>=sizeof(struct Object)*OBJECT_POOL_CAPACITY||
       (address-base)%sizeof(struct Object))return NULL;
    return &bumps[(address-base)/sizeof(struct Object)];
}
void rocket_contacts_forget(struct Object *object) {
    rocket_bully_forget(object);
    struct StagedBump *bump=bump_slot(object);
    if(bump)memset(bump,0,sizeof(*bump));
}
int rocket_contacts_bobomb_yaw(struct Object *object,s16 *yaw) {
    struct StagedBump *bump=bump_slot(object);
    if(!bump||!yaw||!bump->valid)return 0;
    bump->valid=0; // A native consumer may take a staged consequence only once.
    if(bump->frame!=gGlobalTimer||bump->sync_id!=object->oSyncID||
       object->oHeldState!=HELD_FREE||object->oIntangibleTimer||
       (object->oAction!=BOBOMB_ACT_PATROL&&object->oAction!=BOBOMB_ACT_CHASE_MARIO)||
       !rocket_enemy_authority(object))return 0;
    *yaw=bump->yaw;return 1;
}
void rocket_contacts_prepare(void) {
    if(!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)return;
    if(prepared&&preparedFrame==gGlobalTimer&&preparedLevel==gCurrLevelNum&&preparedArea==gCurrentArea)return;
    prepared=1;preparedFrame=gGlobalTimer;preparedLevel=gCurrLevelNum;preparedArea=gCurrentArea;
    rocket_bully_prepare();
    memset(bumps,0,sizeof bumps);
    struct Object *saved=gCurrentObject;
    for(unsigned i=0;i<OBJECT_POOL_CAPACITY;i++) {
        struct Object *object=&gObjectPool[i];
        if(!(object->activeFlags&ACTIVE_FLAG_ACTIVE))continue;
        /* Conservatively suspend car contacts during native time stop. This
         * avoids committing an event whose native consumer is frozen. */
        if(gTimeStopState&TIME_STOP_ACTIVE) {
            rocket_enemy_interrupt(object);rocket_bobomb_forget(object);continue;
        }
        if(object->hitboxRadius<=0||object->hitboxHeight<=0)continue;
        gCurrentObject=object;
        rocket_enemy_attack(object);
        if(object->behavior==bhvBobomb&&!(object->oInteractStatus&INT_STATUS_INTERACTED)&&
           rocket_enemy_authority(object)&&rocket_bobomb_bump_yaw(object,&bumps[i].yaw)) {
            bumps[i].frame=gGlobalTimer;bumps[i].sync_id=object->oSyncID;bumps[i].valid=1;
        }
    }
    gCurrentObject=saved;
}
