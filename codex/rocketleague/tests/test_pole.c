/* Native action/state functions share the explicit audiovisual/level fixture
 * from the lava suite. No game window, native socket or animation playback. */
#define main lava_fixture_main
#define ROCKET_POLE_REAL_TEST
#define rocket_runtime_read_selected_input lava_fixture_read_input
#include "test_lava.c"
#undef rocket_runtime_read_selected_input
#undef main
#include "game/rocket_pole.h"
#include "game/mario_actions_automatic.h"
#include "game/camera.h"
#include "game/memory.h"
#include "behavior_data.h"
#include "level_table.h"
#include "pc/rocket_bindings.h"
#include "../physics/pole_pose.h"
#include "../physics/vanish_collision.h"
u32 gGlobalTimer;s16 gCurrLevelNum;
static struct Object poleObject;
static struct Camera camera;
static RocketGamepad pad;
static RocketTriangle *geometry;
static size_t geometryCount;
const BehaviorScript bhvTree[]={1},bhvPoleGrabbing[]={2},bhvGiantPole[]={3};
s32 obj_has_behavior(struct Object*o,const BehaviorScript*b){return o&&o->behavior==b;}
int character_net_is_car(unsigned index){return index==1;}
int rocket_runtime_read_selected_input(const RocketInput*k,RocketInput*out){*out=rocket_gamepad_merge(k,&pad);return inputReady;}
int rocket_adapter_pole_pose_clear(const RocketSnapshot*s,unsigned flags){
    (void)flags;for(size_t i=0;i<geometryCount;i++)if(rocket_car_triangle_overlap(s,geometry[i].v,2))return 0;return 1;
}
int rocket_adapter_whomp_path_clear(const float*a,const float*b,struct Object*o){(void)a;(void)b;(void)o;return 1;}
void *segmented_to_virtual(const void*p){return (void*)p;}
void *virtual_to_segmented(u32 segment,const void*p){(void)segment;return (void*)p;}
const BehaviorScript *smlua_override_behavior(const BehaviorScript*b){return b;}
struct Object *cur_obj_find_nearest_pole(void){return &poleObject;}
void mario_stop_riding_and_holding(struct MarioState*m){m->heldObj=m->riddenObj=NULL;}
void reset_mario_pitch(struct MarioState*m){m->faceAngle[0]=0;}
s32 set_water_plunge_action(struct MarioState*m){return set_mario_action(m,ACT_WATER_PLUNGE,0);}
s32 f32_find_wall_collision(f32*x,f32*y,f32*z,f32 offset,f32 radius){(void)x;(void)y;(void)z;(void)offset;(void)radius;return 0;}
s32 is_anim_at_end(struct MarioState*m){(void)m;return 1;}
s32 is_anim_past_frame(struct MarioState*m,s16 frame){(void)m;(void)frame;return 0;}
s16 set_character_anim_with_accel(struct MarioState*m,enum CharacterAnimID a,s32 accel){(void)m;(void)a;(void)accel;return 0;}
s16 return_mario_anim_y_translation(struct MarioState*m){(void)m;return 0;}
void set_sound_moving_speed(u8 bank,u8 speed){(void)bank;(void)speed;}
#define NOT_POLE(name) s32 name(struct MarioState*m){(void)m;CHECK(0);return 0;}
NOT_POLE(act_start_hanging) NOT_POLE(act_hanging) NOT_POLE(act_hang_moving)
NOT_POLE(act_ledge_grab) NOT_POLE(act_ledge_climb_down) NOT_POLE(act_ledge_climb_fast)
NOT_POLE(act_grabbed) NOT_POLE(act_in_cannon) NOT_POLE(act_tornado_twirling) NOT_POLE(act_bubbled)
s32 act_ledge_climb_slow(struct MarioState*m){(void)m;CHECK(0);return 0;}
#define POLE_NONE 0
#define POLE_TOUCHED_FLOOR 1
#define POLE_FELL_OFF 2
#include "pole-native.inc.c"
static struct MarioState *setup(float x,float base,float z,float height){
    struct MarioState*m=fresh();rocket_pole_forget(NULL);memset(&poleObject,0,sizeof poleObject);memset(&pad,0,sizeof pad);
    gCurrLevelNum=LEVEL_SSL;area.index=2;area.camera=&camera;camera.yaw=0;
    poleObject.behavior=bhvPoleGrabbing;poleObject.activeFlags=ACTIVE_FLAG_ACTIVE;
    poleObject.header.gfx.activeAreaIndex=2;poleObject.hitboxHeight=height;poleObject.hitboxRadius=80;
    poleObject.oPosX=x;poleObject.oPosY=base;poleObject.oPosZ=z;
    m->pos[0]=x;m->pos[1]=base+40;m->pos[2]=z;vec3f_copy(pose.position,m->pos);
    m->action=ACT_FREEFALL;m->prevAction=ACT_IDLE;pose.grounded=0;gGlobalTimer=0;
    return m;
}
static void step(struct MarioState*m){
    gGlobalTimer++;struct Controller before=controller;struct Controller *pointer=m->controller;
    u16 beforeInput=m->input;int loops=0,result;
    do{result=rocket_pole_execute(m);CHECK(result>=0&&++loops<8);}while(result);
    CHECK(m->controller==pointer&&!memcmp(&before,&controller,sizeof before)&&m->input==beforeInput);
}
static void grab(struct MarioState*m){
    CHECK(interact_pole(m,INTERACT_POLE,&poleObject));CHECK(m->action==ACT_GRAB_POLE_SLOW);
    CHECK(m->usedObj==&poleObject);step(m);CHECK(m->action==ACT_HOLDING_POLE);
}
static void unit(void){
    struct MarioState*m=setup(0,0,0,920);grab(m);
    float start=m->pos[1];pad.connected=pad.isolated=1;pad.left_y=-32768;
    for(int i=0;i<20;i++)step(m);CHECK(m->pos[1]>start+140&&m->action==ACT_CLIMBING_POLE);
    RocketSnapshot picture=pose;CHECK(rocket_pole_present(m,&picture)&&picture.basis[1]==1&&picture.basis[7]==0);
    CHECK(rocket_body_pose_valid(&picture));float saved=m->pos[1];m->freeze=2;step(m);CHECK(m->pos[1]==saved);m->freeze=0;
    sCurrPlayMode=PLAY_MODE_PAUSED;step(m);CHECK(m->pos[1]==saved);sCurrPlayMode=0;
    inputReady=0;step(m);CHECK(m->pos[1]==saved);inputReady=1;pad.left_y=0;step(m);
    pad.jump=1;step(m);CHECK(m->action==ACT_WALL_KICK_AIR&&m->vel[1]==62&&m->forwardVel==24);
    RocketSnapshot launch;int jumped=-1;CHECK(rocket_pole_present(m,&picture));
    m->freeze=1;CHECK(!rocket_pole_take_release(m,&launch,&jumped));m->freeze=0;
    CHECK(rocket_pole_take_release(m,&launch,&jumped)&&jumped&&launch.basis[1]==1);
    CHECK(launch.velocity[1]==1860&&launch.velocity[2]==-720&&!rocket_pole_take_release(m,&launch,&jumped));
    /* Every configurable jump maps through the production mapper to native
     * action initialization, with a held-entry/release/fresh edge requirement. */
    unsigned bits[]={0,0,1,2,3,9,10,7,8,11,12,13,14};
    for(unsigned b=RB_SOUTH;b<RB_COUNT;b++){
        m=setup(0,0,0,920);pad.connected=pad.isolated=1;
        RocketBindings bindings=rocket_default_bindings;bindings.action[RA_JUMP]=b;RocketPadSample raw={0};
        if(b==RB_LT)raw.left_trigger=32767;else if(b==RB_RT)raw.right_trigger=32767;else raw.buttons=1u<<bits[b];
        rocket_bindings_apply(&bindings,&raw,&pad);grab(m);step(m);CHECK(m->action==ACT_HOLDING_POLE);
        RocketPadSample neutral={0};rocket_bindings_apply(&bindings,&neutral,&pad);step(m);
        rocket_bindings_apply(&bindings,&raw,&pad);step(m);CHECK(m->action==ACT_WALL_KICK_AIR&&m->vel[1]==62);
    }
    m=setup(0,0,0,920);grab(m);controller.rawStickY=80;for(int i=0;i<4;i++)step(m);CHECK(m->pos[1]>60);
    controller.rawStickY=0;step(m);controller.buttonDown=A_BUTTON;step(m);CHECK(m->action==ACT_WALL_KICK_AIR);
    m=setup(0,0,0,920);grab(m);pad.connected=pad.isolated=1;pad.left_y=32767;
    for(int i=0;i<40&&(m->action&ACT_FLAG_ON_POLE);i++)step(m);
    CHECK(!(m->action&ACT_FLAG_ON_POLE));CHECK(rocket_pole_take_release(m,&launch,&jumped)&&!jumped);
    m=setup(0,0,0,920);grab(m);rocket_pole_forget(&poleObject);step(m);
    CHECK(m->action==ACT_FREEFALL&&rocket_pole_take_release(m,&launch,&jumped)&&!jumped);
    RocketTriangle ceiling[]={{{{-500,350,-500},{500,350,-500},{500,350,500}},0},{{{-500,350,-500},{500,350,500},{-500,350,500}},0}};
    geometry=ceiling;geometryCount=2;m=setup(0,0,0,920);grab(m);
    pad.connected=pad.isolated=1;pad.left_y=-32768;
    for(int i=0;i<30;i++)step(m);CHECK(m->pos[1]>80&&m->pos[1]<109);
    saved=m->pos[1];for(int i=0;i<30;i++)step(m);CHECK(m->pos[1]==saved);
    picture=pose;CHECK(rocket_pole_present(m,&picture)&&rocket_adapter_pole_pose_clear(&picture,0));
    geometry=NULL;geometryCount=0;
    for(int gate=0;gate<8;gate++){
        m=setup(0,0,0,920);
        switch(gate){case 0:area.index=1;break;case 1:gCurrLevelNum=LEVEL_BOB;break;case 2:poleObject.behavior=bhvTree;break;
            case 3:poleObject.activeFlags=0;break;case 4:m->playerIndex=1;break;case 5:m->freeze=1;break;
            case 6:inputReady=0;break;case 7:pose.grounded=1;break;}
        CHECK(!interact_pole(m,INTERACT_POLE,&poleObject));CHECK(m->action==ACT_FREEFALL);
    }
    m=setup(0,0,0,920);carSelected=0;CHECK(interact_pole(m,INTERACT_POLE,&poleObject)); // Mario unchanged.
}
static void owned(const char*path){
    FILE*f=fopen(path,"r");CHECK(f&&fscanf(f,"%zu",&geometryCount)==1&&geometryCount<100000);
    geometry=calloc(geometryCount,sizeof *geometry);CHECK(geometry);
    for(size_t i=0;i<geometryCount;i++)for(int v=0;v<3;v++)for(int k=0;k<3;k++)CHECK(fscanf(f,"%f",&geometry[i].v[v][k])==1);fclose(f);
    const float locations[2][4]={{2867,640,2867,770},{0,3200,1331,920}};
    for(int index=0;index<2;index++){
        const float*p=locations[index];struct MarioState*m=setup(p[0],p[1],p[2],p[3]);grab(m);
        pad.connected=pad.isolated=1;pad.left_y=-32768;
        for(int i=0;i<140;i++)step(m);
        printf("owned pole %d feet %.2f %.2f %.2f action %08x\n",index,m->pos[0],m->pos[1],m->pos[2],m->action);
        CHECK(m->pos[1]>=p[1]+p[3]-101);
        RocketSnapshot picture=pose;CHECK(rocket_pole_present(m,&picture)&&rocket_adapter_pole_pose_clear(&picture,0));
        pad.left_y=0;step(m);pad.jump=1;step(m);CHECK(m->action==ACT_TOP_OF_POLE_JUMP||m->action==ACT_WALL_KICK_AIR);
    }
    free(geometry);geometry=NULL;geometryCount=0;
}
int main(int argc,char**argv){unit();if(argc==2)owned(argv[1]);printf("PASS native pyramid poles: %d checks\n",checks);return 0;}
