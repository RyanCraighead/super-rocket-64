/* Production floor handler, action transitions, air quarter-steps, gravity,
 * burn action and car bridge. Flat lava/shore, input device and audiovisual
 * services are explicit fixtures; this is not a gameplay recording. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sm64.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/interaction.h"
#include "game/characters.h"
#include "game/rumble_init.h"
#include "game/level_update.h"
#include "game/hardcoded.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "pc/lua/smlua.h"
#include "pc/network/network.h"
#include "audio/external.h"
#include "../../../src/game/rocket_lava.c"
struct MarioState gMarioStates[MAX_PLAYERS];
struct ServerSettings gServerSettings;
struct LevelValues gLevelValues;
struct Surface gWaterSurfacePseudoFloor;
bool gDjuiInMainMenu;
u8 gFindWallDirectionActive,gFindWallDirectionAirborne;
Vec3f gFindWallDirection;
s16 sCurrPlayMode;
s8 gDebugLevelSelect;
s32 gRumblePakTimer;
f32 gGlobalSoundSource[3];
static struct Object player;
static struct MarioBodyState body;
static struct Controller controller;
static struct Area area;
static struct Surface lava,safe;
static RocketSnapshot pose;
static RocketInput mapped;
static int carSelected=1,carPose=1,inputReady=1,allowLava=1,allowDeath=1;
static int checks,warps,bubbles,canBubble,drops,floorMissing;
static float shore=100000,floorHeight;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Lava line %d: %s\n",__LINE__,#x);abort();}}while(0)
int rocket_adapter_car_selected(void){return carSelected;}
int rocket_adapter_body_snapshot(struct Object *obj,RocketSnapshot *out){
    struct MarioState *m=&gMarioStates[0];
    if(!carSelected||!carPose||obj!=&player||m->playerIndex||m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED||
        (m->action!=ACT_IDLE&&m->action!=ACT_FREEFALL))return 0;
    *out=pose;return 1;
}
int rocket_runtime_read_selected_input(const RocketInput *keyboard,RocketInput *input){
    *input=inputReady?mapped:(RocketInput){0};
    if(inputReady&&controller.rawStickY)*input=*keyboard;
    return inputReady;
}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){(void)x;(void)y;*floor=floorMissing?NULL:(z>=shore?&safe:&lava);return floorHeight;}
f32 find_water_level(f32 x,f32 z){(void)x;(void)z;return -10000;}
void resolve_and_return_wall_collisions_data(Vec3f pos,f32 offset,f32 radius,struct WallCollisionData *data){(void)pos;(void)offset;(void)radius;memset(data,0,sizeof(*data));}
f32 vec3f_mario_ceil(Vec3f pos,f32 floor,struct Surface **ceil){(void)pos;(void)floor;*ceil=NULL;return 20000;}
void mario_update_wall(struct MarioState *m,struct WallCollisionData *data){(void)m;(void)data;CHECK(0);}
s32 check_ledge_grab(struct MarioState *m,struct Surface *wall,Vec3f pos,Vec3f next){(void)m;(void)wall;(void)pos;(void)next;CHECK(0);return 0;}
u32 mario_get_terrain_sound_addend(struct MarioState *m){(void)m;return 0;}
s32 mario_get_floor_class(struct MarioState *m){(void)m;return SURFACE_CLASS_DEFAULT;}
s32 mario_facing_downhill(struct MarioState *m,s32 yaw){(void)m;(void)yaw;return 0;}
f32 get_additive_y_vel_for_jumps(void){CHECK(0);return 0;}
void mario_set_forward_vel(struct MarioState *m,f32 v){m->forwardVel=v;m->vel[0]=m->slideVelX=v*sins(m->faceAngle[1]);m->vel[2]=m->slideVelZ=v*coss(m->faceAngle[1]);}
s32 drop_and_set_mario_action(struct MarioState *m,u32 action,u32 arg){drops++;m->heldObj=NULL;return set_mario_action(m,action,arg);}
bool smlua_call_event_hooks_HOOK_BEFORE_SET_MARIO_ACTION(struct MarioState *m,u32 a,u32 arg,u32 *override){(void)m;(void)a;(void)arg;(void)override;return false;}
bool smlua_call_event_hooks_HOOK_ON_SET_MARIO_ACTION(struct MarioState *m){(void)m;return false;}
bool smlua_call_event_hooks_HOOK_ALLOW_HAZARD_SURFACE(struct MarioState *m,s32 type,bool *allow){(void)m;(void)type;*allow=allowLava;return false;}
bool smlua_call_event_hooks_HOOK_ON_DEATH(struct MarioState *m,bool *allow){(void)m;*allow=allowDeath;return false;}
bool smlua_call_event_hooks_HOOK_BEFORE_PHYS_STEP(struct MarioState *m,s32 type,u32 arg,s32 *result){(void)m;(void)type;(void)arg;(void)result;return false;}
bool smlua_call_event_hooks_HOOK_ON_COLLIDE_LEVEL_BOUNDS(struct MarioState *m){(void)m;return false;}
bool smlua_call_event_hooks_HOOK_ALLOW_FORCE_WATER_ACTION(struct MarioState *m,bool arg,bool *allow){(void)m;(void)arg;(void)allow;return false;}
bool smlua_call_action_hook(enum LuaActionHookType hook,struct MarioState *m,s32 *result){(void)hook;(void)m;(void)result;return false;}
void update_mario_sound_and_camera(struct MarioState *m){(void)m;}
void play_character_sound(struct MarioState *m,enum CharacterSound s){(void)m;(void)s;}
void play_character_sound_if_no_flag(struct MarioState *m,enum CharacterSound s,u32 flags){(void)m;(void)s;(void)flags;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
void play_mario_heavy_landing_sound(struct MarioState *m,u32 sound){(void)m;(void)sound;}
void mario_bonk_reflection(struct MarioState *m,u8 arg){(void)arg;m->faceAngle[1]+=0x8000;}
s16 set_character_animation(struct MarioState *m,enum CharacterAnimID anim){(void)m;(void)anim;return 0;}
void set_mario_particle_flags(struct MarioState *m,u32 flags,u8 clear){(void)m;(void)flags;(void)clear;}
void play_sound(s32 sound,f32 *pos){(void)sound;(void)pos;}
void reset_rumble_timers(struct MarioState *m){(void)m;}
u8 is_rumble_finished_and_queue_empty(void){return 1;}
bool mario_can_bubble(struct MarioState *m){(void)m;return canBubble;}
void mario_set_bubbled(struct MarioState *m){bubbles++;m->action=ACT_BUBBLED;}
s16 level_trigger_warp(struct MarioState *m,s32 op){(void)m;CHECK(op==WARP_OP_DEATH);warps++;return 20;}
void check_death_barrier(struct MarioState *m){(void)m;CHECK(0);}
void pss_begin_slide(struct MarioState *m){(void)m;CHECK(0);}
void pss_end_slide(struct MarioState *m){(void)m;CHECK(0);}
#include "lava-native.inc.c"
static struct MarioState *fresh(void){
    memset(gMarioStates,0,sizeof gMarioStates);memset(&player,0,sizeof player);memset(&pose,0,sizeof pose);
    memset(&controller,0,sizeof controller);memset(&mapped,0,sizeof mapped);memset(&lava,0,sizeof lava);memset(&safe,0,sizeof safe);
    memset(&area,0,sizeof area);memset(&body,0,sizeof body);memset(&gLevelValues,0,sizeof gLevelValues);
    struct MarioState *m=&gMarioStates[0];m->marioObj=&player;m->controller=&controller;m->marioBodyState=&body;
    m->area=&area;m->floor=&lava;m->health=0x880;m->flags=MARIO_CAP_ON_HEAD;m->action=ACT_IDLE;m->numLives=4;
    m->pos[1]=40;pose.position[1]=40;pose.basis[2]=pose.basis[3]=pose.basis[7]=1;
    pose.grounded=1;for(int i=0;i<4;i++){pose.wheel_contacts[i]=1;pose.wheel_radius[i]=16;pose.wheel_position[i][1]=16;pose.wheel_position[i][0]=(i&1)?-70:70;pose.wheel_position[i][2]=(i&2)?-90:90;}
    lava.type=SURFACE_BURNING;lava.normal.y=safe.normal.y=1;player.hitboxHeight=160;
    carSelected=carPose=inputReady=allowLava=allowDeath=1;warps=bubbles=canBubble=drops=floorMissing=0;
    gDjuiInMainMenu=0;sCurrPlayMode=0;floorHeight=0;shore=100000;return m;
}
static void test_contacts(void){
    for(int falling=0;falling<2;falling++){
        struct MarioState *m=fresh();if(falling)m->action=ACT_FREEFALL;
        CHECK(rocket_lava_floor_contact(m));mario_handle_special_floors(m);
        CHECK(m->action==ACT_LAVA_BOOST&&m->hurtCounter==12&&m->vel[1]==84&&m->forwardVel==16&&drops==1);
        for(int i=0;i<5;i++)mario_handle_special_floors(m);
        CHECK(m->hurtCounter==12&&drops==1); // No per-frame contact damage during bounce.
    }
    for(int gate=0;gate<10;gate++){
        struct MarioState *m=fresh();
        switch(gate){case 0:floorMissing=1;break;case 1:lava.type=SURFACE_DEFAULT;break;
            case 2:for(int i=0;i<4;i++)pose.wheel_contacts[i]=0;break;
            case 3:for(int i=0;i<4;i++)pose.wheel_position[i][1]+=9;break;
            case 4:carSelected=0;break;case 5:carPose=0;break;case 6:m->playerIndex=1;break;
            case 7:m->freeze=1;break;case 8:sCurrPlayMode=PLAY_MODE_PAUSED;break;
            case 9:pose.basis[0]=NAN;break;}
        CHECK(!rocket_lava_floor_contact(m));mario_handle_special_floors(m);CHECK(m->action==ACT_IDLE&&!m->hurtCounter);
    }
    struct MarioState *m=fresh();for(int i=0;i<4;i++)pose.wheel_contacts[i]=0;
    pose.basis[3]=pose.basis[7]=-1;pose.position[1]=ROCKET_BODY_UP_OFFSET+ROCKET_BODY_HALF_HEIGHT;
    CHECK(rocket_lava_floor_contact(m));mario_handle_special_floors(m);CHECK(m->action==ACT_LAVA_BOOST);
    m=fresh();shore=0;CHECK(rocket_lava_floor_contact(m)); // Rear tires on lava, origin's native floor safe.
    m->floor=&safe;mario_handle_special_floors(m);CHECK(m->action==ACT_LAVA_BOOST&&m->hurtCounter==12);
    m=fresh();allowLava=0;mario_handle_special_floors(m);CHECK(m->action==ACT_IDLE&&!m->hurtCounter&&!drops);
    m=fresh();m->flags|=MARIO_METAL_CAP;mario_handle_special_floors(m);CHECK(m->action==ACT_LAVA_BOOST&&!m->hurtCounter);
    m=fresh();m->flags=0;mario_handle_special_floors(m);CHECK(m->hurtCounter==18);
    m=fresh();m->invincTimer=30;mario_handle_special_floors(m);CHECK(m->hurtCounter==12&&m->invincTimer==30); // Native floor rules do not invent hit-timer immunity.
    m=fresh();carSelected=0;m->pos[1]=0;mario_handle_special_floors(m);CHECK(m->action==ACT_LAVA_BOOST&&m->forwardVel==0&&m->vel[1]==84);
    m=fresh();gDjuiInMainMenu=1;mario_handle_special_floors(m);CHECK(m->action==ACT_IDLE&&!drops);
}
static void test_escape(void){
    struct MarioState *m=fresh();pose.velocity[2]=2400;set_mario_action(m,ACT_LAVA_BOOST,0);
    CHECK(m->forwardVel==32&&m->vel[2]==32&&m->vel[1]==84);
    m=fresh();pose.velocity[0]=-600;set_mario_action(m,ACT_LAVA_BOOST,0);CHECK(m->forwardVel==20&&m->vel[0]<-19.9f&&fabsf(m->vel[2])<.02f);
    m=fresh();pose.velocity[2]=900;m->faceAngle[1]=0x8000;set_mario_action(m,ACT_LAVA_BOOST,1);CHECK(m->vel[2]<-29.9f&&m->vel[1]==84);
    m=fresh();shore=500;mapped.throttle=1;mario_handle_special_floors(m);
    int frames=0;float peak=0;
    while(m->action==ACT_LAVA_BOOST&&frames++<200){act_lava_boost(m);if(m->pos[1]>peak)peak=m->pos[1];CHECK(m->forwardVel<=32&&m->forwardVel>=0);}
    CHECK(m->action==ACT_LAVA_BOOST_LAND&&m->pos[2]>500&&m->hurtCounter==12&&peak>1000&&!warps);
    m=fresh();mapped.throttle=1;mario_handle_special_floors(m);frames=0;
    while(m->hurtCounter==12&&frames++<100)act_lava_boost(m);
    CHECK(m->hurtCounter==24&&m->vel[1]==84&&m->pos[2]>500); // Real native relanding adds damage and another bounce.
    m=fresh();mapped.throttle=1;mario_handle_special_floors(m);frames=0;
    while(!warps&&frames++<250){act_lava_boost(m);update_mario_health(m);}
    CHECK(warps==1&&m->health==0xff&&frames<250); // Sustained lava still drains real native HP and kills.
    m=fresh();m->flags|=MARIO_METAL_CAP;mapped.throttle=1;mario_handle_special_floors(m);
    for(int i=0;i<130;i++){act_lava_boost(m);update_mario_health(m);}
    CHECK(!warps&&!m->hurtCounter&&m->health==0x880&&m->action==ACT_LAVA_BOOST);
    m->flags&=~MARIO_METAL_CAP;frames=0;while(!m->hurtCounter&&frames++<100)act_lava_boost(m);
    CHECK(m->hurtCounter==12&&m->vel[1]==84); // Expiry resumes native damage on the next real landing.
    m=fresh();mapped.throttle=1;mapped.steer=1;set_mario_action(m,ACT_LAVA_BOOST,0);
    CHECK(rocket_lava_update(m)&&m->faceAngle[1]==512&&m->forwardVel==17&&m->vel[1]==84&&m->vel[0]>0);
    mapped.throttle=-1;mapped.steer=0;for(int i=0;i<50;i++)rocket_lava_update(m);CHECK(m->forwardVel==0&&m->vel[1]==84);
    mapped.throttle=1;mapped.steer=-1;inputReady=0;rocket_lava_update(m);CHECK(m->forwardVel==0&&m->faceAngle[1]==512);
    inputReady=1;m->freeze=1;rocket_lava_update(m);CHECK(m->forwardVel==0&&m->faceAngle[1]==512);m->freeze=0;
    sCurrPlayMode=PLAY_MODE_PAUSED;rocket_lava_update(m);CHECK(m->forwardVel==0);sCurrPlayMode=0;
    m->action=ACT_IDLE;CHECK(!rocket_lava_update(m));m->action=ACT_LAVA_BOOST;m->playerIndex=1;CHECK(!rocket_lava_update(m));
    m=fresh();controller.rawStickY=80;controller.rawStickX=80;set_mario_action(m,ACT_LAVA_BOOST,0);rocket_lava_update(m);CHECK(m->forwardVel==17&&m->faceAngle[1]==-512);
    m=fresh();set_mario_action(m,ACT_LAVA_BOOST,0);m->health=0xff;act_lava_boost(m);CHECK(warps==1&&!bubbles);
    m=fresh();set_mario_action(m,ACT_LAVA_BOOST,0);m->health=0xff;allowDeath=0;act_lava_boost(m);CHECK(!warps&&!bubbles);
    m=fresh();set_mario_action(m,ACT_LAVA_BOOST,0);m->health=0xff;canBubble=1;act_lava_boost(m);CHECK(bubbles==1&&!warps&&m->action==ACT_BUBBLED);
}
int main(void){test_contacts();test_escape();printf("PASS lava: %d real native floor/action/air-step/gravity/bounce/landing/death and bounded car-control checks\n",checks);}
