/* Actual host adapter and native squash action; generated geometry and inert
 * engine services. This is not a running-game acceptance test. */
#define main adapter_suite_main
#define level_trigger_warp adapter_fixture_warp
#include "test_adapter.c"
#undef level_trigger_warp
#undef main
#include "game/mario_step.h"
#include "game/mario_actions_cutscene.h"
#include "game/mario_misc.h"
#include "game/rumble_init.h"
#define smlua_call_event_hooks(hook,m,allow) ((void)(m),(void)(allow))
struct MarioState gMarioStates[MAX_PLAYERS];
static unsigned checks,deathWarps,voice,rumble;
static int canBubble;
#define CHECK(x) do{checks++;assert(x);}while(0)
s16 set_character_animation(struct MarioState *m,enum CharacterAnimID id){(void)m;CHECK(id==CHAR_ANIM_A_POSE);return 0;}
void play_character_sound_if_no_flag(struct MarioState *m,enum CharacterSound id,u32 flag){(void)m;(void)id;(void)flag;voice++;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;rumble++;}
bool mario_can_bubble(struct MarioState *m){(void)m;return canBubble;}
void mario_set_bubbled(struct MarioState *m){m->action=ACT_BUBBLED;}
s16 level_trigger_warp(struct MarioState *m,s32 op){(void)m;CHECK(op==WARP_OP_DEATH);deathWarps++;return 20;}
s32 perform_ground_step(struct MarioState *m){(void)m;return GROUND_STEP_NONE;}
void stop_and_set_height_to_floor(struct MarioState *m){m->pos[1]=m->floorHeight;vec3f_set(m->vel,0,0,0);vec3f_copy(m->marioObj->header.gfx.pos,m->pos);}
#include "native_squish.inc.c"
#include "game/rocket_squish_visual.h"
static struct Object crusher;
static struct Surface ceiling;
static struct SurfaceNode ceilingNode;
static Collision collisionIdentity[1];
static void setup(float x,int upside,int network){
    fresh();memset(&whompCrush,0,sizeof whompCrush);memset(&crusher,0,sizeof crusher);memset(&ceiling,0,sizeof ceiling);
    crusher.behavior=bhvSmallWhomp;crusher.activeFlags=ACTIVE_FLAG_ACTIVE;crusher.oAction=5;crusher.oSyncID=7;crusher.collisionData=collisionIdentity;
    ceiling.object=&crusher;ceiling.flags=SURFACE_FLAG_DYNAMIC;ceiling.normal.y=-1;
    ceilingNode.surface=&ceiling;ceilingNode.next=NULL;gDynamicSurfacePartition[8][8][SPATIAL_PARTITION_CEILS].next=&ceilingNode;
    vec3s_set(ceiling.vertex1,-180,140,-500);vec3s_set(ceiling.vertex2,180,140,-500);vec3s_set(ceiling.vertex3,0,140,500);
    nativeFloor=&surfaces[0];nativeFloor->normal.y=1;mario.floor=nativeFloor;mario.floorHeight=0;chimneyGeometry=1;chimneyFloorHeight=0;
    mario.pos[0]=x;step();pose.position[1]=upside?80.f:34.f;vec3f_copy(mario.pos,pose.position);vec3f_copy(lastPosition,mario.pos);
    if(upside){pose.basis[3]=-1;pose.basis[7]=-1;}
    mario.flags=MARIO_CAP_ON_HEAD;mario.invincTimer=0;voice=rumble=deathWarps=0;canBubble=0;
    if(network){gNetworkType=network==1?NT_SERVER:NT_CLIENT;gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkAreaLoaded=true;}
}
static void height(int h){ceiling.vertex1[1]=ceiling.vertex2[1]=ceiling.vertex3[1]=h;}
static void native_frame(void){
    gGlobalTimer++;mario.floorHeight=chimneyFloorHeight;mario.ceilHeight=20000;mario.ceil=NULL;mario.input=0;
    rocket_adapter_prepare_interactions(&mario);CHECK(!rocket_adapter_update(&mario));
    act_squished(&mario);squish_mario_model(&mario);
    CHECK(mario.pos[1]>=mario.floorHeight&&isfinite(mario.pos[1]));
}
static void full_cycle(float x,int upside,int network){
    setup(x,upside,network);int before=steps;
    rocket_adapter_prepare_interactions(&mario);
    CHECK(mario.action==ACT_SQUISHED&&mario.pos[0]==x&&mario.pos[1]==0);
    CHECK(!rocket_adapter_update(&mario)&&steps==before&&!draw);
    act_squished(&mario);CHECK(mario.actionState==0&&mario.hurtCounter==0);
    height(0);native_frame();CHECK(mario.actionState==1&&mario.hurtCounter==12);
    CHECK(fabsf(object.header.gfx.scale[1]-.05f)<.0001f);
    for(int i=0;i<35;i++){native_frame();CHECK(mario.hurtCounter==12&&steps==before);}
    CHECK(voice==1&&rumble==1);mario.hurtCounter=0;
    crusher.oSubAction=10;crusher.oAction=6;height(400);
    for(int i=0;i<16;i++)native_frame();
    CHECK(mario.action==ACT_IDLE&&mario.squishTimer==29);mario.input=0;
    for(int i=0;i<29;i++) {
        rocket_adapter_prepare_interactions(&mario);CHECK(!rocket_adapter_update(&mario)&&steps==before);
        squish_mario_model(&mario);gGlobalTimer++;
    }
    rocket_adapter_prepare_interactions(&mario);step();CHECK(draw&&steps==before+1&&resets==2);
}
#define REJECT(statement) do{setup(170,0,0);statement;rocket_adapter_prepare_interactions(&mario);CHECK(mario.action==ACT_IDLE);}while(0)
static void gates(void){
    REJECT(crusher.oSubAction=10;crusher.oAction=6); // Stand-up back fling stays rigid.
    REJECT(crusher.oAction=3);REJECT(crusher.activeFlags=0);REJECT(crusher.oSyncDeath=1);
    REJECT(crusher.oIntangibleTimer=-1);REJECT(crusher.header.gfx.activeAreaIndex++);
    REJECT(crusher.behavior=bhvFloorSwitchGrills);REJECT(mario.floor->object=&crusher);
    REJECT(mario.health=0xff);REJECT(mario.playerIndex=1);REJECT(mario.freeze=1);
    REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);REJECT(gTimeStopState=TIME_STOP_ACTIVE);
    REJECT(mario.heldObj=&crusher);REJECT(mario.heldByObj=&crusher);REJECT(mario.riddenObj=&crusher);
    REJECT(selected=0);selected=1;
    REJECT(gNetworkType=NT_CLIENT);REJECT(ceiling.flags=SURFACE_FLAG_INTANGIBLE);
    REJECT(height(300));REJECT(pose.position[0]=mario.pos[0]=600);
    REJECT(pose.position[1]=mario.pos[1]=500);REJECT(pose.basis[0]=NAN);
    REJECT(mario.pos[0]+=501);REJECT(gGlobalTimer+=2);
    // An arbitrary car heading and corner-only overlap use the full projection.
    setup(170,0,0);float a=.6f;pose.basis[0]=sinf(a);pose.basis[2]=cosf(a);pose.basis[3]=cosf(a);pose.basis[5]=-sinf(a);
    rocket_adapter_prepare_interactions(&mario);CHECK(mario.action==ACT_SQUISHED);
    // Dynamic support: preserve the newly sampled floor, without horizontal teleport.
    setup(170,0,0);mario.floorHeight=chimneyFloorHeight=15;nativeFloor->flags=SURFACE_FLAG_DYNAMIC;
    rocket_adapter_prepare_interactions(&mario);CHECK(mario.pos[0]==170&&mario.pos[1]==15);
    // Identity/area/selection/respawn do not retain the old crusher or pose.
    for(int reset=0;reset<5;reset++){
        setup(170,0,0);rocket_adapter_prepare_interactions(&mario);rocket_adapter_update(&mario);
        if(reset==0)crusher.oSyncID++;
        if(reset==1)gCurrLevelNum++;
        if(reset==2)selected=0;
        if(reset==3)mario.action=ACT_DISAPPEARED;
        if(reset==4)rocket_adapter_forget_platform(&crusher);
        mario.ceil=NULL;mario.ceilHeight=20000;rocket_adapter_prepare_interactions(&mario);
        CHECK(!whompCrush.object&&!mario.ceil);selected=1;
    }
}
static void native_damage(void){
    for(int condition=0;condition<3;condition++){
        setup(0,0,0);if(condition==0)mario.flags|=MARIO_METAL_CAP;if(condition==1)mario.invincTimer=10;if(condition==2)mario.flags=0;
        rocket_adapter_prepare_interactions(&mario);rocket_adapter_update(&mario);height(0);native_frame();
        CHECK(mario.hurtCounter==(condition==2?18:0));native_frame();CHECK(mario.hurtCounter==(condition==2?18:0));
    }
    setup(0,0,0);rocket_adapter_prepare_interactions(&mario);rocket_adapter_update(&mario);height(0);native_frame();
    mario.health=0xff;mario.hurtCounter=0;crusher.oAction=6;crusher.oSubAction=10;height(400);
    for(int i=0;i<16;i++)native_frame();
    CHECK(deathWarps==1&&mario.action==ACT_DISAPPEARED);
}
static void visuals(void){
    float local[3],remote[3];
    setup(0,0,0);mario.action=ACT_SQUISHED;mario.squishTimer=255;
    vec3f_set(object.header.gfx.scale,1.8f,.05f,1.8f);
    rocket_squish_visual_scale(&mario,local);CHECK(local[1]==.05f);
    mario.playerIndex=1;rocket_squish_visual_scale(&mario,remote);CHECK(!memcmp(local,remote,sizeof local));
    for(int timer=2;timer<=30;timer++){
        mario.playerIndex=0;mario.action=ACT_IDLE;mario.squishTimer=timer;squish_mario_model(&mario);
        rocket_squish_visual_scale(&mario,local);mario.playerIndex=1;rocket_squish_visual_scale(&mario,remote);
        for(int k=0;k<3;k++)CHECK(fabsf(local[k]-remote[k])<.000001f);
    }
    mario.squishTimer=0;rocket_squish_visual_scale(&mario,remote);CHECK(remote[0]==1&&remote[1]==1);
    mario.action=ACT_DISAPPEARED;mario.squishTimer=255;rocket_squish_visual_scale(&mario,remote);CHECK(remote[1]==1);
    mario.playerIndex=0;mario.action=ACT_SQUISHED;object.header.gfx.scale[1]=NAN;rocket_squish_visual_scale(&mario,local);CHECK(local[1]==1);
    const float pivot[]={10,0,20},scale[]={1.8f,.05f,1.8f};float vertex[]={10,100,20},normal[]={0,1,0};
    rocket_squish_vertex(vertex,normal,pivot,scale);CHECK(vertex[0]==10&&vertex[1]==5&&vertex[2]==20&&normal[1]==1);
}
int main(void){
    for(int net=0;net<3;net++)for(int flip=0;flip<2;flip++){full_cycle(0,flip,net);full_cycle(170,flip,net);}
    gates();native_damage();visuals();
    printf("PASS %u Whomp underside/native squash checks: edge, roof, repeated frames, local host/client ownership, damage, recovery and death\n",checks);
}
