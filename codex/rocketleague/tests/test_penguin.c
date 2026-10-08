/* Production adapter + roof carry + extracted native grab/drop/quest functions.
 * Runtime physics, animation, audio and transport are explicit inert boundaries. */
#define ROCKET_PENGUIN_REAL_TEST
#define main adapter_fixture_main
#include "test_adapter.c"
#undef main
#include "game/character_presentation.h"
#include "game/behavior_actions.h"
#include "game/object_helpers.h"
#include "game/obj_behaviors.h"
#include "game/obj_behaviors_2.h"
#include "game/camera.h"
#include "audio/external.h"

#include "pc/rocket_bindings.h"
#include "../../../src/game/rocket_penguin.c"
#define o gCurrentObject
struct Object *gCurrentObject,*gMarioObject;
struct BehaviorValues gBehaviorValues;
const BehaviorScript bhvSmallPenguin[]={31},bhvPenguinBaby[]={32},bhvUnused20E0[]={33};
const BehaviorScript bhvCarrySomething3[]={34},bhvCarrySomething4[]={35},bhvCarrySomething5[]={36};
const BehaviorScript bhvKoopaShellUnderwater[]={37};
static struct Object baby,mother,otherBaby,remoteObject;
static struct SyncObject syncBaby;
static int sends,stars,dialogDone,dialogID,remoteCar,remoteFresh,syncReady=1,syncOwned=1;
static RocketSnapshot remotePose;
int boss_net_managed(const struct Object *obj){(void)obj;return 0;}
int boss_net_simulates(const struct Object *obj){(void)obj;return 1;}
const BehaviorScript *smlua_override_behavior(const BehaviorScript *behavior){return behavior;}
const BehaviorScript *smlua_get_behavior_command(const BehaviorScript *behavior){return behavior;}
void *segmented_to_virtual(const void *address){return (void*)address;}
void stop_shell_music(void){}
bool sync_object_is_initialized(u32 id){return id&&syncReady;}
bool sync_object_is_owned_locally(u32 id){return id&&syncOwned;}
struct SyncObject *sync_object_get(u32 id){return id?&syncBaby:NULL;}
struct SyncObject *sync_object_init(struct Object *obj,f32 distance){(void)distance;obj->oSyncID=1;return &syncBaby;}
void network_send_object(struct Object *obj){assert(obj==&baby||obj==&mother||obj==&otherBaby);sends++;}
int character_net_is_car(unsigned index){return index==1&&remoteCar;}
int character_net_snapshot(unsigned index,RocketSnapshot *out){if(index!=1||!remoteFresh)return 0;*out=remotePose;return 1;}
int character_presentation_car_snapshot(RocketSnapshot *out){(void)out;return 0;}
void cur_obj_init_animation_with_sound(s32 index){(void)index;}
void obj_copy_pos(struct Object *dst,struct Object *src){dst->oPosX=src->oPosX;dst->oPosY=src->oPosY;dst->oPosZ=src->oPosZ;}
void obj_set_behavior(struct Object *obj,const BehaviorScript *behavior){obj->behavior=behavior;}
s32 cur_obj_has_behavior(const BehaviorScript *behavior){return o->behavior==behavior;}
void cur_obj_move_y(f32 gravity,f32 bounce,f32 buoyancy){(void)gravity;(void)bounce;(void)buoyancy;assert(0);}
f32 find_floor_height(f32 x,f32 y,f32 z){struct Surface *floor;return find_floor(x,y,z,&floor);}
void play_sound(s32 sound,f32 *position){(void)sound;(void)position;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
void cur_obj_scale(f32 scale){(void)scale;}
s32 cur_obj_check_anim_frame(s32 frame){(void)frame;return 0;}
s32 cur_obj_is_mario_on_platform(void){return 0;}
s32 cur_obj_can_mario_activate_textbox_2(struct MarioState *m,f32 d,f32 h){(void)m;(void)d;(void)h;return 0;}
s32 cur_obj_update_dialog_with_cutscene(struct MarioState *m,s32 a,s32 b,s32 c,s32 id,u8 (*f)(void)){
    assert(m==&mario&&a==2&&b==1&&c==CUTSCENE_DIALOG&&f());dialogID=id;return dialogDone;
}
struct Object *cur_obj_find_nearest_object_with_behavior(const BehaviorScript *behavior,f32 *distance){
    *distance=10000;struct Object *best=NULL;
    struct Object *objects[]={&baby,&otherBaby};
    for(unsigned i=0;i<2;i++)if(objects[i]->activeFlags&&objects[i]->behavior==behavior){
        f32 d=dist_between_objects(o,objects[i]);if(d<*distance){*distance=d;best=objects[i];}
    }
    return best;
}
void cur_obj_spawn_star_at_y_offset(f32 x,f32 y,f32 z,f32 offset){
    assert(x==1000&&y==2000&&z==3000&&offset==200);stars++;
}
void small_penguin_free_actions(void){cur_obj_become_tangible();cur_obj_enable_rendering();}
void cur_obj_get_thrown_or_placed(f32 f,f32 v,s32 a){assert(f==0&&v==0&&a==0);cur_obj_get_dropped();}
#include "penguin-native.inc.c"

static void ready(void){
    rocket_penguin_suspend(NULL);memset(&carry,0,sizeof carry);carry.boostHeld=1;
    fresh();memset(gMarioStates+1,0,sizeof gMarioStates-sizeof gMarioStates[0]);
    memset(&baby,0,sizeof baby);memset(&otherBaby,0,sizeof otherBaby);memset(&mother,0,sizeof mother);
    memset(&syncBaby,0,sizeof syncBaby);syncBaby.owned=1;syncReady=syncOwned=1;
    sends=stars=dialogDone=dialogID=remoteCar=remoteFresh=0;
    baby.behavior=bhvSmallPenguin;baby.activeFlags=ACTIVE_FLAG_ACTIVE;baby.oFlags=OBJ_FLAG_HOLDABLE;
    baby.oInteractType=INTERACT_GRABBABLE;baby.oPosZ=190;baby.oPosY=24;baby.oSyncID=1;
    baby.header.gfx.node.flags=GRAPH_RENDER_ACTIVE;baby.hitboxHeight=60;baby.hitboxRadius=50;
    objectLists[OBJ_LIST_GENACTOR].next=&baby.header;baby.header.next=&objectLists[OBJ_LIST_GENACTOR];
    gCurrentObject=gMarioObject=&object;gCurrLevelNum=LEVEL_CCM;mario.visibleToObjects=1;
    gLevelValues.floorLowerLimitMisc=-11000;
    gBehaviorValues.dialogs.TuxieMotherBabyFoundDialog=58;gBehaviorValues.dialogs.TuxieMotherBabyWrongDialog=59;
    gLevelValues.starPositions.TuxieMotherStarPos[0]=1000;gLevelValues.starPositions.TuxieMotherStarPos[1]=2000;gLevelValues.starPositions.TuxieMotherStarPos[2]=3000;
    step();
}
static void advance_frame(void){++gGlobalTimer;gCurrentObject=&object;rocket_adapter_prepare_interactions(&mario);rocket_adapter_update(&mario);}
static void key(int down){controller.buttonDown=down?B_BUTTON:0;advance_frame();}
static void acquire(void){key(0);assert(rocket_penguin_hint()==1);key(1);assert(mario.heldObj==&baby&&rocket_penguin_carried(&mario));}
static void native_baby(void){gCurrentObject=&baby;bhv_small_penguin_loop();gCurrentObject=&object;}
static void test_input_and_motion(void){
    ready();key(1);assert(!mario.heldObj); // Opening input must release first.
    acquire();assert(baby.oHeldState==HELD_HELD&&baby.heldByPlayerIndex==0&&sends==1);
    assert(rocket_adapter_body_snapshot(&object,&remotePose));
    assert(rocket_adapter_update(&mario)&&!observed.boost&&mario.action==ACT_IDLE);
    key(1);assert(mario.heldObj==&baby); // No repeats or accidental boost on held pickup.
    native_baby();assert((baby.header.gfx.node.flags&GRAPH_RENDER_ACTIVE)&&!(baby.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
    assert(baby.oIntangibleTimer==-1&&baby.oPosY==pose.position[1]+80&&baby.oPosZ==pose.position[2]-20);
    key(0);pose.velocity[2]=300;pose.grounded=0;pose.velocity[1]=100;
    rocket_penguin_update(&mario,&pose);assert(mario.heldObj==&baby); // Ordinary jumps stay available.
    key(1);assert(mario.heldObj==&baby&&rocket_penguin_hint()==3);
    assert(rocket_adapter_update(&mario)&&observed.boost); // Moving carry retains normal boost.
    pose.flipping=1;rocket_penguin_update(&mario,&pose);
    assert(!mario.heldObj&&baby.oHeldState==HELD_DROPPED&&sends==2);
    assert(baby.oPosY==pose.position[1]+80);native_baby();assert(baby.oHeldState==HELD_FREE&&baby.oIntangibleTimer==0);
    pose.flipping=0;pose.grounded=1;memset(pose.velocity,0,sizeof pose.velocity);
    baby.oPosZ=pose.position[2]+190;baby.oPosY=pose.position[1];step();acquire();
    key(0);key(1);assert(!mario.heldObj&&baby.oHeldState==HELD_DROPPED);
    assert(baby.oPosZ==pose.position[2]+205&&baby.oPosY==pose.position[1]+80);
    assert(rocket_adapter_update(&mario)&&!observed.boost);
    ready();RocketBindings bindings=rocket_default_bindings;bindings.action[RA_BOOST]=RB_LB;
    fixtureGamepad.connected=fixtureGamepad.isolated=1;
    RocketPadSample raw={0};rocket_bindings_apply(&bindings,&raw,&fixtureGamepad);advance_frame();
    raw.buttons=1u<<9;rocket_bindings_apply(&bindings,&raw,&fixtureGamepad);advance_frame();assert(mario.heldObj==&baby);
}
static void test_rejections(void){
    ready();gNetworkType=NT_SERVER;gCLIOpts.offline=1;acquire(); // Actual standalone authority marker.
    for(int which=0;which<24;which++){
        ready();key(0);
        switch(which){
            case 0:pose.velocity[2]=60;break;case 1:controller.rawStickY=80;break;
            case 2:pose.grounded=0;break;case 3:pose.basis[7]=.5f;break;
            case 4:baby.oPosZ=-190;break;case 5:baby.oPosZ=241;break;
            case 6:baby.oPosY=pose.position[1]+101;break;case 7:baby.oIntangibleTimer=-1;break;
            case 8:baby.oHeldState=HELD_HELD;break;case 9:baby.activeFlags=0;break;
            case 10:baby.header.gfx.activeAreaIndex=1;break;case 11:baby.behavior=bhvWarp;break;
            case 12:baby.oFlags=0;break;case 13:baby.oInteractionSubtype=INT_SUBTYPE_NOT_GRABBABLE;break;
            case 14:gMarioStates[1].heldObj=&baby;break;case 15:door_wall(2,0);break;
            case 16:mario.freeze=1;break;case 17:sCurrPlayMode=PLAY_MODE_PAUSED;break;
            case 18:baby.oPosX=NAN;break;
            case 19:baby.activeFlags|=ACTIVE_FLAG_DORMANT;break;
            case 20:baby.activeFlags|=ACTIVE_FLAG_IN_DIFFERENT_ROOM;break;
            case 21:baby.oSyncDeath=1;break;
            case 22:baby.header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;break;
            case 23:baby.header.gfx.node.flags&=~GRAPH_RENDER_ACTIVE;break;
        }
        key(1);assert(!mario.heldObj&&!sends&&!stars);
    }
    for(int which=0;which<7;which++){
        ready();gNetworkType=NT_SERVER;gCLIOpts.characterNet=1;gNetworkAreaLoaded=1;
        gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=1;
        switch(which){case 0:gCLIOpts.characterNet=0;break;case 1:gNetworkAreaLoaded=0;break;
            case 2:gNetworkAreaSyncing=1;break;case 3:gNetworkPlayerLocal->currAreaSyncValid=0;break;
            case 4:syncReady=0;break;case 5:syncOwned=0;break;case 6:baby.oSyncID=0;break;}
        key(0);key(1);assert(!mario.heldObj);
    }
    for(int client=0;client<2;client++){
        ready();gNetworkType=client?NT_CLIENT:NT_SERVER;gCLIOpts.characterNet=1;gNetworkAreaLoaded=1;
        gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=1;
        acquire();assert(sends==1&&baby.heldByPlayerIndex==0);
    }
}
static void test_native_quest(void){
    for(int wrong=0;wrong<2;wrong++){
        ready();baby.oBehParams=(u32)wrong<<16;mother.oPosZ=0;mother.activeFlags=ACTIVE_FLAG_ACTIVE;
        gCurrentObject=&mother;tuxies_mother_act_0();assert(!mother.oAction&&!stars); // Free nearby baby is not a delivery.
        acquire();native_baby();gCurrentObject=&mother;tuxies_mother_act_0();
        assert(mother.oAction==1&&mother.prevObj==&baby);tuxies_mother_act_1();
        assert(dialogID==(wrong?59:58)&&!mother.oSubAction&&!stars);
        mario.action=ACT_READING_NPC_DIALOG;rocket_penguin_suspend(&mario);
        assert(mario.heldObj==&baby&&rocket_adapter_car_selected());
        dialogDone=1;tuxies_mother_act_1();assert(mother.oSubAction==(wrong?2:1));
        assert(baby.oInteractionSubtype&INT_SUBTYPE_DROP_IMMEDIATELY);
        mario.action=ACT_HOLD_IDLE;advance_frame();assert(!mario.heldObj&&mario.action==ACT_IDLE);
        gCurrentObject=&mother;tuxies_mother_act_1();assert(!stars); // Wait for real native release transition.
        native_baby();gCurrentObject=&mother;tuxies_mother_act_1();
        assert(stars==!wrong&&mother.oAction==2&&baby.behavior==(wrong?bhvPenguinBaby:bhvUnused20E0));
    }
}
static void test_lifecycle(void){
    for(int reason=0;reason<9;reason++){
        ready();acquire();float roofY=baby.oPosY,roofZ=baby.oPosZ;
        switch(reason){
            case 0:mario.health=0xff;advance_frame();break;
            case 1:gCurrLevelNum++;mario.pos[2]=3000;advance_frame();break;
            case 2:mario.action=ACT_BACKWARD_GROUND_KB;rocket_adapter_suspend();break;
            case 3:rocket_adapter_set_selected(0);advance_frame();break;
            case 4:rocket_penguin_forget(&object);break;
            case 5:mario.action=ACT_DISAPPEARED;advance_frame();break;
            case 6:pose.basis[7]=-.1f;rocket_penguin_update(&mario,&pose);break;
            case 7:pose.water_mode=ROCKET_WATER_JET;rocket_penguin_update(&mario,&pose);break;
            case 8:mario_drop_held_object(&mario);advance_frame();break;
        }
        assert(!mario.heldObj&&baby.oHeldState==HELD_DROPPED&&!rocket_penguin_carried(&mario));
        if(reason!=6)assert(baby.oPosY==roofY&&baby.oPosZ==roofZ);
    }
    ready();acquire();rocket_adapter_forget_platform(&baby);assert(!mario.heldObj&&!carry.object);
    ready();acquire();mario.action=ACT_WAITING_FOR_DIALOG;rocket_adapter_suspend();assert(mario.heldObj==&baby);
    rocket_penguin_suspend(NULL);assert(mario.heldObj==&baby);mario.action=ACT_IDLE;rocket_penguin_suspend(NULL);assert(!mario.heldObj);
    ready();acquire();key(0);door_wall(2,0);key(1);assert(baby.oPosZ==pose.position[2]-20); // Blocked set-down falls from roof.
    ready();acquire();baby.heldByPlayerIndex=1;gMarioStates[1].heldObj=&baby;key(0);
    assert(!mario.heldObj&&!carry.object&&baby.oHeldState==HELD_HELD&&baby.heldByPlayerIndex==1&&sends==1);
    ready();acquire();sCurrPlayMode=PLAY_MODE_PAUSED;key(0);key(1);assert(mario.heldObj==&baby);
    sCurrPlayMode=PLAY_MODE_NORMAL;key(1);assert(mario.heldObj==&baby);key(0);key(1);assert(!mario.heldObj);
}
static void test_remote_render(void){
    ready();struct MarioState *remote=&gMarioStates[1];remote->playerIndex=1;remote->marioObj=&remoteObject;
    remote->heldObj=&baby;baby.oHeldState=HELD_HELD;baby.heldByPlayerIndex=1;
    remoteCar=remoteFresh=1;remotePose=pose;remotePose.position[0]=1000;remotePose.position[1]=500;
    baby.header.gfx.node.flags=0;native_baby();assert(baby.oPosX==1000&&baby.oPosY==580&&baby.oPosZ==-20);
    assert(rocket_penguin_carried(remote)&&(baby.header.gfx.node.flags&GRAPH_RENDER_ACTIVE));
    remoteCar=0;native_baby();assert(!rocket_penguin_carried(remote)&&!(baby.header.gfx.node.flags&GRAPH_RENDER_ACTIVE));
    remoteCar=1;remoteFresh=0;native_baby();assert(!(baby.header.gfx.node.flags&GRAPH_RENDER_ACTIVE));
    assert(!stars&&!sends);remote->heldObj=NULL;
}
int main(void){
    test_input_and_motion();test_rejections();test_native_quest();test_lifecycle();test_remote_render();
    puts("PASS penguin: real adapter, native grab/drop and matching/wrong-baby quest, render flags, remapped input, repeat gate, jumps/flips/re-pickup, walls, ownership gates, lifecycle and remote pose");
    return 0;
}
