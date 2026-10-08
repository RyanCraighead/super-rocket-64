/* A native injury/door/star action may borrow locomotion without borrowing Mario's
 * mesh. Retain the last validated selected-character pose, following the native
 * position/yaw. No invented source animation, source ticks, attacks or immunity. */
#include "character_presentation.h"
#include "sm64.h"
#include "character_switch.h"
#include "mario.h"
#include "area.h"
#include "level_update.h"
#include "object_fields.h"
#include "pc/cliopts.h"
#include "pc/oot_link_runtime.h"
#include "pc/bm64_runtime.h"
#include "pc/bk_runtime.h"
#include "pc/spiderman_runtime.h"
#include "pc/thps_runtime.h"
#include "engine/math_util.h"
#include <math.h>
#include <string.h>

static enum CharacterSwitchId kind;
static int valid, presenting;
static u32 frameStartAction;
static struct MarioState *owner;
static struct Object *object;
static struct Area *area;
static s16 level;
static union {
    RocketSnapshot car;
    BkRenderSnapshot banjo;
    Bm64RenderSnapshot bomber;
    SpidermanRenderSnapshot spider;
    ThpsRenderSnapshot tony;
} pose;

static enum CharacterSwitchId selected(void) {
    if(gCLIOpts.characterWheel)return character_switch_enabled()?character_switch_active():CHARACTER_MARIO;
    if(gCLIOpts.rocketCar)return CHARACTER_OCTANE;
    if(gCLIOpts.thpsOriginal)return CHARACTER_TONY;
    if(gCLIOpts.spidermanOriginal)return CHARACTER_SPIDERMAN;
    if(gCLIOpts.bkDuo)return CHARACTER_BANJO;
    if(gCLIOpts.bm64Bomberman)return CHARACTER_BOMBERMAN;
    if(gCLIOpts.ootLink)return CHARACTER_LINK;
    return CHARACTER_MARIO;
}
void character_presentation_begin(struct MarioState *m) {
    if(!m||m->playerIndex)return;
    frameStartAction=m->action;
    enum CharacterSwitchId next=selected();
    int changed=owner!=m||object!=m->marioObj||kind!=next||area!=m->area||level!=gCurrLevelNum;
    /* Asset poses can survive a warp; old world transforms and effects cannot.
     * Rebase only after the new native action has produced its current transform. */
    if(owner!=m||kind!=next)valid=0;
    owner=m;object=m->marioObj;kind=next;area=m->area;level=gCurrLevelNum;presenting=0;
    if(changed)return;
    switch(kind) {
        case CHARACTER_OCTANE: if(rocket_runtime_snapshot(&pose.car))valid=1;break;
        case CHARACTER_BANJO: if(bk_runtime_snapshot(&pose.banjo))valid=1;break;
        case CHARACTER_BOMBERMAN: if(bm64_runtime_snapshot(&pose.bomber))valid=1;break;
        case CHARACTER_SPIDERMAN: if(spiderman_runtime_snapshot(&pose.spider))valid=1;break;
        case CHARACTER_TONY: if(thps_runtime_snapshot(&pose.tony))valid=1;break;
        case CHARACTER_LINK: {OotLinkSnapshot s;if(oot_link_runtime_snapshot(&s)&&s.drawable)valid=1;break;}
        default: valid=0;break;
    }
}
void character_presentation_finish(struct MarioState *m,int source_owns_action) {
    if(!m||m->playerIndex||m!=owner||!m->marioObj||!m->area)return;
    if(source_owns_action){character_presentation_begin(m);return;}
    /* Do not draw during despawn/warp. Keep only the selected asset pose for the
     * next area's native spawn/door action; no old gameplay state is restored. */
    if(!m->action||m->action==ACT_DISAPPEARED||m->action==ACT_BUBBLED||!m->floor||
       !(m->marioObj->header.gfx.node.flags&GRAPH_RENDER_ACTIVE)||selected()!=kind)return;
    if(!valid)return;
    for(int k=0;k<3;k++)if(!isfinite(m->marioObj->header.gfx.pos[k]))return;
    float *p=m->marioObj->header.gfx.pos;
    s16 yaw=m->marioObj->header.gfx.angle[1];
    float degrees=(float)(u16)yaw*(360.f/65536.f);
    int ok=0;
    switch(kind) {
        case CHARACTER_OCTANE: {
            if(!rocket_runtime_enabled())break;
            /* Native push/pull root animation uses the door's authored yaw;
             * its backward walk is encoded in Mario's animation, not that yaw.
             * The car has no such animation: face the actual traversal side.
             * On the final frame native code has already reset actionArg and
             * corrected faceAngle, while the graphics angle can still be old. */
            if((m->action==ACT_PULLING_DOOR||m->action==ACT_PUSHING_DOOR)&&m->usedObj)
                yaw=(s16)(m->usedObj->oMoveAngleYaw+((m->actionArg&2)?0x8000:0));
            else if(m->action==ACT_WARP_DOOR_SPAWN||m->action==ACT_ENTERING_STAR_DOOR||
                    m->action==ACT_UNLOCKING_KEY_DOOR||m->action==ACT_UNLOCKING_STAR_DOOR||
                    frameStartAction==ACT_PULLING_DOOR||frameStartAction==ACT_PUSHING_DOOR||
                    frameStartAction==ACT_WARP_DOOR_SPAWN||frameStartAction==ACT_ENTERING_STAR_DOOR)
                yaw=m->faceAngle[1];
            RocketSnapshot old=pose.car;
            pose.car.quicksand_depth=0; // Native graphics position already includes sinking.
            memset(pose.car.basis,0,sizeof pose.car.basis);
            pose.car.basis[0]=sins(yaw);pose.car.basis[2]=coss(yaw);
            pose.car.basis[3]=coss(yaw);pose.car.basis[5]=-sins(yaw);pose.car.basis[7]=1;
            for(int k=0;k<3;k++){pose.car.position[k]=p[k]+(k==1?40.f:0);pose.car.velocity[k]=0;pose.car.angular_velocity[k]=0;}
            for(int i=0;i<4;i++)for(int k=0;k<3;k++) {
                float v=pose.car.position[k];
                for(int axis=0;axis<3;axis++) {
                    float local=0;
                    for(int j=0;j<3;j++)local+=old.basis[axis*3+j]*(old.wheel_position[i][j]-old.position[j]);
                    v+=pose.car.basis[axis*3+k]*local;
                }
                pose.car.wheel_position[i][k]=v;
            }
            ok=1;break;
        }
        case CHARACTER_BANJO:
            vec3f_copy(pose.banjo.position,p);pose.banjo.yaw=degrees;
            bk_runtime_submit(&pose.banjo);ok=bk_runtime_snapshot(&pose.banjo);break;
        case CHARACTER_BOMBERMAN:
            vec3f_copy(pose.bomber.position,p);pose.bomber.yaw=degrees;
            memset(pose.bomber.bombs,0,sizeof pose.bomber.bombs);pose.bomber.held=0;
            bm64_runtime_submit(&pose.bomber);ok=bm64_runtime_snapshot(&pose.bomber);break;
        case CHARACTER_SPIDERMAN:
            vec3f_copy(pose.spider.position,p);pose.spider.yaw_degrees=degrees;
            pose.spider.orientation_mode=SPIDERMAN_ORIENTATION_YAW;
            ok=spiderman_runtime_submit(&pose.spider);break;
        case CHARACTER_TONY:
            vec3f_copy(pose.tony.position,p);pose.tony.yaw_degrees=degrees;pose.tony.use_body_basis=0;
            ok=thps_runtime_submit(&pose.tony);break;
        case CHARACTER_LINK: ok=oot_link_runtime_present(p,yaw);break;
        default: break;
    }
    if(ok){presenting=1;m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;}
}
int character_presentation_car_snapshot(RocketSnapshot *out) {
    if(!out||!presenting||kind!=CHARACTER_OCTANE||!owner||selected()!=kind||
       owner->area!=area||gCurrLevelNum!=level||!rocket_runtime_enabled())return 0;
    *out=pose.car;return 1;
}
void character_presentation_draw(const float *v,const float *p,const int *viewport) {
    if(!presenting||selected()!=kind||!owner||owner->area!=area||gCurrLevelNum!=level)return;
    if(kind==CHARACTER_OCTANE)rocket_runtime_draw_snapshot(&pose.car,v,p,viewport);
    /* These renderers normally require source controller ownership. Native
     * presentation deliberately omits their suspended combat/effect scenes. */
    else if(kind==CHARACTER_SPIDERMAN)spiderman_runtime_draw(v,p,viewport);
    else if(kind==CHARACTER_TONY)thps_runtime_draw(v,p,viewport);
}
