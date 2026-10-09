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
s16 gCurrLevelNum,sCurrPlayMode;
u32 gTimeStopState;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal;
enum NetworkType gNetworkType;
bool gNetworkAreaLoaded,gNetworkAreaSyncing;
struct BehaviorValues gBehaviorValues;
static struct Area area;
static struct Surface *sObjFloor;
static RocketInput mapped;
static int inputReady=1,visible=1,authority=1,initialized=1,stars,coins,checks,presented;
static struct Object coin,bridge,parent;
const BehaviorScript bhvLllTumblingBridge[]={9},bhvMovingYellowCoin[]={10},bhvBobombBullyDeathSmoke[]={11};
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Bully line %d: %s\n",__LINE__,#x);abort();}}while(0)
int rocket_adapter_read_input(RocketInput *input){*input=mapped;return inputReady;}
int rocket_adapter_car_selected(void){return bullyCarActive;}
int character_presentation_car_snapshot(RocketSnapshot *car){if(!presented)return 0;*car=bullyCar;return 1;}
int rocket_runtime_bump(const float delta[3]){
    CHECK(delta[1]==0&&hypotf(delta[0],delta[2])<=2400.1f);
    for(int k=0;k<3;k++)bullyCar.velocity[k]+=delta[k];return 1;
}
int rocket_adapter_enemy_visible(const float from[3],struct Object *obj,unsigned caps){(void)from;(void)obj;(void)caps;return visible;}
bool sync_object_is_initialized(u32 id){return id&&initialized;}
bool sync_object_should_own(u32 id){return id&&authority;}
void cur_obj_init_animation(s32 index){(void)index;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
struct MarioState *nearest_interacting_mario_state_to_object(struct Object *obj){CHECK(gMarioStates[0].interactObj==obj);return &gMarioStates[0];}
struct Object *nearest_player_to_object(struct Object *obj){(void)obj;return &playerObject;}
s16 obj_turn_toward_object(struct Object *obj,struct Object *target,s16 field,s16 turn){(void)obj;(void)target;(void)field;(void)turn;return 0;}
void obj_orient_graph(struct Object *obj,f32 x,f32 y,f32 z){(void)obj;(void)x;(void)y;(void)z;}
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
    rocket_bully_forget(&enemy);
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
    bullyWallZ=100000;bullyWall.normal.z=-1;bullySideWall=bullyFloorMode=presented=0;
    gCurrLevelNum=LEVEL_LLL;sCurrPlayMode=PLAY_MODE_NORMAL;gTimeStopState=0;
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
static void test_outgoing_overlap_does_not_stun(void){
    struct MarioState *m=start(0);enemy.oPosZ=170;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));bully_check_mario_collision();
    bully_step(); // Native stepping plus the new bounded separation clears ordinary contact.
    float cleared[3]={enemy.oPosX,enemy.oPosY-enemy.hitboxDownOffset,enemy.oPosZ};
    CHECK(!rocket_body_overlaps_cylinder(&bullyCar,cleared,enemy.hitboxRadius,enemy.hitboxHeight));
    enemy.oPosZ=170; // Controlled unresolved-contact case (e.g. a blocking wall).
    gGlobalTimer++;bullyCar.position[2]+=10;m->pos[2]+=10;
    float bottom[3]={enemy.oPosX,enemy.oPosY-enemy.hitboxDownOffset,enemy.oPosZ};
    CHECK(rocket_body_overlaps_cylinder(&bullyCar,bottom,enemy.hitboxRadius,enemy.hitboxHeight));
    CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));
    CHECK(!actions&&!m->invincTimer&&m->action==ACT_IDLE);
}
static void test_contact_lifetime(void){
    struct MarioState *m=start(0);enemy.oPosZ=170;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));bully_check_mario_collision();
    for(int frame=0;frame<300;frame++){
        gGlobalTimer++;rocket_bully_prepare();
        CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));
        CHECK(!actions&&!m->invincTimer&&!(enemy.oInteractStatus&INT_STATUS_INTERACTED));
    }
    enemy.oPosZ=500;gGlobalTimer++;rocket_bully_prepare();
    CHECK(!rocket_bully_repeat(m,&enemy));
    enemy.oPosZ=170;enemy.oForwardVel=0;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(enemy.oInteractStatus&INT_STATUS_INTERACTED);
    bully_check_mario_collision();CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));
    CHECK(!(enemy.oInteractStatus&INT_STATUS_INTERACTED)); // Duplicate same native frame.
    m=start(0);mapped.throttle=0;bullyCar.velocity[2]=0;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1&&m->invincTimer==2);
    // Presentation carries the actual native hurt pose across the prepass.
    presented=1;gGlobalTimer++;rocket_bully_prepare();presented=0;
    CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1);
    m->action=ACT_IDLE;m->invincTimer=0;gInteractionInvulnerable=0;
    for(int frame=0;frame<90;frame++){gGlobalTimer++;rocket_bully_prepare();CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1);}
    enemy.oPosZ=500;gGlobalTimer++;rocket_bully_prepare();enemy.oPosZ=170;
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==2); // Distinct incoming collision.
    // Immunity must not consume a contact before it has any native consequence.
    for(int kind=0;kind<3;kind++){
        m=start(0);mapped.throttle=0;
        if(kind==0)gInteractionInvulnerable=1;
        if(kind==1)m->flags|=MARIO_VANISH_CAP;
        if(kind==2)enemy.oInteractionSubtype|=INT_SUBTYPE_DELAY_INVINCIBILITY;
        CHECK(!interact_bully(m,INTERACT_BULLY,&enemy));CHECK(!history(&enemy)&&!actions);
        gInteractionInvulnerable=0;m->flags&=~MARIO_VANISH_CAP;enemy.oInteractionSubtype=0;
        CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1);
    }
}
static void test_fast_native_order(void){
    const float speeds[]={300,1200,2400,4600};
    for(int big=0;big<2;big++)for(unsigned speed=0;speed<4;speed++){
        struct MarioState *m=start(big);enemy.oPosZ=170;bullyCar.velocity[2]=speeds[speed];
        CHECK(interact_bully(m,INTERACT_BULLY,&enemy));
        // Native order: interaction, car physics, then the Bully behavior.
        // Bounded correction may need two frames for an initially deep overlap.
        int clearFrames=0;
        for(int frame=0;frame<12;frame++){
            gGlobalTimer++;rocket_bully_prepare();
            if(rocket_bully_car_contact(m,&enemy))interact_bully(m,INTERACT_BULLY,&enemy);
            bullyCar.position[2]+=bullyCar.velocity[2]/30;m->pos[2]=bullyCar.position[2];
            bully_check_mario_collision();bully_act_knockback();bully_step();
            CHECK(enemy.oPosY==0&&!actions&&enemy.oPosZ>bullyCar.position[2]);
            float bottom[3]={enemy.oPosX,enemy.oPosY,enemy.oPosZ};
            clearFrames+=!rocket_body_overlaps_cylinder(&bullyCar,bottom,enemy.hitboxRadius,enemy.hitboxHeight);
        }
        CHECK(clearFrames>=8); // A deep overlap is cleared; brief later intake stays bounded.
        CHECK(enemy.oPosZ>bullyCar.position[2]); // No pass through/stack on the chassis.
    }
}
static void test_boundaries(void){
    struct MarioState *m;
    for(int condition=0;condition<10;condition++){
        m=start(0);rocket_bully_record(m,&enemy,1);CHECK(history(&enemy));
        switch(condition){
            case 0:rocket_bully_forget(&enemy);break;
            case 1:enemy.behavior=bhvBobomb;break;
            case 2:enemy.oSyncID++;break;
            case 3:gCurrLevelNum++;break;
            case 4:area.index++;break;
            case 5:bullyCarActive=0;break;
            case 6:enemy.oAction=BULLY_ACT_LAVA_DEATH;break;
            case 7:enemy.oAction=BULLY_ACT_DEATH_PLANE_DEATH;break;
            case 8:enemy.oSyncDeath=1;break;
            case 9:m->health=0;break;
        }
        gGlobalTimer++;rocket_bully_prepare();CHECK(!rocket_bully_repeat(m,&enemy)&&!history(&enemy));
        if(condition>=6&&condition<=8)CHECK(!rocket_incoming_overlap(&playerObject,&enemy,0));
    }
    // A same-level warp is rearmed by the measured pose gap, including native presentation.
    m=start(0);rocket_bully_record(m,&enemy,0);m->action=ACT_BACKWARD_GROUND_KB;
    presented=1;bullyCar.position[2]=1000;gGlobalTimer++;rocket_bully_prepare();CHECK(!history(&enemy));
    // An above-roof fall is still an incoming native collision, never a ram or lift.
    m=start(0);enemy.oPosY=60;CHECK(rocket_bully_car_contact(m,&enemy));CHECK(!rocket_bully_ram(m,&enemy));
    CHECK(interact_bully(m,INTERACT_BULLY,&enemy));CHECK(actions==1);
    for(int condition=0;condition<6;condition++){
        m=start(0);enemy.oPosZ=170;CHECK(interact_bully(m,INTERACT_BULLY,&enemy));
        switch(condition){case 0:sCurrPlayMode=PLAY_MODE_PAUSED;break;
            case 1:gTimeStopState=TIME_STOP_ACTIVE;break;
            case 2:bullyWallZ=170;break;
            case 3:bullyWallZ=175;bullySideWall=1;break;
            case 4:bullyFloorMode=1;break;case 5:bullyFloorMode=2;break;}
        float before=enemy.oPosZ;rocket_bully_separate(&enemy);
        CHECK(enemy.oPosZ==before&&enemy.oPosX==0&&enemy.oPosY==0);
        if(condition>=2)CHECK(bullyCar.velocity[2]==0); // Stop driving into an unresolved obstruction.
    }
    m=start(0);enemy.oPosZ=170;CHECK(interact_bully(m,INTERACT_BULLY,&enemy));
    gCLIOpts.offline=0;gCLIOpts.characterNet=1;gNetworkType=NT_SERVER;
    gNetworkAreaLoaded=1;gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=1;authority=0;
    float before=enemy.oPosZ;rocket_bully_separate(&enemy);CHECK(enemy.oPosZ==before); // Ownership handoff.
    authority=1;rocket_bully_separate(&enemy);CHECK(enemy.oPosZ>before&&enemy.oPosY==0);
    // Timer wrap does not create a new impact, while an actual gap does.
    m=start(0);gGlobalTimer=UINT32_MAX;rocket_bully_record(m,&enemy,1);gGlobalTimer++;
    rocket_bully_prepare();CHECK(rocket_bully_repeat(m,&enemy));
    // Consuming one Bully contact never masks damage from a different hazard.
    struct Object hazard=enemy;hazard.oInteractType=INTERACT_DAMAGE;hazard.oDamageOrCoinValue=1;hazard.oInteractStatus=0;
    if(!interact_bully(m,INTERACT_BULLY,&enemy))CHECK(interact_damage(m,INTERACT_DAMAGE,&hazard));
    CHECK(actions==1&&m->hurtCounter==4);
}
static void test_rotated_contact(void){
    for(int yaw=0;yaw<65536;yaw+=4096){
        struct MarioState *m=start(0);float forwardX=sins(yaw),forwardZ=coss(yaw);
        bullyCar.basis[0]=forwardX;bullyCar.basis[2]=forwardZ;
        bullyCar.basis[3]=forwardZ;bullyCar.basis[5]=-forwardX;
        bullyCar.velocity[0]=forwardX*2400;bullyCar.velocity[2]=forwardZ*2400;
        bullyCar.velocity[1]=123;bullyCar.boost=37;m->faceAngle[1]=yaw;
        enemy.oPosX=forwardX*170;enemy.oPosZ=forwardZ*170;
        RocketSnapshot before=bullyCar;CHECK(interact_bully(m,INTERACT_BULLY,&enemy));
        CHECK(bullyCar.boost==37&&bullyCar.velocity[1]==123);
        CHECK(!memcmp(before.position,bullyCar.position,sizeof before.position));
        CHECK(!memcmp(before.basis,bullyCar.basis,sizeof before.basis));
        CHECK(fabsf(bullyCar.velocity[0]*forwardZ-bullyCar.velocity[2]*forwardX)<.01f);
        bully_check_mario_collision();bully_step();
        CHECK(enemy.oPosY==0&&!actions);
        CHECK(!rocket_bully_car_contact(m,&enemy));
    }
}
int main(void){test_intent();test_authority();test_native_ring_out();test_outgoing_overlap_does_not_stun();test_contact_lifetime();test_fast_native_order();test_boundaries();test_rotated_contact();printf("PASS Bully: %d native attack/stun, mapped intent, contact lifetime, geometry, authority and real native ring-out/reward checks\n",checks);return 0;}
