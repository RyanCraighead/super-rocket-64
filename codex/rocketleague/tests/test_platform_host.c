/* Actual car-load/character ingress and native seesaw/pendulum behavior.
 * Level surface query, audio and object sync transport are explicit fixtures. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sm64.h"
#include "game/behavior_actions.h"
#include "object_constants.h"
#include "game/object_helpers.h"
#include "engine/math_util.h"
#include "level_table.h"
#include "../../../src/game/rocket_platform.c"
#include "../../../src/game/platform_displacement.c"
struct CLIOptions gCLIOpts;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct MarioState gMarioStates[MAX_PLAYERS];
struct Object *gCurrentObject;
struct Area *gCurrentArea;
bool gNetworkAreaLoaded;
s16 gTTCSpeedSetting;
u32 gTimeStopState;
const BehaviorScript bhvSeesawPlatform[]={6},bhvSwingPlatform[]={7},bhvTTC2DRotator[]={8},
    bhvTTCCog[]={9},bhvTTCElevator[]={10},bhvTTCMovingBar[]={11},bhvTTCPendulum[]={12},
    bhvTTCPitBlock[]={13},bhvTTCRotatingSolid[]={14},bhvTTCSpinner[]={15},
    bhvCapSwitch[]={16},bhvFloorSwitchHiddenObjects[]={17},bhvFloorSwitchAnimatesObject[]={18},
    bhvFloorSwitchGrills[]={19};
const BehaviorScript bhvWdwSquareFloatingPlatform[]={1},bhvWdwRectangularFloatingPlatform[]={2},
    bhvJrbFloatingPlatform[]={3},bhvLllTiltingInvertedPyramid[]={4},bhvBitfsTiltingInvertedPyramid[]={5};
static struct Object platform,players[2];
static struct Surface floorSurface;
static struct Area area;
static struct SyncObject syncState;
static RocketSnapshot localCar;
static int localSelected,localActive,returns,checks,forgotten,phaseFloor;
static struct Object lowerPlatform;
static struct Surface lowerFloor;
void rocket_adapter_forget_platform(struct Object *object){(void)object;forgotten++;}
void rocket_caps_clear(unsigned index){(void)index;}
void network_coin_boost_clear(unsigned index){(void)index;}
int rocket_adapter_platform_contact(struct MarioState *m,struct Surface *floor,float height){(void)m;(void)floor;(void)height;return 0;}
static double now;
void platform_test_write(struct Packet *packet,struct Object *object);
void platform_test_read(struct Packet *packet,struct Object *object);
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);assert(x);}}while(0)
int rocket_adapter_car_selected(void){return localSelected;}
int rocket_adapter_platform_snapshot(RocketSnapshot *out){if(!localActive)return 0;*out=localCar;return 1;}
f64 clock_elapsed_f64(void){return now;}
u8 is_player_active(struct MarioState *m){return m->marioObj&&m->health>=0x100&&(m->playerIndex==0||gNetworkPlayers[m->playerIndex].connected);}
struct MarioState *get_mario_state_from_object(struct Object *object){for(int i=0;i<2;i++)if(gMarioStates[i].marioObj==object)return &gMarioStates[i];return NULL;}
struct SyncObject *sync_object_get(u32 id){return id==1?&syncState:NULL;}
struct SyncObject *sync_object_init(struct Object *object,float distance){syncState.o=object;object->oSyncID=1;syncState.maxSyncDistance=distance;return &syncState;}
void sync_object_init_field_with_size(struct Object *object,void *field,u8 size){(void)object;unsigned i=syncState.extraFieldCount++;assert(i<64);syncState.extraFields[i]=field;syncState.extraFieldsSizeBytes[i]=size;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **surface){
    if(phaseFloor)CHECK(gCurrentObject==&players[0]||gCurrentObject==&players[1]);
    if(phaseFloor&&gCurrentObject==&players[1]&&(gMarioStates[1].flags&MARIO_VANISH_CAP)){*surface=&lowerFloor;return platform.oPosY-8;}
    *surface=NULL;if(fabsf(x)>400||fabsf(z)>500||y<platform.oPosY-1)return -11000;
    *surface=&floorSurface;return platform.oPosY;
}
void cur_obj_play_sound_1(s32 sound){(void)sound;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
u16 random_u16(void){return 1;}
f32 random_float(void){return .5f;}
static RocketSnapshot car(float z){
    RocketSnapshot c={0};c.position[1]=35;c.position[2]=z;c.basis[2]=c.basis[3]=c.basis[7]=1;c.grounded=1;
    for(int i=0;i<4;i++){c.wheel_contacts[i]=1;c.wheel_radius[i]=32;c.wheel_position[i][0]=(i&1)?70:-70;c.wheel_position[i][1]=32;c.wheel_position[i][2]=z+((i&2)?90:-90);}
    return c;
}
static void remote(float z,unsigned sequence){
    CharacterNetState state={0},decoded;state.sequence=sequence;state.epoch=1;state.kind=CNET_OCTANE;state.active=1;state.interaction=1;state.car=car(z);
    uint8_t wire[CNET_WIRE_SIZE];CHECK(character_net_encode(wire,sizeof wire,&state));CHECK(character_net_decode(&decoded,wire,sizeof wire));
    CHECK(character_net_accept(1,&decoded));
}
static void fresh(void){
    memset(&gCLIOpts,0,sizeof gCLIOpts);memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    memset(&platform,0,sizeof platform);memset(players,0,sizeof players);memset(&syncState,0,sizeof syncState);memset(loads,0,sizeof loads);
    memset(&floorSurface,0,sizeof floorSurface);character_net_clear_all();gNetworkAreaLoaded=true;gCLIOpts.characterNet=true;
    gCurrentArea=&area;area.index=1;platform.header.gfx.activeAreaIndex=1;platform.activeFlags=ACTIVE_FLAG_ACTIVE;
    platform.behavior=bhvSeesawPlatform;syncState.behavior=(void*)platform.behavior;
    phaseFloor=forgotten=0;lowerPlatform=platform;lowerPlatform.oSyncID=2;lowerFloor=floorSurface;lowerFloor.object=&lowerPlatform;lowerFloor.normal.y=1;
    floorSurface.object=&platform;floorSurface.normal.y=1;gCurrentObject=&platform;platform.oSyncID=1;syncState.o=&platform;
    gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=gNetworkPlayerLocal;
    for(int i=0;i<2;i++){gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&players[i];gMarioStates[i].health=0x880;
        gNetworkPlayers[i].localIndex=gNetworkPlayers[i].globalIndex=i;gNetworkPlayers[i].connected=true;
        gNetworkPlayers[i].currAreaIndex=1;gNetworkPlayers[i].currLevelSyncValid=gNetworkPlayers[i].currAreaSyncValid=gNetworkPlayers[i].currPositionValid=true;}
    localCar=car(150);localSelected=localActive=1;now=1;returns=0;
}
/* Exact native frame/allocation functions, with inert scheduling services. */
struct ObjectNode gObjectListArray[NUM_OBJ_LISTS],gFreeObjectList;
struct ObjectNode *gObjectLists;
struct Object *gMarioObject;
u32 gObjectCounter;
s16 gPrevFrameObjectCount,gNumRoomedObjectsInMarioRoom,gNumRoomedObjectsNotInMarioRoom,gCheckingSurfaceCollisionsForCamera,gCurrLevelNum;
static float expectedBeforeTerrain=-1;
static unsigned clears,terrainUpdates;
s64 get_current_clock(void){return 0;}
s64 get_clock_difference(s64 cycles){return cycles;}
void reset_debug_objectinfo(void){}
void stub_debug_5(void){}
void clear_dynamic_surfaces(void){float center[3];clears++;if(expectedBeforeTerrain>=0)CHECK(rocket_platform_load(&platform,center)==expectedBeforeTerrain);}
void update_terrain_objects(void){float center[3];terrainUpdates++;if(expectedBeforeTerrain>=0)CHECK(rocket_platform_load(&platform,center)==expectedBeforeTerrain);}
void detect_object_collisions(void){}
void update_non_terrain_objects(void){}
void spiderman_combat_host_present_objects(void){}
void unload_deactivated_objects(void){}
void try_print_debug_mario_object_info(void){}
struct Object *try_allocate_object(struct ObjectNode *dest,struct ObjectNode *freeList){(void)dest;(void)freeList;return &platform;}
void unload_object(struct Object *object){(void)object;assert(0);}
void rocket_enemy_forget(struct Object *object){(void)object;}
void rocket_bobomb_forget(struct Object *object){(void)object;}
void rocket_contacts_forget(struct Object *object){(void)object;}
void rocket_whomp_forget(struct Object *object){(void)object;}
void rocket_switch_forget(struct Object *obj){(void)obj;}
#include "platform_native_lifecycle.inc"
int main(void){
    float center[3];fresh();rocket_platform_refresh();CHECK(players[0].platform==&platform);
    CHECK(rocket_platform_load(&platform,center)==2&&fabsf(center[2]-150)<.01f);
    bhv_seesaw_platform_update();float single=platform.oSeesawPlatformPitchVel;CHECK(single>0&&single<=50);
    remote(150,1);rocket_platform_refresh();platform.oSeesawPlatformPitchVel=0;bhv_seesaw_platform_update();
    CHECK(rocket_platform_load(&platform,center)==4);CHECK(fabsf(platform.oSeesawPlatformPitchVel-single*2)<.01f);
    remote(-150,2);rocket_platform_refresh();platform.oSeesawPlatformPitchVel=0;bhv_seesaw_platform_update();CHECK(fabsf(platform.oSeesawPlatformPitchVel)<.01f);
    now+=.201;rocket_platform_refresh();CHECK(players[1].platform==NULL&&rocket_platform_load(&platform,center)==2);
    localCar.grounded=0;rocket_platform_refresh();CHECK(!players[0].platform);platform.oFaceAnglePitch=1000;platform.oSeesawPlatformPitchVel=0;bhv_seesaw_platform_update();CHECK(platform.oSeesawPlatformPitchVel==-3);
    fresh();localCar=car(800);rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==0); // Outside platform.
    fresh();for(int i=0;i<4;i++)localCar.wheel_position[i][1]+=100;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==0); // Hover.
    fresh();floorSurface.flags=SURFACE_FLAG_INTANGIBLE;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==0);
    fresh();floorSurface.normal.y=-1;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==0);
    fresh();platform.header.gfx.activeAreaIndex=2;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==0);
    fresh();rocket_platform_refresh();platform.oSyncID++;CHECK(rocket_platform_load(&platform,center)==0); // Object slot reuse.
    fresh();rocket_platform_refresh();localActive=0;rocket_platform_refresh();CHECK(!players[0].platform&&rocket_platform_load(&platform,center)==0);
    fresh();remote(150,1);rocket_platform_refresh();gNetworkPlayers[1].connected=false;rocket_platform_refresh();CHECK(!players[1].platform&&rocket_platform_load(&platform,center)==2);
    fresh();remote(150,1);rocket_platform_refresh();gNetworkPlayers[1].currAreaIndex=2;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);
    fresh();remote(150,1);rocket_platform_refresh();gNetworkPlayers[1].currLevelAreaSeqId++;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);
    fresh();remote(150,1);rocket_platform_refresh();character_net_clear(1);rocket_platform_refresh();CHECK(!players[1].platform&&rocket_platform_load(&platform,center)==2);
    /* A packet accepted between frames invalidates the old tire locations
     * before the next native terrain update (the normal-frame refresh hook). */
    fresh();remote(150,1);rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==4);
    CharacterNetState incoming={0};incoming.sequence=2;incoming.epoch=1;incoming.kind=CNET_OCTANE;
    incoming.active=CNET_DRIVING;incoming.interaction=1;incoming.car=car(150);incoming.car.grounded=0;
    CHECK(character_net_accept(1,&incoming));expectedBeforeTerrain=2;update_objects(0);expectedBeforeTerrain=-1;CHECK(clears==1&&terrainUpdates==1&&rocket_platform_load(&platform,center)==2);
    incoming.sequence++;incoming.car=car(800);CHECK(character_net_accept(1,&incoming));rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);
    incoming.sequence++;incoming.car=car(150);incoming.active=CNET_PRESENTATION;incoming.interaction=0;
    CHECK(character_net_accept(1,&incoming));rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);
    incoming.sequence++;incoming.active=CNET_DRIVING; /* UI focus suppresses attacks, not parked tire weight. */
    CHECK(character_net_accept(1,&incoming));rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==4);
    CharacterNetState strict;CHECK(!character_net_interaction_snapshot(1,&strict));
    gMarioStates[1].freeze=1;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);gMarioStates[1].freeze=0;
    gMarioStates[1].heldByObj=&platform;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);gMarioStates[1].heldByObj=NULL;
    gMarioStates[1].health=0;rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==2);
    fresh();remote(150,1);rocket_platform_refresh();rocket_platform_forget(&platform);
    CHECK(!players[0].platform&&!players[1].platform&&forgotten==1&&rocket_platform_load(&platform,center)==0);
    /* Exact pointer/behavior/sync tuple reuse cannot retain old cached tire loads. */
    CHECK(platform.oSyncID==1&&platform.behavior==bhvSeesawPlatform);rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==4);
    CHECK(allocate_object(&gObjectListArray[OBJ_LIST_SURFACE])==&platform);
    platform.oSyncID=1;platform.behavior=bhvSeesawPlatform;platform.header.gfx.activeAreaIndex=1;
    CHECK(!players[0].platform&&!players[1].platform&&rocket_platform_load(&platform,center)==0);
    rocket_platform_refresh();CHECK(rocket_platform_load(&platform,center)==4);
    phaseFloor=1;gMarioStates[1].flags=MARIO_VANISH_CAP;rocket_platform_refresh();CHECK(gCurrentObject==&platform);
    CHECK(players[0].platform==&platform&&players[1].platform==&lowerPlatform);
    CHECK(rocket_platform_load(&platform,center)==2&&rocket_platform_load(&lowerPlatform,center)==2);
    rocket_platform_forget(&platform);CHECK(rocket_platform_load(&lowerPlatform,center)==2); // Other object's support survives.
    const BehaviorScript *switches[]={bhvCapSwitch,bhvFloorSwitchHiddenObjects,bhvFloorSwitchAnimatesObject,bhvFloorSwitchGrills};
    for(unsigned i=0;i<sizeof switches/sizeof *switches;i++){
        fresh();platform.behavior=switches[i];update_mario_platform();CHECK(players[0].platform==&platform);
    }
    fresh();rocket_platform_refresh();CHECK(rocket_platform_begin(&platform));CHECK(syncState.extraFieldCount==6&&rocket_platform_managed(&syncState));
    CHECK(rocket_platform_begin(&platform)&&syncState.extraFieldCount==6);CHECK(rocket_platform_accept(&syncState,0)&&!rocket_platform_accept(&syncState,1));
    /* The authority's exact pose and native load integrator travel over the
     * native object field channel, not a competing character extension. */
    syncState.hasStandardFields=true;sync_object_init_field(&platform,platform.oSeesawPlatformPitchVel);
    platform.oPosY=88;platform.oFaceAnglePitch=-1234;platform.oSeesawPlatformPitchVel=35;
    struct Packet packet={0};platform_test_write(&packet,&platform);CHECK(!packet.error&&!packet.writeError);
    platform.oPosY=0;platform.oFaceAnglePitch=0;platform.oSeesawPlatformPitchVel=0;
    packet.cursor=0;platform_test_read(&packet,&platform);CHECK(!packet.error&&packet.cursor==packet.dataLength);
    CHECK(platform.oPosY==88&&platform.oFaceAnglePitch==-1234&&platform.oSeesawPlatformPitchVel==35);
    gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;CHECK(!rocket_platform_begin(&platform));
    CHECK(!rocket_platform_accept(&syncState,0)&&rocket_platform_accept(&syncState,1));
    platform.oSeesawPlatformPitchVel=12;platform.oFaceAnglePitch=100;bhv_seesaw_platform_update();CHECK(platform.oFaceAnglePitch==100&&platform.oSeesawPlatformPitchVel==12);
    gNetworkPlayers[1].connected=false;CHECK(rocket_platform_begin(&platform)); // Authority recovery.
    gCLIOpts.characterNet=false;CHECK(rocket_platform_begin(&platform));
    fresh();rocket_platform_refresh();platform.oVelX=50;platform.oVelZ=60;gMarioStates[0].pos[0]=10;apply_platform_displacement(&players[0],&platform);CHECK(gMarioStates[0].pos[0]==10); // No second car displacement.
    fresh();gTTCSpeedSetting=TTC_SPEED_FAST;platform.oTTCPendulumAngle=1000;platform.oTTCPendulumAngleAccel=22;platform.oTTCPendulumAccelDir=1;
    bhv_ttc_pendulum_update();CHECK(platform.oTTCPendulumAngle==978&&platform.oFaceAngleRoll==978);
    gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;bhv_ttc_pendulum_update();CHECK(platform.oFaceAngleRoll==978);
    fresh();localSelected=localActive=0;players[0].platform=players[1].platform=&platform;players[0].oPosZ=100;players[1].oPosZ=200;
    CHECK(rocket_platform_load(&platform,center)==1&&center[2]==150); // Native-only average unchanged.
    printf("platform native/load/network lifecycle: %d checks passed (explicit host fixtures)\n",checks);return 0;
}
