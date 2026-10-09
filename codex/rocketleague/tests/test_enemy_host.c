#include "player_bump_fixture_stubs.h"
/* Production adapter + real raw-pose network accessor, inert world services.
 * No sockets, saves, renderer or controller are used. */
#include "../../../src/game/rocket_enemy.c"
#include <assert.h>
#include <stdio.h>
struct CLIOptions gCLIOpts;
struct MarioState gMarioStates[MAX_PLAYERS];
enum NetworkType gNetworkType;
bool gNetworkAreaLoaded,gNetworkAreaSyncing;
struct Area *gCurrentArea;
struct Object *gCurrentObject;
s16 gCurrLevelNum,sCurrPlayMode;
u32 gGlobalTimer;
const BehaviorScript bhvGoomba[]={1},bhvBobomb[]={2},bhvSpindrift[]={3},bhvScuttlebug[]={4},
    bhvSkeeter[]={5},bhvSnufit[]={6},bhvFlyGuy[]={7},bhvBowser[]={8},bhvKingBobomb[]={9},bhvBobombBuddy[]={10};
struct Object gObjectPool[OBJECT_POOL_CAPACITY];
#define enemy gObjectPool[0]
static struct Object players[MAX_PLAYERS];
static struct Area areaFixture;
static struct SyncObject so;
static RocketSnapshot localCar;
static int localActive,visible=1,sends;
static uint32_t localEpoch;
static double now;
static u32 sentStatus;
static unsigned verifiedCaps[MAX_PLAYERS],visibleCaps;
double clock_elapsed_f64(void){return now;}
void network_coin_boost_clear(unsigned index){(void)index;}
void rocket_caps_clear(unsigned index){(void)index;}
uint32_t rocket_runtime_epoch(void){return localEpoch;}
int rocket_adapter_interaction_snapshot(RocketSnapshot *state){if(!localActive)return 0;*state=localCar;return 1;}
int rocket_adapter_object_visible(const float from[3],struct Object *obj){(void)from;assert(obj==&enemy);return visible;}
uint32_t rocket_caps_active_flags(unsigned index){assert(index<MAX_PLAYERS);return verifiedCaps[index];}
int rocket_adapter_enemy_visible(const float from[3],struct Object *obj,unsigned caps){visibleCaps=caps;return rocket_adapter_object_visible(from,obj);}
struct SyncObject *sync_object_get(u32 id){return id==1?&so:NULL;}
bool sync_object_is_initialized(u32 id){return id==1;}
bool sync_object_should_own(u32 id){
    assert(id==1);u8 override=0,own=0;so.override_ownership(&override,&own);return override&&own;
}
void network_send_object_reliability(struct Object *obj,bool reliable){assert(obj==&enemy&&reliable);sends++;sentStatus=obj->oInteractStatus;}
static RocketSnapshot pose(float x,uint64_t ticks){
    RocketSnapshot car={0};car.position[0]=x;car.velocity[0]=4400;
    car.basis[0]=car.basis[7]=1;car.basis[5]=-1;car.ticks=ticks;
    for(int i=0;i<4;i++)car.wheel_radius[i]=30;
    return car;
}
static void fresh(const BehaviorScript *behavior,int net) {
    memset(histories,0,sizeof histories);memset(&enemy,0,sizeof enemy);memset(&so,0,sizeof so);
    memset(verifiedCaps,0,sizeof verifiedCaps);visibleCaps=0;
    memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof(struct NetworkPlayer)*MAX_PLAYERS);character_net_clear_all();
    enemy.behavior=behavior;enemy.oSyncID=1;enemy.activeFlags=ACTIVE_FLAG_ACTIVE;
    enemy.header.gfx.activeAreaIndex=1;enemy.hitboxRadius=40;enemy.hitboxHeight=80;
    gCurrentObject=&enemy;gCurrentArea=&areaFixture;areaFixture.index=1;
    so.o=&enemy;gCLIOpts.offline=false;gCLIOpts.rocketCar=true;gCLIOpts.characterNet=net;gNetworkType=net?NT_SERVER:NT_NONE;
    gNetworkAreaLoaded=true;gNetworkAreaSyncing=false;gCurrLevelNum=9;gGlobalTimer=1;sCurrPlayMode=PLAY_MODE_NORMAL;
    gNetworkPlayerLocal=&gNetworkPlayers[0];sends=0;localActive=visible=1;localEpoch=1;now=1;
    for(int i=0;i<3;i++){
        struct NetworkPlayer *np=&gNetworkPlayers[i];np->connected=np->currAreaSyncValid=np->currLevelSyncValid=np->currPositionValid=true;
        np->globalIndex=np->localIndex=i;np->currLevelNum=9;np->currAreaIndex=1;
        gMarioStates[i].marioObj=&players[i];gMarioStates[i].health=0x880;gMarioStates[i].action=ACT_IDLE;
    }
    localCar=pose(-400,100);
}
static int hit(void){assert(!rocket_enemy_attack(&enemy));localCar=pose(-100,108);gGlobalTimer++;return rocket_enemy_attack(&enemy);}
static void remote(unsigned index,float x,uint32_t sequence,uint32_t epoch,uint64_t ticks){
    CharacterNetState s={0};s.speed_percent=100;s.kind=CNET_OCTANE;s.active=CNET_DRIVING;s.interaction=1;s.sequence=sequence;s.epoch=epoch;s.car=pose(x,ticks);
    assert(character_net_accept(index,&s));
}
static void other_owner(u8 *override,u8 *own){*override=*own=1;}
static void test_online_switch_sweeps(void){
    /* Both accepted transitions arrive between native enemy updates. A kind
     * round trip must reset history even if a peer reuses the same epoch. */
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);
    assert(!rocket_enemy_attack(&enemy));
    CharacterNetState mario={0};mario.speed_percent=100;mario.kind=CNET_MARIO;mario.sequence=2;mario.epoch=1;
    assert(character_net_accept(1,&mario));
    remote(1,-100,3,1,108);gGlobalTimer++;
    assert(!rocket_enemy_attack(&enemy)&&!sends);
    /* With a lost departure, the lifecycle's new committed epoch still
     * prevents a sweep between the two otherwise adjacent driving packets. */
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);
    assert(!rocket_enemy_attack(&enemy));remote(1,-100,3,3,108);gGlobalTimer++;
    assert(!rocket_enemy_attack(&enemy)&&!sends);
    /* A delayed/rejected transition neither resurrects Mario nor resets a
     * valid current driving sweep. Only accepted transitions change identity. */
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,3,3,100);
    assert(!rocket_enemy_attack(&enemy));
    assert(!character_net_accept(1,&mario));
    remote(1,-100,4,3,108);gGlobalTimer++;
    assert(rocket_enemy_attack(&enemy)&&sends==1);
    puts("online switch authority: observed round trip and lost departure reject stale sweeps; rejected old transition preserves valid sweep");
}
int main(void){
    fresh(bhvGoomba,0);assert(hit());assert(enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|ATTACK_FAST_ATTACK));assert(!sends);
    enemy.oInteractStatus=0;gGlobalTimer++;localCar.ticks+=4;assert(!rocket_enemy_attack(&enemy)); // committed once
    rocket_enemy_forget(&enemy);localCar=pose(-400,100);assert(hit()); // recycled slot, even same identity
    fresh(bhvBobomb,1);assert(hit());assert(sends==1&&(sentStatus&INT_STATUS_TOUCHED_BOB_OMB));
    fresh(bhvGoomba,1);assert(hit());assert(sends==1);enemy.oAction=OBJ_ACT_VERTICAL_KNOCKBACK;assert(so.ignore_if_true());
    fresh(bhvGoomba,1);gCLIOpts.rocketCar=false;assert(hit()&&sends==1); // Mario-first now owns Octane.
    fresh(bhvGoomba,1);gCLIOpts.rocketCar=false;localActive=0;assert(!hit()&&!sends); // Switching back owns no car contact.
    fresh(bhvKingBobomb,1);assert(!hit()&&!sends);fresh(bhvBowser,1);assert(!hit());fresh(bhvBobombBuddy,1);assert(!hit());
    fresh(bhvGoomba,1);enemy.oGoombaSize=GOOMBA_SIZE_HUGE;assert(!hit());
    fresh(bhvGoomba,1);enemy.oGoombaSize=GOOMBA_SIZE_TINY;assert(hit());
    const BehaviorScript *others[]={bhvSpindrift,bhvScuttlebug,bhvSkeeter,bhvSnufit,bhvFlyGuy};
    for(unsigned i=0;i<sizeof others/sizeof others[0];i++){fresh(others[i],1);assert(hit()&&sends==1);}
    fresh(bhvBobomb,1);enemy.oHeldState=HELD_HELD;assert(!hit());
    fresh(bhvBobomb,1);gMarioStates[1].heldObj=&enemy;assert(!hit());
    fresh(bhvGoomba,1);enemy.oIntangibleTimer=-1;assert(!hit());
    fresh(bhvGoomba,1);enemy.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|ATTACK_PUNCH;assert(!hit());
    fresh(bhvGoomba,1);gNetworkPlayers[0].globalIndex=3;assert(!hit()&&!sends); // non-authority peer
    fresh(bhvGoomba,1);gNetworkType=NT_CLIENT;gNetworkPlayers[0].globalIndex=1;
    gNetworkPlayers[1].globalIndex=0;gNetworkPlayers[1].currAreaIndex=2;
    assert(hit()&&sends==1); // area authority can be a client while host is elsewhere
    fresh(bhvGoomba,1);so.override_ownership=other_owner;assert(!hit());assert(so.override_ownership==other_owner);
    fresh(bhvGoomba,1);visible=0;assert(!hit());
    fresh(bhvGoomba,1);sCurrPlayMode=PLAY_MODE_PAUSED;assert(!hit());
    fresh(bhvGoomba,1);gNetworkAreaSyncing=true;assert(!hit());
    fresh(bhvGoomba,1);localActive=0;gCLIOpts.rocketCar=false;remote(1,-400,1,1,100);
    assert(!rocket_enemy_attack(&enemy));remote(1,-100,2,1,108);gGlobalTimer++;
    assert(rocket_enemy_attack(&enemy)&&sends==1); // Mario authority resolves remote car
    enemy.oInteractStatus=0;gGlobalTimer++;assert(!rocket_enemy_attack(&enemy)&&sends==1);
    fresh(bhvGoomba,1);localActive=0;verifiedCaps[0]=MARIO_VANISH_CAP;
    remote(1,-400,1,1,100);assert(!rocket_enemy_attack(&enemy));remote(1,-100,2,1,108);gGlobalTimer++;
    assert(rocket_enemy_attack(&enemy)&&visibleCaps==0); // Local phase cannot authorize a remote car.
    fresh(bhvGoomba,1);localActive=0;verifiedCaps[1]=MARIO_VANISH_CAP;
    remote(1,-400,1,1,100);assert(!rocket_enemy_attack(&enemy));remote(1,-100,2,1,108);gGlobalTimer++;
    assert(rocket_enemy_attack(&enemy)&&visibleCaps==MARIO_VANISH_CAP);
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);remote(2,-400,1,1,100);
    assert(!rocket_enemy_attack(&enemy));remote(1,-100,2,1,108);remote(2,-100,2,1,108);gGlobalTimer++;
    assert(rocket_enemy_attack(&enemy)&&sends==1); // two cars, one event
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);assert(!rocket_enemy_attack(&enemy));
    remote(1,-100,2,2,108);gGlobalTimer++;assert(!rocket_enemy_attack(&enemy)); // reset cannot sweep
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);assert(!rocket_enemy_attack(&enemy));
    remote(1,-100,2,1,108);now+=.251;gGlobalTimer++;assert(!rocket_enemy_attack(&enemy));
    fresh(bhvGoomba,1);localActive=0;remote(1,-400,1,1,100);assert(!rocket_enemy_attack(&enemy));
    character_net_clear(1);remote(1,-100,1,1,108);gGlobalTimer++;
    assert(!rocket_enemy_attack(&enemy)); // reconnect with same epoch/tick range must not inherit a sweep
    fresh(bhvGoomba,1);assert(!rocket_enemy_attack(&enemy));gNetworkPlayers[0].globalIndex=3;gGlobalTimer++;assert(!rocket_enemy_attack(&enemy));
    gNetworkPlayers[0].globalIndex=0;localCar=pose(-100,108);gGlobalTimer++;assert(!rocket_enemy_attack(&enemy)); // authority handoff warms up
    test_online_switch_sweeps();
    puts("enemy host: native status, seven enemy types, boss/NPC/held gates, stable authority, remote/replay/reset/stale/two-car checks passed");
    return 0;
}
