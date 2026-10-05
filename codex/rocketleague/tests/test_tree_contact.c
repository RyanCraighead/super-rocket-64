/* Actual native hitbox/pole actions and adapter; explicit engine/runtime
 * services. The screenshot motivates this test but cannot identify an action. */
#define main adapter_suite_main
#include "test_adapter.c"
#undef main
#include "game/mario_step.h"
#include "pc/character_net.h"
static unsigned checks;
static int remoteCar;
static struct Object tree;
#define CHECK(x) do { checks++; assert(x); } while (0)
int character_net_is_car(unsigned index){return index==1&&remoteCar;}
void mario_stop_riding_and_holding(struct MarioState *m){m->heldObj=m->riddenObj=NULL;}
void reset_mario_pitch(struct MarioState *m){m->faceAngle[0]=0;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
struct Object *cur_obj_find_nearest_pole(void){return &tree;}
s32 f32_find_wall_collision(f32 *x,f32 *y,f32 *z,f32 offset,f32 radius){(void)x;(void)y;(void)z;(void)offset;(void)radius;return 0;}
f32 vec3f_mario_ceil(Vec3f p,f32 height,struct Surface **ceil){(void)p;(void)height;*ceil=NULL;return 20000;}
/* Trees are pole contacts, not an incoming-damage chassis override. */
int rocket_incoming_overlap(struct Object *a,struct Object *b,int hurt){(void)a;(void)b;(void)hurt;return -1;}
#define POLE_NONE 0
#define POLE_TOUCHED_FLOOR 1
#define POLE_FELL_OFF 2
#include "native_tree.inc.h"
static void setup(void){
    fresh();memset(&tree,0,sizeof tree);remoteCar=0;
    tree.oInteractType=INTERACT_POLE;tree.hitboxRadius=80;tree.hitboxHeight=500;
    object.oInteractType=INTERACT_PLAYER;object.hitboxRadius=37;object.hitboxHeight=160;
    mario.pos[0]=60;mario.pos[1]=250;step();pose.grounded=0;step();
}
static int contact(void){
    object.oPosX=mario.pos[0];object.oPosY=mario.pos[1];object.oPosZ=mario.pos[2];
    object.numCollidedObjs=tree.numCollidedObjs=0;
    object.collidedObjInteractTypes=tree.collidedObjInteractTypes=0;
    if(!detect_object_hitbox_overlap(&object,&tree))return 0;
    CHECK(object.collidedObjInteractTypes&INTERACT_POLE);
    return interact_pole(&mario,INTERACT_POLE,&tree);
}
static void native_grabs(void){
    for(int remote=0;remote<2;remote++)for(int fast=0;fast<2;fast++){
        setup();rocket_adapter_set_selected(0);mario.playerIndex=remote;
        mario.action=ACT_FREEFALL;mario.prevAction=ACT_IDLE;mario.forwardVel=fast?40:0;
        CHECK(contact());CHECK(mario.action==(fast?ACT_GRAB_POLE_FAST:ACT_GRAB_POLE_SLOW));
        CHECK(mario.usedObj==&tree&&mario.vel[1]==0&&mario.forwardVel==0);
        CHECK(set_pole_position(&mario,0)==POLE_NONE);
        CHECK(mario.pos[0]==tree.oPosX&&mario.pos[2]==tree.oPosZ);
    }
}
int main(void){
#ifdef TREE_BASELINE
    setup();CHECK(contact());CHECK(mario.action==ACT_GRAB_POLE_SLOW);
    CHECK(!rocket_adapter_update(&mario)&&!draw);
    CHECK(set_pole_position(&mario,0)==POLE_NONE&&mario.pos[0]==0);
    printf("CONFIRMED pre-fix tree grab suspends car physics and pins the host: %u checks\n",checks);
#else
    native_grabs();
    for(int network=0;network<3;network++)for(int captured=0;captured<2;captured++){
        setup();gNetworkType=network==1?NT_SERVER:network==2?NT_CLIENT:NT_NONE;
        uiBlocked=captured;pose.boost=23;pose.jumped=1;pose.double_jumped=1;
        int beforeResets=resets;uint64_t beforeTicks=pose.ticks;
        for(int tick=0;tick<90;tick++){
            pose.position[0]=tick<30?60:tick<60?250:-60;
            vec3f_copy(mario.pos,pose.position);mario.action=ACT_FREEFALL;
            controller.buttonDown=(tick%3==0?A_BUTTON:0)|(tick%4==0?B_BUTTON:0);
            RocketSnapshot before=pose;
            CHECK(!contact());CHECK(mario.action==ACT_FREEFALL&&!mario.usedObj);
            CHECK(!memcmp(&before,&pose,sizeof pose));step();
            CHECK(draw&&resets==beforeResets&&pose.boost==23&&pose.jumped&&pose.double_jumped);
            CHECK(observed.jump==(tick%3==0)&&observed.boost==(tick%4==0));
        }
        CHECK(pose.ticks==beforeTicks+360&&meshCount[0]==1&&meshCount[1]==1);
        gCurrLevelNum++;step();CHECK(resets==beforeResets+1);
        mario.pos[0]+=700;step();CHECK(resets==beforeResets+2);
        rocket_adapter_set_selected(0);CHECK(!rocket_adapter_update(&mario));
        rocket_adapter_set_selected(1);step();CHECK(!contact());
    }
    /* The receiving host/client must not locally grab a remote owner's car. */
    for(int network=1;network<=2;network++){
        setup();gNetworkType=network==1?NT_SERVER:NT_CLIENT;mario.playerIndex=1;remoteCar=1;
        CHECK(!contact());CHECK(mario.action==ACT_FREEFALL&&!mario.usedObj);
        remoteCar=0;CHECK(contact()); // Switching/reconnected Mario uses native grabbing.
    }
    setup();enabled=0;CHECK(contact()); // Runtime disabled: ordinary native behavior.
    CHECK(!interact_pole(NULL,INTERACT_POLE,&tree));CHECK(!interact_pole(&mario,INTERACT_POLE,NULL));
    printf("PASS %u native tree/adapter checks: enter/leave, repeated jump/boost, no-input capture, host/client, lifecycle, Mario grabbing\n",checks);
#endif
    return 0;
}
