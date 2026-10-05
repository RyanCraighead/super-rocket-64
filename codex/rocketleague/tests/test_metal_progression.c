#include <assert.h>
#include <stdio.h>
#include "../../../src/game/rocket_adapter.h"
#ifdef ROCKET_NATIVE_SLICE
#include "metal_progression_native.inc"
#else
#include "../../../src/game/behavior_actions.c"
#endif
struct Object *gCurrentObject,*gMarioObject;
struct MarioState gMarioStates[MAX_PLAYERS];
f32 gGlobalSoundSource[3];
static struct Object switchObject,playerObject;
static u32 saveFlags;
static unsigned saves,sends;
int rocket_adapter_vanish_switch_contact(struct Object *object){(void)object;return 0;}
u32 save_file_get_flags(void){return saveFlags;}
void save_file_set_flags(u32 flags){saveFlags|=flags;saves++;}
s32 cur_obj_is_mario_on_platform(void){return gMarioObject->platform==gCurrentObject;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
void network_send_object(struct Object *object){assert(object==gCurrentObject);sends++;}
bool sync_object_is_initialized(u32 id){(void)id;return true;}
struct SyncObject *sync_object_init(struct Object *object,float distance){(void)object;(void)distance;return NULL;}
void sync_object_init_field_with_size(struct Object *object,void *field,u8 size){(void)object;(void)field;(void)size;}
u8 is_player_active(struct MarioState *m){return m==&gMarioStates[0];}
void cur_obj_set_model(s32 model){(void)model;}
u16 smlua_model_util_load(enum ModelExtendedId model){return model;}
void cur_obj_scale(f32 scale){(void)scale;}
void cur_obj_scale_over_time(s32 a,s32 b,f32 c,f32 d){(void)a;(void)b;(void)c;(void)d;}
f32 lateral_dist_between_objects(struct Object *a,struct Object *b){return hypotf(a->oPosX-b->oPosX,a->oPosZ-b->oPosZ);}
void cur_obj_shake_screen(s32 shake){(void)shake;}
void queue_rumble_data_object(struct Object *object,s16 a,s16 b){(void)object;(void)a;(void)b;}
void play_sound(s32 sound,f32 *position){(void)sound;(void)position;}
int main(void){
    gCurrentObject=&switchObject;gMarioObject=&playerObject;gMarioStates[0].marioObj=gMarioObject;
    switchObject.oBehParams2ndByte=1; // Native index: wing=0, metal=1, vanish=2.
    exclamation_box_act_0();assert(switchObject.oAction==1&&!saveFlags); // Locked outline is not the unlock switch.
    cap_switch_act_1();assert(!saveFlags&&!saves&&!sends); // Mere proximity never unlocks.
    playerObject.platform=&switchObject;cap_switch_act_1();
    assert(saveFlags==SAVE_FLAG_HAVE_METAL_CAP&&saves==1&&sends==1&&switchObject.oAction==2);
    exclamation_box_act_0();assert(switchObject.oAction==2&&saves==1); // Save unlock makes boxes tangible.
    switchObject.oBehParams2ndByte=0;exclamation_box_act_0();assert(switchObject.oAction==1); // Wing still locked.
    switchObject.oBehParams2ndByte=2;exclamation_box_act_0();assert(switchObject.oAction==1); // Vanish still locked.
    memset(&switchObject,0,sizeof switchObject);playerObject.platform=NULL;
    gMarioStates[0].flags=MARIO_METAL_CAP;gMarioStates[0].action=ACT_METAL_WATER_STANDING;
    bhv_purple_switch_loop();assert(switchObject.oAction==PURPLE_SWITCH_IDLE);
    playerObject.platform=&switchObject;bhv_purple_switch_loop();assert(switchObject.oAction==PURPLE_SWITCH_PRESSED&&sends==2);
    switchObject.oTimer=3;bhv_purple_switch_loop();assert(switchObject.oAction==PURPLE_SWITCH_TICKING);
    assert(saveFlags==SAVE_FLAG_HAVE_METAL_CAP&&saves==1); // Gate switch is not a cap unlock.
    puts("PASS actual native cap unlock/save flag, locked/available colored blocks and underwater metal floor gate switch");
}
