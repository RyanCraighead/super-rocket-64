#include <math.h>
#include <string.h>
#include "sm64.h"
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
#include "oot_link_adapter.h"
#include "pc/oot_link_runtime.h"
#include "../../codex/oot/movement/oot_movement.h"

/* Existing public native interaction entry point; older interaction.h does not declare it. */
extern u32 attack_object(struct MarioState *m, struct Object *o, s32 interaction);

/* Legacy launchers select their sole controller by default. The opt-in wheel
 * explicitly assigns one owner after all available assets are preloaded. */
static int sSelected = 1, sWheelMode;
static struct MarioState *sPlayer;
static struct Area *sArea;
static s16 sLevel;
static Vec3f sLastPosition;
static int sHavePosition;
static uint64_t sSwing;
static u8 sHitObjects[OBJECT_POOL_CAPACITY];
static int sOwnHide;

static void restore_visibility(void) {
    if (sOwnHide && sPlayer && sPlayer->marioObj)
        sPlayer->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
    sOwnHide = 0;
}
void oot_link_adapter_set_selected(int selected) {
    sWheelMode = 1;
    if (!selected) oot_link_adapter_suspend();
    sSelected = !!selected;
}
void oot_link_adapter_suspend(void) {
    restore_visibility(); oot_link_runtime_suspend();
    sPlayer = NULL; sArea = NULL; sHavePosition = 0; sSwing = 0;
    memset(sHitObjects, 0, sizeof(sHitObjects));
}
static int supported(u32 action) {
    switch (action) {
        case ACT_IDLE: case ACT_WALKING: case ACT_DECELERATING:
        case ACT_BRAKING: case ACT_BRAKING_STOP: case ACT_TURNING_AROUND:
        case ACT_FINISH_TURNING_AROUND: case ACT_FREEFALL: case ACT_FREEFALL_LAND:
            return 1;
        default: return 0;
    }
}
static int native_enemy(const struct Object *o) {
    /* Explicit bounded enemy list avoids talking/grabbable NPCs (including
       Bob-omb Buddies and racing Koopas), doors, coins and level triggers. */
    if (o->behavior != bhvGoomba && o->behavior != bhvBobomb &&
        o->behavior != bhvSpindrift && o->behavior != bhvScuttlebug &&
        o->behavior != bhvPiranhaPlant && o->behavior != bhvSkeeter) return 0;
    if (o->oInteractType & (INTERACT_TEXT | INTERACT_DOOR | INTERACT_WARP_DOOR |
        INTERACT_STAR_OR_KEY | INTERACT_PLAYER | INTERACT_COIN)) return 0;
    return o->activeFlags && o->oIntangibleTimer == 0 && o->hitboxRadius > 0 && o->hitboxHeight > 0;
}
static void apply_sword(struct MarioState *m) {
    OotSwordQuad quads[2]; uint64_t swing = 0;
    if (!oot_link_runtime_quads(quads, &swing)) return;
    if (sSwing != swing) { memset(sHitObjects, 0, sizeof(sHitObjects)); sSwing = swing; }
    for (int index = 0; index < OBJECT_POOL_CAPACITY; ++index) {
        struct Object *o = &gObjectPool[index];
        if (sHitObjects[index] || !native_enemy(o)) continue;
        int hit = 0;
        for (int q = 0; q < 2; ++q)
            if (oot_sword_quad_hits_cylinder(&quads[q], o->oPosX, o->oPosY - o->hitboxDownOffset,
                    o->oPosZ, o->hitboxRadius, o->hitboxHeight)) hit = 1;
        if (!hit) continue;
        Vec3f from = { m->pos[0], m->pos[1] + 40.0f * OOT_LINK_WORLD_SCALE, m->pos[2] };
        Vec3f direction = { o->oPosX - from[0], o->oPosY - o->hitboxDownOffset + o->hitboxHeight * .5f - from[1], o->oPosZ - from[2] };
        Vec3f position; struct Surface *surface = NULL;
        find_surface_on_ray(from, direction, &surface, position, 1.0f);
        if (surface && surface->object != o) continue;
        /* Native enemy behaviour consumes this attack status and decides its
           own response. No object deletion/health bypass or radial damage. */
        attack_object(m, o, INT_PUNCH);
        sHitObjects[index] = 1;
    }
}
int oot_link_adapter_update(struct MarioState *m) {
    if (!m || m->playerIndex != 0) return 0;
    if (!sSelected || !oot_link_runtime_enabled() || !m->marioObj || !m->controller || !m->area || !m->floor) {
        oot_link_adapter_suspend(); return 0;
    }
    if (sPlayer && (sPlayer != m || sArea != m->area || sLevel != gCurrLevelNum)) oot_link_adapter_suspend();
    if (sHavePosition) {
        f32 x=m->pos[0]-sLastPosition[0], y=m->pos[1]-sLastPosition[1], z=m->pos[2]-sLastPosition[2];
        /* Host teleports are not animation movement. Never sweep between them. */
        if (x*x+y*y+z*z > 200.0f*200.0f) oot_link_adapter_suspend();
    }
    sPlayer=m; sArea=m->area; sLevel=gCurrLevelNum;
    if (!supported(m->action) || m->heldObj || m->riddenObj || m->heldByObj || m->quicksandDepth > 1.0f ||
        (m->input & INPUT_SQUISHED)) { oot_link_adapter_suspend(); return 0; }
    if (m->freeze || sCurrPlayMode == PLAY_MODE_PAUSED) {
        oot_link_runtime_pause();
        if (oot_link_runtime_visible()) { m->marioObj->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE; sOwnHide=1; }
        return 1;
    }
    if (m->pos[1] < m->waterLevel - 100.0f) {
        oot_link_adapter_suspend(); set_water_plunge_action(m); return 0;
    }
    int rawX = m->controller->rawStickX, rawY = m->controller->rawStickY;
    rawX = rawX < -128 ? -128 : rawX > 127 ? 127 : rawX;
    rawY = rawY < -128 ? -128 : rawY > 127 ? 127 : rawY;
    s16 x=oot_move_adjust_stick_axis((s8)rawX), y=oot_move_adjust_stick_axis((s8)rawY);
    s16 cameraYaw=m->area->camera ? m->area->camera->yaw : m->faceAngle[1];
    if (gLakituState.mode == CAMERA_MODE_NEWCAM)
        cameraYaw=get_first_person_enabled() ? gLakituState.yaw : -gNewCamera.yaw+0x4000;
    OotLinkInput input = {0};
    vec3f_copy(input.position,m->pos);
    input.host_forward_velocity=m->forwardVel; input.host_y_velocity=m->vel[1];
    input.stick_magnitude=oot_move_stick_magnitude((s8)rawX,(s8)rawY);
    input.stick_yaw=atan2s(-y,x); input.world_yaw=input.stick_yaw+cameraYaw;
    input.facing_yaw=m->faceAngle[1]; input.floor_pitch=find_floor_slope(m,0);
    input.grounded=m->action != ACT_FREEFALL;
    input.attack_pressed=(m->controller->buttonPressed & B_BUTTON)!=0;
    input.attack_held=(m->controller->buttonDown & B_BUTTON)!=0;
    /* Z chooses the source directional/parallel-target selector. This slice
       does not implement OoT hostile lock-on targeting or shield defence. */
    input.targeting=(m->controller->buttonDown & Z_TRIG)!=0;
    OotLinkStep step;
    if (!oot_link_runtime_step(&input,&step)) { oot_link_adapter_suspend(); return 0; }
    OotLinkCollision collision={0}; collision.grounded=input.grounded;
    m->faceAngle[1]=step.shape_yaw;
    if (step.source_tick) {
        vec3f_copy(m->vel,step.displacement);
        if (input.grounded) {
            /* Host ground_step multiplies XZ by floor normal.y. The source
               actor delta is already the intended world displacement. */
            if (m->floor->normal.y > .01f) {
                m->vel[0]/=m->floor->normal.y; m->vel[2]/=m->floor->normal.y;
            }
            int result=perform_ground_step(m);
            collision.hit_wall=result==GROUND_STEP_HIT_WALL;
            if (result==GROUND_STEP_LEFT_GROUND) {
                collision.left_ground=1; collision.grounded=0;
                set_mario_action(m,ACT_FREEFALL,0);
            } else if (!step.attacking) {
                /* Public native action metadata for camera/save/interaction;
                   the actual motion still comes exclusively from source. */
                m->prevAction=m->action; m->action=step.host_forward_velocity>.01f ? ACT_WALKING : ACT_IDLE;
            }
        } else {
            f32 requestedY=m->vel[1];
            int result=perform_air_step(m,0);
            /* perform_air_step includes SM64 gravity/wind AFTER collision.
               Restore the source value, then let source collision reconcile. */
            m->vel[1]=requestedY;
            collision.hit_wall=result==AIR_STEP_HIT_WALL;
            collision.hit_ceiling=requestedY>0 && m->ceil && m->pos[1]+160.0f>=m->ceilHeight;
            if (result==AIR_STEP_LANDED) {
                collision.grounded=1; set_mario_action(m,ACT_IDLE,0);
            } else if (result==AIR_STEP_HIT_LAVA_WALL) {
                oot_link_adapter_suspend(); set_mario_action(m,ACT_LAVA_BOOST,0); return 0;
            }
        }
    }
    vec3f_copy(collision.position,m->pos);
    collision.distance_to_floor=m->pos[1]-m->floorHeight;
    oot_link_runtime_commit(&collision,&step);
    /* The source action follows actor collision. Animation root motion is a
       separate task after action selection, so a B edge at a ledge can never
       start a grounded sword or leak a canceled attack's root displacement. */
    if (step.source_tick && (step.animation_displacement[0] != 0 || step.animation_displacement[2] != 0)) {
        if (!collision.grounded || !m->floor) {
            oot_link_adapter_suspend(); return 0;
        }
        vec3f_copy(m->vel,step.animation_displacement);
        if (m->floor->normal.y > .01f) {
            m->vel[0]/=m->floor->normal.y; m->vel[2]/=m->floor->normal.y;
        }
        int result=perform_ground_step(m);
        collision.hit_wall=collision.hit_wall || result==GROUND_STEP_HIT_WALL;
        if (result==GROUND_STEP_LEFT_GROUND) {
            collision.left_ground=1; collision.grounded=0;
            set_mario_action(m,ACT_FREEFALL,0);
        }
        vec3f_copy(collision.position,m->pos);
        collision.distance_to_floor=m->pos[1]-m->floorHeight;
    }
    oot_link_runtime_finish(&collision,&step);
    m->faceAngle[1]=step.shape_yaw;
    m->forwardVel=step.host_forward_velocity;
    m->vel[0]=step.host_forward_velocity*oot_move_sin(step.shape_yaw);
    m->vel[2]=step.host_forward_velocity*oot_move_cos(step.shape_yaw);
    m->vel[1]=collision.grounded ? 0 : step.host_y_velocity;
    m->slideVelX=m->vel[0]; m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);
    vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    if (step.source_tick) apply_sword(m);
    OotLinkSnapshot snapshot;
    int drawable = sWheelMode && oot_link_runtime_snapshot(&snapshot) && snapshot.drawable;
    if (drawable || oot_link_runtime_visible()) { m->marioObj->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE; sOwnHide=1; }
    else restore_visibility();
    vec3f_copy(sLastPosition,m->pos); sHavePosition=1;
    return 1;
}
