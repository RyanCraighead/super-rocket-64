/* Combined production policy, ordinary bumper staging and real collision pass.
 * Reuse only inert world/transport services from the enemy authority fixture. */
#define main enemy_component_main
#include "test_enemy_host.c"
#undef main
#define histories ordinaryHistories
#include "../../../src/game/rocket_bobomb.c"
#undef histories
#include "../../../src/game/rocket_bully.c"
#include "../../../src/game/rocket_contacts.c"
#include "../../../src/game/object_collision.c"
#include "../../../src/game/rocket_incoming.c"
#include "level_table.h"
#include "game/spawn_object.h"
#include "engine/math_util.h"
u32 gTimeStopState;
struct ObjectNode gFreeObjectList;
struct Object *gMarioObject;
struct ObjectNode *gObjectLists;
static struct ObjectNode lists[NUM_OBJ_LISTS];
int codex_panel_gl_is_focused(void){return 0;}
int rocket_adapter_car_selected(void){return localActive;}
int character_presentation_car_snapshot(RocketSnapshot *car){(void)car;return 0;}
bool sync_object_is_owned_locally(u32 id){return sync_object_should_own(id);}
int rocket_adapter_body_snapshot(struct Object *object,RocketSnapshot *car){
    if(object!=&players[0]||!localActive)return 0;
    *car=localCar;return 1;
}
struct Object *try_allocate_object(struct ObjectNode *dest,struct ObjectNode *freeList){(void)dest;(void)freeList;return &enemy;}
struct Object *find_unimportant_object(void){return NULL;}
void unload_object(struct Object *object){(void)object;assert(0);}
/* Platform cache lifetime is verified through the same allocator in test_platform_host. */
void rocket_platform_forget(struct Object *object){(void)object;}
void rocket_whomp_forget(struct Object *object){(void)object;}
void rocket_switch_forget(struct Object *object){(void)object;}
#include "contacts_allocation.inc"
static void start(const BehaviorScript *behavior,int net){
    fresh(behavior,net);memset(ordinaryHistories,0,sizeof ordinaryHistories);
    memset(bumps,0,sizeof bumps);prepared=0;gTimeStopState=0;
    gObjectLists=lists;
    for(int i=0;i<NUM_OBJ_LISTS;i++)lists[i].next=lists[i].prev=&lists[i];
    memset(players,0,sizeof players);gMarioObject=&players[0];
    for(int i=1;i<MAX_PLAYERS;i++)gMarioStates[i].marioObj=NULL;
    players[0].oInteractType=INTERACT_PLAYER;players[0].activeFlags=ACTIVE_FLAG_ACTIVE;
    players[0].header.gfx.activeAreaIndex=-1;
    players[0].header.gfx.areaIndex=1;
    lists[OBJ_LIST_PLAYER].next=&players[0].header;
    players[0].header.next=&lists[OBJ_LIST_PLAYER];
    lists[OBJ_LIST_GENACTOR].next=&enemy.header;
    enemy.header.next=&lists[OBJ_LIST_GENACTOR];
    enemy.oInteractType=behavior==bhvBobomb?INTERACT_GRABBABLE:INTERACT_BOUNCE_TOP;
    enemy.hitboxRadius=enemy.hurtboxRadius=40;enemy.hitboxHeight=75;enemy.hurtboxHeight=60;
}
static void step(float x,uint64_t ticks){localCar.position[0]=x;localCar.ticks=ticks;++gGlobalTimer;detect_object_collisions();}
static void offline_start(const BehaviorScript *behavior) {
    start(behavior,0);
    // Actual shipped --offline startup uses NT_SERVER for local save/object
    // authority, without character-net or an initialized transport/sync object.
    gCLIOpts.offline=true;gNetworkType=NT_SERVER;gNetworkPlayerLocal=NULL;enemy.oSyncID=0;
}
static void test_offline_impacts(void) {
    const BehaviorScript *small[]={bhvGoomba,bhvBobomb,bhvSpindrift,bhvScuttlebug,bhvSkeeter,bhvSnufit,bhvFlyGuy};
    unsigned checks=0;
    for(unsigned kind=0;kind<sizeof small/sizeof *small;kind++)for(int ground=0;ground<2;ground++)for(int heading=0;heading<4;heading++)for(int speed=50;speed<=100;speed+=50) {
        offline_start(small[kind]);fixtureSpeedPercent=speed;
        localCar=pose(-250,100);localCar.grounded=ground;
        localCar.velocity[0]=ROCKET_ENEMY_SUPERSONIC_SPEED*rocket_speed_scale();
        float angle=heading*1.57079632679f;
        localCar.basis[0]=cosf(angle);localCar.basis[2]=sinf(angle);
        localCar.basis[3]=sinf(angle);localCar.basis[5]=-cosf(angle);
        detect_object_collisions();assert(!enemy.oInteractStatus);checks++;
        step(-100,108);
        assert(enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|
            (small[kind]==bhvBobomb?INT_STATUS_TOUCHED_BOB_OMB:ATTACK_FAST_ATTACK)));checks++;
        assert(!sends);checks++;
        if(small[kind]==bhvBobomb){s16 yaw;assert(!rocket_contacts_bobomb_yaw(&enemy,&yaw));checks++;}
    }
    for(int speed=50;speed<=100;speed+=50) {
        offline_start(bhvGoomba);fixtureSpeedPercent=speed;
        localCar=pose(-250,100);localCar.velocity[0]=ROCKET_ENEMY_SUPERSONIC_SPEED*rocket_speed_scale()-.1f;
        detect_object_collisions();step(-100,108);assert(!enemy.oInteractStatus&&!sends);checks++;
        offline_start(bhvGoomba);localCar=pose(-100,100);localCar.velocity[0]=0;
        detect_object_collisions();step(-100,108);assert(!enemy.oInteractStatus&&!sends);checks++;
        assert(detect_object_hurtbox_overlap(&players[0],&enemy));checks++; // Stationary car remains vulnerable.
    }
    fixtureSpeedPercent=100;
    const BehaviorScript *excluded[]={bhvKingBobomb,bhvBowser,bhvBobombBuddy};
    for(unsigned i=0;i<sizeof excluded/sizeof *excluded;i++){
        offline_start(excluded[i]);detect_object_collisions();step(-100,108);assert(!enemy.oInteractStatus&&!sends);checks++;
    }
    offline_start(bhvGoomba);enemy.oGoombaSize=GOOMBA_SIZE_HUGE;
    detect_object_collisions();step(-100,108);assert(!enemy.oInteractStatus);checks++;
    offline_start(bhvGoomba);localActive=0;detect_object_collisions();step(-100,108);assert(!enemy.oInteractStatus);checks++;
    gCLIOpts.offline=false;
    printf("PASS offline supersonic production collision pass: %u checks; seven small enemies, four faces, ground/air, 50/100 percent thresholds, lower-speed/stationary/boss/selection gates, no transport\n",checks);
}
int main(void){
    test_offline_impacts();
    s16 yaw=0;
    start(bhvGoomba,0);localCar=pose(-146.6667f,100);localCar.position[1]=74;
    detect_object_collisions();assert(!enemy.oInteractStatus);
    // The enemy jumped after the prior native attack hook. At the new boundary
    // its moved hurtbox and the car's actual advanced pose are evaluated together.
    enemy.oPosY=21;step(0,104);
    assert(enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|ATTACK_FAST_ATTACK));
    assert(detect_object_hurtbox_overlap(&players[0],&enemy));
    assert(players[0].numCollidedObjs==1); // Native dispatcher sees the already claimed status.
    assert(gCurrentObject==&enemy);
    enemy.oInteractStatus=0;gGlobalTimer+=3;rocket_contacts_prepare();
    assert(histories[0].committed&&!enemy.oInteractStatus); // Gaps do not uncommit a lifetime.
    // Same-level pool resets skip unload_object. The actual allocator must
    // still clear all histories before reusing the same pointer and sync ID.
    bumps[0].valid=1;ordinaryHistories[0].object=&enemy;ordinaryHistories[0].contact[0].valid=1;
    assert(allocate_object(&lists[OBJ_LIST_GENACTOR])==&enemy);
    assert(!histories[0].object&&!bumps[0].valid&&!ordinaryHistories[0].object);
    // The actual prepass and allocator own Bully contact lifetime too.
    gMarioStates[0].area=&areaFixture;enemy.oInteractType=INTERACT_BULLY;
    enemy.oIntangibleTimer=0;enemy.activeFlags=ACTIVE_FLAG_ACTIVE;enemy.header.gfx.activeAreaIndex=1;
    enemy.hitboxRadius=73;enemy.hitboxHeight=123;
    localCar=pose(0,120);enemy.oPosX=100;enemy.oPosY=0;
    rocket_bully_record(&gMarioStates[0],&enemy,1);assert(history(&enemy));
    assert(allocate_object(&lists[OBJ_LIST_GENACTOR])==&enemy);assert(!history(&enemy));
    enemy.oInteractType=INTERACT_BULLY;enemy.oIntangibleTimer=0;enemy.activeFlags=ACTIVE_FLAG_ACTIVE;
    enemy.header.gfx.activeAreaIndex=1;enemy.hitboxRadius=73;enemy.hitboxHeight=123;
    rocket_bully_record(&gMarioStates[0],&enemy,1);assert(history(&enemy));
    localCar.position[0]=-1000;gGlobalTimer++;detect_object_collisions();assert(!history(&enemy));
    enemy.oPosX=0;enemy.oPosY=0;enemy.oInteractType=INTERACT_BOUNCE_TOP;
    enemy.behavior=bhvGoomba;enemy.oSyncID=1;enemy.oIntangibleTimer=0;
    enemy.hitboxRadius=40;enemy.hitboxHeight=75;
    localCar=pose(-400,200);++gGlobalTimer;rocket_contacts_prepare();assert(!enemy.oInteractStatus);
    localCar=pose(-100,208);++gGlobalTimer;rocket_contacts_prepare();assert(enemy.oInteractStatus&INT_STATUS_WAS_ATTACKED);
    start(bhvGoomba,0);detect_object_collisions();step(-100,108);
    assert(enemy.oInteractStatus&INT_STATUS_WAS_ATTACKED); // Static approach unchanged.
    start(bhvGoomba,0);detect_object_collisions();enemy.oIntangibleTimer=1;step(-100,108);
    assert(!enemy.oIntangibleTimer&&(enemy.oInteractStatus&INT_STATUS_WAS_ATTACKED));
    start(bhvGoomba,0);detect_object_collisions();gTimeStopState=TIME_STOP_ACTIVE;step(-100,108);
    assert(!enemy.oInteractStatus);gTimeStopState=0;step(-50,112);assert(!enemy.oInteractStatus);
    start(bhvBobomb,0);enemy.hitboxRadius=65;enemy.hitboxHeight=113;detect_object_collisions();step(-100,108);
    assert(enemy.oInteractStatus&INT_STATUS_TOUCHED_BOB_OMB);
    assert(!rocket_contacts_bobomb_yaw(&enemy,&yaw)); // Supersonic cannot also kick.
    start(bhvBobomb,0);enemy.hitboxRadius=65;enemy.hitboxHeight=113;
    detect_object_collisions();step(-260,104);
    assert(!enemy.oInteractStatus&&!rocket_contacts_bobomb_yaw(&enemy,&yaw));
    step(-120,108);assert(enemy.oInteractStatus&INT_STATUS_TOUCHED_BOB_OMB);
    assert(!rocket_contacts_bobomb_yaw(&enemy,&yaw)); // Wider ordinary envelope cannot win one frame earlier.
    start(bhvBobomb,0);enemy.hitboxRadius=65;enemy.hitboxHeight=113;
    localCar=pose(-400,100);localCar.velocity[0]=600;
    detect_object_collisions();step(-320,104);step(-240,108);
    assert(!enemy.oInteractStatus);rocket_contacts_prepare(); // Duplicate prepass preserves the staged result.
    assert(rocket_contacts_bobomb_yaw(&enemy,&yaw));
    assert(!rocket_contacts_bobomb_yaw(&enemy,&yaw));
    start(bhvBobomb,1);enemy.hitboxRadius=65;enemy.hitboxHeight=113;
    gNetworkPlayers[0].globalIndex=3;detect_object_collisions();step(-100,108);
    assert(!enemy.oInteractStatus&&!rocket_contacts_bobomb_yaw(&enemy,&yaw)); // Nonowner never predicts a kill.
    puts("PASS combined contacts: target motion, tangibility countdown, time stop, lifetime dedup, supersonic/ordinary precedence, staged consumption and authority");
    return 0;
}
