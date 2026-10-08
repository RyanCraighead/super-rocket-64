#include "rocket_pole.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "area.h"
#include "display.h"
#include "level_update.h"
#include "mario.h"
#include "mario_actions_automatic.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "interaction.h"
#include "behavior_data.h"
#include "level_table.h"
#include "engine/math_util.h"
#include "pc/rocket_runtime.h"
#include "../../codex/rocketleague/physics/pole_pose.h"

static struct MarioState *owner;
static struct Object *body,*pole;
static struct Area *area;
static s16 level,yaw;
static Vec3f feet;
static float poleHeight;
static int jumpHeld=1,haveInputFrame,jumpPressed,pending,releaseJump;
static u32 inputFrame,releaseAction;
static RocketSnapshot releasePose;
static struct Object *departedPole,*departedBody;
static struct Area *departedArea;
static s16 departedLevel;

static void update_departure(struct MarioState *m){
    if(!departedPole)return;
    if(!m||m->playerIndex||m->marioObj!=departedBody||m->area!=departedArea||gCurrLevelNum!=departedLevel||
       !rocket_adapter_car_selected()||!(departedPole->activeFlags&ACTIVE_FLAG_ACTIVE)){
        departedPole=NULL;return;
    }
    float radius=departedPole->hitboxRadius+m->marioObj->hitboxRadius;
    float dx=m->pos[0]-departedPole->oPosX,dz=m->pos[2]-departedPole->oPosZ;
    if(dx*dx+dz*dz>=radius*radius||m->pos[1]>departedPole->oPosY+departedPole->hitboxHeight||
       m->pos[1]+m->marioObj->hitboxHeight<departedPole->oPosY-departedPole->hitboxDownOffset)departedPole=NULL;
}

static int scoped(struct MarioState *m,struct Object *o){
    return m&&m->playerIndex==0&&m->marioObj&&m->controller&&m->area&&
        gCurrLevelNum==LEVEL_SSL&&m->area->index==2&&rocket_adapter_car_selected()&&
        o&&(o->activeFlags&ACTIVE_FLAG_ACTIVE)&&o->header.gfx.activeAreaIndex==m->area->index&&
        obj_has_behavior(o,bhvPoleGrabbing)&&isfinite(o->hitboxHeight)&&o->hitboxHeight>100;
}
static int same_owner(struct MarioState *m){
    return m&&m==owner&&m->marioObj==body&&m->area==area&&gCurrLevelNum==level&&
        !m->playerIndex&&rocket_adapter_car_selected();
}
void rocket_pole_forget(struct Object *object){
    if(!object||object==departedPole||object==departedBody)departedPole=NULL;
    if(object&&object!=pole&&object!=body)return;
    if(object&&object==pole){pole=NULL;return;} // Detach before native code can reuse a freed pole slot.
    owner=NULL;body=pole=NULL;area=NULL;pending=haveInputFrame=jumpPressed=0;jumpHeld=1;
}
static RocketSnapshot upright_basis(void){
    RocketSnapshot pose={0};pose.basis[2]=pose.basis[3]=pose.basis[7]=1;return pose;
}
static int clear_at(struct MarioState *m,const float at[3],s16 angle){
    RocketSnapshot pose=upright_basis();rocket_pole_pose(&pose,at,sins(angle),coss(angle));
    return rocket_adapter_pole_pose_clear(&pose,m->flags);
}
int rocket_pole_can_grab(struct MarioState *m,struct Object *o){
    RocketSnapshot car;
    update_departure(m);if(o&&o==departedPole)return 0;
    if(!scoped(m,o)||m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED||m->health<0x100||
       !rocket_adapter_body_snapshot(m->marioObj,&car)||car.grounded||
       !rocket_runtime_read_selected_input(&(RocketInput){0},&(RocketInput){0}))return 0;
    float at[3]={o->oPosX,m->pos[1],o->oPosZ};
    if(!clear_at(m,at,m->faceAngle[1]))return 0;
    float distance=0;for(int k=0;k<3;k++){float d=at[k]-car.position[k];distance+=d*d;}
    return distance<1.f||rocket_adapter_whomp_path_clear(car.position,at,o);
}
void rocket_pole_prepare(struct MarioState *m){
    RocketSnapshot car;
    if(!m||m->playerIndex||!gObjectLists||!m->marioObj||!rocket_adapter_body_snapshot(m->marioObj,&car)||car.grounded)return;
    struct ObjectNode *head=&gObjectLists[OBJ_LIST_POLELIKE];
    for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next){
        struct Object *o=(struct Object*)node;
        if(!scoped(m,o)||o->oInteractType!=INTERACT_POLE||o->oIntangibleTimer)continue;
        float at[3]={o->oPosX,o->oPosY-o->hitboxDownOffset,o->oPosZ};
        if(!rocket_body_overlaps_cylinder(&car,at,o->hitboxRadius,o->hitboxHeight)||!rocket_pole_can_grab(m,o))continue;
        int i=0;for(;i<m->marioObj->numCollidedObjs&&i<4;i++)if(m->marioObj->collidedObjs[i]==o)break;
        if(i>=4)return;
        if(i==m->marioObj->numCollidedObjs)m->marioObj->collidedObjs[m->marioObj->numCollidedObjs++]=o;
        m->marioObj->collidedObjInteractTypes|=INTERACT_POLE;m->collidedObjInteractTypes|=INTERACT_POLE;
    }
}
int rocket_pole_present(struct MarioState *m,RocketSnapshot *pose){
    if(!pose||!same_owner(m)||(!pending&&!(m->action&ACT_FLAG_ON_POLE)))return 0;
    rocket_pole_pose(pose,m->marioObj->header.gfx.pos,sins(yaw),coss(yaw));return 1;
}
int rocket_pole_take_release(struct MarioState *m,RocketSnapshot *pose,int *jump){
    update_departure(m);
    if(!pending)return 0;
    if(!same_owner(m)||m->action!=releaseAction||m->health<0x100){rocket_pole_forget(NULL);return 0;}
    if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED)return 0;
    *pose=releasePose;*jump=releaseJump;
    struct Object *previous=pole;rocket_pole_forget(NULL);
    departedPole=previous;departedBody=m->marioObj;departedArea=m->area;departedLevel=gCurrLevelNum;
    return 1;
}
static void queue_release(struct MarioState *m){
    releasePose=upright_basis();rocket_pole_pose(&releasePose,m->pos,sins(yaw),coss(yaw));
    releaseJump=m->action==ACT_WALL_KICK_AIR||m->action==ACT_TOP_OF_POLE_JUMP;
    for(int k=0;k<3;k++)releasePose.velocity[k]=m->vel[k]*30.f;
    if(releaseJump){
        releasePose.velocity[0]=m->forwardVel*sins(m->faceAngle[1])*30.f;
        releasePose.velocity[2]=m->forwardVel*coss(m->faceAngle[1])*30.f;
    }
    releaseAction=m->action;pending=1;
}
int rocket_pole_execute(struct MarioState *m){
    if(!m||m->playerIndex||!(m->action&ACT_FLAG_ON_POLE)||!rocket_adapter_car_selected())return -1;
    if((same_owner(m)&&!pole)||!scoped(m,m->usedObj)){
        if(!same_owner(m))return -1;
        set_mario_action(m,ACT_FREEFALL,0);queue_release(m);return 0;
    }
    if(!same_owner(m)||pole!=m->usedObj){
        rocket_pole_forget(NULL);owner=m;body=m->marioObj;area=m->area;level=gCurrLevelNum;pole=m->usedObj;
        vec3f_copy(feet,m->pos);feet[0]=pole->oPosX;feet[2]=pole->oPosZ;
        poleHeight=body->oMarioPolePos;yaw=m->faceAngle[1];
    }
    if(m->health<0x100){rocket_pole_forget(NULL);return mario_execute_automatic_action(m);}
    RocketInput keyboard={0},input={0};
    keyboard.steer=-m->controller->rawStickX/80.f;keyboard.pitch=-m->controller->rawStickY/80.f;
    keyboard.jump=!!(m->controller->buttonDown&A_BUTTON);
    keyboard.powerslide=!!(m->controller->buttonDown&Z_TRIG);
    int allowed=!m->freeze&&sCurrPlayMode!=PLAY_MODE_PAUSED&&m->health>=0x100&&
        rocket_runtime_read_selected_input(&keyboard,&input);
    if(!allowed){jumpHeld=1;jumpPressed=0;return 0;}
    if(!haveInputFrame||inputFrame!=gGlobalTimer){
        jumpPressed=input.jump&&!jumpHeld;jumpHeld=!!input.jump;inputFrame=gGlobalTimer;haveInputFrame=1;
    }
    /* Use a private controller copy only while the native action executes.
     * Preserve the original keyboard/controller state for cameras and peers. */
    struct Controller mapped=*m->controller,*saved=m->controller;
    mapped.stickX=-input.steer*64.f;mapped.stickY=-input.pitch*64.f;
    mapped.stickMag=hypotf(mapped.stickX,mapped.stickY);
    u16 savedInput=m->input;
    m->input&=~(INPUT_A_PRESSED|INPUT_A_DOWN|INPUT_Z_PRESSED|INPUT_Z_DOWN);
    if(jumpPressed)m->input|=INPUT_A_PRESSED;
    if(input.jump)m->input|=INPUT_A_DOWN;
    // Stick down retains native sliding/detach. Shared air-roll bindings must
    // not accidentally detach while steering around a pole.
    m->controller=&mapped;int cancel=mario_execute_automatic_action(m);
    m->controller=saved;m->input=savedInput;jumpPressed=0;
    if(!(m->action&ACT_FLAG_ON_POLE)){
        if(m->action!=ACT_WALL_KICK_AIR&&m->action!=ACT_TOP_OF_POLE_JUMP&&m->action!=ACT_SOFT_BONK&&
           m->action!=ACT_FREEFALL&&m->action!=ACT_IDLE){rocket_pole_forget(NULL);return cancel;}
        queue_release(m);return 0; // Car physics consumes this exact native launch next frame.
    }
    /* Native capsule checks still run. Sweep the full vertical car in short
     * translation/rotation increments too, retaining the last clear position
     * when a ceiling or a narrow ledge blocks further climbing. */
    float distance=0;for(int k=0;k<3;k++)distance=fmaxf(distance,fabsf(m->pos[k]-feet[k]));
    s16 turn=(s16)(m->faceAngle[1]-yaw);
    int steps=(int)ceilf(fmaxf(distance/4.f,fabsf((float)turn)/256.f));if(steps<1)steps=1;
    int clear=steps<=128;
    for(int i=1;clear&&i<=steps;i++){
        float at[3];for(int k=0;k<3;k++)at[k]=feet[k]+(m->pos[k]-feet[k])*(float)i/steps;
        clear=clear_at(m,at,(s16)(yaw+(int)turn*i/steps));
    }
    if(!clear){
        vec3f_copy(m->pos,feet);m->faceAngle[1]=yaw;body->oMarioPolePos=poleHeight;
        vec3f_copy(body->header.gfx.pos,m->pos);body->header.gfx.angle[1]=yaw;
    }else{vec3f_copy(feet,m->pos);yaw=m->faceAngle[1];poleHeight=body->oMarioPolePos;}
    return cancel;
}
