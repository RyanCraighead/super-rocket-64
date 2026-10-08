/* Actual native sinking/escape functions and car bridge, fixture floor queries. */
#include "sm64.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/area.h"
#include "game/level_update.h"
#include "pc/lua/smlua.h"
#include "pc/rocket_bindings.h"
#include "audio/external.h"
#include "../../../src/game/rocket_quicksand.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks,allowHazard=1,selectedCar=1,deaths,mixed;
static int warps,bubbles,allowDeath=1,canBubble,deathHooks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"quicksand line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
u32 gGlobalTimer;bool gDjuiInMainMenu;s16 gCurrLevelNum,sCurrPlayMode;
static struct MarioState mario;
static struct Object car;
static struct Area fixtureArea;
static struct Surface sand,pit;
static RocketSnapshot pose;
int rocket_adapter_car_selected(void){return selectedCar;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){(void)x;(void)y;*floor=mixed&&z>0?&pit:&sand;return 0;}
bool smlua_call_event_hooks_HOOK_ALLOW_HAZARD_SURFACE(struct MarioState *m,s32 type,bool *allow){CHECK(m&&type==HAZARD_TYPE_QUICKSAND);*allow=allowHazard;return false;}
void update_mario_sound_and_camera(struct MarioState *m){(void)m;}
s32 drop_and_set_mario_action(struct MarioState*m,u32 action,u32 arg){CHECK(action==ACT_QUICKSAND_DEATH);m->action=action;m->actionArg=arg;deaths++;return 1;}
u32 set_mario_action(struct MarioState*m,u32 action,u32 arg){m->action=action;m->actionArg=arg;return 1;}
void play_mario_jump_sound(struct MarioState*m){(void)m;}
s16 set_character_animation(struct MarioState*m,enum CharacterAnimID id){(void)m;(void)id;return 0;}
s32 apply_landing_accel(struct MarioState*m,f32 friction){(void)m;CHECK(friction==.95f);return 0;}
s32 perform_ground_step(struct MarioState*m){(void)m;return GROUND_STEP_NONE;}
void set_anim_to_frame(struct MarioState*m,s16 frame){CHECK(m&&frame==60);}
void play_character_sound_if_no_flag(struct MarioState*m,enum CharacterSound sound,u32 flags){(void)m;(void)sound;(void)flags;}
bool smlua_call_event_hooks_HOOK_ON_DEATH(struct MarioState*m,bool*allow){CHECK(m==&mario);deathHooks++;*allow=allowDeath;return false;}
bool mario_can_bubble(struct MarioState*m){CHECK(m==&mario);return canBubble;}
void mario_set_bubbled(struct MarioState*m){CHECK(m==&mario);bubbles++;}
s16 level_trigger_warp(struct MarioState*m,s32 op){CHECK(m==&mario&&op==WARP_OP_DEATH);warps++;return 1;}
s32 stationary_ground_step(struct MarioState*m){(void)m;return GROUND_STEP_NONE;}
void play_sound(s32 sound,f32*pos){(void)sound;(void)pos;}
#include "quicksand-native.inc.c"
static void fresh(unsigned type){
    rocket_quicksand_suspend();memset(&mario,0,sizeof mario);memset(&car,0,sizeof car);
    memset(&sand,0,sizeof sand);memset(&pit,0,sizeof pit);memset(&pose,0,sizeof pose);
    mario.marioObj=&car;mario.area=&fixtureArea;mario.action=ACT_IDLE;mario.health=0x880;mario.floor=&sand;
    sand.type=type;sand.normal.y=pit.normal.y=1;pit.type=SURFACE_INSTANT_MOVING_QUICKSAND;car.hitboxHeight=160;
    pose.basis[2]=pose.basis[3]=pose.basis[7]=1;pose.position[1]=34;pose.grounded=1;
    for(int i=0;i<4;i++){pose.wheel_contacts[i]=1;pose.wheel_radius[i]=32;pose.wheel_position[i][1]=32;pose.wheel_position[i][2]=(i&1)?90:-90;}
    allowHazard=selectedCar=1;deaths=mixed=0;gGlobalTimer=0;sCurrPlayMode=0;gDjuiInMainMenu=0;
    warps=bubbles=canBubble=deathHooks=0;allowDeath=1;
}
static int tick(int jump,int blocked){
    gGlobalTimer++;int action=rocket_quicksand_update(&mario,&pose);
    RocketInput input={0};input.jump=jump;rocket_quicksand_filter_input(&input,blocked);
    if(!action)CHECK(!rocket_quicksand_update(&mario,&pose));
    return input.jump;
}
int main(void){
    unsigned kinds[]={SURFACE_SHALLOW_QUICKSAND,SURFACE_SHALLOW_MOVING_QUICKSAND,SURFACE_QUICKSAND,SURFACE_MOVING_QUICKSAND};
    float caps[]={10,25,60,60};
    for(int k=0;k<4;k++){
        fresh(kinds[k]);for(int i=0;i<300;i++){CHECK(!tick(0,0));CHECK(mario.action==ACT_IDLE&&!deaths);}
        CHECK(mario.quicksandDepth==caps[k]&&rocket_quicksand_depth()==caps[k]);
    }
    for(int moving=0;moving<2;moving++){
        fresh(moving?SURFACE_DEEP_MOVING_QUICKSAND:SURFACE_DEEP_QUICKSAND);
        for(int i=0;i<317;i++)tick(0,0);CHECK(!deaths&&mario.quicksandDepth<160);
        tick(0,0);CHECK(deaths==1&&mario.action==ACT_QUICKSAND_DEATH);
    }
    for(int moving=0;moving<2;moving++){
        fresh(moving?SURFACE_INSTANT_MOVING_QUICKSAND:SURFACE_INSTANT_QUICKSAND);
        tick(0,0);CHECK(deaths==1&&mario.action==ACT_QUICKSAND_DEATH);
    }
    fresh(SURFACE_QUICKSAND);tick(0,0);float old=mario.quicksandDepth;
    CHECK(!rocket_quicksand_update(&mario,&pose)&&mario.quicksandDepth==old);
    mixed=1;CHECK(rocket_quicksand_update(&mario,&pose));CHECK(deaths==1); // newly entered pit same frame
    fresh(SURFACE_QUICKSAND);mario.flags=MARIO_METAL_CAP;
    tick(0,0);CHECK(mario.quicksandDepth>1); // Native sand does not grant Metal immunity.
    allowHazard=0;tick(0,0);CHECK(mario.quicksandDepth==0);allowHazard=1;
    // One mapped edge starts exactly the native extraction depth/time sequence.
    fresh(SURFACE_QUICKSAND);tick(0,0);mario.quicksandDepth=30;
    ++gGlobalTimer;CHECK(!rocket_quicksand_update(&mario,&pose));struct MarioState reference=mario;
    RocketInput input={0};input.jump=1;rocket_quicksand_filter_input(&input,0);CHECK(!input.jump);
    quicksand_jump_land_action(&reference,0,0,ACT_JUMP_LAND_STOP,ACT_FREEFALL);
    CHECK(mario.quicksandDepth==reference.quicksandDepth);
    for(int i=1;i<13;i++){
        tick(1,0);mario_update_quicksand(&reference,.25f);
        quicksand_jump_land_action(&reference,0,0,ACT_JUMP_LAND_STOP,ACT_FREEFALL);
        CHECK(mario.quicksandDepth==reference.quicksandDepth);
    }
    CHECK(reference.action==ACT_JUMP_LAND_STOP);old=mario.quicksandDepth;
    for(int i=0;i<10;i++)CHECK(!tick(1,0));CHECK(mario.quicksandDepth>old); // no held extraction repeat
    CHECK(!tick(0,0));old=mario.quicksandDepth;CHECK(!tick(1,0));CHECK(mario.quicksandDepth<old);
    fresh(SURFACE_QUICKSAND);tick(0,0);mario.quicksandDepth=15;
    for(int i=0;i<13;i++)CHECK(!tick(1,0));CHECK(mario.quicksandDepth<11);
    CHECK(!tick(1,0));tick(0,0);CHECK(tick(1,0)); // fresh ordinary jump after extraction
    fresh(SURFACE_QUICKSAND);tick(0,0);mario.quicksandDepth=30;old=mario.quicksandDepth;
    CHECK(!tick(1,1));CHECK(!tick(1,0));CHECK(mario.quicksandDepth>old); // focus/held gate
    tick(0,0);old=mario.quicksandDepth;tick(1,0);CHECK(mario.quicksandDepth<old);
    // Air/safe exit resets sink without moving body or granting an ability.
    pose.grounded=0;pose.position[1]+=1000;for(int i=0;i<4;i++){pose.wheel_contacts[i]=0;pose.wheel_position[i][1]+=1000;}
    RocketSnapshot unchanged=pose;tick(1,0);CHECK(mario.quicksandDepth==0&&!memcmp(&pose,&unchanged,sizeof pose));
    // Chassis roof contact preserves instant-pit death without tire support.
    fresh(SURFACE_INSTANT_QUICKSAND);pose.grounded=0;pose.position[1]=80;pose.basis[3]=-1;pose.basis[7]=-1;
    for(int i=0;i<4;i++)pose.wheel_contacts[i]=0;tick(0,0);CHECK(deaths==1);
    fresh(SURFACE_INSTANT_QUICKSAND);mario.playerIndex=1;tick(0,0);CHECK(!deaths&&mario.quicksandDepth==0);
    fresh(SURFACE_INSTANT_QUICKSAND);selectedCar=0;tick(0,0);CHECK(!deaths);
    fresh(SURFACE_QUICKSAND);mario.freeze=1;tick(1,0);CHECK(mario.quicksandDepth==0);
    /* Active extraction pauses even when the button remains held. Resume the
     * existing interval, never synthesize a new press on focus restoration. */
    fresh(SURFACE_QUICKSAND);tick(0,0);mario.quicksandDepth=60;tick(1,0);
    old=mario.quicksandDepth;int timer=escapeTimer;mario.freeze=1;
    for(int i=0;i<20;i++)CHECK(!tick(1,0));CHECK(mario.quicksandDepth==old&&escapeTimer==timer);
    mario.freeze=0;sCurrPlayMode=PLAY_MODE_PAUSED;
    for(int i=0;i<20;i++)CHECK(!tick(1,0));CHECK(mario.quicksandDepth==old&&escapeTimer==timer);
    sCurrPlayMode=0;tick(1,0);CHECK(mario.quicksandDepth<old&&escapeTimer==timer+1);
    /* Actual configurable controller mapping and keyboard merge feed the same
     * extraction edge. Cover every supported digital/analog binding. */
    const unsigned bits[]={0,0,1,2,3,9,10,7,8,11,12,13,14};
    for(unsigned binding=RB_SOUTH;binding<RB_COUNT;binding++){
        fresh(SURFACE_QUICKSAND);tick(0,0);mario.quicksandDepth=30;
        RocketBindings bindings=rocket_default_bindings;bindings.action[RA_JUMP]=binding;
        RocketPadSample raw={0};RocketGamepad pad={0};pad.connected=pad.isolated=1;
        if(binding==RB_LT)raw.left_trigger=32767;else if(binding==RB_RT)raw.right_trigger=32767;
        else raw.buttons=1u<<bits[binding];
        rocket_bindings_apply(&bindings,&raw,&pad);RocketInput keyboard={0};
        input=rocket_gamepad_merge(&keyboard,&pad);CHECK(input.jump);gGlobalTimer++;
        rocket_quicksand_filter_input(&input,0);CHECK(!input.jump&&mario.quicksandDepth<30);
        old=mario.quicksandDepth;input=rocket_gamepad_merge(&keyboard,&pad);
        rocket_quicksand_filter_input(&input,0);CHECK(!input.jump&&mario.quicksandDepth==old);
    }
    /* Actual native fatal action owns depth, death hook, bubble/warp and its
     * one-shot transition after the car hands control back. Engine services
     * are explicit fixtures; this is not a full-game warp session. */
    for(int mode=0;mode<3;mode++){
        fresh(SURFACE_INSTANT_QUICKSAND);tick(0,0);CHECK(mario.action==ACT_QUICKSAND_DEATH);
        rocket_quicksand_suspend();mario.numLives=3;canBubble=mode==1;allowDeath=mode!=2;
        for(int i=0;i<60;i++)act_quicksand_death(&mario);
        CHECK(mario.actionState==2&&deathHooks==1);
        CHECK(warps==(mode==0)&&bubbles==(mode==1));
    }
    printf("native quicksand bridge: %d checks passed\n",checks);
}
