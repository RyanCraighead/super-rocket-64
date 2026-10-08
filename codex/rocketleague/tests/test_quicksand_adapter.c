/* Actual adapter plus native sand bridge; explicit existing runtime fixture. */
#define ROCKET_QUICKSAND_REAL_TEST
#define main baseline_adapter_main
#include "test_adapter.c"
#undef main
#include "pc/lua/smlua.h"
bool gDjuiInMainMenu;
static int sandDeaths;
bool smlua_call_event_hooks_HOOK_ALLOW_HAZARD_SURFACE(struct MarioState*m,s32 type,bool*allow){(void)m;assert(type==HAZARD_TYPE_QUICKSAND);*allow=true;return false;}
void update_mario_sound_and_camera(struct MarioState*m){(void)m;}
s32 drop_and_set_mario_action(struct MarioState*m,u32 action,u32 arg){sandDeaths++;return set_mario_action(m,action,arg);}
#include "quicksand-update.inc.c"
static void sand_setup(unsigned type){
    fresh();step();chimneyGeometry=1;chimneyFloorHeight=0;nativeFloor->type=type;nativeFloor->normal.y=1;
    object.hitboxHeight=160;mario.floor=nativeFloor;pose.position[1]=34;
    for(int i=0;i<4;i++){pose.wheel_contacts[i]=1;pose.wheel_radius[i]=32;pose.wheel_position[i][1]=32;}
    sandDeaths=0;
}
int main(void){
    sand_setup(SURFACE_QUICKSAND);int resetBefore=resets;
    for(int i=0;i<125;i++)step();assert(mario.quicksandDepth==60&&pose.quicksand_depth==60);
    assert(resets==resetBefore&&player==&mario&&draw); // depth no longer strands controls
    controller.buttonDown=A_BUTTON;step();assert(!observed.jump&&mario.quicksandDepth<60);
    for(int i=0;i<12;i++)step();float depth=mario.quicksandDepth;
    for(int i=0;i<10;i++)step();assert(mario.quicksandDepth>depth); // held cannot repeat
    controller.buttonDown=0;step();controller.buttonDown=A_BUTTON;depth=mario.quicksandDepth;
    step();assert(mario.quicksandDepth<depth&&!observed.jump&&resets==resetBefore);
    nativeFloor->type=SURFACE_DEFAULT;step();assert(mario.quicksandDepth==0&&pose.quicksand_depth==0);
    sand_setup(SURFACE_INSTANT_MOVING_QUICKSAND);gGlobalTimer++;
    assert(!rocket_adapter_update(&mario)&&mario.action==ACT_QUICKSAND_DEATH&&sandDeaths==1&&!draw);
    sand_setup(SURFACE_DEEP_QUICKSAND);mario.quicksandDepth=159.9f;gGlobalTimer++;
    assert(!rocket_adapter_update(&mario)&&sandDeaths==1&&mario.action==ACT_QUICKSAND_DEATH);
    sand_setup(SURFACE_QUICKSAND);step();float saved=mario.quicksandDepth;mario.freeze=1;step();
    assert(mario.quicksandDepth==saved);mario.freeze=0;sCurrPlayMode=PLAY_MODE_PAUSED;step();assert(mario.quicksandDepth==saved);
    puts("PASS actual adapter: retained buried controls, native mapped extraction, no repeated reset, dry exit, instant/deep handoff, pause/freeze");
}
