#include "speed_fixture_stubs.h"
/* Real adapter and native Bob-omb behavior; inert host services. No sockets/input. */
#include <assert.h>
#include <stdio.h>
#include "../../../src/game/rocket_bobomb.c"
#include "sm64.h"
#include "behavior_data.h"
#include "game/behavior_actions.h"
#include "game/object_helpers.h"
#include "game/obj_behaviors.h"
#include "game/interaction.h"
#include "game/save_file.h"
#include "game/spawn_object.h"
#include "game/spawn_sound.h"
#include "dialog_ids.h"
#include "game/camera.h"
#include "game/mario_actions_cutscene.h"
#include "game/hardcoded.h"
#include "audio/external.h"
#include "engine/math_util.h"
#define o gCurrentObject
#define OBJ_COL_FLAG_GROUNDED 1
static struct Surface *sObjFloor;
/* Services normally defined earlier in obj_behaviors.c. */
void obj_spawn_yellow_coins(struct Object *,s8);
s16 object_step(void);
s8 obj_return_home_if_safe(struct Object *,f32,f32,f32,s32);
s8 obj_check_if_facing_toward_angle(u32,u32,s16);
void obj_check_floor_death(s16,struct Surface *);
s8 obj_lava_death(void);
s8 is_point_within_radius_of_mario(f32,f32,f32,s32);
void set_object_visibility(struct Object *,s32);
#include "../../../src/game/behaviors/bobomb.inc.c"
struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal;
struct MarioState gMarioStates[MAX_PLAYERS];
struct Area *gCurrentArea;
struct Object *gCurrentObject;
s16 gCurrLevelNum,sCurrPlayMode;
u32 gGlobalTimer;
bool gNetworkAreaLoaded;
static struct Area testArea;
static struct Object bomb;
static CharacterNetState cars[2];
static uint32_t peerGeneration;
static int available[2],owned,visible,sends,grounded,coins,respawns;
/* Isolated policy/native-consequence fixture. test_contacts.c separately
 * exercises the production staging pass and supersonic precedence. */
int rocket_contacts_bobomb_yaw(struct Object *object,s16 *yaw){return rocket_bobomb_bump_yaw(object,yaw);}
static struct Object explosion;
const BehaviorScript bhvExplosion[]={0},bhvBobomb[]={1};
bool sync_object_is_owned_locally(u32 id){assert(id==1);return owned;}
int codex_panel_gl_is_focused(void){return 0;}
int rocket_adapter_interaction_snapshot(RocketSnapshot *s){*s=cars[0].car;return available[0];}
int character_net_interaction_state(unsigned i,CharacterNetState *s,uint32_t *generation){if(i!=1)return 0;*s=cars[1];*generation=peerGeneration;return available[1];}
uint32_t rocket_runtime_epoch(void){return cars[0].epoch;}
int rocket_adapter_object_visible(const float *p,struct Object *obj){(void)p;assert(obj==&bomb);return visible;}
void obj_set_hitbox(struct Object *obj,struct ObjectHitbox *h){obj->hitboxRadius=h->radius;obj->hitboxHeight=h->height;}
struct Object *nearest_player_to_object(struct Object *obj){(void)obj;return NULL;}
s32 obj_attack_collided_from_other_object(struct Object *obj){(void)obj;return 0;}
void network_send_object_reliability(struct Object *obj,bool reliable){assert(obj==&bomb&&reliable);sends++;}
s16 object_step(void){return grounded?OBJ_COL_FLAG_GROUNDED:0;}
void cur_obj_scale(f32 scale){(void)scale;}
struct Object *spawn_object(struct Object *parent,s32 model,const BehaviorScript *bhv){assert(parent==&bomb&&model==MODEL_EXPLOSION&&bhv==bhvExplosion);return &explosion;}
void obj_spawn_yellow_coins(struct Object *obj,s8 n){assert(obj==&bomb&&n==1);coins++;}
void set_object_respawn_info_bits(struct Object *obj,u8 bits){assert(obj==&bomb&&bits==1);}
void create_respawner(s32 model,const BehaviorScript *bhv,s32 distance){assert(model==MODEL_BLACK_BOBOMB&&bhv==bhvBobomb&&distance==3000);respawns++;}
static void fresh(int online,int local){
 memset(histories,0,sizeof histories);memset(&bomb,0,sizeof bomb);memset(cars,0,sizeof cars);
 memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
 memset(&gCLIOpts,0,sizeof gCLIOpts);memset(&testArea,0,sizeof testArea);
 gCurrentArea=&testArea;gCurrentObject=&bomb;gGlobalTimer=0;sCurrPlayMode=0;
 bomb.activeFlags=ACTIVE_FLAG_ACTIVE;bomb.oSyncID=1;bomb.oAction=BOBOMB_ACT_PATROL;
 bomb.hitboxRadius=65;bomb.hitboxHeight=113;bomb.oPosZ=400;
 gNetworkType=online?NT_CLIENT:NT_NONE;gNetworkAreaLoaded=true;
 gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerLocal->currAreaSyncValid=true;gNetworkPlayerLocal->currLevelSyncValid=true;
 gCLIOpts.rocketCar=local;gCLIOpts.characterNet=online;available[0]=local;available[1]=!local;
 gNetworkPlayers[0].globalIndex=2;gNetworkPlayers[1].globalIndex=1;
 owned=visible=1;sends=grounded=coins=respawns=0;peerGeneration=0;
 for(int i=0;i<2;i++){cars[i].epoch=1;cars[i].car.basis[2]=cars[i].car.basis[3]=cars[i].car.basis[7]=1;cars[i].car.position[1]=40;cars[i].car.velocity[2]=600;cars[i].car.ticks=4;}
}
static void frame(float z){
 ++gGlobalTimer;for(int i=0;i<2;i++){cars[i].car.position[2]=z;cars[i].car.ticks+=4;}bobomb_check_interactions();
}
static void bump(void){frame(0);frame(80);frame(160);}
int main(void){
 fresh(0,1);bomb.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_MARIO_UNK1;bomb.oMoveAngleYaw=0x4000;
 bobomb_check_interactions();assert(!sends&&bomb.oAction==BOBOMB_ACT_LAUNCHED&&bomb.oMoveAngleYaw==0x4000);
 assert(bomb.oForwardVel==25&&bomb.oVelY==30); /* unchanged native kick path */
 fresh(0,1);bump();assert(sends==1&&bomb.oAction==BOBOMB_ACT_LAUNCHED&&bomb.oMoveAngleYaw==0);
 assert(bomb.oForwardVel==25&&bomb.oVelY==30);frame(165);assert(sends==1);
 bobomb_act_launched();assert(bomb.oAction==BOBOMB_ACT_LAUNCHED);
 grounded=1;bobomb_act_launched();assert(bomb.oAction==BOBOMB_ACT_EXPLODE);
 bomb.oTimer=5;bobomb_act_explode();assert(coins==1&&respawns==1&&bomb.activeFlags==ACTIVE_FLAG_DEACTIVATED);
 fresh(1,0);bump();assert(sends==1); /* client-native owner reacts to another car */
 fresh(1,0);gNetworkType=NT_SERVER;bump();assert(sends==1); /* Mario host sees client car */
 fresh(1,1);available[1]=1;
 cars[1].car.basis[0]=-1;cars[1].car.basis[2]=0;cars[1].car.basis[3]=0;cars[1].car.basis[5]=1;
 cars[1].car.velocity[0]=-600;cars[1].car.velocity[2]=0;
 for(int n=0;n<3;n++){
  ++gGlobalTimer;cars[0].car.position[2]=n*80;cars[1].car.position[0]=400-n*80;cars[1].car.position[2]=400;
  for(int i=0;i<2;i++)cars[i].car.ticks+=4;
  bobomb_check_interactions();
 }
 assert(sends==1&&(u16)bomb.oMoveAngleYaw==0xc000); /* remote global1 wins over local global2 */
 fresh(1,1);owned=0;bump();assert(!sends&&bomb.oAction==BOBOMB_ACT_PATROL);
 owned=1;frame(165);assert(!sends); /* authority acquisition inside cannot synthesize entry */
 fresh(1,0);frame(0);available[1]=0;frame(80);available[1]=1;frame(160);assert(!sends);
 fresh(1,0);frame(0);cars[1].epoch++;frame(160);assert(!sends);
 fresh(1,0);frame(0);gNetworkPlayers[1].currLevelAreaSeqId++;frame(160);assert(!sends);
 fresh(1,0);frame(0);peerGeneration++;frame(160);assert(!sends); /* reconnect with same epoch */
 fresh(0,1);frame(0);rocket_bobomb_forget(&bomb);frame(160);assert(!sends); /* reused native pool slot */
 fresh(0,1);visible=0;bump();assert(!sends);
 fresh(0,1);cars[0].car.velocity[2]=100;bump();assert(!sends);
 fresh(0,1);cars[0].car.velocity[2]=-600;bump();assert(!sends);
 fresh(0,1);cars[0].car.position[1]=1000;bump();assert(!sends);
 fresh(0,1);bomb.oHeldState=HELD_HELD;bump();assert(!sends);
 fresh(0,1);bomb.oAction=BOBOMB_ACT_EXPLODE;bump();assert(!sends);
 fresh(0,1);frame(0);bomb.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_TOUCHED_BOB_OMB;
 frame(160);assert(!sends&&bomb.oAction==BOBOMB_ACT_EXPLODE); /* native touch wins */
 fresh(0,1);bomb.oIntangibleTimer=-1;bump();assert(!sends);
 fresh(0,1);frame(0);sCurrPlayMode=PLAY_MODE_PAUSED;frame(80);sCurrPlayMode=0;frame(160);assert(!sends);
 fresh(0,1);bomb.oTimer=30;frame(0);bomb.oTimer=0;bomb.oAction=BOBOMB_ACT_CHASE_MARIO;
 frame(160);assert(sends==1); /* native patrol-to-chase must not swallow the bumper entry */
 puts("PASS Bob-omb: native kick/landing/explosion/coin/respawn, offline/server/client owner, stale/reset/held/pause/wall/slow/reverse rejection");
}
