#include "rocket_whomp.h"
#include "sm64.h"
#include "rocket_adapter.h"
#include "../../codex/rocketleague/physics/whomp_impact.h"
#include "pc/rocket_runtime.h"
#include "pc/rocket_boost.h"
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
#include "behavior_data.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "surface_terrains.h"
#ifdef ROCKET_CAR_QA
#include <stdio.h>
#endif

static struct WhompHistory {
    u32 rule;
    struct Object *object;
    u32 sync,frame,authority;
    s16 level,area;
    RocketWhompContact contacts[MAX_PLAYERS];
    uint32_t generations[MAX_PLAYERS];
} whompHistories[OBJECT_POOL_CAPACITY];
static struct WhompHistory *whomp_history(struct Object *object) {
    uintptr_t at=(uintptr_t)object,base=(uintptr_t)gObjectPool;
    if(at<base||at-base>=sizeof(struct Object)*OBJECT_POOL_CAPACITY||(at-base)%sizeof(struct Object))return NULL;
    return &whompHistories[(at-base)/sizeof(struct Object)];
}
void rocket_whomp_forget(struct Object *object) {
    struct WhompHistory *h=whomp_history(object);if(h)memset(h,0,sizeof(*h));
}
/* Read the owned native collision box, not visual bounds or an enemy cylinder.
 * Unknown/replaced collision meshes fail closed. No ROM vertices are embedded. */
static int whomp_back(struct Object *o,RocketWhompBack *b) {
    const s16 *data=o->collisionData;
    if(!data||data[0]!=TERRAIN_LOAD_VERTICES||data[1]!=8||
       (s16)o->oFaceAnglePitch!=0x4000||(s16)o->oFaceAngleRoll!=0)return 0;
    float low[3]={32767,32767,32767},high[3]={-32768,-32768,-32768};
    for(int i=0;i<8;i++)for(int k=0;k<3;k++){
        float v=data[2+3*i+k];low[k]=fminf(low[k],v);high[k]=fmaxf(high[k],v);
    }
    unsigned corners=0;
    for(int i=0;i<8;i++){
        unsigned corner=0;
        for(int k=0;k<3;k++){
            float v=data[2+3*i+k];
            if(v==high[k])corner|=1u<<k;else if(v!=low[k])return 0;
        }
        corners|=1u<<corner;
    }
    if(corners!=255)return 0;
    for(int k=0;k<3;k++)if(!isfinite(o->header.gfx.scale[k])||o->header.gfx.scale[k]<=0||o->header.gfx.scale[k]>4)return 0;
    memset(b,0,sizeof(*b));
    b->position[0]=o->oPosX;b->position[1]=o->oPosY;b->position[2]=o->oPosZ;
    b->right[0]=coss(o->oFaceAngleYaw);b->right[1]=-sins(o->oFaceAngleYaw);
    b->forward[0]=sins(o->oFaceAngleYaw);b->forward[1]=coss(o->oFaceAngleYaw);
    for(int k=0;k<2;k++){b->low[k]=low[k]*o->header.gfx.scale[k];b->high[k]=high[k]*o->header.gfx.scale[k];}
    b->height=o->oPosY-low[2]*o->header.gfx.scale[2];
    /* Whomps are native collision surfaces. Their default -1 cylinder timer
     * does not make the loaded back intangible; test the actual surface. */
    b->eligible=o->oAction==6&&o->oSubAction==0&&o->oTimer<=100&&
        !o->oSyncDeath&&!(o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)&&
        (o->oBehParams2ndByte==0||o->oHealth>0);
    return 1;
}
int rocket_whomp_ground_pound(struct Object *object) {
    if(!object||(object->behavior!=bhvSmallWhomp&&object->behavior!=bhvWhompKingBoss))return 0;
    struct WhompHistory *h=whomp_history(object);if(!h)return 0;
    if((!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)||!gCurrentArea||!boss_net_simulates(object)||
       sCurrPlayMode!=PLAY_MODE_NORMAL||(gTimeStopState&TIME_STOP_ACTIVE)||
       !(object->activeFlags&ACTIVE_FLAG_ACTIVE)||(object->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
       object->header.gfx.activeAreaIndex!=gCurrentArea->index) {rocket_whomp_forget(object);return 0;}
    u32 authority=boss_net_epoch(object);
    if(h->object!=object||h->sync!=object->oSyncID||h->authority!=authority||h->level!=gCurrLevelNum||
       h->area!=gCurrentArea->index||(u32)(gGlobalTimer-h->frame)>1)memset(h,0,sizeof(*h));
    if(h->rule!=rocket_rule_revision())memset(&h->contacts,0,sizeof h->contacts);
    h->rule=rocket_rule_revision();
    h->object=object;h->sync=object->oSyncID;h->authority=authority;
    h->level=gCurrLevelNum;h->area=gCurrentArea->index;h->frame=gGlobalTimer;
    RocketWhompBack back;
    if(!whomp_back(object,&back)){memset(h->contacts,0,sizeof h->contacts);return 0;}
    int accepted=0;
    for(unsigned global=0;global<MAX_PLAYERS;global++){
        unsigned index=0;
        if(gCLIOpts.characterNet){
            struct NetworkPlayer *np=network_player_from_global_index(global);
            if(!np||!np->connected)continue;
            index=np->localIndex;
        }else if(global)break;
        RocketSnapshot car;uint32_t epoch=rocket_runtime_epoch(),generation=0;
        int available;
        if(!index)available=rocket_adapter_interaction_snapshot(&car);
        else{
            CharacterNetState state;available=character_net_interaction_state(index,&state,&generation);
            if(available){car=state.car;epoch=state.epoch;}
        }
        struct MarioState *m=&gMarioStates[index];
        available=available&&m->marioObj&&m->health>=0x100&&!m->freeze&&!m->heldObj&&!m->heldByObj&&!m->riddenObj&&
            (m->action==ACT_IDLE||m->action==ACT_WALKING||m->action==ACT_FREEFALL);
        RocketWhompContact *track=&h->contacts[index];
        if(h->generations[index]!=generation)memset(track,0,sizeof(*track));
        h->generations[index]=generation;
        if(!available){memset(track,0,sizeof(*track));continue;}
        float previous[3],point[3],wheels[4][3];rocket_whomp_lowest(&track->previous,previous);
        int fresh=!track->valid||epoch!=track->epoch||car.ticks!=track->previous.ticks;
        int kind=rocket_whomp_contact_at_speed(track,&car,epoch,&back,point,rocket_speed_scale());
        if(fresh&&rocket_whomp_wheels(&car,&back,wheels))kind=3;
        if(!kind||accepted)continue;
        if(kind==3){
            int supported=1;
            for(int i=0;i<4&&supported;i++){
                struct Surface *floor=NULL;struct Object *saved=gCurrentObject;gCurrentObject=m->marioObj;
                float height=find_floor(wheels[i][0],back.height+24.f,wheels[i][2],&floor);gCurrentObject=saved;
                supported=floor&&floor->object==object&&floor->normal.y>=.99f&&
                    !(floor->flags&SURFACE_FLAG_INTANGIBLE)&&fabsf(height-back.height)<=3.f&&
                    rocket_adapter_whomp_path_clear(car.position,wheels[i],object);
            }
            if(!supported)continue;
            /* Native oSubAction/health progression still consumes one hit per
             * vulnerable attack cycle, even if the car stays parked or returns. */
            accepted=1;continue;
        }
        /* Require the actual native upward-facing triangle at the chassis
         * witness. A roof, missing mesh or a different platform fails closed. */
        struct Surface *floor=NULL;struct Object *saved=gCurrentObject;gCurrentObject=m->marioObj;
        float height=find_floor(point[0],back.height+24.f,point[2],&floor);gCurrentObject=saved;
        if(!floor||floor->object!=object||floor->normal.y<.99f||
           (floor->flags&SURFACE_FLAG_INTANGIBLE)||fabsf(height-back.height)>3.f||
           !rocket_adapter_whomp_path_clear(previous,point,object))continue;
        accepted=1;
#ifdef ROCKET_CAR_QA
        fprintf(stderr,"ROCKET_WHOMP_IMPACT frame=%u sync=%u kind=%d source=%u authority=%u authority_epoch=%u source_epoch=%u source_tick=%llu health=%d action=%d sub=%d\n",
            gGlobalTimer,object->oSyncID,kind,global,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:0,
            authority,epoch,(unsigned long long)car.ticks,object->oHealth,object->oAction,object->oSubAction);
#endif
    }
    return accepted;
}
