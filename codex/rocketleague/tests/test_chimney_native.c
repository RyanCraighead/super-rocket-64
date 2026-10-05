/* Exact native collision-list dispatch, warp interaction, disappeared action,
 * and node lookup; CCM params/destination come from its actual level script.
 * Inert boundary services never create a world, perform a warp, or use sockets.
 * Chassis geometry/staging is separately exercised by the adapter fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sm64.h"
#include "level_table.h"
#include "object_fields.h"
#include "game/area.h"
#include "game/characters.h"
#include "game/interaction.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/level_update.h"
#include "game/spiderman_adapter.h"
#include "engine/math_util.h"
#include "pc/network/network.h"
#include "pc/lua/smlua_hooks.h"
#include "audio/external.h"
#include "actors/common1.h"

struct MarioState gMarioStates[MAX_PLAYERS];
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];
enum NetworkType gNetworkType;
struct Area *gCurrentArea;
const Collision warp_pipe_seg3_collision_03009AC8[]={0};
static struct Object players[2],chimney;
static struct Area area;
static struct ObjectWarpNode firstNode,chimneyNode;
static unsigned checks,actionCalls,warpCalls,sounds,rumbles,animations,allowHooks,onHooks,allHooks;
static s32 requestedWarp;
static struct ObjectWarpNode *requestedNode;
static int permitInteraction;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);abort();}}while(0)
#define CHIMNEY_UNUSED_INTERACTION(name) \
 u32 name(struct MarioState *m,u32 type,struct Object *o){(void)m;(void)type;(void)o;fprintf(stderr,"unexpected interaction: %s\n",#name);abort();}

/* Explicit boundaries. The action setter records native handler output; it
 * deliberately does not simulate the full Mario initialization/animation VM. */
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){
    actionCalls++;m->action=action;m->actionArg=arg;return TRUE;
}
s16 set_character_animation(struct MarioState *m,enum CharacterAnimID id){
    CHECK(m==&gMarioStates[0]);CHECK(id==CHAR_ANIM_A_POSE);animations++;return 0;
}
void mario_set_forward_vel(struct MarioState *m,f32 v){m->forwardVel=v;}
void *segmented_to_virtual(const void *p){return (void *)p;}
void play_sound(s32 sound,f32 *position){
    CHECK(sound==SOUND_MENU_ENTER_HOLE);CHECK(position==gMarioStates[0].marioObj->header.gfx.cameraToObject);sounds++;
}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){CHECK(m==&gMarioStates[0]&&a==12&&b==80);rumbles++;}
void network_send_object_reliability(struct Object *o,bool reliable){(void)o;(void)reliable;abort();}
void stop_shell_music(void){abort();} /* No riding object in this fixture. */
u8 is_player_active(struct MarioState *m){return m&&(m->playerIndex<2);}
int spiderman_adapter_process_interaction(struct MarioState *m,struct Object *o){(void)m;(void)o;return 0;}
u32 interact_player_pvp(struct MarioState *a,struct MarioState *b){(void)a;(void)b;abort();}
void check_kick_or_punch_wall(struct MarioState *m){CHECK(!(m->flags&(MARIO_PUNCHING|MARIO_KICKING|MARIO_TRIPPING)));}
bool smlua_call_event_hooks_HOOK_ALLOW_INTERACT(struct MarioState *m,struct Object *o,u32 type,bool *allow){
    CHECK(m&&o==&chimney&&type==INTERACT_WARP);allowHooks++;*allow=permitInteraction;return false;
}
bool smlua_call_event_hooks_HOOK_ON_INTERACT(struct MarioState *m,struct Object *o,u32 type,bool value){
    CHECK(m&&o==&chimney&&type==INTERACT_WARP);CHECK(value==(m->action==ACT_DISAPPEARED));onHooks++;return false;
}
bool smlua_call_event_hooks_HOOK_ON_INTERACTIONS(struct MarioState *m){CHECK(m);allHooks++;return false;}
s16 level_trigger_warp(struct MarioState *m,s32 operation){
    CHECK(m==&gMarioStates[0]);CHECK(m->usedObj==&chimney);warpCalls++;requestedWarp=operation;
    requestedNode=area_get_warp_node_from_params(m->usedObj);return 1;
}
#include "chimney_native.inc"

static struct MarioState *fresh(enum NetworkType role,u32 action){
    memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    memset(players,0,sizeof players);memset(&chimney,0,sizeof chimney);memset(&area,0,sizeof area);
    memset(&firstNode,0,sizeof firstNode);memset(&chimneyNode,0,sizeof chimneyNode);
    gNetworkType=role;gCurrentArea=&area;area.index=1;
    firstNode.node.id=0x0A;firstNode.next=&chimneyNode;chimneyNode.node=nativeCcmChimneyNode;area.warpNodes=&firstNode;
    chimney.oBehParams=NATIVE_CCM_CHIMNEY_PARAMS;chimney.oInteractType=INTERACT_WARP;
    for(int i=0;i<2;i++){
        struct MarioState *m=&gMarioStates[i];m->playerIndex=i;m->marioObj=&players[i];m->area=&area;
        m->action=action;m->health=0x880;m->numCoins=29;m->capTimer=135;m->flags=MARIO_CAP_ON_HEAD|MARIO_WING_CAP;
        m->pos[1]=2960;m->floorHeight=2918;players[i].header.gfx.node.flags=GRAPH_RENDER_ACTIVE;
    }
    actionCalls=warpCalls=sounds=rumbles=animations=allowHooks=onHooks=allHooks=0;
    requestedWarp=-1;requestedNode=NULL;permitInteraction=1;sJustTeleported=sDisplayingDoorText=0;
    return &gMarioStates[0];
}
static void collided(struct MarioState *m){
    m->marioObj->numCollidedObjs=1;m->marioObj->collidedObjs[0]=&chimney;
    m->marioObj->collidedObjInteractTypes=m->collidedObjInteractTypes=INTERACT_WARP;
}
static void accepted(struct MarioState *m){
    CHECK(m->action==ACT_DISAPPEARED&&m->actionArg==((WARP_OP_WARP_OBJECT<<16)+2));
    CHECK(m->usedObj==&chimney&&m->interactObj==&chimney&&chimney.oInteractStatus==INT_STATUS_INTERACTED);
    CHECK(actionCalls==1&&!warpCalls&&sounds==1&&rumbles==1);
    CHECK(m->health==0x880&&m->numCoins==29&&m->capTimer==135&&(m->flags&MARIO_WING_CAP));
    CHECK(chimney.oBehParams==0x0F1E0000);
    CHECK(area_get_warp_node_from_params(m->usedObj)==&chimneyNode);
    CHECK(chimneyNode.node.id==0x1E&&chimneyNode.node.destLevel==LEVEL_CCM&&chimneyNode.node.destArea==2&&chimneyNode.node.destNode==0x0A);
    CHECK(((NATIVE_CCM_SLIDE_SPAWN_PARAMS>>16)&0xff)==chimneyNode.node.destNode);
    CHECK(!act_disappeared(m));CHECK(!warpCalls&&(m->actionArg&0xffff)==1);
    CHECK(!(m->marioObj->header.gfx.node.flags&GRAPH_RENDER_ACTIVE));CHECK(m->pos[1]==m->floorHeight);
    CHECK(!act_disappeared(m));CHECK(warpCalls==1&&requestedWarp==WARP_OP_WARP_OBJECT&&requestedNode==&chimneyNode);
    CHECK(animations==2&&m->health==0x880&&m->numCoins==29&&m->capTimer==135);
}
int main(void){
    /* Native Mario and adapter-supported walking/freefall use the same warp
     * consumer. Network role is only a boundary context, never simulated I/O. */
    const u32 actions[]={ACT_IDLE,ACT_WALKING,ACT_FREEFALL};
    for(int role=NT_NONE;role<=NT_CLIENT;role++)for(unsigned a=0;a<sizeof actions/sizeof *actions;a++){
        struct MarioState *m=fresh(role,actions[a]);collided(m);mario_process_interactions(m);
        CHECK(allowHooks==1&&onHooks==1&&allHooks==1);accepted(m);
    }
    struct MarioState *m=fresh(NT_SERVER,ACT_IDLE);
    m->skipWarpInteractionsTimer=1;CHECK(!interact_warp(m,INTERACT_WARP,&chimney));CHECK(!actionCalls&&!warpCalls&&!m->usedObj);
    /* Actual interaction pass decrements before dispatch: 2 refuses this pass,
     * then 1 becomes zero and the ordinary native interaction can accept. */
    m->skipWarpInteractionsTimer=2;collided(m);mario_process_interactions(m);
    CHECK(m->skipWarpInteractionsTimer==1&&m->action==ACT_IDLE&&!actionCalls&&!chimney.oInteractStatus);
    collided(m);mario_process_interactions(m);CHECK(m->skipWarpInteractionsTimer==0);accepted(m);
    for(int role=NT_SERVER;role<=NT_CLIENT;role++)for(int permitRemote=0;permitRemote<2;permitRemote++){
        fresh(role,ACT_IDLE);m=&gMarioStates[1];chimney.allowRemoteInteractions=permitRemote;collided(m);mario_process_interactions(m);
        CHECK(m->action==ACT_IDLE&&!actionCalls&&!warpCalls&&!m->usedObj&&!chimney.oInteractStatus);
        CHECK(!interact_warp(m,INTERACT_WARP,&chimney));
    }
    m=fresh(NT_NONE,ACT_EMERGE_FROM_PIPE);collided(m);mario_process_interactions(m);CHECK(!actionCalls&&!warpCalls&&m->action==ACT_EMERGE_FROM_PIPE);
    m=fresh(NT_CLIENT,ACT_DISAPPEARED);collided(m);mario_process_interactions(m);CHECK(!actionCalls&&!allowHooks); // Native intangible gate.
    m=fresh(NT_SERVER,ACT_IDLE);chimney.oInteractStatus=INT_STATUS_INTERACTED;collided(m);mario_process_interactions(m);CHECK(!actionCalls&&!allowHooks);
    m=fresh(NT_SERVER,ACT_IDLE);permitInteraction=0;collided(m);mario_process_interactions(m);CHECK(!actionCalls&&allowHooks==1&&!onHooks);
    m=fresh(NT_NONE,ACT_IDLE);CHECK(!interact_warp(NULL,INTERACT_WARP,&chimney));CHECK(!interact_warp(m,INTERACT_WARP,NULL));
    CHECK(!area_get_warp_node_from_params(NULL));CHECK(!area_get_warp_node(0xff));
    /* Area 2's native slide-door exit remains node14 -> CCM area1/node14,
     * whose reciprocal door entry is distinct from chimney node1E. */
    area.index=2;chimneyNode.node=nativeCcmSlideExitNode;chimney.oBehParams=0x00140000;
    CHECK(area_get_warp_node_from_params(&chimney)==&chimneyNode);
    CHECK(chimneyNode.node.id==0x14&&chimneyNode.node.destLevel==LEVEL_CCM&&chimneyNode.node.destArea==1&&chimneyNode.node.destNode==0x14);
    CHECK(nativeCcmDoorEntryNode.id==0x14&&nativeCcmDoorEntryNode.destLevel==LEVEL_CCM&&nativeCcmDoorEntryNode.destArea==2&&nativeCcmDoorEntryNode.destNode==0x14);
    CHECK(!area_get_warp_node(0x1e));
    gCurrentArea=NULL;CHECK(!area_get_warp_node(0x1e));gCurrentArea=&area;area.warpNodes=NULL;CHECK(!area_get_warp_node(0x1e));
    printf("chimney native warp: %u checks passed (actual dispatch/handler/action/node; inert warp/network boundaries)\n",checks);
    return 0;
}
