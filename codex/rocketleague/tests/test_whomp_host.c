/* Real car adapter, raw CNET accessor and native Whomp damage functions.
 * Inert surface/audio/transport services; no game, controller or saves. */
#define main enemy_fixture_main
#include "test_enemy_host.c"
#undef main
#include "../../../src/game/rocket_whomp.c"
#include "game/behavior_actions.h"
#include "game/object_helpers.h"
#include "game/obj_behaviors.h"
#include "game/obj_behaviors_2.h"
#include "game/spawn_sound.h"
#include "game/mario_actions_cutscene.h"
#include "game/hardcoded.h"
#include "game/camera.h"
#include "audio/external.h"
#include "engine/surface_load.h"
#include "level_table.h"
#include "seq_ids.h"
#define o gCurrentObject
int mario_is_far_below_object(f32 distance);
#include "../../../src/game/behaviors/whomp.inc.c"
const BehaviorScript bhvWhompKingBoss[]={11},bhvSmallWhomp[]={12};
u32 gTimeStopState;
static int authority=1,groundPound,onPlatform,loot,stars,deleted;
static u32 authorityEpoch=1;
static struct Surface backSurface,otherSurface;
static int wrongWheel;
/* Observe the pose submitted by the actual native loop to its collision loader.
 * World movement stays inert; shake/rise pose changes come from native actions. */
static int checkLoopOrder,collisionLoads,nativeActions,nativeMoved;
static int loadedAfterAction,loadedAfterMove,witnessQueries;
static f32 loadedY;
static s32 loadedPitch;
struct LevelValues gLevelValues;
static s16 collision[]={TERRAIN_LOAD_VERTICES,8,
    -180,100,-100,180,100,-100,180,450,-100,-180,450,-100,
    -180,100,0,180,100,0,180,450,0,-180,450,0};
int boss_net_simulates(const struct Object *obj){assert(obj==&enemy);return authority;}
int boss_net_managed(const struct Object *obj){assert(obj==&enemy);return gCLIOpts.characterNet;}
uint32_t boss_net_epoch(const struct Object *obj){assert(obj==&enemy);return authorityEpoch;}
int rocket_adapter_whomp_path_clear(const float a[3],const float b[3],struct Object *obj){(void)a;(void)b;assert(obj==&enemy);return visible;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){
    (void)x;(void)y;(void)z;
    if(checkLoopOrder){assert(collisionLoads==1&&!nativeActions&&!nativeMoved);witnessQueries++;}
    *floor=wrongWheel&&x>0&&z>300?&otherSurface:&backSurface;return 100.f*enemy.header.gfx.scale[2];
}
/* These services let bhv_whomp_loop itself dispatch the real native actions. */
struct SyncObject *sync_object_init(struct Object *obj,float distance){
    (void)obj;(void)distance;assert(0);return NULL; // Fixture identity is already initialized.
}
void sync_object_init_field_with_size(struct Object *obj,void *field,u8 size){
    (void)obj;(void)field;(void)size;assert(0);
}
int boss_net_begin(struct Object *obj){assert(obj==&enemy);return authority;}
void boss_net_end(struct Object *obj){assert(obj==&enemy);}
void cur_obj_update_floor_and_walls(void){}
void cur_obj_call_action_function(void (*actions[])(void),uint32_t count){
    assert(gCurrentObject==&enemy&&(uint32_t)enemy.oAction<count);
    actions[enemy.oAction]();nativeActions++;
}
void cur_obj_move_standard(s16 slope){assert(slope==-20);nativeMoved++;}
s32 cur_obj_hide_if_mario_far_away_y(f32 distance){(void)distance;return 0;}
void load_object_collision_model(void){
    assert(gCurrentObject==&enemy&&checkLoopOrder);
    collisionLoads++;loadedY=enemy.oPosY;loadedPitch=enemy.oFaceAnglePitch;
    loadedAfterAction=nativeActions;loadedAfterMove=nativeMoved;
}
struct Object *nearest_player_to_object(struct Object *obj){(void)obj;return &players[0];}
struct MarioState *nearest_mario_state_to_object(struct Object *obj){(void)obj;return &gMarioStates[0];}
s32 cur_obj_is_mario_ground_pounding_platform(void){return groundPound;}
s32 cur_obj_is_any_player_on_platform(void){return onPlatform;}
s32 cur_obj_is_mario_on_platform(void){return onPlatform;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
void spawn_mist_particles_variable(s32 a,s32 b,f32 c){(void)a;(void)b;(void)c;}
void spawn_triangle_break_particles(s16 a,s16 b,f32 c,s16 d){(void)a;(void)b;(void)c;(void)d;}
void cur_obj_shake_screen(s32 shake){(void)shake;}
void obj_spawn_loot_yellow_coins(struct Object *obj,s32 count,f32 speed){assert(obj==&enemy&&speed==20);loot+=count;obj->oNumLootCoins-=count;}
void cur_obj_spawn_loot_coin_at_mario_pos(struct MarioState *m){(void)m;loot++;enemy.oNumLootCoins--;}
u8 should_start_or_continue_dialog(struct MarioState *m,struct Object *obj){(void)m;(void)obj;return 1;}
s32 cur_obj_update_dialog_with_cutscene(struct MarioState *m,s32 a,s32 b,s32 c,s32 d,u8 (*f)(void)){
    (void)m;(void)a;(void)b;(void)c;(void)d;return f();
}
void obj_set_angle(struct Object *obj,s16 x,s16 y,s16 z){(void)obj;(void)x;(void)y;(void)z;}
void cur_obj_hide(void){}
void cur_obj_become_intangible(void){enemy.oIntangibleTimer=-1;}
void create_sound_spawner(s32 sound){(void)sound;}
void obj_mark_for_deletion(struct Object *obj){assert(obj==&enemy);deleted++;obj->activeFlags=0;}
int boss_net_reward_available(struct Object *obj){assert(obj==&enemy);return !stars;}
struct Object *spawn_default_star(f32 x,f32 y,f32 z){(void)x;(void)y;(void)z;stars++;return NULL;}
void network_send_object(struct Object *obj){assert(obj==&enemy);sends++;}
struct BehaviorValues gBehaviorValues;
/* Native chase/animation services are inert; action transitions stay native. */
struct Object *gSecondCameraFocus;
s32 cur_obj_check_anim_frame(s32 frame){(void)frame;return 0;}
s32 cur_obj_check_anim_frame_in_range(s32 frame,s32 count){(void)frame;(void)count;return 0;}
s32 cur_obj_check_if_near_animation_end(void){return 0;}
void cur_obj_init_animation_with_accel_and_sound(s32 index,f32 accel){(void)index;(void)accel;}
void cur_obj_set_pos_to_home(void){}
void cur_obj_scale(f32 scale){for(int k=0;k<3;k++)enemy.header.gfx.scale[k]=scale;}
void seq_player_lower_volume(u8 player,u16 duration,u8 percent){(void)player;(void)duration;(void)percent;}
void stop_background_music(u16 seq){(void)seq;}
f32 dist_between_objects(struct Object *a,struct Object *b){(void)a;(void)b;return 1000;}
s16 obj_angle_to_object(struct Object *a,struct Object *b){(void)a;(void)b;return 0;}
s16 abs_angle_diff(s16 a,s16 b){return abs(a-b);}
f32 cur_obj_lateral_dist_to_home(void){return 0;}
s32 cur_obj_rotate_yaw_toward(s16 target,s16 step){(void)target;(void)step;return 0;}
int mario_is_far_below_object(f32 distance){(void)distance;return 0;}
static RocketSnapshot down(float gap,uint64_t ticks){
    RocketSnapshot c={0};c.basis[1]=-1;c.basis[3]=1;c.basis[8]=1;
    c.position[1]=100+ROCKET_ENEMY_OFFSET+ROCKET_ENEMY_HALF_LENGTH+gap;c.position[2]=250;
    c.velocity[1]=-1400;c.boosting=1;c.air_time=.5f;c.ticks=ticks;
    for(int i=0;i<4;i++){c.wheel_radius[i]=30;memcpy(c.wheel_position[i],c.position,sizeof c.position);}
    return c;
}
static void start(int king,int net){
    fresh(king?bhvWhompKingBoss:bhvSmallWhomp,net);memset(whompHistories,0,sizeof whompHistories);
    enemy.oBehParams2ndByte=king;enemy.oHealth=3;enemy.oAction=6;enemy.oFaceAnglePitch=0x4000;
    enemy.oNumLootCoins=5;enemy.collisionData=collision;
    for(int k=0;k<3;k++)enemy.header.gfx.scale[k]=1;
    memset(&backSurface,0,sizeof backSurface);backSurface.object=&enemy;backSurface.normal.y=1;
    gTimeStopState=groundPound=onPlatform=loot=stars=deleted=wrongWheel=0;authority=authorityEpoch=1;
    localCar=down(45,100);sWhompCarGroundPound=0;
}
static int advance(void){assert(!rocket_whomp_ground_pound(&enemy));localCar=down(1,104);gGlobalTimer++;return rocket_whomp_ground_pound(&enemy);}
static void send_down(unsigned index,float gap,unsigned sequence){
    CharacterNetState state={0},decoded;uint8_t wire[CNET_WIRE_SIZE];
    state.kind=CNET_OCTANE;state.active=CNET_DRIVING;state.interaction=1;state.epoch=1;state.sequence=sequence;
    state.car=down(gap,96+4*sequence);
    assert(character_net_encode(wire,sizeof wire,&state)&&character_net_decode(&decoded,wire,sizeof wire));
    assert(character_net_accept(index,&decoded));
}
static void observed_native_loop(void){
    collisionLoads=nativeActions=nativeMoved=witnessQueries=0;
    loadedAfterAction=loadedAfterMove=0;checkLoopOrder=1;
    bhv_whomp_loop();checkLoopOrder=0;
    assert(collisionLoads==1&&nativeActions==1&&nativeMoved==1);
}
static void collision_order_tests(void){
    // Warm up, then make a real qualifying contact through the complete loop.
    start(1,0);observed_native_loop();
    assert(!loadedAfterAction&&!loadedAfterMove&&!witnessQueries&&enemy.oHealth==3);
    localCar=down(1,104);gGlobalTimer++;observed_native_loop();
    assert(!loadedAfterAction&&!loadedAfterMove&&witnessQueries==1);
    assert(enemy.oHealth==2&&enemy.oSubAction==1&&loadedY==enemy.oPosY);

    // Native king shake must submit its newly changed Y, including Mario-only play.
    for(int odd=0;odd<2;odd++){
        start(1,0);gCLIOpts.rocketCar=false;localActive=0;
        enemy.oSubAction=1;enemy.oWhompShakeVal=odd;
        observed_native_loop();
        assert(enemy.oPosY==(odd?8.f:-8.f)&&loadedY==enemy.oPosY);
        assert(loadedAfterAction==1&&loadedAfterMove==1&&!witnessQueries);
    }
    // Both native sizes rise by -0x200 before loading their updated transform.
    for(int king=0;king<2;king++){
        start(king,0);enemy.oSubAction=10;observed_native_loop();
        assert(enemy.oFaceAnglePitch==0x3e00&&loadedPitch==enemy.oFaceAnglePitch);
        assert(loadedAfterAction==1&&loadedAfterMove==1&&!witnessQueries);
    }
    // A non-flat action-6 pose must not take the early-load path either.
    for(int roll=0;roll<2;roll++){
        start(1,0);
        if(roll)enemy.oFaceAngleRoll=0x200;else enemy.oFaceAnglePitch=0x3e00;
        observed_native_loop();
        assert(loadedAfterAction==1&&loadedAfterMove==1&&!witnessQueries);
    }
}
static RocketSnapshot flip_down(float gap,uint64_t ticks,int end){
    RocketSnapshot c=down(gap,ticks);c.boosting=0;c.velocity[1]=-80;
    c.flipped=c.flipping=1;c.flip_time=.15f;c.angular_velocity[0]=4;
    if(end){
        c.basis[1]=-cosf(.1f);c.basis[2]=sinf(.1f);c.basis[7]=sinf(.1f);c.basis[8]=cosf(.1f);
        float p[3];rocket_whomp_lowest(&c,p);c.position[1]+=101-p[1];
    }
    return c;
}
static void send_flip(unsigned index,float gap,unsigned sequence,int end){
    CharacterNetState state={0},decoded;uint8_t wire[CNET_WIRE_SIZE];
    state.kind=CNET_OCTANE;state.active=CNET_DRIVING;state.interaction=1;state.epoch=1;state.sequence=sequence;
    state.car=flip_down(gap,96+4*sequence,end);
    assert(character_net_encode(wire,sizeof wire,&state)&&character_net_decode(&decoded,wire,sizeof wire));
    assert(character_net_accept(index,&decoded));
}
static RocketSnapshot resting(uint64_t ticks){
    RocketSnapshot c={0};c.basis[2]=c.basis[3]=c.basis[7]=1;
    c.position[1]=134;c.position[2]=250;c.grounded=1;c.ticks=ticks;
    for(int i=0;i<4;i++){
        c.wheel_contacts[i]=1;c.wheel_radius[i]=30;
        c.wheel_position[i][0]=(i&1)?70:-70;c.wheel_position[i][1]=130;
        c.wheel_position[i][2]=250+((i&2)?70:-70);
    }
    return c;
}
static void send_rest(unsigned sequence){
    CharacterNetState s={0},decoded;uint8_t wire[CNET_WIRE_SIZE];
    s.kind=CNET_OCTANE;s.active=CNET_DRIVING;s.interaction=1;s.epoch=1;s.sequence=sequence;
    s.car=resting(100+4*sequence);
    assert(character_net_encode(wire,sizeof wire,&s)&&character_net_decode(&decoded,wire,sizeof wire));
    assert(character_net_accept(1,&decoded));
}
static void resting_native_tests(void){
    for(int client=0;client<2;client++)for(int remote=0;remote<2;remote++){
        start(1,1);gNetworkType=client?NT_CLIENT:NT_SERVER;
        if(client){gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;}
        localActive=!remote;localCar=resting(100);if(remote)send_rest(1);
        observed_native_loop();assert(enemy.oHealth==2&&enemy.oSubAction==1&&sends==1&&witnessQueries==4);
        assert(!rocket_whomp_ground_pound(&enemy));
        for(unsigned frame=2;frame<10;frame++){
            localCar.ticks+=4;if(remote)send_rest(frame);gGlobalTimer++;
            observed_native_loop();assert(enemy.oHealth==2&&sends==1); // parked through shake
        }
        // Leave and return while invulnerable: no second damage in this cycle.
        localCar.position[2]+=1000;gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
        localCar=resting(160);if(remote)send_rest(11);gGlobalTimer++;
        assert(!rocket_whomp_ground_pound(&enemy)&&enemy.oHealth==2);
        // A new prone cycle is eligible again; no boost/flip edge is required.
        enemy.oPosY=0;enemy.oAction=6;enemy.oSubAction=0;enemy.oTimer=0;
        localCar=resting(168);if(remote)send_rest(12);gGlobalTimer++;
        observed_native_loop();assert(enemy.oHealth==1&&sends==2&&witnessQueries==4);
    }
    start(0,0);localCar=resting(100);observed_native_loop();assert(enemy.oAction==8&&loot==5&&!stars);
    for(int reject=0;reject<17;reject++){
        start(1,1);localCar=resting(100);
        switch(reject){
            case 0:localCar.wheel_contacts[3]=0;break;
            case 1:localCar.wheel_position[3][1]+=20;break;
            case 2:localCar.wheel_position[3][0]=175;break;
            case 3:localCar.grounded=0;break;
            case 4:localCar.basis[3]=-1;localCar.basis[7]=-1;break;
            case 5:localCar.position[1]=90;break;
            case 6:enemy.oAction=5;break;
            case 7:enemy.oSubAction=1;break;
            case 8:enemy.oIntangibleTimer=-1;break;
            case 9:authority=0;break;
            case 10:backSurface.object=&players[0];break;
            case 11:backSurface.normal.y=.5f;break;
            case 12:visible=0;break;
            case 13:localCar.wheel_radius[0]=NAN;break;
            case 14:wrongWheel=1;break;
            case 15:backSurface.flags=SURFACE_FLAG_INTANGIBLE;break;
            case 16:enemy.behavior=bhvGoomba;break;
        }
        assert(!rocket_whomp_ground_pound(&enemy)&&enemy.oHealth==3&&!sends);
    }
    start(1,1);localActive=0;send_rest(1);now+=.21;
    assert(!rocket_whomp_ground_pound(&enemy)); // stale remote parking is not evidence
    start(1,1);localCar=resting(100);send_rest(1);observed_native_loop();
    assert(enemy.oHealth==2&&sends==1); // simultaneous parked cars, one native consequence
    puts("PASS four-wheel Whomp contact: upright rest/landing, small loot, king cooldown/new cycle, local/remote authority, duplicates/stale poses, side/underneath/partial contact rejection");
}
static void flip_native_tests(void){
    for(int client=0;client<2;client++)for(int remote=0;remote<2;remote++){
        start(1,1);gNetworkType=client?NT_CLIENT:NT_SERVER;
        if(client){gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;}
        localActive=!remote;
        for(unsigned stage=0;stage<3;stage++){
            // Separate native vulnerability windows; never damage shake/rise.
            enemy.oSubAction=0;enemy.oTimer=10;
            localCar=flip_down(45,100+8*stage,0);
            if(remote)send_flip(1,45,1+2*stage,0);
            gGlobalTimer++;observed_native_loop();assert(enemy.oHealth==3-(int)stage);
            localCar=flip_down(1,104+8*stage,1);
            if(remote)send_flip(1,1,2+2*stage,1);
            gGlobalTimer++;observed_native_loop();assert(enemy.oHealth==2-(int)stage&&sends==(int)stage+1);
            assert(!rocket_whomp_ground_pound(&enemy)); // Duplicate physical tick.
        }
        assert(enemy.oAction==8&&!stars);whomp_act_8();assert(stars==1);
    }
    start(0,1);localCar=flip_down(45,100,0);observed_native_loop();
    localCar=flip_down(1,104,1);gGlobalTimer++;observed_native_loop();
    assert(enemy.oAction==8&&loot==5&&enemy.oNumLootCoins==0&&sends==1);
    for(int reset=0;reset<5;reset++){
        start(1,1);localActive=0;send_flip(1,45,1,0);assert(!rocket_whomp_ground_pound(&enemy));
        if(reset==0)character_net_clear(1);
        if(reset==1)authorityEpoch++;
        if(reset==2){CharacterNetState mario={0};mario.kind=CNET_MARIO;mario.epoch=1;mario.sequence=2;assert(character_net_accept(1,&mario));}
        if(reset==3)rocket_whomp_forget(&enemy);
        if(reset==4)authority=0;
        send_flip(1,1,3,1);gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    }
    start(1,1);localCar=flip_down(45,100,0);send_flip(1,45,1,0);observed_native_loop();
    localCar=flip_down(1,104,1);send_flip(1,1,2,1);gGlobalTimer++;observed_native_loop();
    assert(enemy.oHealth==2&&sends==1);localActive=0;gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    puts("PASS native flips: host/client authority with local/remote zero-boost impacts, three separate king stages/star, small death/five coins, simultaneous/duplicate hits, reconnect/epoch/character/pool reset");
}

#ifndef ROCKET_WHOMP_FIXTURE_NO_MAIN
int main(void){
    resting_native_tests();
    flip_native_tests();
    collision_order_tests();
    start(1,0);assert(advance());sWhompCarGroundPound=1;king_whomp_on_ground();
    assert(enemy.oHealth==2&&enemy.oSubAction==1&&enemy.oAction==6&&!stars);
    king_whomp_on_ground();assert(enemy.oHealth==2); // Native subaction also debounces.
    for(int health=2;health>0;health--){
        enemy.oSubAction=0;king_whomp_on_ground();assert(enemy.oHealth==health-1);
    }
    assert(enemy.oAction==8&&!stars);sWhompCarGroundPound=0;whomp_act_8();assert(stars==1&&enemy.oAction==9);
    whomp_act_8();assert(stars==1); // Canonical native reward guard.
    start(0,0);assert(advance());sWhompCarGroundPound=1;whomp_on_ground();
    assert(enemy.oAction==8&&loot==5&&enemy.oNumLootCoins==0);whomp_act_8();assert(deleted==1&&!stars);
    start(1,0);groundPound=1;king_whomp_on_ground();assert(enemy.oHealth==2); // Native Mario untouched.
    start(1,1);groundPound=1;king_whomp_on_ground();assert(enemy.oHealth==2&&sends==1); // Native stage is reliable.
    start(0,0);groundPound=onPlatform=1;whomp_on_ground();assert(loot==5&&enemy.oAction==8);
    start(0,0);onPlatform=1;whomp_on_ground();assert(loot==1&&enemy.oAction==6); // Native step reward.
    enemy.oSubAction=0;sWhompCarGroundPound=1;whomp_on_ground();
    assert(loot==6&&enemy.oAction==8); // Ground pound has its own native five, after a step coin.
    start(1,0);enemy.oAction=5;assert(!advance());
    start(1,0);enemy.oSubAction=1;assert(!advance());
    start(1,0);enemy.oSubAction=10;assert(!advance());
    start(1,0);enemy.oFaceAnglePitch=0;assert(!advance());
    start(1,0);backSurface.object=&players[0];assert(!advance());
    start(1,0);backSurface.normal.y=.5f;assert(!advance());
    start(1,0);visible=0;assert(!advance());
    start(1,0);gTimeStopState=TIME_STOP_ACTIVE;assert(!advance());
    start(1,0);sCurrPlayMode=PLAY_MODE_PAUSED;assert(!advance());
    start(1,1);authority=0;assert(!advance()); // Follower never predicts damage.
    start(1,1);assert(!rocket_whomp_ground_pound(&enemy));authorityEpoch++;localCar=down(1,104);gGlobalTimer++;
    assert(!rocket_whomp_ground_pound(&enemy)); // Lease handoff has no inherited sweep.
    start(1,1);localActive=0;send_down(1,45,1);assert(!rocket_whomp_ground_pound(&enemy));
    send_down(1,1,2);gGlobalTimer++;assert(rocket_whomp_ground_pound(&enemy));
    assert(!rocket_whomp_ground_pound(&enemy)); // Raw duplicate cannot hit twice.
    start(1,1);send_down(1,45,1);assert(!rocket_whomp_ground_pound(&enemy));
    localCar=down(1,104);send_down(1,1,2);gGlobalTimer++;assert(rocket_whomp_ground_pound(&enemy));
    localActive=0;gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy)); // Both simultaneous entries consumed.
    start(1,1);localActive=0;send_down(1,45,1);assert(!rocket_whomp_ground_pound(&enemy));
    character_net_clear(1);send_down(1,1,2);gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    start(1,1);localActive=0;send_down(1,45,1);assert(!rocket_whomp_ground_pound(&enemy));
    send_down(1,1,2);now+=.21;gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    start(1,1);localActive=0;send_down(1,45,1);assert(!rocket_whomp_ground_pound(&enemy));
    send_down(1,1,2);gMarioStates[1].action=ACT_HARD_BACKWARD_AIR_KB;gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    start(1,0);assert(!rocket_whomp_ground_pound(&enemy));rocket_whomp_forget(&enemy);
    localCar=down(1,104);gGlobalTimer++;assert(!rocket_whomp_ground_pound(&enemy));
    puts("PASS Whomp adapter/native source: staged king damage/star, small death/loot, Mario attacks, geometry, native collision ordering, authority, CNET replay/reconnect/stale/injury and pool lifetime");
}

#endif
