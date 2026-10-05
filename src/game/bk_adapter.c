#include <math.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "area.h"
#include "behavior_data.h"
#include "camera.h"
#include "bettercamera.h"
#include "first_person_cam.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "level_update.h"
#include "interaction.h"
#include "mario.h"
#include "mario_step.h"
#include "object_fields.h"
#include "object_list_processor.h"
#include "bk_adapter.h"
#include "pc/bk_runtime.h"
#include "../../codex/bk/movement/bk_movement.h"

extern u32 attack_object(struct MarioState *m, struct Object *o, s32 interaction);
/* BK units are mapped 1:1 to SM64 units. The engine keeps its original collision
 * cylinder, camera, enemies, health and progression. They are not BK collision. */
#define BK_PI 3.14159265358979323846f
/* Legacy launchers select their sole controller by default. The opt-in wheel
 * explicitly assigns one owner after all available assets are preloaded. */
static int sSelected = 1, sWheelMode;
static struct MarioState *sPlayer;
static struct Area *sArea;
static s16 sLevel;
static BkMovement sMovement;
static Vec3f sLastPosition;
static int sHavePosition, sOwnHide, sAttackActive;
static u8 sHitObjects[OBJECT_POOL_CAPACITY];

static void restore_visibility(void) {
    if (sOwnHide && sPlayer && sPlayer->marioObj)
        sPlayer->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
    sOwnHide=0;
}
void bk_adapter_set_selected(int selected) {
    sWheelMode = 1;
    if (!selected) bk_adapter_suspend();
    sSelected = !!selected;
}
void bk_adapter_suspend(void) {
    restore_visibility(); bk_runtime_suspend();
    sPlayer=NULL; sArea=NULL; sHavePosition=0; sAttackActive=0;
    memset(&sMovement,0,sizeof sMovement);
    memset(sHitObjects,0,sizeof sHitObjects);
}
static int supported(u32 action) {
    switch(action) {
        case ACT_IDLE: case ACT_WALKING: case ACT_DECELERATING:
        case ACT_BRAKING: case ACT_BRAKING_STOP: case ACT_TURNING_AROUND:
        case ACT_FINISH_TURNING_AROUND: case ACT_FREEFALL: case ACT_FREEFALL_LAND:
            return 1;
        default:return 0;
    }
}
static float degrees(s16 a) { return (float)(u16)a*(360.0f/65536.0f); }
static s16 angle(float a) {
    a=fmodf(a,360.0f); if(a<0)a+=360.0f;
    return (s16)(u16)(a*(65536.0f/360.0f));
}
static int native_enemy(const struct Object *o) {
    if(o->behavior!=bhvGoomba && o->behavior!=bhvBobomb &&
       o->behavior!=bhvSpindrift && o->behavior!=bhvScuttlebug &&
       o->behavior!=bhvPiranhaPlant && o->behavior!=bhvSkeeter) return 0;
    if(o->oInteractType & (INTERACT_TEXT|INTERACT_DOOR|INTERACT_WARP_DOOR|
       INTERACT_STAR_OR_KEY|INTERACT_PLAYER|INTERACT_COIN)) return 0;
    return o->activeFlags && o->oIntangibleTimer==0 && o->hitboxRadius>0 && o->hitboxHeight>0;
}
static void apply_attack(struct MarioState *m,const BkMotion *out) {
    /* The source decides WHEN a move hits. This bounded forward sphere is an
     * explicit host shape mapping until original bone-hitbox collision is ported. */
    if(!out->attack_active) {sAttackActive=0;return;}
    if(!sAttackActive) memset(sHitObjects,0,sizeof sHitObjects);
    sAttackActive=1;
    float radians=out->yaw*(BK_PI/180.0f);
    Vec3f center={m->pos[0]+sinf(radians)*55.0f,m->pos[1]+65.0f,m->pos[2]+cosf(radians)*55.0f};
    float radius=55.0f;
    if(out->action==BK_BUSTER) {center[0]=m->pos[0];center[1]=m->pos[1]+20.0f;center[2]=m->pos[2];radius=60.0f;}
    for(int i=0;i<OBJECT_POOL_CAPACITY;++i) {
        struct Object *o=&gObjectPool[i];
        if(sHitObjects[i]||!native_enemy(o))continue;
        float bottom=o->oPosY-o->hitboxDownOffset;
        float x=center[0]-o->oPosX,z=center[2]-o->oPosZ;
        float h=fmaxf(0.0f,sqrtf(x*x+z*z)-o->hitboxRadius);
        float v=center[1]<bottom?bottom-center[1]:center[1]>bottom+o->hitboxHeight?center[1]-bottom-o->hitboxHeight:0;
        if(h*h+v*v>=radius*radius)continue;
        Vec3f origin={m->pos[0],m->pos[1]+65.0f,m->pos[2]};
        Vec3f ray={o->oPosX-origin[0],bottom+o->hitboxHeight*.5f-origin[1],o->oPosZ-origin[2]};
        Vec3f hit;struct Surface *wall=NULL;
        find_surface_on_ray(origin,ray,&wall,hit,1.0f);
        if(wall&&wall->object!=o)continue;
        attack_object(m,o,INT_PUNCH);sHitObjects[i]=1;
    }
}
int bk_adapter_update(struct MarioState *m) {
    if(!m||m->playerIndex!=0)return 0;
    if(!sSelected || !bk_runtime_enabled()||!m->marioObj||!m->controller||!m->area||!m->floor) {
        bk_adapter_suspend();return 0;
    }
    if(sPlayer&&(sPlayer!=m||sArea!=m->area||sLevel!=gCurrLevelNum))bk_adapter_suspend();
    if(sHavePosition) {
        float x=m->pos[0]-sLastPosition[0],y=m->pos[1]-sLastPosition[1],z=m->pos[2]-sLastPosition[2];
        if(x*x+y*y+z*z>200.0f*200.0f)bk_adapter_suspend();
    }
    if(!supported(m->action)||m->health<0x100||m->heldObj||m->riddenObj||m->heldByObj||m->quicksandDepth>1.0f||
       (m->action!=ACT_FREEFALL && SURFACE_IS_QUICKSAND(m->floor->type)) || (m->input&INPUT_SQUISHED)) {
        bk_adapter_suspend();return 0;
    }
    if(!sPlayer) {
        bk_movement_init(&sMovement,degrees(m->faceAngle[1]),m->action!=ACT_FREEFALL);
        /* A scripted/damage/flight host action can hand back an airborne actor.
         * Preserve finite host momentum across that ownership boundary; the
         * next BK tick then applies its own air control and gravity. */
        if(m->action==ACT_FREEFALL) {
            for(int k=0;k<3;++k)
                sMovement.velocity[k]=isfinite(m->vel[k])?m->vel[k]/(BK_TICK_SECONDS*BK_HOST_SCALE):0;
            sMovement.target_speed=hypotf(sMovement.velocity[0],sMovement.velocity[2]);
            if(sMovement.target_speed>0)
                sMovement.target_yaw=atan2f(sMovement.velocity[0],sMovement.velocity[2])*(180.0f/BK_PI);
        }
    }
    sPlayer=m;sArea=m->area;sLevel=gCurrLevelNum;
    if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED) {
        if(bk_runtime_visible()){m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;}
        return 1;
    }
    if(m->pos[1]<m->waterLevel-100.0f){bk_adapter_suspend();set_water_plunge_action(m);return 0;}
    int rawX=m->controller->rawStickX,rawY=m->controller->rawStickY;
    rawX=rawX<-128?-128:rawX>127?127:rawX;rawY=rawY<-128?-128:rawY>127?127:rawY;
    BkStick stick=bk_controller_stick((s8)rawX,(s8)rawY);
    s16 cameraYaw=m->area->camera?m->area->camera->yaw:m->faceAngle[1];
    if(gLakituState.mode==CAMERA_MODE_NEWCAM)
        cameraYaw=get_first_person_enabled()?gLakituState.yaw:-gNewCamera.yaw+0x4000;
    BkInput input={0};input.stick_magnitude=stick.magnitude;
    /* SM64 camera's backward-axis convention; controller angle is +Y forward. */
    input.world_yaw=degrees(cameraYaw)+180.0f-stick.angle;
    input.floor_type=2; /* BK default coefficient; not a guessed material table. */
    u16 held=m->controller->buttonDown;
    if(held&A_BUTTON)input.buttons|=BK_BUTTON_A;
    if(held&B_BUTTON)input.buttons|=BK_BUTTON_B;
    if(held&Z_TRIG)input.buttons|=BK_BUTTON_Z;
    if(held&L_CBUTTONS)input.buttons|=BK_BUTTON_C_LEFT;
    BkMotion out;bk_movement_tick(&sMovement,&input,&out);
    Vec3f before;vec3f_copy(before,m->pos);
    for(int k=0;k<3;++k)m->vel[k]=out.delta[k]*BK_HOST_SCALE;
    m->faceAngle[1]=angle(out.yaw);
    int grounded=0,wall=0,ceiling=0;
    if(!out.airborne) {
        if(m->floor->normal.y>.01f){m->vel[0]/=m->floor->normal.y;m->vel[2]/=m->floor->normal.y;}
        int result=perform_ground_step(m);
        grounded=result!=GROUND_STEP_LEFT_GROUND;
        wall=result==GROUND_STEP_HIT_WALL||result==GROUND_STEP_HIT_WALL_STOP_QSTEPS;
    } else {
        float requestedY=m->vel[1];
        int result=perform_air_step(m,0);
        /* Host includes Mario gravity/wind after collision; discard both. */
        m->vel[1]=requestedY;
        grounded=result==AIR_STEP_LANDED;
        wall=result==AIR_STEP_HIT_WALL;
        /* Native quarter-step rejects the intended point at a low ceiling;
         * the final position can remain below it. Detect shortened ascent and
         * confirm a ceiling along that same four-quarter-step segment. */
        if(requestedY>0 && !grounded && m->pos[1]-before[1]<requestedY-.001f) {
            for(int q=0;q<=4 && !ceiling;++q) {
                float fraction=q*.25f;
                Vec3f probe={before[0]+out.delta[0]*BK_HOST_SCALE*fraction,
                    before[1]+requestedY*fraction,before[2]+out.delta[2]*BK_HOST_SCALE*fraction};
                struct Surface *ceilSurface=NULL;
                float height=vec3f_mario_ceil(probe,m->floorHeight,&ceilSurface);
                if(ceilSurface && probe[1]+m->marioObj->hitboxHeight>=height-.001f)
                    ceiling=1;
            }
        }
        if(result==AIR_STEP_HIT_LAVA_WALL){bk_adapter_suspend();set_mario_action(m,ACT_LAVA_BOOST,0);return 0;}
    }
    int blockedX=wall&&fabsf(m->pos[0]-before[0]-out.delta[0]*BK_HOST_SCALE)>.5f;
    int blockedZ=wall&&fabsf(m->pos[2]-before[2]-out.delta[2]*BK_HOST_SCALE)>.5f;
    bk_movement_contact(&sMovement,grounded,ceiling,blockedX,blockedZ);
    m->prevAction=m->action;
    m->action=grounded?(hypotf(sMovement.velocity[0],sMovement.velocity[2])>.1f?ACT_WALKING:ACT_IDLE):ACT_FREEFALL;
    m->forwardVel=hypotf(sMovement.velocity[0],sMovement.velocity[2])*BK_TICK_SECONDS*BK_HOST_SCALE;
    for(int k=0;k<3;++k)m->vel[k]=sMovement.velocity[k]*BK_TICK_SECONDS*BK_HOST_SCALE;
    m->slideVelX=m->vel[0];m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    apply_attack(m,&out);
    BkRenderSnapshot snapshot={0};vec3f_copy(snapshot.position,m->pos);
    snapshot.yaw=out.yaw;snapshot.animation_asset=out.animation_asset;snapshot.animation_time=out.animation_time;
    snapshot.ticks=out.ticks;snapshot.action=out.action;snapshot.model_flags=out.model_flags;
    bk_runtime_submit(&snapshot);
    /* In wheel mode a validated submission owns this render frame, including
     * its first draw. Waiting for last frame's visibility overlaps Mario. */
    int drawable = sWheelMode && bk_runtime_snapshot(&snapshot);
    if(drawable || bk_runtime_visible()){m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;}
    else restore_visibility();
    vec3f_copy(sLastPosition,m->pos);sHavePosition=1;
    return 1;
}
