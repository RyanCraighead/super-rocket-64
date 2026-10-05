/* Non-damaging contacts are arbitrated by the lowest global ID in the area,
 * following existing coin/platform ownership. Only each recipient's own
 * simulation applies its velocity change. Reliable grants are acknowledged;
 * the arbiter waits for both acknowledgements and newer owner poses. */
#include "player_bump.h"
#include "sm64.h"
#include "character_net.h"
#include "rocket_runtime.h"
#include "network/network.h"
#include "utils/misc.h"
#include "game/rocket_adapter.h"
#include "game/rocket_caps.h"
#include "game/area.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/object_list_processor.h"
#include "engine/math_util.h"
#include "object_fields.h"
#include "../../codex/rocketleague/physics/player_bump_contact.h"
#include <string.h>

enum { BUMP_WIRE=68, BUMP_HISTORY=32 };
typedef struct BumpPose { CharacterNetState state; RocketBumpBody body; double time; int valid; } BumpPose;
typedef struct BumpEvent {
    u8 kind,authority,target,other;
    u16 authorityArea,targetArea,otherArea;
    u32 event,targetEpoch,otherEpoch,targetSequence,otherSequence;
    u8 targetKind,otherKind;
    float position[3],otherPosition[3],delta[3];
} BumpEvent;
typedef struct BumpPair {
    u32 event,sequence[2],epoch[2],frame;
    u16 area[2];u8 global[2],kind[2],acks;
    int pending;
} BumpPair;
typedef struct BumpSeen { u32 newest; uint64_t bits; } BumpSeen;
static struct BumpSession {
    BumpPose observed[MAX_PLAYERS],previous[MAX_PLAYERS],sent[BUMP_HISTORY];
    BumpPair pairs[MAX_PLAYERS][MAX_PLAYERS];
    BumpSeen seen[MAX_PLAYERS];
    u32 event,frame;int haveFrame;
} bumps;

void player_bump_clear(unsigned index){
    if(index>=MAX_PLAYERS)return;
    bumps.observed[index].valid=bumps.previous[index].valid=0;
    memset(bumps.pairs,0,sizeof bumps.pairs);
    memset(bumps.seen,0,sizeof bumps.seen);
    /* Retire every pending grant's local reference on disconnect/respawn too.
     * A late authority event cannot reuse history after any slot is recycled. */
    memset(bumps.sent,0,sizeof bumps.sent);
    if(!index)bumps.haveFrame=0;
}
void player_bump_observe(unsigned index,const CharacterNetState *state,const float position[3],const float velocity[3]){
    if(index>=MAX_PLAYERS||!state||!position||!velocity)return;
    BumpPose pose={0};pose.state=*state;RocketSnapshot car=state->car;
    if(state->kind==CNET_MARIO){
        memset(&car,0,sizeof car);car.basis[0]=car.basis[4]=car.basis[8]=1;
        for(int k=0;k<3;k++){car.position[k]=position[k];car.velocity[k]=velocity[k]*30.f;}
    }else if(state->kind!=CNET_OCTANE||state->active!=CNET_DRIVING){bumps.observed[index].valid=0;return;}
    if(!rocket_bump_body(&pose.body,&car,state->kind==CNET_OCTANE)){bumps.observed[index].valid=0;return;}
    pose.time=clock_elapsed_f64();if(!isfinite(pose.time))return;pose.valid=1;
    bumps.previous[index]=bumps.observed[index];bumps.observed[index]=pose;
    if(!index)bumps.sent[state->sequence%BUMP_HISTORY]=pose;
}
static int bump_same_area(const struct NetworkPlayer *a,const struct NetworkPlayer *b){
    return a&&b&&a->connected&&b->connected&&a->currLevelSyncValid&&a->currAreaSyncValid&&
        b->currLevelSyncValid&&b->currAreaSyncValid&&a->currCourseNum==b->currCourseNum&&
        a->currActNum==b->currActNum&&a->currLevelNum==b->currLevelNum&&a->currAreaIndex==b->currAreaIndex;
}
static struct NetworkPlayer *bump_authority(const struct NetworkPlayer *member){
    struct NetworkPlayer *owner=NULL;
    for(unsigned i=0;i<MAX_PLAYERS;i++)if(bump_same_area(member,&gNetworkPlayers[i])&&
        (!owner||gNetworkPlayers[i].globalIndex<owner->globalIndex))owner=&gNetworkPlayers[i];
    return owner;
}
static int bump_enabled(void){
    return gCLIOpts.characterNet&&!gCLIOpts.offline&&gNetworkType!=NT_NONE&&gNetworkAreaLoaded&&!gNetworkAreaSyncing&&
        gCurrentArea&&gCurrentArea->localAreaTimer>=60&&gServerSettings.playerInteractions!=PLAYER_INTERACTIONS_NONE&&
        sCurrPlayMode==PLAY_MODE_NORMAL&&!gWarpTransition.isActive&&sWarpDest.type==WARP_TYPE_NOT_WARPING&&
        sDelayedWarpOp==WARP_OP_NONE&&!(gTimeStopState&TIME_STOP_ACTIVE);
}
static int bump_eligible(unsigned i){
    if(i>=MAX_PLAYERS||!bump_same_area(gNetworkPlayerLocal,&gNetworkPlayers[i]))return 0;
    const struct MarioState *m=&gMarioStates[i];
    return m->marioObj&&m->area==gCurrentArea&&m->health>=0x100&&!m->freeze&&!m->hurtCounter&&!m->healCounter&&
        !m->squishTimer&&!m->quicksandDepth&&!m->heldObj&&!m->heldByObj&&!m->riddenObj&&!m->skipWarpInteractionsTimer&&
        !(m->action&(ACT_FLAG_INTANGIBLE|ACT_FLAG_INVULNERABLE))&&
        (m->action&ACT_GROUP_MASK)!=ACT_GROUP_CUTSCENE&&(m->action&ACT_GROUP_MASK)!=ACT_GROUP_AUTOMATIC&&
        !(rocket_caps_active_flags(i)&MARIO_VANISH_CAP);
}
static int bump_pose(unsigned i,BumpPose *pose){
    if(!bump_eligible(i))return 0;
    if(!i){
        *pose=bumps.observed[0];RocketSnapshot car={0};
        pose->state.kind=character_net_local_kind();pose->state.epoch=rocket_runtime_epoch();
        pose->state.area_sequence=gNetworkPlayerLocal->currLevelAreaSeqId;
        if(pose->state.kind==CNET_OCTANE){if(!rocket_adapter_body_snapshot(gMarioStates[0].marioObj,&car))return 0;}
        else{car.basis[0]=car.basis[4]=car.basis[8]=1;for(int k=0;k<3;k++){car.position[k]=gMarioStates[0].pos[k];car.velocity[k]=gMarioStates[0].vel[k]*30.f;}}
        pose->time=clock_elapsed_f64();pose->valid=rocket_bump_body(&pose->body,&car,pose->state.kind==CNET_OCTANE);
        return pose->valid;
    }
    const BumpPose *source=&bumps.observed[i];double age=clock_elapsed_f64()-source->time;
    if(!source->valid||!isfinite(age)||age<0||age>.2||!gNetworkPlayers[i].currPositionValid||
        source->state.area_sequence!=gNetworkPlayers[i].currLevelAreaSeqId)return 0;
    *pose=*source;return 1;
}
int player_bump_car_pair(const struct MarioState *a,const struct MarioState *b){
    if(!gCLIOpts.characterNet||!a||!b)return 0;
    return (a->playerIndex?character_net_is_car(a->playerIndex):rocket_adapter_car_selected())||
        (b->playerIndex?character_net_is_car(b->playerIndex):rocket_adapter_car_selected());
}
static void bump_write(struct Packet *p,const BumpEvent *e){
#define FIELD(f) packet_write(p,(void*)&e->f,sizeof e->f)
    FIELD(kind);FIELD(authority);FIELD(target);FIELD(other);FIELD(authorityArea);FIELD(targetArea);FIELD(otherArea);
    FIELD(event);FIELD(targetEpoch);FIELD(otherEpoch);FIELD(targetSequence);FIELD(otherSequence);FIELD(targetKind);FIELD(otherKind);
    FIELD(position);FIELD(otherPosition);FIELD(delta);
#undef FIELD
}
static int bump_read(struct Packet *p,BumpEvent *e){
    if(p->error||p->cursor+BUMP_WIRE!=p->dataLength)return 0;
#define FIELD(f) packet_read(p,&e->f,sizeof e->f)
    FIELD(kind);FIELD(authority);FIELD(target);FIELD(other);FIELD(authorityArea);FIELD(targetArea);FIELD(otherArea);
    FIELD(event);FIELD(targetEpoch);FIELD(otherEpoch);FIELD(targetSequence);FIELD(otherSequence);FIELD(targetKind);FIELD(otherKind);
    FIELD(position);FIELD(otherPosition);FIELD(delta);
#undef FIELD
    if(p->error||e->kind>1||!e->event||e->target==e->other||
       (e->targetKind!=CNET_MARIO&&e->targetKind!=CNET_OCTANE)||
       (e->otherKind!=CNET_MARIO&&e->otherKind!=CNET_OCTANE)||(!e->targetKind&&!e->otherKind))return 0;
    float magnitude=0;for(int k=0;k<3;k++){
        if(!isfinite(e->position[k])||!isfinite(e->otherPosition[k])||!isfinite(e->delta[k])||
           fabsf(e->position[k])>131072||fabsf(e->otherPosition[k])>131072)return 0;
        magnitude+=e->delta[k]*e->delta[k];
    }
    return magnitude>=1&&magnitude<=2400.1f*2400.1f;
}
bool player_bump_packet_allowed(struct Packet *p){
    if(!gCLIOpts.characterNet||!p||p->localIndex==0||p->localIndex>=MAX_PLAYERS||!gNetworkPlayers[p->localIndex].connected)return false;
    BumpEvent e={0};u16 cursor=p->cursor;int valid=bump_read(p,&e);p->cursor=cursor;
    if(!valid||!p->levelMustMatch||p->levelAreaMustMatch||p->requestBroadcast||
       p->destGlobalId!=(e.kind?e.authority:e.target))return false;
    u8 source=e.kind?e.target:e.authority;
    if(gNetworkPlayers[p->localIndex].globalIndex!=source&&!(gNetworkType==NT_CLIENT&&gNetworkPlayerServer&&
        p->localIndex==gNetworkPlayerServer->localIndex))return false;
    struct NetworkPlayer *a=network_player_from_global_index(e.authority),*t=network_player_from_global_index(e.target),*o=network_player_from_global_index(e.other);
    return bump_same_area(a,t)&&bump_same_area(a,o)&&bump_authority(t)==a&&
        a->currLevelAreaSeqId==e.authorityArea&&t->currLevelAreaSeqId==e.targetArea&&o->currLevelAreaSeqId==e.otherArea&&
        p->courseNum==a->currCourseNum&&p->actNum==a->currActNum&&p->levelNum==a->currLevelNum;
}
static int bump_new_event(BumpSeen *seen,u32 event){
    if(!seen->newest){seen->newest=event;seen->bits=1;return 1;}
    int32_t d=(int32_t)(event-seen->newest);
    if(d>0){seen->bits=d>=64?1:(seen->bits<<d)|1;seen->newest=event;return 1;}
    u32 back=seen->newest-event;if(back>=64||(seen->bits&(UINT64_C(1)<<back)))return 0;
    seen->bits|=UINT64_C(1)<<back;return 1;
}
static void bump_send(const BumpEvent *e,struct NetworkPlayer *to){
    struct Packet p={0};packet_init(&p,PACKET_ROCKET_PLAYER_BUMP,true,PLMT_LEVEL);bump_write(&p,e);network_send_to(to->localIndex,&p);
}
static int bump_apply(const BumpEvent *e,int localAuthority){
    if(!bump_enabled()||!bump_eligible(0)||!gNetworkPlayerLocal||gNetworkPlayerLocal->globalIndex!=e->target||
       gNetworkPlayerLocal->currLevelAreaSeqId!=e->targetArea||rocket_runtime_epoch()!=e->targetEpoch||
       character_net_local_kind()!=e->targetKind)return 0;
    BumpPose current;if(!bump_pose(0,&current))return 0;
    if(!localAuthority){
        BumpPose *sent=&bumps.sent[e->targetSequence%BUMP_HISTORY];double age=clock_elapsed_f64()-sent->time;
        if(!sent->valid||sent->state.sequence!=e->targetSequence||sent->state.epoch!=e->targetEpoch||
           sent->state.kind!=e->targetKind||sent->state.area_sequence!=e->targetArea||age<0||age>.4)return 0;
        float distance=0,travel=0;for(int k=0;k<3;k++){
            float d=sent->body.car.position[k]-e->position[k];distance+=d*d;
            d=current.body.car.position[k]-e->position[k];travel+=d*d;
        }
        if(distance>4.f||travel>1000.f*1000.f)return 0;
    }
    if(e->targetKind==CNET_OCTANE)return rocket_runtime_bump(e->delta);
    struct MarioState *m=&gMarioStates[0];float vx=m->vel[0]+e->delta[0]/30.f,vz=m->vel[2]+e->delta[2]/30.f;
    float vy=m->vel[1]+e->delta[1]/30.f;
    float speed=fminf(80.f,hypotf(vx,vz));
    if(speed>1.f){
        m->faceAngle[1]=atan2s(-vz,-vx);
        set_mario_action(m,m->pos[1]<=m->floorHeight+4?ACT_SOFT_BACKWARD_GROUND_KB:ACT_BACKWARD_AIR_KB,0);
        mario_set_forward_vel(m,-speed);
    }
    m->vel[1]=fmaxf(-80.f,fminf(80.f,vy));
    return 1;
}
static void bump_ack(const BumpEvent *e){
    for(unsigned i=0;i<MAX_PLAYERS;i++)for(unsigned j=i+1;j<MAX_PLAYERS;j++){
        BumpPair *pair=&bumps.pairs[i][j];if(!pair->pending||pair->event!=e->event)continue;
        for(int k=0;k<2;k++)if(pair->global[k]==e->target&&pair->sequence[k]==e->targetSequence&&pair->epoch[k]==e->targetEpoch)
            pair->acks|=1u<<k;
    }
}
void player_bump_receive(struct Packet *p){
    if(!player_bump_packet_allowed(p)||!bump_enabled())return;
    BumpEvent e={0};if(!bump_read(p,&e))return;
    struct NetworkPlayer *authority=network_player_from_global_index(e.authority);
    if(e.kind){if(authority==gNetworkPlayerLocal)bump_ack(&e);return;}
    if(!gNetworkPlayerLocal||e.target!=gNetworkPlayerLocal->globalIndex)return;
    struct NetworkPlayer *other=network_player_from_global_index(e.other);BumpPose otherPose;
    if(other&&bump_pose(other->localIndex,&otherPose)&&otherPose.state.epoch==e.otherEpoch&&otherPose.state.kind==e.otherKind&&
       bump_new_event(&bumps.seen[authority->localIndex],e.event))bump_apply(&e,0);
    e.kind=1;bump_send(&e,authority);
}
static int bump_identity(const BumpPair *pair,const BumpPose *a,const BumpPose *b,unsigned i,unsigned j){
    const BumpPose *poses[]={a,b};unsigned ids[]={i,j};
    for(int k=0;k<2;k++)if(pair->global[k]!=gNetworkPlayers[ids[k]].globalIndex||pair->epoch[k]!=poses[k]->state.epoch||
        pair->area[k]!=poses[k]->state.area_sequence||pair->kind[k]!=poses[k]->state.kind)return 0;
    return 1;
}
static int bump_contact(unsigned i,unsigned j,const BumpPose *a,const BumpPose *b,float delta[3]){
    /* Bounded translation sweep between actual accepted owner samples. Large
     * rotations, reset epochs and stale intervals never create swept hits. */
    const BumpPose *old[]={&bumps.previous[i],&bumps.previous[j]},*now[]={a,b};
    for(int k=0;k<2;k++){
        double dt=now[k]->time-old[k]->time;
        if(!old[k]->valid||dt<0||dt>.2||old[k]->state.epoch!=now[k]->state.epoch||old[k]->state.kind!=now[k]->state.kind||
            old[k]->state.area_sequence!=now[k]->state.area_sequence)goto current;
        for(int axis=0;axis<3;axis++)if(rocket_bump_dot(old[k]->body.axes+3*axis,now[k]->body.axes+3*axis)<.98f)goto current;
        float travel=0;for(int axis=0;axis<3;axis++){float d=now[k]->body.center[axis]-old[k]->body.center[axis];travel+=d*d;}
        if(travel>1000.f*1000.f)goto current;
    }
    for(int step=1;step<32;step++){
        RocketBumpBody bodies[]={a->body,b->body};float t=(float)step/32;
        for(int k=0;k<2;k++)for(int axis=0;axis<3;axis++){
            float d=(old[k]->body.car.position[axis]-now[k]->body.car.position[axis])*(1-t);
            bodies[k].car.position[axis]+=d;bodies[k].center[axis]+=d;
        }
        if(rocket_bump_impulse(&bodies[0],&bodies[1],delta))return 1;
    }
current:
    return rocket_bump_impulse(&a->body,&b->body,delta);
}
void player_bump_update(void){
    if(bumps.haveFrame&&bumps.frame==gGlobalTimer)return;
    bumps.frame=gGlobalTimer;bumps.haveFrame=1;
    if(!bump_enabled()||bump_authority(gNetworkPlayerLocal)!=gNetworkPlayerLocal){memset(bumps.pairs,0,sizeof bumps.pairs);return;}
    BumpPose poses[MAX_PLAYERS];int valid[MAX_PLAYERS];
    for(unsigned i=0;i<MAX_PLAYERS;i++)valid[i]=bump_pose(i,&poses[i]);
    for(unsigned i=0;i<MAX_PLAYERS;i++)for(unsigned j=i+1;j<MAX_PLAYERS;j++){
        BumpPair *pair=&bumps.pairs[i][j];BumpPose *a=&poses[i],*b=&poses[j];
        if(!valid[i]||!valid[j]||(!a->body.isCar&&!b->body.isCar)){memset(pair,0,sizeof *pair);continue;}
        if(pair->pending&&!bump_identity(pair,a,b,i,j))memset(pair,0,sizeof *pair);
        if(pair->pending){
            float normal[3],depth;
            if(gGlobalTimer-pair->frame>30&&!rocket_bump_overlap(&a->body,&b->body,normal,&depth))memset(pair,0,sizeof *pair);
        }
        if(pair->pending){
            if(pair->acks!=3||gGlobalTimer-pair->frame<4||
                (i&&a->state.sequence==pair->sequence[0])||b->state.sequence==pair->sequence[1])continue;
            pair->pending=0;
        }
        float delta[3];if(!bump_contact(i,j,a,b,delta))continue;
        if(!rocket_adapter_whomp_path_clear(a->body.center,b->body.center,NULL))continue;
        if(++bumps.event==0)++bumps.event;
        const BumpPose *source[]={a,b};unsigned ids[]={i,j};
        memset(pair,0,sizeof *pair);pair->event=bumps.event;pair->frame=gGlobalTimer;pair->pending=1;
        for(int k=0;k<2;k++){
            pair->global[k]=gNetworkPlayers[ids[k]].globalIndex;pair->epoch[k]=source[k]->state.epoch;
            pair->sequence[k]=source[k]->state.sequence;pair->area[k]=source[k]->state.area_sequence;pair->kind[k]=source[k]->state.kind;
        }
        for(int k=0;k<2;k++){
            BumpEvent e={0};e.authority=gNetworkPlayerLocal->globalIndex;e.authorityArea=gNetworkPlayerLocal->currLevelAreaSeqId;
            e.target=pair->global[k];e.other=pair->global[1-k];e.targetArea=pair->area[k];e.otherArea=pair->area[1-k];
            e.event=pair->event;e.targetEpoch=pair->epoch[k];e.otherEpoch=pair->epoch[1-k];
            e.targetSequence=pair->sequence[k];e.otherSequence=pair->sequence[1-k];e.targetKind=pair->kind[k];e.otherKind=pair->kind[1-k];
            for(int axis=0;axis<3;axis++){e.position[axis]=source[k]->body.car.position[axis];e.otherPosition[axis]=source[1-k]->body.car.position[axis];e.delta[axis]=delta[axis]*(k?1:-1);}
            if(!ids[k]){bump_apply(&e,1);bump_ack(&e);}else bump_send(&e,&gNetworkPlayers[ids[k]]);
        }
    }
    /* Current local pose is the sweep baseline for the following logic frame. */
    if(valid[0])bumps.previous[0]=poses[0];
}
