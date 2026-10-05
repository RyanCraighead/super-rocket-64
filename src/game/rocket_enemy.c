#include "rocket_enemy.h"
#include "sm64.h"
#include "rocket_adapter.h"
#include "rocket_caps.h"
#include "../../codex/rocketleague/physics/enemy_impact.h"
#include "pc/rocket_runtime.h"
#include "pc/character_net.h"
#include "pc/network/network.h"
#include "area.h"
#include "display.h"
#include "level_update.h"
#include "mario.h"
#include "interaction.h"
#include "object_fields.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include "behavior_data.h"
#ifdef ROCKET_CAR_QA
#include <stdio.h>
#include <stdlib.h>
#endif

/* Bounded per-enemy/per-peer history, never stored in saves or object rawData. */
static struct EnemyHistory {
    struct Object *object;
    const BehaviorScript *behavior;
    u32 sync_id,frame;
    s16 level,area;
    int committed;
    uint32_t generation[MAX_PLAYERS];
    RocketEnemyContact car[MAX_PLAYERS];
} histories[OBJECT_POOL_CAPACITY];

static struct EnemyHistory *enemy_history(struct Object *enemy) {
    uintptr_t address=(uintptr_t)enemy,base=(uintptr_t)gObjectPool;
    if(address<base||address-base>=sizeof(struct Object)*OBJECT_POOL_CAPACITY||
       (address-base)%sizeof(struct Object)) return NULL;
    return &histories[(address-base)/sizeof(struct Object)];
}
void rocket_enemy_forget(struct Object *enemy) {
    struct EnemyHistory *history=enemy_history(enemy);
    if(history&&history->object==enemy) memset(history,0,sizeof(*history));
}
void rocket_enemy_interrupt(struct Object *enemy) {
    struct EnemyHistory *history=enemy_history(enemy);
    if(history&&history->object==enemy)memset(history->car,0,sizeof history->car);
}

static int supported_enemy(const struct Object *enemy) {
    if(enemy->behavior==bhvGoomba)
        return enemy->oGoombaSize==GOOMBA_SIZE_REGULAR||enemy->oGoombaSize==GOOMBA_SIZE_TINY;
    return enemy->behavior==bhvBobomb || enemy->behavior==bhvSpindrift ||
        enemy->behavior==bhvScuttlebug || enemy->behavior==bhvSkeeter ||
        enemy->behavior==bhvSnufit || enemy->behavior==bhvFlyGuy;
}
static int terminal(const struct Object *enemy) {
    if(enemy->behavior==bhvBobomb) return enemy->oAction>=BOBOMB_ACT_EXPLODE;
    return enemy->oAction>=OBJ_ACT_HORIZONTAL_KNOCKBACK;
}
/* Stable native object authority avoids two cars/nearest-player handoffs
 * resolving the same impact. Native held-object ownership is preserved. */
static void enemy_ownership(u8 *override,u8 *own) {
    *override=gCLIOpts.characterNet&&gCurrentObject&&gCurrentObject->oHeldState==HELD_FREE;
    *own=0;
    if(*override&&gNetworkAreaLoaded&&gNetworkPlayerLocal&&gNetworkPlayerLocal->currAreaSyncValid)
        *own=get_network_player_smallest_global()==gNetworkPlayerLocal;
}
static u8 enemy_ignore_updates(void) {
    /* A late ordinary pose must not resurrect a native death already underway. */
    return gCLIOpts.characterNet&&gCurrentObject&&terminal(gCurrentObject);
}
int rocket_enemy_authority(struct Object *enemy) {
    if(!enemy||!supported_enemy(enemy)) return 0;
    if(gNetworkType==NT_NONE) return 1;
    if(!gCLIOpts.characterNet||!gNetworkAreaLoaded||gNetworkAreaSyncing||
       !gNetworkPlayerLocal||!gNetworkPlayerLocal->currAreaSyncValid) return 0;
    struct SyncObject *so=sync_object_get(enemy->oSyncID);
    if(!so||so->o!=enemy||!sync_object_is_initialized(enemy->oSyncID)) return 0;
    /* Never replace another feature's/native behavior's callbacks silently. */
    if(so->override_ownership&&so->override_ownership!=enemy_ownership) return 0;
    if(so->ignore_if_true&&so->ignore_if_true!=enemy_ignore_updates) return 0;
    so->override_ownership=enemy_ownership;so->ignore_if_true=enemy_ignore_updates;
    return sync_object_should_own(enemy->oSyncID);
}
static int source(unsigned index,CharacterNetState *state,uint32_t *generation) {
    memset(state,0,sizeof(*state));
    *generation=0;
    if(index==0) {
        /* Selected adapter ownership also covers Mario-first online switching. */
        if(!rocket_adapter_interaction_snapshot(&state->car)) return 0;
        state->epoch=rocket_runtime_epoch();return 1;
    }
    if(!character_net_interaction_state(index,state,generation)) return 0;
    const struct MarioState *m=&gMarioStates[index];
    return m->marioObj&&m->health>=0x100&&!m->heldObj&&!m->heldByObj&&!m->riddenObj&&!m->freeze&&
        (m->action==ACT_IDLE||m->action==ACT_WALKING||m->action==ACT_FREEFALL);
}
#ifdef ROCKET_CAR_QA
static void enemy_qa_decision(struct Object *enemy,int owner,int eligible,unsigned index,
        const CharacterNetState *state,const RocketEnemyContact *before,int hit,int previousVisible,int currentVisible) {
    const char *scenario=getenv("SM64_ROCKET_QA_COMBINED");
    if(!gCLIOpts.loopbackOnly||!scenario||strcmp(scenario,"supersonic")||enemy->behavior!=bhvGoomba)return;
    const RocketSnapshot *car=state?&state->car:before->valid?&before->previous:NULL;
    if(!car)return;
    float speed2=rocket_enemy_dot(car->velocity,car->velocity),distance2=0;
    const float position[3]={enemy->oPosX,enemy->oPosY,enemy->oPosZ};
    for(int k=0;k<3;k++){float d=car->position[k]-position[k];distance2+=d*d;}
    if(speed2<4000.f*4000.f||distance2>1400.f*1400.f)return;
    float dots[3]={0};
    if(before->valid&&state)for(int k=0;k<3;k++)dots[k]=rocket_enemy_dot(before->previous.basis+3*k,car->basis+3*k);
    fprintf(stderr,"ROCKET_ENEMY_QA {\"frame\":%u,\"sync\":%u,\"index\":%u,\"owner\":%d,\"eligible\":%d,\"source\":%d,\"history\":%d,\"epoch\":%u,\"previous_epoch\":%u,\"ticks\":%llu,\"delta\":%llu,\"speed\":%.7g,\"basis_dot\":[%.7g,%.7g,%.7g],\"hit\":%d,\"previous_visible\":%d,\"current_visible\":%d,\"native_action\":%u,\"target_action\":%d,\"target_status\":%u,\"target_radius\":%.7g,\"target_height\":%.7g,\"pos\":[%.7g,%.7g,%.7g],\"target\":[%.7g,%.7g,%.7g]}\n",
        gGlobalTimer,enemy->oSyncID,index,owner,eligible,state!=NULL,before->valid,state?state->epoch:0,before->epoch,
        (unsigned long long)car->ticks,(unsigned long long)(state&&before->valid?car->ticks-before->previous.ticks:0),
        sqrtf(speed2),dots[0],dots[1],dots[2],hit,previousVisible,currentVisible,
        gMarioStates[index].action,enemy->oAction,enemy->oInteractStatus,enemy->hitboxRadius,enemy->hitboxHeight,
        car->position[0],car->position[1],car->position[2],position[0],position[1],position[2]);
}
#endif
int rocket_enemy_attack(struct Object *enemy) {
    if(!enemy||(!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)||!gCurrentArea||!supported_enemy(enemy)) return 0;
    int owner=rocket_enemy_authority(enemy);
    struct EnemyHistory *history=enemy_history(enemy);
    if(!history) return 0;
    if(history->object!=enemy||history->behavior!=enemy->behavior||history->sync_id!=enemy->oSyncID||
       history->level!=gCurrLevelNum||history->area!=gCurrentArea->index)
        memset(history,0,sizeof(*history));
    else if((u32)(gGlobalTimer-history->frame)>1)
        memset(history->car,0,sizeof history->car); // Observation gaps do not reset a lifetime's committed hit.
    history->object=enemy;history->behavior=enemy->behavior;history->sync_id=enemy->oSyncID;
    history->level=gCurrLevelNum;history->area=gCurrentArea->index;history->frame=gGlobalTimer;
    int eligible=owner&&!history->committed&&sCurrPlayMode!=PLAY_MODE_PAUSED&&
        (enemy->activeFlags&ACTIVE_FLAG_ACTIVE)&&!(enemy->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))&&
        enemy->header.gfx.activeAreaIndex==gCurrentArea->index&&
        !(enemy->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)&&enemy->oIntangibleTimer==0&&
        enemy->oHeldState==HELD_FREE&&!enemy->oSyncDeath&&!terminal(enemy)&&
        !(enemy->oInteractStatus&INT_STATUS_INTERACTED);
    for(int i=0;i<MAX_PLAYERS;i++) if(gMarioStates[i].heldObj==enemy||gMarioStates[i].heldByObj==enemy) eligible=0;
    if(!eligible){
#ifdef ROCKET_CAR_QA
        for(unsigned i=0;i<MAX_PLAYERS;i++){
            CharacterNetState state;uint32_t generation;
            int valid=source(i,&state,&generation);
            enemy_qa_decision(enemy,owner,0,i,valid?&state:NULL,&history->car[i],-1,-1,-1);
        }
#endif
        memset(history->car,0,sizeof(history->car));return 0;
    }
    RocketEnemyTarget target={{enemy->oPosX,enemy->oPosY,enemy->oPosZ},enemy->hitboxRadius,
        enemy->oPosY-enemy->hitboxDownOffset,enemy->hitboxHeight};
    for(unsigned i=0;i<MAX_PLAYERS;i++) {
        CharacterNetState state;
        uint32_t generation;
        if(!source(i,&state,&generation)){
#ifdef ROCKET_CAR_QA
            enemy_qa_decision(enemy,owner,eligible,i,NULL,&history->car[i],-1,-1,-1);
#endif
            memset(&history->car[i],0,sizeof(history->car[i]));continue;
        }
        if(history->generation[i]!=generation)memset(&history->car[i],0,sizeof(history->car[i]));
        history->generation[i]=generation;
#ifdef ROCKET_CAR_QA
        RocketEnemyContact before=history->car[i];
#endif
        float previous[3];memcpy(previous,history->car[i].previous.position,sizeof(previous));
        int hit=rocket_enemy_contact(&history->car[i],&state.car,state.epoch,&target);
        if(!hit){
#ifdef ROCKET_CAR_QA
            enemy_qa_decision(enemy,owner,eligible,i,&state,&before,0,-1,-1);
#endif
            continue;
        }
        unsigned caps=rocket_caps_active_flags(i);
        int previousVisible=rocket_adapter_enemy_visible(previous,enemy,caps);
        int currentVisible=rocket_adapter_enemy_visible(state.car.position,enemy,caps);
#ifdef ROCKET_CAR_QA
        enemy_qa_decision(enemy,owner,eligible,i,&state,&before,hit,previousVisible,currentVisible);
#endif
        if(!previousVisible||!currentVisible)continue;
        history->committed=1;
        enemy->oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|
            (enemy->behavior==bhvBobomb?INT_STATUS_TOUCHED_BOB_OMB:ATTACK_FAST_ATTACK);
        /* Send the native interaction before its consumer clears it. Reliable
         * delivery and native unload/death replication retain their normal IDs,
         * loot counters and respawn rules. Never mint coins in this adapter. */
        if(gNetworkType!=NT_NONE) network_send_object_reliability(enemy,TRUE);
        return 1;
    }
    return 0;
}
