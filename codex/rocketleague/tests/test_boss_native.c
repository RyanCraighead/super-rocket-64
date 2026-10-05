/* Compile the actual native behavior implementations. Floor/movement/audio/net
 * services are explicit fixture stubs; this is not a native game playthrough. */
#include <assert.h>
#include <stdio.h>
#include "../../../src/game/behavior_actions.c"
struct Object *gCurrentObject;
static struct Object bossObject,tailObject,mineObject;
struct MarioState *gMarioState;
const BehaviorScript bhvBowserBomb[]={0};
static float mineDistance;
static int placedCalls,movementCalls,sends;
static f32 requestedForward,requestedVertical;
void cur_obj_become_tangible(void){o->oIntangibleTimer=0;}
void cur_obj_become_intangible(void){o->oIntangibleTimer=-1;}
void cur_obj_enable_rendering(void){o->header.gfx.node.flags|=GRAPH_RENDER_ACTIVE;}
void cur_obj_move_after_thrown_or_dropped(f32 forward,f32 vertical){
    movementCalls++;requestedForward=forward;requestedVertical=vertical;
    o->oMoveFlags=0;o->oForwardVel=forward;o->oVelY=vertical;
}
void cur_obj_get_thrown_or_placed(f32 forward,f32 vertical,s32 action){
    placedCalls++;o->oHeldState=HELD_FREE;o->oAction=action;
    cur_obj_move_after_thrown_or_dropped(forward,vertical);
}
void network_send_object(struct Object *object){assert(object==o);sends++;}
void cur_obj_play_sound_2(s32 sound){(void)sound;}
s32 cur_obj_init_animation_and_check_if_near_end(s32 animation){(void)animation;return 1;}
void cur_obj_init_animation_with_sound(s32 animation){(void)animation;}
void cur_obj_start_cam_event(struct Object *object,s32 event){(void)object;(void)event;}
void cur_obj_spawn_particles(struct SpawnParticlesInfo *info){(void)info;}
u8 is_nearest_mario_state_to_object(struct MarioState *m,struct Object *object){(void)m;(void)object;return 1;}
struct Object *cur_obj_find_nearest_object_with_behavior(const BehaviorScript *behavior,f32 *distance){
    assert(behavior==bhvBowserBomb);*distance=mineDistance;return &mineObject;
}
static void fresh(void){
    memset(&bossObject,0,sizeof(bossObject));memset(&tailObject,0,sizeof(tailObject));
    gCurrentObject=&bossObject;placedCalls=movementCalls=sends=0;
    o->oPosX=100;o->oPosY=o->oHomeY=4500;o->oPosZ=-200;
    o->oHealth=3;o->prevObj=&tailObject;o->oHeldState=HELD_THROWN;
    o->oMoveFlags=OBJ_MOVE_LANDED;o->oBowserHeldAngleVelYaw=0x1000;o->oBowserHeldAnglePitch=-0x1000;
    tailObject.oAction=2;tailObject.oTimer=12;tailObject.oSubAction=2;
}
int main(void){
    fresh();king_bobomb_thrown_update(20,50);
    assert(placedCalls==1&&movementCalls==1&&requestedForward==20&&requestedVertical==50);
    assert(o->oAction==4&&o->oHeldState==HELD_FREE&&o->oIntangibleTimer==-1&&!o->oMoveFlags&&o->oPosY==4520&&o->oHealth==3);
    king_bobomb_act_4();assert(o->oHealth==3); // Flying never directly damages.
    o->oMoveFlags=OBJ_MOVE_LANDED;king_bobomb_act_4();assert(o->oHealth==2&&o->oAction==6&&o->oForwardVel==0&&o->oVelY==0);
    fresh();o->oHealth=1;o->oAction=4;king_bobomb_act_4();assert(o->oHealth==0&&o->oAction==7);
    fresh();o->oPosY=o->oHomeY-200;o->oAction=4;o->oMoveFlags=OBJ_MOVE_ON_GROUND;
    king_bobomb_act_4();assert(o->oHealth==3&&o->oSubAction==1);
    king_bobomb_act_4();assert(o->oHealth==3&&o->oAction==5); // Original off-hill recovery.
    fresh();o->oSubAction=2;o->oBowserUnk10E=2;bowser_thrown_dropped_update();
    float originalForward=o->oForwardVel,originalVertical=o->oVelY;
    assert(placedCalls==1&&sends==1);
    assert(fabsf(hypotf(originalForward,originalVertical)-(4096.f/3000.f*70.f*2.5f))<.001f);
    fresh();o->oBowserUnkF4=0x20010;bowser_throw_update(TRUE);
    assert(!placedCalls&&movementCalls==1&&sends==1); // Impact never uses held-parent placement.
    assert(o->oPosX==100&&o->oPosY==4500&&o->oPosZ==-200&&o->oHealth==3);
    assert(o->oForwardVel==originalForward&&o->oVelY==originalVertical);
    assert(o->oBowserUnkF4==0x10);
    assert(o->oAction==1&&o->oHeldState==HELD_FREE&&o->oIntangibleTimer==-1&&!o->oMoveFlags&&!o->oTimer&&!o->oSubAction);
    assert(tailObject.oAction==1&&!tailObject.oTimer&&!tailObject.oSubAction);
    // Actual native mine handler: proximity is the fixture input, damage is not.
    fresh();o->oAction=1;o->oMoveFlags=0;mineDistance=800;mineObject.oInteractStatus=0;
    bowser_act_thrown_dropped();assert(o->oHealth==3&&o->oAction==1&&!mineObject.oInteractStatus);
    mineDistance=799;bowser_act_thrown_dropped();
    assert(o->oHealth==2&&o->oAction==12&&(mineObject.oInteractStatus&INT_STATUS_HIT_MINE));
    fresh();o->oAction=1;o->oHealth=1;o->oMoveFlags=0;bowser_act_thrown_dropped();
    assert(o->oHealth==0&&o->oAction==4);
    puts("native boss source: original release velocities/tail reset, landing damage and off-hill recovery passed (explicit host stubs)");
    return 0;
}
