/* Small Bob-ombs use native object ownership on both server and clients.
 * Only that owner resolves a bump, against accepted latest car snapshots.
 * This module never changes Mario health/immunity or writes enemy state. */
#include "rocket_bobomb.h"
#include "rocket_adapter.h"
#include "../../codex/rocketleague/physics/boss_impact.h"
#include "../../codex/rocketleague/physics/enemy_impact.h"
#include "pc/character_net.h"
#include "pc/rocket_runtime.h"
#include "pc/rocket_boost.h"
#include "pc/network/network.h"
#include "area.h"
#include "display.h"
#include "level_update.h"
#include "mario.h"
#include "object_fields.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include <string.h>

#define ROCKET_BOBOMB_MIN_SPEED (180.f * ROCKET_HOST_SCALE)
static struct BobombHistory {
    u32 rule;
    struct Object *object;
    u32 sync_id,frame;
    s16 level,area;
    RocketBossContact contact[MAX_PLAYERS];
    u16 area_sequence[MAX_PLAYERS];
    u8 global_index[MAX_PLAYERS];
    uint32_t generation[MAX_PLAYERS];
} histories[64];

void rocket_bobomb_forget(struct Object *bomb) {
    for(unsigned i=0;i<sizeof histories/sizeof *histories;i++)
        if(histories[i].object==bomb)memset(&histories[i],0,sizeof histories[i]);
}

int rocket_bobomb_bump_yaw(struct Object *bomb,s16 *yaw) {
    if(!bomb||!yaw||!gCurrentArea||(!gCLIOpts.rocketCar&&!gCLIOpts.characterNet))return 0;
    struct BobombHistory *h=NULL,*oldest=&histories[0];
    for(unsigned i=0;i<sizeof histories/sizeof *histories;i++) {
        if(histories[i].object==bomb){h=&histories[i];break;}
        if(!histories[i].object||(u32)(gGlobalTimer-histories[i].frame)>(u32)(gGlobalTimer-oldest->frame))oldest=&histories[i];
    }
    if(!h){h=oldest;memset(h,0,sizeof *h);h->object=bomb;}
    if(h->rule!=rocket_rule_revision())memset(&h->contact,0,sizeof h->contact);
    h->rule=rocket_rule_revision();
    int online=gNetworkType!=NT_NONE;
    int authority=!online||(gCLIOpts.characterNet&&gNetworkAreaLoaded&&gNetworkPlayerLocal&&
        gNetworkPlayerLocal->currAreaSyncValid&&gNetworkPlayerLocal->currLevelSyncValid&&
        sync_object_is_owned_locally(bomb->oSyncID));
    /* Combined build: rocket_enemy_attack runs before the native consumer and
     * installs rocket_enemy_authority's callbacks even for ordinary-speed cars.
     * sync_object_is_owned_locally honors those exact callbacks here. */
    if(!authority||h->sync_id!=bomb->oSyncID||h->level!=gCurrLevelNum||h->area!=gCurrentArea->index||
       (u32)(gGlobalTimer-h->frame)>1)memset(h->contact,0,sizeof h->contact);
    h->sync_id=bomb->oSyncID;h->level=gCurrLevelNum;h->area=gCurrentArea->index;h->frame=gGlobalTimer;
    if(!authority||sCurrPlayMode==PLAY_MODE_PAUSED||0) {
        memset(h->contact,0,sizeof h->contact);return 0;
    }
    int eligible=(bomb->activeFlags&ACTIVE_FLAG_ACTIVE)&&bomb->header.gfx.activeAreaIndex==gCurrentArea->index&&
        !(bomb->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)&&bomb->oHeldState==HELD_FREE&&
        !bomb->oIntangibleTimer&&(bomb->oAction==BOBOMB_ACT_PATROL||bomb->oAction==BOBOMB_ACT_CHASE_MARIO)&&
        bomb->oBobombFuseTimer<151;
    for(int i=0;i<MAX_PLAYERS;i++)if(gMarioStates[i].heldObj==bomb||gMarioStates[i].heldByObj==bomb)eligible=0;
    RocketBossTarget target={{bomb->oPosX,bomb->oPosY,bomb->oPosZ},bomb->hitboxRadius,
        bomb->oPosY-bomb->hitboxDownOffset,bomb->hitboxHeight,{0,0},eligible,0};
    int hit=0;unsigned winner=256;
    for(unsigned i=0;i<MAX_PLAYERS;i++) {
        CharacterNetState source={0};
        uint32_t generation=0;
        int available=i?online&&character_net_interaction_state(i,&source,&generation):rocket_adapter_interaction_snapshot(&source.car);
        if(!i)source.epoch=rocket_runtime_epoch();
        unsigned global=online?gNetworkPlayers[i].globalIndex:0;
        u16 area_seq=online?gNetworkPlayers[i].currLevelAreaSeqId:0;
        if(h->global_index[i]!=global||h->area_sequence[i]!=area_seq||h->generation[i]!=generation)memset(&h->contact[i],0,sizeof h->contact[i]);
        h->global_index[i]=global;h->area_sequence[i]=area_seq;h->generation[i]=generation;
        if(!available){memset(&h->contact[i],0,sizeof h->contact[i]);continue;}
        if(!rocket_bumper_contact(&h->contact[i],&source.car,source.epoch,&target,ROCKET_BOBOMB_MIN_SPEED*rocket_speed_scale()))continue;
        /* The authored ordinary bumper envelope is wider than the chassis.
         * It must not kick a fast car's target before the supersonic body reaches
         * it. Advance the entry history above even when this source is too fast. */
        if(rocket_enemy_supersonic_at_speed(&source.car,rocket_speed_scale()))continue;
        if(global>=winner||!rocket_adapter_object_visible(source.car.position,bomb))continue;
        double heading=atan2((double)source.car.basis[0],(double)source.car.basis[2]);
        if(heading<0)heading+=6.28318530718;
        *yaw=(s16)(u16)(heading*(65536.0/6.28318530718));winner=global;hit=1;
    }
    return hit;
}
