#include "speed_fixture_stubs.h"
/* Real host adapter; explicit runtime/surface mocks. No physics parity claim. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/network/network.h"
#include "pc/cliopts.h"
#include "level_table.h"
#ifndef ROCKET_ADAPTER_SOURCE
#define ROCKET_ADAPTER_SOURCE "../../../src/game/rocket_adapter.c"
#endif
#include ROCKET_ADAPTER_SOURCE
SpatialPartitionCell gStaticSurfacePartition[NUM_CELLS][NUM_CELLS],gDynamicSurfacePartition[NUM_CELLS][NUM_CELLS];
s32 gSurfaceNodesAllocated=32;
struct Area *gCurrentArea;
struct WarpTransition gWarpTransition;
struct WarpDest sWarpDest;
s16 sDelayedWarpOp;
u32 gTimeStopState;
enum NetworkType gNetworkType;
bool gNetworkAreaLoaded,gNetworkAreaSyncing;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal;
struct CLIOptions gCLIOpts;
struct ObjectWarpNode *area_get_warp_node(u8 id){
    for(struct ObjectWarpNode *n=gCurrentArea?gCurrentArea->warpNodes:NULL;n;n=n->next)
        if(n->node.id==id)return n;
    return NULL;
}
uint32_t rocket_caps_active_flags(unsigned index){(void)index;return player?player->flags:0;}
s16 gCurrLevelNum,sCurrPlayMode,gCurrActNum;
static int shipWarps;
s16 level_trigger_warp(struct MarioState *m,s32 op){assert(m->playerIndex==0&&op==WARP_OP_WARP_FLOOR);++shipWarps;sDelayedWarpOp=op;return 20;}
u32 gGlobalTimer;
static int enabled,steps,resets,interrupts,meshCalls[2],meshCount[2],draw,uiBlocked,metalWater;
static int recoveries;
struct LevelValues gLevelValues;
const BehaviorScript bhvSmallWhomp[]={20},bhvWhompKingBoss[]={21},bhvWarpPipe[]={22};
const Collision warp_pipe_seg3_collision_03009AC8[]={0};
const BehaviorScript bhvCapSwitch[]={0},bhvExclamationBox[]={1},bhvVanishCap[]={2},bhvWingCap[]={3},bhvMetalCap[]={4},bhvWarp[]={5},bhvInSunkenShip[]={6},bhvUnagi[]={7},bhvUnagiSubobject[]={8};
struct ObjectNode *gObjectLists;
static struct ObjectNode objectLists[NUM_OBJ_LISTS];
static RocketSnapshot pose;
static RocketInput observed;
static RocketGamepad fixtureGamepad;
static float nativeWater=-10000,recoveryWater;
static int environmentRegions;
static int chimneyGeometry;
static float chimneyFloorHeight;
static struct Surface *nativeFloor,*recoveryFloor;
f32 find_water_level(f32 x,f32 z){(void)x;return environmentRegions&&z<-200.f?recoveryWater:nativeWater;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){
    (void)x;(void)y;
    if(chimneyGeometry){*floor=nativeFloor;return chimneyFloorHeight;}
    *floor=environmentRegions&&z<-200.f?recoveryFloor:nativeFloor;
    return environmentRegions&&z<-200.f?-40.f:24.f;
}
void rocket_runtime_set_water(int present,float level,int metal){pose.water_mode=rocket_water_classify(pose.water_mode,present,level,pose.position[1],metal);metalWater=pose.water_mode==ROCKET_WATER_METAL;}
void rocket_runtime_set_water_query(RocketWaterQuery query){(void)query;}
static int environmentCalls,environmentSamples;
static struct MarioState sampledEnvironment,sampledNative;
static RocketSnapshot sampledPose;
int rocket_runtime_set_environment(const RocketEnvironment *e){assert(e);environmentCalls++;return 1;}
void rocket_environment_sample(struct MarioState *m,const RocketSnapshot *s,RocketEnvironment *e){
    /* This fixture checks the adapter/sampler boundary. The native environment
     * suite separately exercises actual force calculations from these inputs. */
    environmentSamples++;sampledEnvironment=*m;sampledNative=*player;sampledPose=*s;
    memset(e,0,sizeof *e);
}
unsigned rocket_environment_material(struct MarioState *m,struct Surface *s){(void)m;return s->type==SURFACE_ICE?ROCKET_MATERIAL_VERY_SLIPPERY:0;}
const BehaviorScript bhvFloorSwitchGrills[]={0},bhvFloorSwitchHardcodedModel[]={0},bhvFloorSwitchAnimatesObject[]={0};
s32 obj_has_behavior(struct Object *object,const BehaviorScript *behavior){return object->behavior==behavior;}
static RocketPlatform observedPlatform;
static RocketTriangle observedTriangle;
static size_t platformCount;
static int platformCalls;
int rocket_runtime_enabled(void){return enabled;}
void rocket_runtime_suspend(void){draw=metalWater=0;}
void rocket_runtime_set_metal_water(int active){metalWater=active;}
void rocket_runtime_interrupt(void){++interrupts;}
int rocket_runtime_reset(const float p[3],const float v[3],float yaw){(void)yaw;++resets;memset(&pose,0,sizeof pose);memcpy(pose.position,p,sizeof pose.position);memcpy(pose.velocity,v,sizeof pose.velocity);pose.basis[2]=pose.basis[3]=pose.basis[7]=1;pose.grounded=1;draw=0;return 1;}
int rocket_runtime_mesh(int layer,const RocketTriangle *triangles,size_t count){(void)triangles;++meshCalls[layer];meshCount[layer]=(int)count;return 1;}
int rocket_runtime_platforms(const RocketPlatform *platforms,size_t count){
    platformCalls++;platformCount=count;
    if(count){observedPlatform=platforms[0];observedTriangle=platforms[0].triangles[0];}
    return 1;
}
int rocket_runtime_frame(uint64_t frame,const RocketInput *input,int paused,int blocked){(void)frame;(void)paused;observed=rocket_gamepad_merge(input,&fixtureGamepad);if(blocked)memset(&observed,0,sizeof observed);++steps;pose.ticks+=4;draw=1;return 4;}
int rocket_runtime_snapshot(RocketSnapshot *out){if(!draw)return 0;*out=pose;return 1;}
int rocket_runtime_recover(const RocketSnapshot *out){++recoveries;memcpy(pose.position,out->position,sizeof pose.position);memcpy(pose.basis,out->basis,sizeof pose.basis);memset(pose.velocity,0,sizeof pose.velocity);return 1;}
int rocket_runtime_read_input(const RocketInput *keyboard,RocketInput *out){*out=rocket_gamepad_merge(keyboard,&fixtureGamepad);return !uiBlocked&&draw&&enabled;}
// The host exports a nonstandard atan2f. Heading conversion must not use it.
#ifndef TEST_NATIVE_MATH
f32 atan2f(f32 y,f32 x){(void)y;(void)x;return 1234.f;}
#endif
s32 set_water_plunge_action(struct MarioState *m){m->action=ACT_WATER_PLUNGE;return 1;}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){m->action=action;m->actionArg=arg;m->actionState=m->actionTimer=0;return 1;}
s32 transition_submerged_to_walking(struct MarioState *m){m->action=ACT_WALKING;return 1;}
static struct MarioState mario;
static struct Object object;
static struct Controller controller;
static struct Area testArea;
static struct MarioBodyState bodyState;
static struct Surface surfaces[4];
static struct SurfaceNode nodes[6];
static void fresh(void){
    rocket_adapter_set_selected(1);rocket_adapter_suspend();memset(&mario,0,sizeof mario);memset(&object,0,sizeof object);memset(&controller,0,sizeof controller);memset(&testArea,0,sizeof testArea);
    memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);memset(surfaces,0,sizeof surfaces);memset(nodes,0,sizeof nodes);
    memset(&fixtureGamepad,0,sizeof fixtureGamepad);
    enabled=1;steps=resets=interrupts=uiBlocked=0;memset(meshCalls,0,sizeof meshCalls);sCurrPlayMode=0;gGlobalTimer=0;
    recoveries=0;memset(&gLevelValues,0,sizeof gLevelValues);
    memset(&gWarpTransition,0,sizeof gWarpTransition);memset(&sWarpDest,0,sizeof sWarpDest);
    memset(&gCLIOpts,0,sizeof gCLIOpts);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    sDelayedWarpOp=WARP_OP_NONE;gTimeStopState=0;gNetworkType=NT_NONE;gNetworkPlayerLocal=NULL;
    gNetworkAreaLoaded=gNetworkAreaSyncing=false;chimneyGeometry=0;gSurfaceNodesAllocated=32;gCurrLevelNum=LEVEL_BOB;gCurrActNum=1;shipWarps=0;
    environmentCalls=environmentSamples=environmentRegions=0;nativeWater=-10000;gLevelValues.floorLowerLimit=-11000;
    nativeFloor=&surfaces[1];recoveryFloor=&surfaces[0];recoveryWater=gLevelValues.floorLowerLimit;
    gObjectLists=objectLists;memset(objectLists,0,sizeof objectLists);
    for(int i=0;i<NUM_OBJ_LISTS;++i)objectLists[i].next=&objectLists[i];
    memset(&bodyState,0,sizeof bodyState);mario.marioBodyState=&bodyState;
    gCurrentArea=&testArea;mario.area=&testArea;mario.marioObj=&object;mario.controller=&controller;mario.floor=&surfaces[0];mario.waterLevel=-10000;mario.health=0x880;mario.action=ACT_IDLE;
    // Same surface occurs in several partition cells; it must appear once.
    nodes[0].surface=&surfaces[0];nodes[1].surface=&surfaces[0];nodes[0].next=&nodes[1];
    gStaticSurfacePartition[0][0][0].next=&nodes[0];
    nodes[2].surface=&surfaces[1];gDynamicSurfacePartition[0][0][0].next=&nodes[2];
}
static void step(void){++gGlobalTimer;assert(rocket_adapter_update(&mario)==1);}
static struct Object door;
static void door_setup(void){
    fresh();step();controller.rawStickY=80;
    memset(&door,0,sizeof door);door.activeFlags=ACTIVE_FLAG_ACTIVE;
    door.oInteractType=INTERACT_WARP_DOOR;door.oPosZ=200;door.hitboxRadius=80;door.hitboxHeight=100;
    objectLists[OBJ_LIST_SURFACE].next=&door.header;door.header.next=&objectLists[OBJ_LIST_SURFACE];
}
static void no_door(void){rocket_adapter_prepare_interactions(&mario);assert(mario.action==ACT_IDLE&&!object.numCollidedObjs);}
static void door_wall(int index,int dynamic){
    vec3s_set(surfaces[index].vertex1,-100,0,100);vec3s_set(surfaces[index].vertex2,100,0,100);vec3s_set(surfaces[index].vertex3,0,200,100);
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    nodes[index+1].surface=&surfaces[index];nodes[index+1].next=partition[NUM_CELLS/2][NUM_CELLS/2][1].next;
    partition[NUM_CELLS/2][NUM_CELLS/2][1].next=&nodes[index+1];
}
static void test_doors(void){
    door_setup();int before=steps;rocket_adapter_prepare_interactions(&mario);
    assert(mario.action==ACT_WALKING&&object.numCollidedObjs==1&&object.collidedObjs[0]==&door);
    assert(mario.collidedObjInteractTypes==INTERACT_WARP_DOOR&&object.collidedObjInteractTypes==INTERACT_WARP_DOOR);
    assert(steps==before&&!door.oInteractStatus&&!door.oAction&&!mario.usedObj); // Native handler still owns the transition.
    rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1&&steps==before);
    mario.action=ACT_PUSHING_DOOR;assert(!rocket_adapter_update(&mario)&&!draw); // Host cutscene takes over.
    mario.action=ACT_IDLE;step();assert(resets==2); // Fresh car after the native transition.
    door_setup();door.oInteractType=INTERACT_DOOR;door.oBehParams=50u<<24;
    rocket_adapter_prepare_interactions(&mario);assert(mario.collidedObjInteractTypes==INTERACT_DOOR&&door.oBehParams==(50u<<24)&&!door.oAction);
    door_setup();controller.rawStickY=0;no_door();controller.rawStickY=-80;no_door();
    door_setup();controller.buttonDown=A_BUTTON;no_door();
    door_setup();uiBlocked=1;no_door();
    door_setup();sCurrPlayMode=PLAY_MODE_PAUSED;no_door();sCurrPlayMode=0;mario.freeze=1;no_door();
    door_setup();pose.grounded=0;no_door();pose.grounded=1;pose.basis[7]=-.9f;no_door();
    door_setup();door.oPosZ=-200;no_door();door.oPosZ=400;no_door();door.oPosZ=200;door.oPosX=200;no_door();
    door_setup();door.oPosY=500;no_door();
    door_setup();door.activeFlags=0;no_door();door.activeFlags=ACTIVE_FLAG_ACTIVE;door.oIntangibleTimer=-1;no_door();
    door_setup();door.header.gfx.activeAreaIndex=1;no_door();
    door_setup();door.oAction=1;no_door();
    door_setup();door.oInteractType=INTERACT_POLE;no_door();
    door_setup();door_wall(2,1);no_door();surfaces[2].object=&door;rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1);
    for(int kind=0;kind<4;++kind){
        door_setup();surfaces[3].flags=kind==0?SURFACE_FLAG_INTANGIBLE:0;
        surfaces[3].type=kind==1?SURFACE_INTANGIBLE:kind==2?SURFACE_CAMERA_BOUNDARY:kind==3?SURFACE_RAYCAST:SURFACE_DEFAULT;
        door_wall(2,1);door_wall(3,0);no_door(); // Coincident excluded and solid triangles still block.
        for(int v=0;v<3;++v){s16 *vertices[]={surfaces[2].vertex1,surfaces[2].vertex2,surfaces[2].vertex3};vertices[v][2]=120;}
        no_door(); // Real wall behind the excluded triangle also blocks.
        surfaces[2].flags=SURFACE_FLAG_INTANGIBLE;rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1);
    }
    door_setup();mario.pos[2]=pose.position[2]=-50;door.oPosZ=150;door_wall(2,0);no_door(); // Short ray crosses z=0 cell boundary.
    door_setup();mario.pos[0]+=501;no_door();
    door_setup();object.numCollidedObjs=4;rocket_adapter_prepare_interactions(&mario);assert(mario.action==ACT_IDLE&&object.numCollidedObjs==4);
    door_setup();rocket_adapter_suspend();no_door();
}
static void test_interaction_snapshot(void){
    RocketSnapshot snapshot;
    fresh();assert(!rocket_adapter_interaction_snapshot(&snapshot));step();
    int before=steps,oldResets=resets;
    assert(rocket_adapter_interaction_snapshot(&snapshot)&&snapshot.ticks==pose.ticks);
    assert(rocket_adapter_interaction_snapshot(&snapshot)&&steps==before&&resets==oldResets);
    assert(!rocket_adapter_interaction_snapshot(NULL));
    uiBlocked=1;assert(rocket_adapter_platform_snapshot(&snapshot));uiBlocked=0; // Menus do not remove physical weight.
    uiBlocked=1;assert(!rocket_adapter_interaction_snapshot(&snapshot));uiBlocked=0;
    sCurrPlayMode=PLAY_MODE_PAUSED;assert(!rocket_adapter_interaction_snapshot(&snapshot));sCurrPlayMode=0;
    mario.freeze=1;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.freeze=0;
    mario.health=0xff;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.health=0x880;
    mario.heldObj=&door;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.heldObj=NULL;
    mario.heldByObj=&door;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.heldByObj=NULL;
    mario.riddenObj=&door;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.riddenObj=NULL;
    mario.action=ACT_READING_AUTOMATIC_DIALOG;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.action=ACT_IDLE;
    mario.pos[0]+=501;assert(!rocket_adapter_interaction_snapshot(&snapshot));mario.pos[0]-=501;
    gCurrLevelNum++;assert(!rocket_adapter_interaction_snapshot(&snapshot));gCurrLevelNum--;
    assert(rocket_adapter_interaction_snapshot(&snapshot)&&steps==before&&resets==oldResets);
    rocket_adapter_set_selected(0);assert(!rocket_adapter_interaction_snapshot(&snapshot));
}
static void test_body_snapshot(void){
    RocketSnapshot snapshot;struct Object remote={0};
    fresh();assert(!rocket_adapter_body_snapshot(&object,&snapshot));step();
    int before=steps,oldResets=resets;
    assert(rocket_adapter_body_snapshot(&object,&snapshot));
    assert(!rocket_adapter_body_snapshot(&remote,&snapshot));
    assert(!rocket_adapter_body_snapshot(NULL,&snapshot));
    assert(!rocket_adapter_body_snapshot(&object,NULL));
    uiBlocked=1;assert(rocket_adapter_body_snapshot(&object,&snapshot));
    uiBlocked=0;gGlobalTimer++;assert(rocket_adapter_body_snapshot(&object,&snapshot));
    gGlobalTimer++;assert(!rocket_adapter_body_snapshot(&object,&snapshot));gGlobalTimer-=2;
    mario.freeze=1;assert(!rocket_adapter_body_snapshot(&object,&snapshot));mario.freeze=0;
    sCurrPlayMode=PLAY_MODE_PAUSED;assert(!rocket_adapter_body_snapshot(&object,&snapshot));sCurrPlayMode=0;
    mario.action=ACT_BACKWARD_GROUND_KB;assert(!rocket_adapter_body_snapshot(&object,&snapshot));mario.action=ACT_IDLE;
    mario.playerIndex=1;assert(!rocket_adapter_body_snapshot(&object,&snapshot));mario.playerIndex=0;
    mario.pos[0]+=501;assert(!rocket_adapter_body_snapshot(&object,&snapshot));mario.pos[0]-=501;
    mario.heldObj=&remote;assert(!rocket_adapter_body_snapshot(&object,&snapshot));mario.heldObj=NULL;
    gCurrLevelNum++;assert(!rocket_adapter_body_snapshot(&object,&snapshot));gCurrLevelNum--;
    assert(steps==before&&resets==oldResets);
    rocket_adapter_suspend();assert(!rocket_adapter_body_snapshot(&object,&snapshot));
}
static void grate_setup(int dynamic){
    fresh();door_wall(2,dynamic);
    surfaces[2].type=SURFACE_VANISH_CAP_WALLS;surfaces[2].normal.z=-1;
    mario.pos[2]=-300;step();
}
static void test_vanish(void){
    for(int dynamic=0;dynamic<2;dynamic++){
        grate_setup(dynamic);assert(meshCount[dynamic]==2);
        mario.flags=MARIO_METAL_CAP|MARIO_WING_CAP;step();assert(meshCount[dynamic]==2);
        mario.flags|=MARIO_VANISH_CAP;step();assert(meshCount[dynamic]==1);
        pose.position[2]=20;pose.boost=17;step();assert(phase_overlap(&pose,0));
        assert(rocket_adapter_switch_reason()); // Complete chassis still intersects, despite its origin being clear.
        mario.flags&=~MARIO_VANISH_CAP;step();
        assert(recoveries==1&&pose.position[2]==-300&&pose.boost==17&&meshCount[dynamic]==2);
        step();assert(recoveries==1); // No repeated reset or free fuel.
        grate_setup(dynamic);mario.flags=MARIO_VANISH_CAP;step();
        pose.position[2]=400;step();mario.flags=0;step();assert(!recoveries&&pose.position[2]==400);
        // Native fixVanishFloors controls grates used as floors/ceilings.
        grate_setup(dynamic);mario.flags=MARIO_VANISH_CAP;surfaces[2].normal.y=1;step();assert(meshCount[dynamic]==2);
        gLevelValues.fixVanishFloors=1;step();assert(meshCount[dynamic]==1);
        // Ordinary geometry, including coincident triangles, remains solid.
        surfaces[2].type=SURFACE_DEFAULT;step();assert(meshCount[dynamic]==2);
    }
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    int before=steps;sCurrPlayMode=PLAY_MODE_PAUSED;mario.flags=0;step();assert(steps==before&&!recoveries);
    sCurrPlayMode=0;step();assert(recoveries==1);
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    rocket_adapter_set_selected(0);assert(mario.pos[2]==-300&&!phaseActive&&!haveClearPose);
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    mario.pos[2]=2000;rocket_adapter_suspend();assert(mario.pos[2]==2000); // External warp wins.
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    mario.health=0xff;assert(!rocket_adapter_update(&mario));assert(!phaseActive&&!haveClearPose&&mario.health==0xff);
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    // No prior safe pose: bounded normal exit, without a cap extension.
    haveClearPose=0;mario.flags=0;step();assert(recoveries==1&&!phase_overlap(&pose,0));
    grate_setup(0);mario.flags=MARIO_VANISH_CAP;step();pose.position[2]=20;step();
    gCurrLevelNum++;rocket_adapter_suspend();assert(mario.pos[2]==20&&!phaseActive&&!haveClearPose);
}
static void test_vanish_environment_recovery(void){
    for(int dynamic=0;dynamic<2;dynamic++)for(int wet=0;wet<2;wet++){
        grate_setup(dynamic);mario.flags=MARIO_VANISH_CAP|MARIO_WING_CAP;step();
        environmentRegions=1;nativeWater=500;
        recoveryWater=wet?250:gLevelValues.floorLowerLimit;
        nativeFloor->type=SURFACE_FLOWING_WATER;nativeFloor->force=0x100;
        recoveryFloor->type=wet?SURFACE_FLOWING_WATER:SURFACE_DEFAULT;recoveryFloor->force=0x240;
        // Enter the grate on a different native water/force floor. The last
        // safe chassis pose remains z=-300 across that boundary.
        pose.position[2]=20;mario.floor=nativeFloor;mario.floorHeight=24;mario.waterLevel=500;
        step();assert(mario.action==ACT_WATER_IDLE&&phase_overlap(&pose,0));
        assert(sampledEnvironment.floor==nativeFloor&&sampledEnvironment.waterLevel==500);
        pose.boost=17;pose.jump_time=.2f;pose.flip_time=.3f;pose.air_time=.4f;
        mario.flags&=~MARIO_VANISH_CAP;mario.capTimer=87;mario.health=0x740;mario.hurtCounter=2;
        struct MarioState original=mario;
        int calls=environmentCalls,samples=environmentSamples;u64 ticks=pose.ticks;
        step();
        assert(recoveries==1&&pose.position[2]==-300&&pose.boost==17&&pose.ticks==ticks+4);
        assert(pose.jump_time==.2f&&pose.flip_time==.3f&&pose.air_time==.4f);
        assert(environmentCalls==calls+1&&environmentSamples==samples+1);
        assert(sampledEnvironment.floor==recoveryFloor&&sampledEnvironment.floorHeight==-40);
        assert(sampledPose.position[2]==-300&&sampledEnvironment.pos[2]==-300);
        assert(sampledEnvironment.floor->force==0x240&&sampledEnvironment.waterLevel==recoveryWater);
        u32 action=wet?ACT_WATER_IDLE:(sampledPose.grounded?ACT_IDLE:ACT_FREEFALL);
        assert(sampledEnvironment.action==action);
        // The copy sees current native powers/progression, never old clearPose
        // state. Sampling must not overwrite the actual native floor/action.
        assert(!memcmp(&sampledNative,&original,sizeof original));
        assert(sampledEnvironment.flags==original.flags&&sampledEnvironment.capTimer==87);
        assert(sampledEnvironment.health==0x740&&sampledEnvironment.hurtCounter==2);
        assert(mario.flags==original.flags&&mario.capTimer==87&&mario.health==0x740&&mario.hurtCounter==2);
        assert(mario.floor==nativeFloor&&mario.floorHeight==24);
        assert(rocket_adapter_update(&mario)==1&&environmentSamples==samples+1&&environmentCalls==calls+1);
    }
}
static void test_vanish_progression(void){
    door_setup();door.behavior=bhvCapSwitch;door.oBehParams2ndByte=2;door.oAction=1;
    door.oInteractType=0;surfaces[1].object=&door;surfaces[1].normal.y=1;
    vec3s_set(surfaces[1].vertex1,-500,0,-500);vec3s_set(surfaces[1].vertex2,0,0,500);vec3s_set(surfaces[1].vertex3,500,0,-500);
    pose.wheel_contacts[0]=1;pose.wheel_position[0][1]=30;pose.wheel_radius[0]=30;
    assert(rocket_adapter_vanish_switch_contact(&door));assert(door.oAction==1&&!mario.flags); // Detector only.
    pose.wheel_contacts[0]=0;assert(!rocket_adapter_vanish_switch_contact(&door));pose.wheel_contacts[0]=1;
    pose.wheel_position[0][0]=600;assert(!rocket_adapter_vanish_switch_contact(&door));pose.wheel_position[0][0]=0;
    door.oAction=3;assert(!rocket_adapter_vanish_switch_contact(&door));door.oAction=1;
    door.behavior=bhvExclamationBox;assert(!rocket_adapter_vanish_switch_contact(&door));
    door_setup();door.behavior=bhvExclamationBox;door.oBehParams2ndByte=2;door.oAction=2;
    door_wall(2,1);surfaces[2].object=&door;surfaces[2].normal.z=-1;
    assert(rocket_adapter_vanish_box_contact(&door));assert(door.oAction==2&&!mario.flags);
    controller.rawStickY=0;assert(!rocket_adapter_vanish_box_contact(&door));controller.rawStickY=80;
    door.oAction=1;assert(!rocket_adapter_vanish_box_contact(&door));door.oAction=2;
    door.oIntangibleTimer=-1;assert(!rocket_adapter_vanish_box_contact(&door));door.oIntangibleTimer=0;
    door.behavior=bhvCapSwitch;assert(!rocket_adapter_vanish_box_contact(&door));
    door_setup();controller.rawStickY=0;door.behavior=bhvVanishCap;door.oInteractType=INTERACT_CAP;
    rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1&&object.collidedObjs[0]==&door&&!mario.flags&&!mario.capTimer);
    object.numCollidedObjs=0;door_wall(2,0);rocket_adapter_prepare_interactions(&mario);assert(!object.numCollidedObjs);
    surfaces[2].type=SURFACE_VANISH_CAP_WALLS;mario.flags=MARIO_VANISH_CAP;step();
    rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1);
    object.numCollidedObjs=0;door.oInteractType=INTERACT_STAR_OR_KEY;rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1);
    object.numCollidedObjs=0;uiBlocked=1;rocket_adapter_prepare_interactions(&mario);assert(!object.numCollidedObjs);
}
static void test_water(void){
    fresh();nativeWater=mario.waterLevel=200;mario.pos[1]=-500;
    mario.flags=MARIO_WING_CAP;mario.capTimer=80;
    step();assert(resets==1&&mario.pos[1]==-500&&mario.action==ACT_WATER_IDLE);
    assert(mario.action&ACT_FLAG_SWIMMING);assert(mario.capTimer==80&&mario.flags==MARIO_WING_CAP);
    int before=resets;pose.position[1]=mario.pos[1]=lastPosition[1]=110;step();
    assert(mario.action==ACT_WATER_IDLE&&resets==before); // surface hysteresis
    pose.position[1]=mario.pos[1]=lastPosition[1]=130;step();
    assert(mario.action==ACT_IDLE&&pose.water_mode==ROCKET_WATER_DRY&&resets==before);
    pose.position[1]=mario.pos[1]=lastPosition[1]=-500;step();assert(mario.action==ACT_WATER_IDLE);
    mario.flags|=MARIO_METAL_CAP;int oldEnvironmentCalls=environmentCalls;step();
    assert(mario.action==ACT_METAL_WATER_STANDING&&!(mario.action&ACT_FLAG_SWIMMING));
    assert(pose.water_mode==ROCKET_WATER_METAL&&environmentCalls==oldEnvironmentCalls+1&&resets==before);
    mario.flags&=~MARIO_METAL_CAP;step();
    assert(mario.action==ACT_WATER_IDLE&&mario.pos[1]==-500&&resets==before);
    nativeWater=gLevelValues.floorLowerLimit;step();
    assert(mario.action==ACT_IDLE&&pose.water_mode==ROCKET_WATER_DRY&&resets==before);
    const u32 hazards[]={ACT_WATER_SHOCKED,ACT_CAUGHT_IN_WHIRLPOOL,ACT_DROWNING,
        ACT_WATER_DEATH,ACT_BACKWARD_WATER_KB,ACT_FORWARD_WATER_KB,ACT_EATEN_BY_BUBBA};
    for(unsigned i=0;i<sizeof hazards/sizeof *hazards;i++){
        fresh();nativeWater=mario.waterLevel=500;step();mario.action=hazards[i];
        assert(!rocket_adapter_update(&mario)&&mario.action==hazards[i]&&!draw);
    }
    fresh();nativeWater=mario.waterLevel=500;step();mario.health=0xff;
    assert(!rocket_adapter_update(&mario)&&mario.action==ACT_WATER_IDLE&&!draw);
    // Native submerged dispatcher receives ACT_WATER_IDLE and owns drowning.
}
static void test_metal_water(void){
    fresh();mario.flags=MARIO_METAL_CAP|MARIO_CAP_ON_HEAD;mario.capTimer=500;nativeWater=mario.waterLevel=200;
    step();assert(metalWater&&draw&&mario.action==ACT_METAL_WATER_STANDING);
    assert(!(mario.action&ACT_FLAG_SWIMMING)&&mario.capTimer==500&&mario.flags&MARIO_METAL_CAP);
    pose.grounded=0;pose.velocity[1]=-540;step();assert(mario.action==ACT_METAL_WATER_FALLING);
    RocketSnapshot s;assert(rocket_adapter_interaction_snapshot(&s));
    float depth=mario.pos[1],velocity=mario.vel[1];mario.flags&=~MARIO_METAL_CAP;
    step();assert(draw&&!metalWater&&mario.action==ACT_WATER_IDLE);
    assert(mario.pos[1]==depth&&mario.vel[1]==velocity); // Never teleport on underwater expiry.
    mario.flags|=MARIO_METAL_CAP;step();assert(metalWater&&resets==1); // Legitimate recollection keeps the same world/tank.
    pose.grounded=1;pose.velocity[2]=300;step();assert(mario.action==ACT_METAL_WATER_WALKING);
    pose.position[1]=mario.waterLevel;step();assert(mario.action==ACT_IDLE);step();assert(!metalWater);
    rocket_adapter_set_selected(0);
    assert(!draw&&!metalWater&&mario.capTimer==500&&(mario.flags&MARIO_METAL_CAP));
    const u32 nativeActions[]={ACT_SHOCKED,ACT_WATER_SHOCKED,ACT_CAUGHT_IN_WHIRLPOOL,ACT_EATEN_BY_BUBBA,ACT_DROWNING,ACT_LAVA_BOOST,ACT_PUTTING_ON_CAP,ACT_STAR_DANCE_WATER};
    for(unsigned i=0;i<sizeof(nativeActions)/sizeof(nativeActions[0]);i++){
        fresh();mario.flags=MARIO_METAL_CAP;nativeWater=mario.waterLevel=200;step();mario.action=nativeActions[i];
        assert(!rocket_adapter_update(&mario)&&mario.action==nativeActions[i]&&!draw&&!metalWater);
    }
    fresh();mario.flags=MARIO_METAL_CAP;nativeWater=mario.waterLevel=200;step();sCurrPlayMode=PLAY_MODE_PAUSED;
    int count=steps;step();assert(steps==count&&metalWater);sCurrPlayMode=0;mario.health=0xff;
    assert(!rocket_adapter_update(&mario)&&!draw&&!metalWater);
    fresh();mario.flags=MARIO_METAL_CAP;nativeWater=mario.waterLevel=200;step();mario.pos[0]+=501;step();assert(resets==2&&metalWater);
    puts("PASS metal water ownership: native actions, expiry depth, recovery, surface exit, pause/death/warp and native exceptions");
}
static void test_switch_platform(void){
    fresh();step();struct Surface *floor=&surfaces[0];floor->normal.y=1;floor->object=&door;door.behavior=bhvCapSwitch;
    pose.wheel_contacts[0]=1;pose.wheel_position[0][1]=25;pose.wheel_radius[0]=25;
    assert(rocket_adapter_platform_contact(&mario,floor,0));
    door.behavior=NULL;assert(!rocket_adapter_platform_contact(&mario,floor,0));door.behavior=bhvFloorSwitchGrills;
    assert(rocket_adapter_platform_contact(&mario,floor,0));door.behavior=bhvCapSwitch;
    pose.grounded=0;assert(!rocket_adapter_platform_contact(&mario,floor,0));pose.grounded=1;
    pose.wheel_position[0][1]=80;assert(!rocket_adapter_platform_contact(&mario,floor,0));pose.wheel_position[0][1]=25;
    floor->normal.y=0;assert(!rocket_adapter_platform_contact(&mario,floor,0));floor->normal.y=1;
    pose.basis[7]=0;assert(!rocket_adapter_platform_contact(&mario,floor,0));pose.basis[7]=1;
    struct MarioState remote=mario;remote.playerIndex=1;assert(!rocket_adapter_platform_contact(&remote,floor,0));
    uiBlocked=1;assert(!rocket_adapter_platform_contact(&mario,floor,0));uiBlocked=0;
    mario.flags=MARIO_METAL_CAP;nativeWater=mario.waterLevel=200;step();assert(rocket_adapter_platform_contact(&mario,floor,0));
    assert(!door.oAction&&!door.oInteractStatus); // Supplies platform only, no switch/save unlock writes.
}
static void test_shared_cap_geometry(void){
    const BehaviorScript *caps[]={bhvWingCap,bhvMetalCap,bhvVanishCap};
    for(int i=0;i<3;i++){
        door_setup();door.behavior=caps[i];door.oInteractType=INTERACT_CAP;
        door.hitboxRadius=20;door.hitboxHeight=80;door.oPosZ=160;door.oPosY=60;
        assert(rocket_adapter_cap_pickup_contact(&pose,&door,0)); // Beyond Mario capsule.
        controller.rawStickY=0;rocket_adapter_prepare_interactions(&mario);
        assert(object.numCollidedObjs==1&&!mario.flags&&!mario.capTimer);
        door.oPosZ=230;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.oPosZ=160;
        door.oPosY=600;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.oPosY=60;
        door.hitboxRadius=NAN;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.hitboxRadius=20;
        door.oIntangibleTimer=-1;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.oIntangibleTimer=0;
        door.oInteractStatus=INT_STATUS_INTERACTED;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.oInteractStatus=0;
        door.header.gfx.activeAreaIndex=9;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.header.gfx.activeAreaIndex=0;
        door.activeFlags=0;assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0));door.activeFlags=ACTIVE_FLAG_ACTIVE;
        RocketSnapshot copy=pose;copy.basis[0]=NAN;assert(!rocket_adapter_cap_pickup_contact(&copy,&door,0));
        rocket_adapter_set_selected(0);assert(rocket_adapter_cap_pickup_contact(&copy,&door,0)==0);
        assert(rocket_adapter_cap_pickup_contact(&pose,&door,0)); // Host may be native Mario.
    }
    door_setup();door.behavior=bhvMetalCap;door.oInteractType=INTERACT_CAP;
    door_wall(2,0);surfaces[2].type=SURFACE_VANISH_CAP_WALLS;
    mario.flags=MARIO_VANISH_CAP;step();
    assert(!rocket_adapter_cap_pickup_contact(&pose,&door,0)); // Host Vanish cannot lend remote immunity.
    mario.flags=0;phaseActive=0;
    assert(rocket_adapter_cap_pickup_contact(&pose,&door,MARIO_VANISH_CAP));
    surfaces[2].type=SURFACE_DEFAULT;
    assert(!rocket_adapter_cap_pickup_contact(&pose,&door,MARIO_VANISH_CAP));
    for(int i=0;i<3;i++){
        door_setup();door.behavior=bhvExclamationBox;door.oBehParams2ndByte=i;door.oAction=2;
        door_wall(2,1);surfaces[2].object=&door;surfaces[2].normal.z=-1;
        assert(rocket_adapter_cap_box_contact(&door));
        controller.rawStickY=0;assert(!rocket_adapter_cap_box_contact(&door));
        assert(rocket_adapter_cap_box_pose_contact(&pose,&door,0)); // Host validates request geometry, not Mario attack bits.
        door.oAction=1;assert(!rocket_adapter_cap_box_pose_contact(&pose,&door,0));door.oAction=2;
        door.oExclamationBoxForce=1;assert(!rocket_adapter_cap_box_pose_contact(&pose,&door,0));door.oExclamationBoxForce=0;
        RocketSnapshot far=pose;far.position[2]-=1000;assert(!rocket_adapter_cap_box_pose_contact(&far,&door,0));
    }
    door_setup();RocketSnapshot read;assert(rocket_adapter_pickup_pose(&read)==1);
    gGlobalTimer+=2;assert(rocket_adapter_pickup_pose(&read)==-1);step();
    mario.action=ACT_READING_SIGN;assert(rocket_adapter_pickup_pose(&read)==-1);
    rocket_adapter_set_selected(0);assert(rocket_adapter_pickup_pose(&read)==0);
}
static void test_platform_mesh(void){
    fresh();struct Object platform={0};
    platform.transform[0][2]=-1;platform.transform[1][1]=1;platform.transform[2][0]=1;
    platform.transform[3][0]=100;platform.transform[3][1]=200;platform.transform[3][2]=300;
    surfaces[1].object=&platform;
    vec3s_set(surfaces[1].vertex1,120,200,290);
    vec3s_set(surfaces[1].vertex2,120,200,270);
    vec3s_set(surfaces[1].vertex3,140,200,290);
    step();assert(platformCount==1&&observedPlatform.count==1&&meshCount[1]==0);
    assert(observedPlatform.position[0]==100&&observedPlatform.basis[2]==-1);
    assert(observedTriangle.v[0][0]==10&&observedTriangle.v[0][1]==0&&observedTriangle.v[0][2]==20);
    surfaces[1].type=SURFACE_ICE;step();assert(observedTriangle.material==ROCKET_MATERIAL_VERY_SLIPPERY);
    surfaces[1].type=SURFACE_DEFAULT;step();assert(observedTriangle.material==ROCKET_MATERIAL_NORMAL);
    uint64_t id=observedPlatform.object_id;
    rocket_adapter_forget_platform(&platform);step();assert(observedPlatform.object_id!=id);id=observedPlatform.object_id;
    int calls=platformCalls;
    assert(rocket_adapter_update(&mario)==1&&platformCalls==calls); // Duplicate host frame does not submit motion.
    platform.transform[3][0]+=10;surfaces[1].vertex1[0]+=10;surfaces[1].vertex2[0]+=10;surfaces[1].vertex3[0]+=10;
    step();assert(observedPlatform.object_id==id&&observedPlatform.position[0]==110&&observedTriangle.v[0][0]==10);
    platform.oSyncID++;step();assert(observedPlatform.object_id!=id);id=observedPlatform.object_id;
    surfaces[1].flags=SURFACE_FLAG_INTANGIBLE;step();assert(platformCount==0);
    surfaces[1].flags=0;step();assert(observedPlatform.object_id!=id); // Omitted objects cannot retain prior velocity.
    surfaces[1].type=SURFACE_VANISH_CAP_WALLS;mario.flags=MARIO_VANISH_CAP;step();assert(platformCount==0);
    mario.flags=0;step();assert(platformCount==1); // Vanish removal/restoration never resurrects filtered orphan faces.
    surfaces[1].object=NULL;surfaces[1].type=SURFACE_DEFAULT;step();int orphanCalls=meshCalls[1];
    surfaces[1].type=SURFACE_ICE;step();assert(meshCalls[1]==orphanCalls+1);

}
#include "test_ccm_chimney_adapter.inc.c"
#include "test_jrb_entry_adapter.inc.c"
#include "test_pss_entry_adapter.inc.c"
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"--ccm-body")){chimney_single_case(0);return 0;}
    if(argc==2&&!strcmp(argv[1],"--ccm-wheel")){chimney_single_case(1);return 0;}
    test_pss_entry_adapter();
    test_jrb_entry_adapter();
    test_ccm_chimney_adapter();
    test_platform_mesh();test_shared_cap_geometry();test_water();
    fresh();step();assert(steps==1&&resets==1&&meshCount[0]==1&&meshCount[1]==1);assert(object.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE);
    assert(rocket_adapter_update(&mario)==1&&steps==1);step();assert(steps==2&&meshCalls[0]==1&&meshCalls[1]==1);
    surfaces[1].flags=SURFACE_FLAG_INTANGIBLE;step();assert(meshCount[1]==0&&meshCalls[1]==2);surfaces[1].flags=0;step();assert(meshCount[1]==1&&meshCalls[1]==3);
    for(int i=0;i<3;++i){surfaces[1].type=i==0?SURFACE_CAMERA_BOUNDARY:i==1?SURFACE_RAYCAST:SURFACE_INTANGIBLE;step();assert(meshCount[1]==0);}
    surfaces[1].type=SURFACE_DEFAULT;step();assert(meshCount[1]==1);
    controller.rawStickX=80;controller.rawStickY=80;controller.buttonDown=A_BUTTON|B_BUTTON|Z_TRIG;step();assert(observed.throttle==1&&observed.steer==-1&&observed.pitch==-1&&observed.roll==-1&&observed.yaw==0&&observed.powerslide&&observed.boost&&observed.jump);
    controller.rawStickX=-80;controller.rawStickY=0;controller.buttonDown=0;step();assert(observed.steer==1&&observed.yaw==1&&observed.roll==0&&observed.throttle==0);
    controller.rawStickY=80;controller.buttonDown=A_BUTTON|B_BUTTON;
    int before=steps;sCurrPlayMode=PLAY_MODE_PAUSED;step();step();assert(steps==before&&interrupts==2);sCurrPlayMode=0;step();assert(steps==before+1);
    mario.action=ACT_READING_AUTOMATIC_DIALOG;assert(!rocket_adapter_update(&mario));assert(!(object.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
    fresh();mario.freeze=2;assert(!rocket_adapter_update(&mario));assert(!resets&&!(object.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));mario.freeze=0;step();assert(resets==1);
    mario.pos[0]+=1000;step();assert(resets==2);assert(pose.position[0]==1000);
    mario.health=0xff;assert(!rocket_adapter_update(&mario));assert(!(object.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
    fresh();mario.playerIndex=1;assert(!rocket_adapter_update(&mario)&&!resets);
    fresh();enabled=0;assert(!rocket_adapter_update(&mario)&&!resets);
    fresh();nativeWater=mario.waterLevel=200;step();assert(mario.action==ACT_WATER_IDLE&&environmentCalls==1&&pose.water_mode==ROCKET_WATER_JET);
    fresh();step();gCurrLevelNum++;step();assert(resets==2);
    fresh();step();assert(mario.faceAngle[1]==0);
    pose.basis[0]=1;pose.basis[2]=0;step();assert(abs(mario.faceAngle[1]-0x4000)<=1);
    pose.basis[0]=0;pose.basis[2]=-1;step();assert(abs((u16)mario.faceAngle[1]-0x8000)<=1);
    pose.basis[0]=-1;pose.basis[2]=0;step();assert(abs((u16)mario.faceAngle[1]-0xc000)<=1);
    fresh();assert(rocket_adapter_switch_reason());step();
    for(int i=0;i<4;i++)pose.wheel_contacts[i]=1;
    assert(!rocket_adapter_switch_reason());pose.wheel_contacts[3]=0;assert(rocket_adapter_switch_reason());pose.wheel_contacts[3]=1;
    pose.flipping=1;assert(rocket_adapter_switch_reason());pose.flipping=0;
    pose.basis[7]=.97f;assert(rocket_adapter_switch_reason());pose.basis[7]=1;
    pose.velocity[0]=61;assert(rocket_adapter_switch_reason());pose.velocity[0]=0;
    pose.angular_velocity[2]=.21f;assert(rocket_adapter_switch_reason());pose.angular_velocity[2]=0;
    pose.velocity[1]=NAN;assert(rocket_adapter_switch_reason());pose.velocity[1]=0;
    int oldSteps=steps,oldResets=resets;rocket_adapter_set_selected(1);step();assert(resets==oldResets);
    rocket_adapter_set_selected(0);assert(!draw&&!(object.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
    assert(!rocket_adapter_update(&mario)&&steps==oldSteps+1);assert(rocket_adapter_switch_reason());
    rocket_adapter_set_selected(1);step();assert(resets==oldResets+1);
    fresh();step();int count=meshCalls[0];surfaces[0].type=SURFACE_ICE;step();assert(meshCalls[0]==count+1);
    surfaces[0].type=SURFACE_DEFAULT;step();assert(meshCalls[0]==count+2);
    count=environmentCalls;sCurrPlayMode=PLAY_MODE_PAUSED;assert(rocket_adapter_update(&mario)==1&&environmentCalls==count);
    sCurrPlayMode=0;step();assert(environmentCalls==count+1);
    test_doors();
    test_interaction_snapshot();
    test_body_snapshot();
    test_vanish();test_vanish_environment_recovery();test_vanish_progression();
    test_metal_water();test_switch_platform();
    puts("PASS real host adapter: collision/dedup, controls, pause, reset/doors, selective Vanish/full-body recovery, blue switch/box detection and native pickups/stars");
    return 0;
}
