/* Original native boss launch/landing behavior is called by the two behaviors.
 * This module only recognizes the authored crossover contact policy. */
#include "rocket_boss.h"
#include "rocket_adapter.h"
#include "../../codex/rocketleague/physics/boss_impact.h"
#include "pc/rocket_runtime.h"
#include "pc/rocket_boost.h"
#include "pc/cliopts.h"
#include "pc/character_net.h"
#include "pc/boss_net.h"
#include "pc/network/network.h"
#include "area.h"
#include "display.h"
#include "level_update.h"
#include "mario.h"
#include "object_fields.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include "hardcoded.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include <string.h>
#ifdef ROCKET_CAR_QA
#include <stdio.h>
#endif

/* One native boss per arena; a small bounded cache also handles object reloads.
 * Never use host interpolation or modify a car to manufacture a collision. */
static struct BossHistory {
    u32 rule;
    struct Object *object;
    u32 sync_id,frame;
    s16 level,area;
    u32 authority;
    RocketBossContact contacts[MAX_PLAYERS];
    unsigned generations[MAX_PLAYERS];
} histories[8];

int rocket_boss_impact_yaw(struct Object *boss,enum RocketBossKind kind,s16 *yaw) {
    if(!boss||!yaw||!gCurrentArea||(!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)||!boss_net_simulates(boss)||
       !(boss->activeFlags&ACTIVE_FLAG_ACTIVE)||boss->header.gfx.activeAreaIndex!=gCurrentArea->index)return 0;
    struct BossHistory *history=NULL,*oldest=&histories[0];
    for(unsigned i=0;i<sizeof(histories)/sizeof(histories[0]);i++) {
        if(histories[i].object==boss){history=&histories[i];break;}
        if((u32)(gGlobalTimer-histories[i].frame)>(u32)(gGlobalTimer-oldest->frame))oldest=&histories[i];
        if(!histories[i].object)oldest=&histories[i];
    }
    if(!history){history=oldest;memset(history,0,sizeof(*history));history->object=boss;}
    if(history->sync_id!=boss->oSyncID||history->level!=gCurrLevelNum||history->area!=gCurrentArea->index||
       history->authority!=boss_net_epoch(boss)||(u32)(gGlobalTimer-history->frame)>1)memset(history->contacts,0,sizeof(history->contacts));
    if(history->rule!=rocket_rule_revision())memset(&history->contacts,0,sizeof history->contacts);
    history->rule=rocket_rule_revision();
    history->authority=boss_net_epoch(boss);
    history->sync_id=boss->oSyncID;history->level=gCurrLevelNum;history->area=gCurrentArea->index;history->frame=gGlobalTimer;
    int eligible=boss->oHeldState==HELD_FREE&&boss->oHealth>0&&
        !(boss->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE);
    if(kind==ROCKET_BOSS_KING_BOBOMB)eligible=eligible&&boss->oAction==2&&boss->oIntangibleTimer==0;
    else {
        int action=boss->oAction;
        // Tail intangibility is normal for Bowser; cutscenes/recovery are not.
        eligible=eligible&&boss->oOpacity==255&&boss->oBowserUnk1AC==255&&
            (action==0||action==3||action==7||action==8||action==9||action==10||action==11||action==14||action==15);
    }
    for(int i=0;i<MAX_PLAYERS;i++)if(gMarioStates[i].heldObj==boss||gMarioStates[i].heldByObj==boss)eligible=0;
    RocketBossTarget target={{boss->oPosX,boss->oPosY,boss->oPosZ},boss->hitboxRadius,
        boss->oPosY-boss->hitboxDownOffset,boss->hitboxHeight,
        {sins(boss->oMoveAngleYaw),coss(boss->oMoveAngleYaw)},eligible,kind==ROCKET_BOSS_BOWSER};
    int accepted=0;
    /* Global-ID order is stable across peers; consume every overlap even when
     * another car wins the one permitted native launch this simulation step. */
    for(unsigned global=0;global<MAX_PLAYERS;global++) {
    unsigned index=0;
    if(gCLIOpts.characterNet) {
        struct NetworkPlayer *np=network_player_from_global_index(global);
        if(!np||!np->connected)continue;
        index=np->localIndex;
    } else if(global)break;
    RocketSnapshot car;
    u32 epoch=rocket_runtime_epoch();
    unsigned generation=0;
    int available=0;
    if(!index)available=rocket_adapter_interaction_snapshot(&car);
    else {
        CharacterNetState state;
        available=character_net_contact(index,&state,&generation);
        if(available){car=state.car;epoch=state.epoch;}
    }
    RocketBossContact *track=&history->contacts[index];
    if(history->generations[index]!=generation)memset(track,0,sizeof(*track));
    history->generations[index]=generation;
    if(!available){rocket_boss_contact(track,NULL,0,NULL);continue;}
#ifdef ROCKET_CAR_QA
    int wasArmed=track->valid&&track->armed;
    float priorX=track->previous.position[0]-track->boss_position[0];
    float priorZ=track->previous.position[2]-track->boss_position[2];
    float priorDistance=hypotf(priorX,priorZ);
    float rearCos=priorDistance>0?(priorX*target.forward[0]+priorZ*target.forward[1])/priorDistance:0;
#endif
    int contact=rocket_bumper_contact(track,&car,epoch,&target,ROCKET_BOSS_MIN_SPEED*rocket_speed_scale());
#ifdef ROCKET_CAR_QA
    if(wasArmed&&!track->armed)
        fprintf(stderr,"ROCKET_BOSS_CONTACT kind=%d frame=%u accepted=%d eligible=%d speed=%.2f action=%d health=%d rear_cos=%.4f source=%u authority=%u authority_epoch=%u source_epoch=%u source_tick=%llu\n",kind,gGlobalTimer,contact,eligible,car.velocity[0]*car.basis[0]+car.velocity[2]*car.basis[2],boss->oAction,boss->oHealth,rearCos,global,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:0,history->authority,epoch,(unsigned long long)car.ticks);
#endif
    if(!contact||accepted)continue;
    // Do not let the native out-of-bounds throw failsafe relocate the boss.
    struct Surface *floor=NULL;
    if(find_floor(boss->oPosX,boss->oPosY+160.f,boss->oPosZ,&floor)<gLevelValues.floorLowerLimitMisc||!floor)continue;
    if(!rocket_adapter_object_visible(car.position,boss))continue;
    double heading=atan2((double)car.basis[0],(double)car.basis[2]);
    if(heading<0)heading+=6.28318530718;
    *yaw=(s16)(u16)(heading*(65536.0/6.28318530718));
#ifdef ROCKET_CAR_QA
    fprintf(stderr,"ROCKET_BOSS_IMPACT kind=%d frame=%u speed=%.2f health=%d rear_cos=%.4f source=%u authority=%u authority_epoch=%u source_epoch=%u source_tick=%llu\n",kind,gGlobalTimer,hypotf(car.velocity[0],car.velocity[2]),boss->oHealth,rearCos,global,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:0,history->authority,epoch,(unsigned long long)car.ticks);
#endif
    accepted=1;
    }
    return accepted;
}
