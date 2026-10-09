/* Production Lakitu init/trigger/flight, NPC action, cutscene dialog dispatch and
 * save flags/checksum/commit. Boundaries: character pose, scene/audio services,
 * camera scheduling, UI rendering and a private synthetic EEPROM file. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sm64.h"
#include "game/area.h"
#include "game/save_file.h"
#include "game/level_update.h"
#include "game/ingame_menu.h"
#include "game/hardcoded.h"
#include "game/mario.h"
#include "game/camera.h"
#include "game/mario_actions_cutscene.h"
#include "game/object_helpers.h"
#include "game/obj_behaviors.h"
#include "game/obj_behaviors_2.h"
#include "game/object_list_processor.h"
#include "game/spawn_sound.h"
#include "game/rocket_welcome.h"
#include "game/rocket_adapter.h"
#include "engine/math_util.h"
#include "pc/network/network.h"
#include "pc/cliopts.h"
#include "pc/rocket_runtime.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "object_constants.h"
#include "level_table.h"
#include "seq_ids.h"
#include "game/game_init.h"

static unsigned checks,writes,opens,deleted,clouds,frames;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"welcome:%d %s\n",__LINE__,#x);exit(1);}}while(0)
struct CLIOptions gCLIOpts;
struct BehaviorValues gBehaviorValues;
struct LevelValues gLevelValues;
struct ServerSettings gServerSettings;
struct WarpDest sWarpDest;
s16 sDelayedWarpOp,gChangeLevel;
struct MarioState gMarioStates[MAX_PLAYERS];
struct Object *gCurrentObject,*gContinueDialogFunctionObject;
u8 (*gContinueDialogFunction)(void);
static BehaviorScript *localDialogNPCBehavior;
struct SaveBuffer gSaveBuffer;
s8 gSaveFileModified;
u8 gSaveFileUsingBackupSlot;
s16 gCurrSaveFileNum,gCurrLevelNum,sCurrPlayMode;
u32 gGlobalTimer;
bool gNeverEnteredCastle,gDjuiInMainMenu;
struct WarpTransition gWarpTransition;
enum NetworkType gNetworkType;
struct CreditsEntry *gCurrCreditsEntry;
static struct Camera camera;
struct Camera *gCamera=&camera;
struct LakituState gLakituState;
u32 gTimeStopState;
static struct Object lakitu,player,cloud;
static struct Area area;
static struct Surface floorSurface;
static struct MarioBodyState bodyState;
static RocketSnapshot car;
static int selected=1,poseReady=1,blocked,allowDialog=1;
static FILE *eeprom;
#define o gCurrentObject
static u8 lakituTargetLocalIndex=UNKNOWN_LOCAL_INDEX;
const BehaviorScript bhvCloud[]={0};
static const BehaviorScript lakituBehavior[]={1};
int rocket_adapter_car_selected(void){return selected;}
int rocket_runtime_snapshot(RocketSnapshot *s){*s=car;return poseReady;}
int character_wheel_blocks_gameplay(void){return blocked;}
void obj_mark_for_deletion(struct Object *obj){obj->activeFlags=ACTIVE_FLAG_DEACTIVATED;deleted++;}
bool sync_object_is_initialized(u32 id){(void)id;return true;}
struct SyncObject *sync_object_init(struct Object *obj,float distance){(void)obj;(void)distance;return NULL;}
void sync_object_init_field_with_size(struct Object *obj,void *field,u8 size){(void)obj;(void)field;(void)size;}
struct MarioState *nearest_mario_state_to_object(struct Object *obj){(void)obj;return &gMarioStates[0];}
struct Object *spawn_object_relative_with_scale(s16 p,s16 x,s16 y,s16 z,f32 s,struct Object *parent,s32 model,const BehaviorScript *behavior){
    (void)p;(void)x;(void)y;(void)z;(void)s;(void)parent;(void)model;CHECK(behavior==bhvCloud);clouds++;return &cloud;
}
void cur_obj_play_sound_1(s32 sound){(void)sound;}
void play_music(u8 playerIndex,u16 args,u16 fade){(void)playerIndex;(void)args;(void)fade;}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){m->prevAction=m->action;m->action=action;m->actionArg=arg;m->actionState=0;m->actionTimer=0;return 1;}
s16 set_character_animation(struct MarioState *m,enum CharacterAnimID anim){(void)m;(void)anim;return 0;}
const BehaviorScript *smlua_override_behavior(const BehaviorScript *behavior){return behavior;}
s16 mario_obj_angle_to_object(struct MarioState *m,struct Object *obj){return obj_angle_to_object(m->marioObj,obj);}

#define INVALID_FILE_INDEX(i) ((u32)(i)>=NUM_SAVE_FILES)
#define INVALID_SRC_SLOT(i) ((u32)(i)>=2)
#define SAVE_FILE_MAGIC 0x4441
static s32 write_eeprom_data(void *data,s32 size,uintptr_t offset){
    CHECK(offset==(uintptr_t)((u8*)&gSaveBuffer.files[gCurrSaveFileNum-1][0]-(u8*)&gSaveBuffer));
    CHECK(size==2*sizeof(struct SaveFile));CHECK(fseek(eeprom,(long)offset,SEEK_SET)==0);
    CHECK(fwrite(data,1,size,eeprom)==(size_t)size);fflush(eeprom);writes++;return 0;
}
static void save_main_menu_data(void){}
void network_send_save_file(s32 file){(void)file;CHECK(0);}
void network_send_save_set_flag(s32 file,s32 course,u8 stars,u32 flags){(void)course;CHECK(file==gCurrSaveFileNum-1&&stars==0&&flags==SAVE_FLAG_FILE_EXISTS);}
#include "save.inc.c"

s16 gDialogID=DIALOG_NONE;
s8 gDialogBoxType;
static int handle_dialog_hook(s16 id){(void)id;return allowDialog;}
#define DIALOG_TYPE_ROTATE 0
#include "dialog.inc.c"
s32 get_dialog_id(void){return gDialogID;}
static u8 sObjectCutscene;
static s16 sCutsceneDialogResponse;
static s32 sCutsceneDialogID;
u8 gRecentCutscene;
static struct {Vec3s angle;} sCutsceneVars[9];
void start_object_cutscene(u8 cutscene,struct Object *obj){CHECK(obj==&lakitu);camera.cutscene=cutscene;sObjectCutscene=cutscene;}
void create_dialog_box_with_response(s32 id){(void)id;CHECK(0);}
s16 cutscene_object_without_dialog(u8 c,struct Object *obj){(void)c;(void)obj;CHECK(0);return 0;}
#include "camera.inc.c"
#include "npc.inc.c"
#include "move.inc.c"
#include "approach.inc.c"
#include "lakitu.inc.c"
void init_mario_from_save_file(void){}
void disable_warp_checkpoint(void){}
void save_file_move_cap_to_default_location(void){}
void select_mario_cam_mode(void){}
void fadeout_music(u16 framesToFade){(void)framesToFade;}
#include "level.inc.c"

static void fresh(int slot){
    rocket_welcome_reset(&lakitu);memset(&lakitu,0,sizeof lakitu);memset(&player,0,sizeof player);memset(gMarioStates,0,sizeof gMarioStates);
    memset(&gCLIOpts,0,sizeof gCLIOpts);gCLIOpts.offline=true;gCLIOpts.rocketCar=true;gCLIOpts.skipIntro=true;
    gCurrSaveFileNum=slot;gCurrLevelNum=LEVEL_CASTLE_GROUNDS;gNetworkType=NT_SERVER;gDjuiInMainMenu=false;
    gServerSettings.skipIntro=1;fake_lvl_init_from_save_file();CHECK(!gNeverEnteredCastle);
    gSaveFileUsingBackupSlot=0;sCurrPlayMode=PLAY_MODE_NORMAL;gWarpTransition.isActive=0;camera.cutscene=0;gDialogID=DIALOG_NONE;
    sObjectCutscene=0;sCutsceneDialogResponse=0;gRecentCutscene=0;gCurrentObject=&lakitu;
    memset(sCutsceneVars,0,sizeof sCutsceneVars);
    lakitu.activeFlags=ACTIVE_FLAG_ACTIVE;lakitu.behavior=lakituBehavior;lakitu.oBehParams2ndByte=1;
    lakitu.oPosX=11;lakitu.oPosY=803;lakitu.oPosZ=-3015;
    player.activeFlags=ACTIVE_FLAG_ACTIVE;player.oPosX=-1328;player.oPosY=260;player.oPosZ=4664;
    area.index=1;struct MarioState *m=&gMarioStates[0];m->marioObj=&player;m->area=&area;m->floor=&floorSurface;m->marioBodyState=&bodyState;
    m->action=ACT_IDLE;m->visibleToObjects=true;m->health=0x880;m->waterLevel=-11000;
    m->faceAngle[1]=(s16)0x8000;player.oFaceAngleYaw=m->faceAngle[1]; // Native MARIO_POS yaw 180.
    m->pos[0]=player.oPosX;m->pos[1]=player.oPosY;m->pos[2]=player.oPosZ;
    memset(&car,0,sizeof car);car.grounded=1;car.basis[7]=1;car.boost=65;selected=poseReady=1;blocked=0;allowDialog=1;
    gBehaviorValues.dialogs.LakituIntroDialog=DIALOG_034;
    deleted=clouds=opens=frames=0;gGlobalTimer+=100;
}
static void tick(void){
    ++gGlobalTimer;++frames;
    if(lakitu.oAction==0)camera_lakitu_intro_act_trigger_cutscene();
    else if(lakitu.oAction==1)camera_lakitu_intro_act_spawn_cloud();
    else camera_lakitu_intro_act_show_dialog();
    if(gMarioStates[0].action==ACT_READING_NPC_DIALOG)act_reading_npc_dialog(&gMarioStates[0]);
    player.oFaceAngleYaw=gMarioStates[0].faceAngle[1];
    if(camera.cutscene && gDialogID==DIALOG_NONE && !sCutsceneVars[8].angle[0]){
        cutscene_dialog_create_dialog_box(&camera);if(gDialogID!=DIALOG_NONE)opens++;
    }
}
static void finish(void){
    unsigned priorWrites=writes;
    unsigned limit=frames+600;while(!opens&&frames<limit)tick();
    if(!opens)fprintf(stderr,"flight: action=%d speed=%f pos=%f,%f,%f NPC=%x state=%u distance=%f\n",lakitu.oAction,lakitu.oCameraLakituSpeed,lakitu.oPosX,lakitu.oPosY,lakitu.oPosZ,gMarioStates[0].action,gMarioStates[0].actionState,dist_between_objects(&lakitu,&player));
    CHECK(opens==1);CHECK(gDialogID==DIALOG_034);
    printf("Fresh courtyard native dialog opened after %u host frames\n",frames);
    tick();CHECK(writes==priorWrites); // opening / viewing is not completion
    gDialogID=DIALOG_NONE;camera.cutscene=0;sObjectCutscene=0;sCutsceneDialogResponse=3;gRecentCutscene=CUTSCENE_DIALOG;
    for(int i=0;i<40&&!lakitu.oCameraLakituFinishedDialog;i++)tick();
    CHECK(lakitu.oCameraLakituFinishedDialog);CHECK(gMarioStates[0].action==ACT_IDLE);CHECK(writes==priorWrites+1);
}
int main(int argc,char **argv){
    // Windows CRT tmpfile targets the drive root, which may not be writable.
    // The Windows runner supplies its own private OS temporary-file path.
    CHECK(argc==1||argc==2);
    eeprom=argc==2?fopen(argv[1],"w+b"):tmpfile();CHECK(eeprom);memset(&gSaveBuffer,0,sizeof gSaveBuffer);
    for(int i=0;i<NUM_SAVE_FILES;i++)if(i!=2){gSaveBuffer.files[i][0].flags=SAVE_FLAG_FILE_EXISTS|SAVE_FLAG_HAVE_WING_CAP;gSaveBuffer.files[i][0].courseStars[0]=3;}
    struct SaveBuffer before=gSaveBuffer,disk=before;
    for(int i=0;i<NUM_SAVE_FILES;i++)for(int j=0;j<2;j++)bswap_savefile(&disk.files[i][j]);
    CHECK(fwrite(&disk,1,sizeof disk,eeprom)==sizeof disk);fflush(eeprom);
    fresh(3);CHECK(!save_file_exists(2));bhv_camera_lakitu_init();CHECK(!deleted);CHECK(lakitu.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE);
    for(int i=0;i<14;i++)tick();CHECK(lakitu.oAction==0);camera_lakitu_intro_act_trigger_cutscene();CHECK(lakitu.oAction==0);
    tick();CHECK(lakitu.oAction==1&&gMarioStates[0].action==ACT_READING_NPC_DIALOG);CHECK(!(lakitu.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
    finish();CHECK(clouds==1&&car.boost==65);CHECK(save_file_exists(2));
    before.files[2][0]=gSaveBuffer.files[2][0];CHECK(!memcmp(&before,&gSaveBuffer,sizeof before));CHECK(gSaveBuffer.files[2][0].flags==SAVE_FLAG_FILE_EXISTS);
    memset(&gSaveBuffer,0,sizeof gSaveBuffer);rewind(eeprom);CHECK(fread(&gSaveBuffer,1,sizeof gSaveBuffer,eeprom)==sizeof gSaveBuffer);
    for(int i=0;i<NUM_SAVE_FILES;i++)for(int j=0;j<2;j++)bswap_savefile(&gSaveBuffer.files[i][j]);
    CHECK(!memcmp(&before,&gSaveBuffer,sizeof before));
    CHECK(gSaveBuffer.files[2][0].signature.magic==SAVE_FILE_MAGIC);
    CHECK(gSaveBuffer.files[2][0].signature.chksum==calc_checksum((u8*)&gSaveBuffer.files[2][0],sizeof(struct SaveFile)));
    fresh(3);bhv_camera_lakitu_init();CHECK(deleted==1);for(int i=0;i<60;i++)tick();CHECK(opens==0&&writes==1);
    for(int slot=1;slot<=4;slot++){fresh(slot);bhv_camera_lakitu_init();CHECK(deleted==1);CHECK(!rocket_welcome_pending());}
    // New synthetic slot, switching away defers; switching back needs fresh stable frames.
    memset(gSaveBuffer.files[2],0,sizeof gSaveBuffer.files[2]);fresh(3);bhv_camera_lakitu_init();for(int i=0;i<10;i++)tick();
    selected=0;for(int i=0;i<30;i++)tick();CHECK(lakitu.oAction==0);selected=1;for(int i=0;i<14;i++)tick();CHECK(lakitu.oAction==0);tick();CHECK(lakitu.oAction==1);
    // Unsafe conditions each reset the waiting window without changing saves.
    for(int c=0;c<12;c++){
        fresh(3);bhv_camera_lakitu_init();
        switch(c){case 0:car.grounded=0;break;case 1:car.velocity[0]=300;break;case 2:car.boosting=1;break;case 3:gWarpTransition.isActive=1;break;case 4:blocked=1;break;case 5:sCurrPlayMode=PLAY_MODE_PAUSED;break;case 6:gMarioStates[0].action=ACT_INTRO_CUTSCENE;break;case 7:gMarioStates[0].waterLevel=300;break;case 8:car.basis[7]=NAN;break;case 9:gDialogID=DIALOG_001;break;case 10:sDelayedWarpOp=WARP_OP_EXIT;break;case 11:gMarioStates[0].health=0xff;break;}
        for(int i=0;i<35;i++)tick();CHECK(lakitu.oAction==0&&opens==0);CHECK(!save_file_exists(2));
    }
    // Lua veto / interrupted welcome never consumes the fresh-save marker.
    fresh(3);bhv_camera_lakitu_init();allowDialog=0;
    for(int i=0;i<600&&!camera.cutscene;i++)tick();CHECK(camera.cutscene&&opens==0&&gDialogID==DIALOG_NONE);
    camera.cutscene=0;sObjectCutscene=0;sCutsceneDialogResponse=3;gRecentCutscene=CUTSCENE_DIALOG;
    for(int i=0;i<40&&!lakitu.oCameraLakituFinishedDialog;i++)tick();
    CHECK(lakitu.oCameraLakituFinishedDialog&&writes==1&&!save_file_exists(2));
    fresh(3);bhv_camera_lakitu_init();for(int i=0;i<15;i++)tick();CHECK(lakitu.oAction==1);
    rocket_welcome_finished(&lakitu);CHECK(writes==1&&!save_file_exists(2));
    // Stable-frame counter works through wrap and does not reuse another slot's wait.
    fresh(3);bhv_camera_lakitu_init();gGlobalTimer=UINT32_MAX-7;
    for(int i=0;i<14;i++)tick();CHECK(lakitu.oAction==0);tick();CHECK(lakitu.oAction==1);
    // Classic Mario skip policy and original bridge trigger remain intact.
    fresh(3);gCLIOpts.rocketCar=false;selected=0;bhv_camera_lakitu_init();CHECK(deleted==1);
    fresh(3);gCLIOpts.rocketCar=false;selected=0;gNeverEnteredCastle=true;bhv_camera_lakitu_init();CHECK(!deleted);
    for(int i=0;i<30;i++)tick();CHECK(lakitu.oAction==0);
    player.oPosX=0;player.oPosY=900;player.oPosZ=-1000;tick();CHECK(lakitu.oAction==1);CHECK(!save_file_exists(2));
    fresh(3);gCLIOpts.offline=false;bhv_camera_lakitu_init();CHECK(deleted==1);
    fresh(3);gDjuiInMainMenu=true;bhv_camera_lakitu_init();CHECK(deleted==1);
    fresh(3);gSaveFileUsingBackupSlot=1;bhv_camera_lakitu_init();CHECK(deleted==1);
    CHECK(writes==1);
    for(int slot=1;slot<=4;slot++){
        memset(gSaveBuffer.files[slot-1],0,sizeof gSaveBuffer.files[slot-1]);struct SaveBuffer preserved=gSaveBuffer;
        fresh(slot);gMarioStates[0].faceAngle[1]=(s16)((slot-1)*0x4000);player.oFaceAngleYaw=gMarioStates[0].faceAngle[1];
        bhv_camera_lakitu_init();for(int i=0;i<15;i++)tick();finish();CHECK(save_file_exists(slot-1));
        preserved.files[slot-1][0]=gSaveBuffer.files[slot-1][0];CHECK(!memcmp(&preserved,&gSaveBuffer,sizeof preserved));
        selected=0;for(int i=0;i<10;i++)tick();selected=1;CHECK(!rocket_welcome_pending());
    }
    CHECK(writes==5);fclose(eeprom);if(argc==2)remove(argv[1]);printf("PASS fresh welcome: %u checks; production Lakitu/NPC/flight/dialog/serialized-save path, all unused slots, reload, four headings, switch, safety and classic/online gates\n",checks);
}
