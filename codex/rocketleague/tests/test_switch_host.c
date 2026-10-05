/* Native switch loop + production contact adapter; inert audio/world services.
 * Remote cars are deliberately not replayed: each peer owns its local input,
 * exactly as Mario's native reliable switch event. */
#define ROCKET_WHOMP_FIXTURE_NO_MAIN
#include "test_whomp_host.c"
#undef ROCKET_WHOMP_FIXTURE_NO_MAIN
#include "../../../src/game/rocket_switch.c"
#include "pc/lua/smlua_hooks.h"
#include "../../../src/game/behaviors/blue_coin.inc.c"

const BehaviorScript bhvBlueCoinSwitch[]={13},bhvHiddenBlueCoin[]={14},bhvBlueCoinNumber[]={15},bhvGoldenCoinSparkles[]={16};
struct Object *gMarioObject;
Vec3f gGlobalSoundSource;
static int remainingCoins;
static struct Object coin;
void play_sound(s32 bits,f32 *pos){(void)bits;(void)pos;}
void cur_obj_move_using_fvel_and_gravity(void){enemy.oPosY+=enemy.oVelY;}
void cur_obj_unhide(void){}
struct Object *cur_obj_nearest_object_with_behavior(const BehaviorScript *behavior){assert(behavior==bhvHiddenBlueCoin);return remainingCoins?&coin:NULL;}
s32 obj_is_mario_ground_pounding_platform(struct MarioState *m,struct Object *obj){assert(m==&gMarioStates[0]&&obj==&enemy);return groundPound;}

/* Synthetic box only. Native scale 3 makes its top 300 units high. */
static s16 switchCollision[]={TERRAIN_LOAD_VERTICES,8,
    -90,0,-90,90,0,-90,90,100,-90,-90,100,-90,
    -90,0,90,90,0,90,90,100,90,-90,100,90};
static RocketSnapshot flip_pose(float gap,uint64_t ticks,int end){
    RocketSnapshot c=down(gap,ticks);c.boosting=0;c.position[1]+=200;c.position[2]=0;
    c.flipped=c.flipping=1;c.flip_time=.15f;c.angular_velocity[0]=4;c.velocity[1]=-80;
    if(end){
        c.basis[1]=-cosf(.1f);c.basis[2]=sinf(.1f);c.basis[7]=sinf(.1f);c.basis[8]=cosf(.1f);
        float p[3];rocket_whomp_lowest(&c,p);c.position[1]+=301-p[1];
    }
    return c;
}
static void switch_start(int net){
    start(0,net);enemy.behavior=bhvBlueCoinSwitch;enemy.oFaceAnglePitch=0;enemy.oAction=BLUE_COIN_SWITCH_ACT_IDLE;
    enemy.collisionData=switchCollision;enemy.oIntangibleTimer=-1; // Native surface-only switch default.
    gMarioObject=&players[0];remainingCoins=1;
    for(int k=0;k<3;k++)enemy.header.gfx.scale[k]=3;
    rocket_switch_forget(&enemy);localCar=flip_pose(45,100,0);
}
static void switch_loop(void){
    checkLoopOrder=1;collisionLoads=nativeActions=nativeMoved=0;
    bhv_blue_coin_switch_loop();checkLoopOrder=0;
}
static void switch_hit(void){switch_loop();assert(enemy.oAction==0&&!sends);localCar=flip_pose(1,104,1);gGlobalTimer++;switch_loop();}
int main(void){
    for(int client=0;client<2;client++){
        switch_start(1);gNetworkType=client?NT_CLIENT:NT_SERVER;
        if(client){gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;}
        switch_hit();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_RECEDING&&enemy.oVelY==-20&&enemy.oGravity==0&&sends==1);
        gGlobalTimer++;switch_loop();assert(sends==1); // Same contact cannot press again.
        enemy.oTimer=6;gGlobalTimer++;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_TICKING);
        enemy.oTimer=240;gGlobalTimer++;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_TICKING);
        gLevelValues.respawnBlueCoinsSwitch=1;enemy.oTimer=241;gGlobalTimer++;switch_loop();
        assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_RESPAWNING&&sends==2);
        enemy.oPosY=enemy.oHomeY;gGlobalTimer++;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_IDLE);
        localCar=flip_pose(1,108,1);gGlobalTimer++;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_IDLE&&sends==2);
    }
    switch_start(0);groundPound=1;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_RECEDING&&sends==1);
    switch_start(0);enemy.oAction=BLUE_COIN_SWITCH_ACT_TICKING;remainingCoins=0;switch_loop();assert(deleted==1);
    // Drive-by, passive spin/fall, grounded contact, obstructed or edge hits.
    for(int invalid=0;invalid<8;invalid++){
        switch_start(1);
        if(invalid==0)localCar.flipping=0;
        if(invalid==1)localCar.flipped=0;
        if(invalid==2)localCar.grounded=1;
        if(invalid==3)visible=0;
        if(invalid==4)backSurface.object=&players[0];
        if(invalid==5)localCar.position[0]=300;
        if(invalid==6)localCar.angular_velocity[0]=0;
        if(invalid==7)localCar.air_time=0;
        switch_loop();localCar=flip_pose(1,104,1);if(invalid==5)localCar.position[0]=300;
        gGlobalTimer++;switch_loop();assert(enemy.oAction==BLUE_COIN_SWITCH_ACT_IDLE&&!sends);
    }
    // Reconnect/area reset, character change/reset, unload/reuse and pause.
    for(int reset=0;reset<5;reset++){
        switch_start(1);switch_loop();
        if(reset==0)gNetworkPlayerLocal->currLevelAreaSeqId++;
        if(reset==1)localEpoch++;
        if(reset==2)rocket_switch_forget(&enemy);
        if(reset==3){sCurrPlayMode=PLAY_MODE_PAUSED;gGlobalTimer++;switch_loop();sCurrPlayMode=PLAY_MODE_NORMAL;}
        if(reset==4){gNetworkAreaSyncing=1;gGlobalTimer++;switch_loop();gNetworkAreaSyncing=0;}
        localCar=flip_pose(1,104,1);gGlobalTimer++;switch_loop();assert(!sends&&enemy.oAction==BLUE_COIN_SWITCH_ACT_IDLE);
    }
    switch_start(1);localActive=0;send_down(1,45,1);switch_loop();send_down(1,1,2);gGlobalTimer++;switch_loop();
    assert(!sends&&enemy.oAction==BLUE_COIN_SWITCH_ACT_IDLE); // Never duplicate another peer's switch event.
    puts("PASS native blue switch: host/client local zero-boost flip, single native event, Mario, timer/respawn/deletion, passive/geometry rejection, reconnect/epoch/pause/pool resets, no remote replay");
}
