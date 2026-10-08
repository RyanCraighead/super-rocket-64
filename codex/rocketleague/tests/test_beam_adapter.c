/* Actual adapter/input merge/beam integration. Runtime and floor are explicit
 * component fixtures; the native warp itself runs separately in test_beam.c. */
#define ROCKET_BEAM_REAL_TEST
#define main adapter_fixture_main
#define level_trigger_warp adapter_fixture_warp
#include "test_adapter.c"
#undef level_trigger_warp
#undef main
#include "pc/rocket_bindings.h"
#include "game/save_file.h"
static unsigned beamWarps,beamChecks;
static struct ObjectWarpNode beamNode;
s16 gCurrSaveFileNum=1;
#define BEAM_CHECK(x) do{beamChecks++;assert(x);}while(0)
s32 save_file_get_total_star_count(s32 file,s32 min,s32 max){BEAM_CHECK(file==0&&min==COURSE_MIN-1&&max==COURSE_MAX-1);return 10;}
s16 level_trigger_warp(struct MarioState *m,s32 op){BEAM_CHECK(m==&mario&&op==WARP_OP_LOOK_UP);beamWarps++;sDelayedWarpOp=op;return 30;}
static void setup(void){
    fresh();gCurrLevelNum=LEVEL_CASTLE;testArea.index=1;gLevelValues.wingCapLookUpReq=10;
    nativeFloor->type=SURFACE_LOOK_UP_WARP;beamNode.node.id=WARP_NODE_F2;beamNode.next=NULL;testArea.warpNodes=&beamNode;
    beamWarps=0;step();pose.boost=23;
    for(int i=0;i<4;i++)pose.wheel_contacts[i]=1;
}
static void frames(unsigned n){while(n--)step();}
static void wait_and_fire(void){frames(59);BEAM_CHECK(!beamWarps);frames(1);BEAM_CHECK(beamWarps==1&&pose.boost==23&&resets==1);}
int main(void){
    setup();wait_and_fire();frames(100);BEAM_CHECK(beamWarps==1);
    for(int gate=0;gate<7;gate++){
        setup();frames(59);
        switch(gate){case 0:uiBlocked=1;break;case 1:mario.freeze=1;break;case 2:sCurrPlayMode=PLAY_MODE_PAUSED;break;
        case 3:controller.buttonDown=A_BUTTON;break;case 4:controller.buttonDown=B_BUTTON;break;case 5:controller.rawStickY=80;break;case 6:controller.rawStickX=80;break;}
        frames(1);BEAM_CHECK(!beamWarps);
        uiBlocked=mario.freeze=0;sCurrPlayMode=PLAY_MODE_NORMAL;controller.buttonDown=controller.rawStickX=controller.rawStickY=0;
        wait_and_fire();
    }
    const int keys[]={-1,0,1,2,3,9,10,7,8,11,12,13,14};
    for(int action=RA_THROTTLE;action<=RA_BOOST;action++)for(int binding=RB_SOUTH;binding<RB_COUNT;binding++){
        setup();fixtureGamepad.connected=1;frames(59);
        RocketBindings b=rocket_default_bindings;for(int i=0;i<RA_COUNT;i++)b.action[i]=RB_NONE;b.action[action]=binding;
        RocketPadSample sample={0};
        if(binding==RB_LT)sample.left_trigger=32767;else if(binding==RB_RT)sample.right_trigger=32767;else sample.buttons=1u<<keys[binding];
        rocket_bindings_apply(&b,&sample,&fixtureGamepad);frames(1);BEAM_CHECK(!beamWarps);
        sample=(RocketPadSample){0};rocket_bindings_apply(&b,&sample,&fixtureGamepad);wait_and_fire();
    }
    setup();frames(59);mario.action=ACT_READING_NPC_DIALOG;gGlobalTimer++;BEAM_CHECK(!rocket_adapter_update(&mario));
    mario.action=ACT_IDLE;step();for(int i=0;i<4;i++)pose.wheel_contacts[i]=1;pose.boost=23;
    frames(59);BEAM_CHECK(!beamWarps);frames(1);BEAM_CHECK(beamWarps==1&&resets==2);
    printf("PASS beam/adapter: %u checks, mapped controller/keyboard intent, focus/menu, freeze, pause and native suspension\n",beamChecks);
}
