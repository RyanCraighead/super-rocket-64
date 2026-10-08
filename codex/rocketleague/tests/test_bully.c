/* Real native interaction, object stepping, floor death and Bully reward code.
 * Flat ring geometry, rendering/audio and transport are explicit boundaries. */
#define ROCKET_BULLY_REAL_TEST
#define main incoming_regression_main
#include "test_incoming_native.c"
#undef main
#include "../../../src/game/rocket_bully.c"
#include "pc/rocket_bindings.h"
#include "game/obj_behaviors.h"
#include "game/behavior_actions.h"
#include "game/hardcoded.h"
#define o gCurrentObject
struct Object *gCurrentObject;
struct CLIOptions gCLIOpts;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal;
enum NetworkType gNetworkType;
bool gNetworkAreaLoaded,gNetworkAreaSyncing;
struct BehaviorValues gBehaviorValues;
static struct Area area;
static struct Surface *sObjFloor;
static RocketInput mapped;
static int inputReady=1,visible=1,authority=1,initialized=1,stars,coins,checks;
static struct Object coin,bridge,parent;
const BehaviorScript bhvLllTumblingBridge[]={9},bhvMovingYellowCoin[]={10},bhvBobombBullyDeathSmoke[]={11};
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Bully line %d: %s\n",__LINE__,#x);abort();}}while(0)
int rocket_adapter_read_input(RocketInput *input){*input=mapped;return inputReady;}
int rocket_adapter_enemy_visible(const float from[3],struct Object *obj,unsigned caps){(void)from;(void)obj;(void)caps;return visible;}
bool sync_object_is_initialized(u32 id){return id&&initialized;}
bool sync_object_should_own(u32 id){return id&&authority;}
void cur_obj_init_animation(s32 index){(void)index;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
struct MarioState *nearest_interacting_mario_state_to_object(struct Object *obj){CHECK(gMarioStates[0].interactObj==obj);return &gMarioStates[0];}
struct Object *nearest_player_to_object(struct Object *obj){(void)obj;return &playerObject;}
s16 obj_turn_toward_object(struct Object *obj,struct Object *target,s16 field,s16 turn){(void)obj;(void)target;(void)field;(void)turn;return 0;}
void obj_orient_graph(struct Object *obj,f32 x,f32 y,f32 z){(void)obj;(void)x;(void)y;(void)z;}
s8 obj_find_wall(f32 x,f32 y,f32 z,f32 vx,f32 vz){(void)x;(void)y;(void)z;(void)vx;(void)vz;return 1;}
f32 find_water_level(f32 x,f32 z){(void)x;(void)z;return -11000;}
void calc_new_obj_vel_and_pos_y_underwater(struct Surface *s,f32 y,f32 x,f32 z,f32 water){(void)s;(void)y;(void)x;(void)z;(void)water;CHECK(0);}
void obj_splash(s32 water,s32 y){(void)water;(void)y;}
f32 random_float(void){return .5f;}
struct Object *spawn_object(struct Object *obj,s32 model,const BehaviorScript *behavior){
    (void)obj;(void)model;if(behavior==bhvMovingYellowCoin){coins++;return &coin;}return NULL;
}
void spawn_mist_particles(void){}
struct Object *cur_obj_nearest_object_with_behavior(const BehaviorScript *behavior){CHECK(behavior==bhvLllTumblingBridge);return &bridge;}
struct Object *spawn_networked_default_star(f32 x,f32 y,f32 z,u8 index){CHECK(x==1000&&y==2000&&z==3000&&index==7);stars++;return NULL;}
#include "bully-native.inc.c"
static struct MarioState *start(int big){
    struct MarioState *m=fresh();memset(&bullyCar,0,sizeof bullyCar);memset(&mapped,0,sizeof mapped);
    memset(&area,0,sizeof area);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);memset(&gCLIOpts,0,sizeof gCLIOpts);
    memset(&floorObject,0,sizeof floorObject);memset(&lavaFloor,0,sizeof lavaFloor);memset(&parent,0,sizeof parent);
    floorObject.normal.y=lavaFloor.normal.y=1;lavaFloor.type=SURFACE_BURNING;
    m->area=&area;area.index=1;m->marioObj->header.gfx.areaIndex=1;
    bullyCar.basis[2]=bullyCar.basis[3]=bullyCar.basis[7]=1;bullyCar.grounded=1;
    bullyCar.velocity[2]=300;mapped.throttle=1;bullyCarActive=inputReady=visible=authority=initialized=1;
    gNetworkAreaLoaded=gNetworkAreaSyncing=0;gNetworkType=NT_NONE;gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkPlayerLocal->globalIndex=7;gCLIOpts.offline=1;
    enemy.oInteractType=INTERACT_BULLY;enemy.hitboxRadius=big?115:73;enemy.hitboxHeight=big?235:123;
    enemy.oPosY=0;enemy.oPosZ=190;enemy.oBehParams2ndByte=big?BULLY_BP_SIZE_BIG:BULLY_BP_SIZE_SMALL;
    enemy.oAction=BULLY_ACT_CHASE_MARIO;enemy.oGravity=big?5:4;enemy.oFriction=big?.93f:.91f;
    enemy.oSyncID=1;enemy.parentObj=&parent;gCurrentObject=&enemy;stars=coins=bullyRingFloor=0;
    gLevelValues.floorLowerLimitMisc=-11000;
    gLevelValues.starPositions.BigBullyTrioStarPos[0]=1000;gLevelValues.starPositions.BigBullyTrioStarPos[1]=2000;gLevelValues.starPositions.BigBullyTrioStarPos[2]=3000;
    bridge.oIntangibleTimer=-1;
    return m;
}
static void test_intent(void){
    for(int big=0;big<2;big++){
        struct MarioState *m=start(big);CHECK(rocket_bully_ram(m,&enemy));
        CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(m->action==ACT_IDLE&&!actions&&!m->invincTimer&&!m->hurtCounter);
        CHECK(enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|ATTACK_FAST_ATTACK));
        CHECK(fabsf(enemy.oForwardVel-3392.f/enemy.hitboxRadius)<.001f&&enemy.oMoveAngleYaw==0);
        bully_check_mario_collision();CHECK(enemy.oAction==BULLY_ACT_KNOCKBACK&&enemy.oBullyLastNetworkPlayerIndex==7);
        for(int which=0;which<16;which++){
            m=start(big);
            switch(which){case 0:mapped.throttle=.2f;break;case 1:mapped.throttle=-1;break;
                case 2:bullyCar.velocity[2]=179.99f;break;case 3:bullyCar.velocity[2]=0;break;
                case 4:enemy.oPosZ=-100;break;case 5:enemy.oPosX=190;enemy.oPosZ=40;break;
                case 6:bullyCar.velocity[0]=600;break;case 7:enemy.oForwardVel=11;break;
                case 8:bullyCar.grounded=0;break;case 9:visible=0;break;
                case 10:inputReady=0;break;case 11:bullyCarActive=0;break;
                case 12:enemy.oPosZ=800;break;case 13:enemy.oPosY=500;break;
                case 14:bullyCar.velocity[2]=NAN;break;case 15:enemy.oForwardVel=NAN;break;}
            CHECK(!rocket_bully_ram(m,&enemy));
            if(which<8){CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1&&m->action!=ACT_IDLE&&m->invincTimer==2);}
        }
    }
    struct MarioState *m=start(1);bullyCar.velocity[2]=180;enemy.oForwardVel=2;CHECK(rocket_bully_ram(m,&enemy));
    enemy.oForwardVel=2.001f;CHECK(!rocket_bully_ram(m,&enemy)); // Relative closing120 boundary.
    enemy.oForwardVel=30;enemy.oMoveAngleYaw=0x8000;CHECK(rocket_bully_ram(m,&enemy));
    bullyCar.velocity[2]=0;CHECK(!rocket_bully_ram(m,&enemy)); // Incoming speed alone grants no attack.
    m=start(1);mapped.throttle=.201f;CHECK(rocket_bully_ram(m,&enemy));
    RocketBindings bindings=rocket_default_bindings;bindings.action[RA_THROTTLE]=RB_RB;
    RocketGamepad pad={.connected=1,.isolated=1};RocketPadSample raw={.buttons=1u<<10};RocketInput keyboard={0};
    rocket_bindings_apply(&bindings,&raw,&pad);mapped=rocket_gamepad_merge(&keyboard,&pad);CHECK(rocket_bully_ram(m,&enemy));
    bindings.action[RA_THROTTLE]=RB_RT;raw.buttons=0;raw.right_trigger=15000;
    rocket_bindings_apply(&bindings,&raw,&pad);mapped=rocket_gamepad_merge(&keyboard,&pad);CHECK(rocket_bully_ram(m,&enemy));
    raw.right_trigger=4000;rocket_bindings_apply(&bindings,&raw,&pad);mapped=rocket_gamepad_merge(&keyboard,&pad);CHECK(!rocket_bully_ram(m,&enemy));
    m=start(1);bullyCarActive=0;m->action=ACT_PUNCHING;m->flags|=MARIO_PUNCHING;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(enemy.oInteractStatus&INT_STATUS_WAS_ATTACKED); // Mario retained.
    m=start(1);bullyCar.velocity[2]=0;m->flags|=MARIO_METAL_CAP;CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(!actions);
}
static void test_authority(void){
    for(int client=0;client<2;client++)for(int gate=0;gate<8;gate++){
        struct MarioState *m=start(1);gCLIOpts.offline=0;gCLIOpts.characterNet=1;gNetworkType=client?NT_CLIENT:NT_SERVER;
        gNetworkAreaLoaded=1;gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=1;
        switch(gate){case 1:authority=0;break;case 2:initialized=0;break;case 3:gNetworkAreaSyncing=1;break;
            case 4:gNetworkPlayerLocal->currAreaSyncValid=0;break;case 5:gNetworkPlayerLocal->currLevelSyncValid=0;break;
            case 6:m->playerIndex=1;break;case 7:enemy.oSyncID=0;break;}
        CHECK(rocket_bully_ram(m,&enemy)==(gate==0));CHECK(!enemy.oInteractStatus&&!actions&&!sends);
    }
    struct MarioState *m=start(1);gNetworkType=NT_SERVER;CHECK(rocket_bully_ram(m,&enemy)); // Standalone marker.
}
static void test_native_ring_out(void){
    for(int big=0;big<2;big++)for(int minion=0;minion<2;minion++){
        struct MarioState *m=start(big);if(minion&&!big)enemy.oBullySubtype=BULLY_STYPE_MINION;
        CHECK(interact_bully(m,INTERACT_BULLY,&enemy));bully_check_mario_collision();bullyRingFloor=1;
        for(int frame=0;frame<20&&enemy.oAction!=BULLY_ACT_LAVA_DEATH;frame++){
            enemy.oBullyPrevX=enemy.oPosX;enemy.oBullyPrevZ=enemy.oPosZ;bully_act_knockback();bully_step();
        }
        CHECK(enemy.oAction==BULLY_ACT_LAVA_DEATH&&enemy.oPosZ>=200);
        CHECK(!rocket_bully_ram(m,&enemy));
        for(int timer=0;timer<=31;timer++){enemy.oTimer=timer;bully_act_level_death();}
        CHECK(!enemy.activeFlags&&stars==big&&coins==!big);
        CHECK(parent.oBullyKBTimerAndMinionKOCounter==(!big&&minion));
        if(big)CHECK(bridge.oIntangibleTimer==0&&sends==1);
        else CHECK(coin.oForwardVel==10&&coin.oVelY==100);
    }
}
int main(void){test_intent();test_authority();test_native_ring_out();printf("PASS Bully: %d native attack/stun, mapped intent, relative geometry, authority and real native ring-out/reward checks\n",checks);return 0;}
