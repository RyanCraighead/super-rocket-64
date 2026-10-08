/* Real beam bridge and verbatim native F2 scheduling/destination functions.
 * Surface queries, saved-star reads and audiovisual services are fixtures. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sm64.h"
#include "game/area.h"
#include "game/display.h"
#include "game/level_update.h"
#include "game/hardcoded.h"
#include "game/save_file.h"
#include "game/rocket_beam.h"
#include "game/object_list_processor.h"
#include "game/screen_transition.h"
#include "level_table.h"
#include "course_table.h"
#include "seq_ids.h"
#include "audio/external.h"
#include "surface_terrains.h"
#include "engine/surface_collision.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Beam line %d: %s\n",__LINE__,#x);abort();}}while(0)
struct MarioState gMarioStates[MAX_PLAYERS];
struct Area *gCurrentArea;
struct WarpTransition gWarpTransition;
struct WarpDest sWarpDest;
struct LevelValues gLevelValues;
s16 gCurrLevelNum,gCurrSaveFileNum,sCurrPlayMode,sDelayedWarpOp,sDelayedWarpTimer,sSourceWarpNodeId,gChangeLevel,gCurrActNum;
s32 sDelayedWarpArg;
s8 gDebugLevelSelect;
s16 gSavedCourseNum,gCurrActStarNum;
u32 gGlobalTimer,gTimeStopState;
bool gDjuiInMainMenu;
struct DemoInput *gCurrDemoInput;
struct CreditsEntry sCreditsSequence[2],*gCurrCreditsEntry;
f32 gGlobalSoundSource[3];
static struct Area area;
static struct Object body;
static struct Surface beam;
static struct ObjectWarpNode node;
static RocketSnapshot pose;
static RocketInput input;
static int selected,ready,stars,missing;
static unsigned transitions,sounds,fades,initiations,checkpoints,dialogs;
static float floorHeight;
int rocket_adapter_car_selected(void){return selected;}
int rocket_adapter_read_input(RocketInput *out){*out=input;return ready;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **out){(void)x;(void)y;(void)z;*out=missing?NULL:&beam;return floorHeight;}
s32 save_file_get_total_star_count(s32 file,s32 min,s32 max){CHECK(file==gCurrSaveFileNum-1&&min==COURSE_MIN-1&&max==COURSE_MAX-1);return stars;}
void play_transition(s16 type,s16 time,u8 red,u8 green,u8 blue){CHECK(type==WARP_TRANSITION_FADE_INTO_COLOR&&time==30&&red==255&&green==255&&blue==255);transitions++;}
void play_sound(s32 sound,f32 *pos){CHECK(sound==SOUND_MENU_STAR_SOUND&&pos==gGlobalSoundSource);sounds++;}
void fadeout_music(s16 time){CHECK(time==358);fades++;}
bool dynos_warp_to_start_level(void){CHECK(0);return false;}
void stop_demo(struct DjuiBase *b){(void)b;CHECK(0);}
s16 music_changed_through_warp(s16 id){(void)id;CHECK(0);return 0;}
void warp_special(s32 value){(void)value;CHECK(0);}
void lvl_skip_credits(void){CHECK(0);}
void reset_dialog_render_state(void){dialogs++;}
void level_set_transition(s16 time,void (*fn)(s16 *)){(void)time;(void)fn;CHECK(0);}
void check_if_should_set_warp_checkpoint(struct WarpNode *n){CHECK(n==&node.node);checkpoints++;}
void sound_banks_enable(u8 player,u16 mask){(void)player;(void)mask;CHECK(0);}
void sound_banks_disable(u8 player,u16 mask){(void)player;(void)mask;CHECK(0);}
void initiate_warp(s16 level,s16 areaIndex,s16 id,s32 arg){
    CHECK(level==LEVEL_TOTWC&&areaIndex==1&&id==0x0A&&!arg);
    sWarpDest=(struct WarpDest){.type=WARP_TYPE_CHANGE_LEVEL,.levelNum=level,.areaIdx=areaIndex,.nodeId=id,.arg=arg};initiations++;
}
#include "beam-native.inc.c"

static struct MarioState *fresh(void){
    rocket_beam_reset();memset(gMarioStates,0,sizeof gMarioStates);memset(&area,0,sizeof area);memset(&body,0,sizeof body);
    memset(&pose,0,sizeof pose);memset(&beam,0,sizeof beam);memset(&input,0,sizeof input);memset(&gWarpTransition,0,sizeof gWarpTransition);
    memset(&sWarpDest,0,sizeof sWarpDest);memset(&gLevelValues,0,sizeof gLevelValues);
    gCurrentArea=&area;area.index=1;area.warpNodes=&node;node.node=nativeBeamNode;node.next=NULL;
    gCurrLevelNum=LEVEL_CASTLE;gCurrSaveFileNum=2;gLevelValues.wingCapLookUpReq=10;
    sCurrPlayMode=PLAY_MODE_NORMAL;sDelayedWarpOp=sDelayedWarpTimer=sSourceWarpNodeId=sDelayedWarpArg=0;gTimeStopState=0;gGlobalTimer=100;
    transitions=sounds=fades=initiations=checkpoints=dialogs=0;selected=ready=1;stars=10;missing=0;floorHeight=0;
    struct MarioState *m=&gMarioStates[0];m->marioObj=&body;m->area=&area;m->action=ACT_IDLE;m->health=0x880;m->numCoins=29;m->capTimer=140;
    pose.grounded=1;pose.basis[2]=pose.basis[3]=pose.basis[7]=1;pose.position[1]=40;pose.boost=23;
    for(int i=0;i<4;i++)pose.wheel_contacts[i]=1;
    beam.type=SURFACE_LOOK_UP_WARP;
    return m;
}
static void frames(struct MarioState *m,unsigned count){for(unsigned i=0;i<count;i++){gGlobalTimer++;rocket_beam_update(m,&pose);}}
static void waits_then_fires(struct MarioState *m){
    frames(m,59);CHECK(!transitions&&sDelayedWarpOp==WARP_OP_NONE);frames(m,1);
    CHECK(transitions==1&&sounds==1&&fades==1&&sDelayedWarpOp==WARP_OP_LOOK_UP&&sDelayedWarpTimer==30&&sSourceWarpNodeId==WARP_NODE_F2);
    CHECK(m->health==0x880&&m->numCoins==29&&m->capTimer==140&&pose.boost==23);
}
int main(void){
    struct MarioState *m=fresh();waits_then_fires(m);frames(m,100);CHECK(transitions==1);
    for(int i=0;i<29;i++){initiate_delayed_warp();CHECK(!initiations);}initiate_delayed_warp();
    CHECK(initiations==1&&checkpoints==1&&dialogs==1&&sWarpDest.levelNum==LEVEL_TOTWC);
    CHECK(!level_trigger_warp(&gMarioStates[1],WARP_OP_LOOK_UP)&&transitions==1);
    // Duplicate render/update calls cannot shorten the two-second interval.
    m=fresh();for(int i=0;i<60;i++){for(int j=0;j<8;j++)rocket_beam_update(m,&pose);CHECK(transitions==(unsigned)(i==59));gGlobalTimer++;}
    // Skipped logic time never turns a short visit into a completed dwell.
    m=fresh();frames(m,59);gGlobalTimer+=1000;waits_then_fires(m);
    // Crossing the global frame wrap still counts exactly sixty distinct ticks.
    m=fresh();gGlobalTimer=UINT32_MAX-25;waits_then_fires(m);
    // Native configurable star requirement is read, never awarded or bypassed.
    m=fresh();stars=9;frames(m,200);CHECK(!transitions);stars=10;waits_then_fires(m);
    m=fresh();gLevelValues.wingCapLookUpReq=25;frames(m,100);CHECK(!transitions);stars=25;waits_then_fires(m);
    // Each eligibility failure resets an almost-completed dwell. Restore only
    // that condition so the same owner has to park a fresh full two seconds.
    for(int gate=0;gate<31;gate++){
        m=fresh();frames(m,59);struct Object replacement={0};
        switch(gate){
        case 0:selected=0;break;case 1:ready=0;break;case 2:m->freeze=1;break;
        case 3:sCurrPlayMode=PLAY_MODE_PAUSED;break;case 4:gTimeStopState=1;break;
        case 5:beam.type=SURFACE_DEFAULT;break;case 6:missing=1;break;
        case 7:pose.position[1]=100;break;case 8:pose.position[1]=-1;break;
        case 9:pose.grounded=0;break;case 10:pose.flipping=1;break;
        case 11:pose.basis[7]=.5f;break;case 12:pose.velocity[0]=31;break;
        case 13:pose.velocity[1]=31;break;case 14:pose.angular_velocity[2]=.21f;break;
        case 15:pose.wheel_contacts[3]=0;break;case 16:pose.water_mode=ROCKET_WATER_JET;break;
        case 17:input.throttle=.2f;break;case 18:input.steer=-.2f;break;
        case 19:input.jump=1;break;case 20:input.boost=1;break;
        case 21:m->action=ACT_FREEFALL;break;case 22:m->health=0xff;break;
        case 23:m->playerIndex=1;break;case 24:gCurrLevelNum=LEVEL_BOB;break;
        case 25:area.index=2;break;case 26:area.warpNodes=NULL;break;
        case 27:pose.position[0]=NAN;break;case 28:pose.velocity[2]=NAN;break;
        case 29:pose.angular_velocity[1]=NAN;break;case 30:m->marioObj=&replacement;break;
        }
        frames(m,1);CHECK(!transitions);
        selected=ready=1;m->freeze=0;sCurrPlayMode=PLAY_MODE_NORMAL;gTimeStopState=0;beam.type=SURFACE_LOOK_UP_WARP;missing=0;
        pose.position[0]=0;pose.position[1]=40;pose.grounded=1;pose.flipping=0;pose.basis[7]=1;
        memset(pose.velocity,0,sizeof pose.velocity);memset(pose.angular_velocity,0,sizeof pose.angular_velocity);
        pose.wheel_contacts[3]=1;pose.water_mode=ROCKET_WATER_DRY;input=(RocketInput){0};m->action=ACT_IDLE;m->health=0x880;m->playerIndex=0;
        gCurrLevelNum=LEVEL_CASTLE;area.index=1;area.warpNodes=&node;m->marioObj=&body;
        waits_then_fires(m);
    }
    for(int gate=0;gate<3;gate++){
        m=fresh();frames(m,59);if(gate==0)sDelayedWarpOp=WARP_OP_DEATH;else if(gate==1)sWarpDest.type=WARP_TYPE_CHANGE_LEVEL;else gWarpTransition.isActive=1;
        frames(m,100);CHECK(!transitions);sDelayedWarpOp=0;sWarpDest.type=0;gWarpTransition.isActive=0;waits_then_fires(m);
    }
    // Once fired, even a cleared native pending flag cannot retrigger while
    // still parked. Leaving the beam rearms a later complete visit.
    m=fresh();waits_then_fires(m);sDelayedWarpOp=0;frames(m,200);CHECK(transitions==1);
    beam.type=SURFACE_DEFAULT;frames(m,1);beam.type=SURFACE_LOOK_UP_WARP;transitions=sounds=fades=0;waits_then_fires(m);
    rocket_beam_update(NULL,&pose);rocket_beam_update(m,NULL);
    printf("PASS castle beam: %u checks, native star gate/F2 scheduling/destination and dwell lifecycle\n",checks);
}
