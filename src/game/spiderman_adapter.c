#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "area.h"
#include "camera.h"
#include "bettercamera.h"
#include "first_person_cam.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "level_update.h"
#include "mario.h"
#include "object_fields.h"
#include "spiderman_adapter.h"
#include "spiderman_world.h"
#include "spiderman_combat_host.h"
#include "spiderman_web_attack_host.h"
#include "spiderman_dome_host.h"
#include "pc/spiderman_runtime.h"
#include "pc/spiderman_web_scene.h"
#include "pc/spiderman_trail_scene.h"
#include "pc/spiderman_effect_scene.h"
#include "pc/spiderman_dome_scene.h"
#include "../../codex/spiderman/controller/character_n64.h"
#include "../../codex/spiderman/controller/input_n64.h"

/* Native fixed12 X,Y-down,Z -> host X,-Y,-Z, one physics unit per host unit.
 * Original body origin96 above floor maps to the host's foot origin. The source
 * controller owns motion and contacts; no Mario gravity/collision step follows. */
static int sSelected = 1, sWheelMode;
/* Exact source health is retained separately from the coarser host HUD units.
 * Saved host health is the conversion anchor: a switch with no intervening
 * gameplay has zero delta and cannot round up or refill either health pool. */
static int sSavedHealth = -1, sSavedMaximum, sSavedHostHealth, sSavedLives;
static int sHostHealthRemainder;
static int sHaveInventory, sInventoryLives;
static int32_t sSavedWebRemaining, sSavedWebCartridges;
static struct MarioState *sOwner;
static struct Area *sArea;
static s16 sLevel;
static SmN64Character sSource;
static uint16_t sCounts[300];
static Vec3f sLastPosition;
static int sHavePosition,sOwnHide,sFailed,sGameoverRequested,sRenderWaiting;
static char sFailure[160];
static uint32_t sUnavailableCommands,sUnavailableUntil;
static const char *sStatus="Original Spider-Man inactive";
const char *spiderman_adapter_status(void){return sStatus;}
const char *spiderman_adapter_unavailable_label(void){
    uint32_t remaining=sUnavailableUntil-sSource.ticks;
    if(!sOwner||!sUnavailableCommands||!remaining||remaining>=0x80000000u)return NULL;
    if(sUnavailableCommands&SMN64_COMMAND_AIM)return "AIM NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_DOME)return "WEB DOME NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_TRAP)return "WEB TRAP NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_YANK)return "WEB YANK NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_GRAB)return "GRAB NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_CARRY)return "CARRY NOT READY";
    if(sUnavailableCommands&SMN64_COMMAND_MOUNTED)return "MOUNTED ATTACK NOT READY";
    return "ACTION NOT READY";
}
uint16_t spiderman_adapter_reserved_buttons(void){
    return sSelected&&sOwner&&!sFailed&&spiderman_runtime_enabled()&&sCurrPlayMode!=PLAY_MODE_PAUSED
        ?U_CBUTTONS|D_CBUTTONS|L_CBUTTONS|L_TRIG|R_TRIG|Z_TRIG:0;
}
static void restore_visibility(void){
    if(sOwnHide&&sOwner&&sOwner->marioObj)sOwner->marioObj->header.gfx.node.flags&=~GRAPH_RENDER_INVISIBLE;
    sOwnHide=0;
}
static void remember_shared_health(struct MarioState *m) {
    if (!sWheelMode || !m || sSource.maximum_health <= 0) return;
    sSavedHealth = sSource.combat.damage.health;
    sSavedMaximum = sSource.maximum_health;
    sSavedHostHealth = m->health;
    sSavedLives = m->numLives;
}
static void host_health_into_source(struct MarioState *m) {
    if (!sWheelMode || !m || sSource.maximum_health <= 0) return;
    int maximum = sSource.maximum_health;
    int health;
    if (sSavedHealth < 0 || sSavedMaximum != maximum || sSavedLives != m->numLives) {
        int scaled = (m->health - 0x80) * maximum;
        health = scaled / 0x800;
        sHostHealthRemainder = scaled % 0x800;
    } else {
        /* Only actual host healing/damage since the last committed frame can
         * change retained source health. Suspension/switching is not healing. */
        int scaled = (m->health - sSavedHostHealth) * maximum + sHostHealthRemainder;
        health = sSavedHealth + scaled / 0x800;
        sHostHealthRemainder = scaled % 0x800;
    }
    if (health < 0) { health = 0; sHostHealthRemainder = 0; }
    if (health > maximum) { health = maximum; sHostHealthRemainder = 0; }
    sSource.combat.damage.health = (int16_t)health;
    sSource.web_owner.health = (int16_t)health;
    remember_shared_health(m);
}
static void source_health_into_host(struct MarioState *m) {
    if (!sWheelMode || !m || sSavedHealth < 0 || sSource.maximum_health <= 0) return;
    int delta = sSource.combat.damage.health - sSavedHealth;
    int maximum = sSource.maximum_health;
    if (delta < 0) m->health -= (s16)(((-delta) * 0x800 + maximum - 1) / maximum);
    else if (delta > 0) m->health += (s16)(delta * 0x800 / maximum);
    /* Keep native death dispatch out of the original source death animation.
     * Source zero health blocks switching; its existing gameover request owns
     * the death warp/life decrement. Native hazards keep their normal owner. */
    if (m->health < 0x100) m->health = 0x100;
    if (m->health > 0x880) m->health = 0x880;
    remember_shared_health(m);
}
static void remember_shared_inventory(struct MarioState *m) {
    if (!sWheelMode || !m) return;
    sSavedWebRemaining = sSource.web.player.resource.remaining;
    sSavedWebCartridges = sSource.web.player.resource.cartridges;
    sInventoryLives = m->numLives;
    sHaveInventory = 1;
}
static void restore_shared_inventory(struct MarioState *m) {
    if (!sWheelMode || !m || !sHaveInventory || sInventoryLives != m->numLives) return;
    sSource.web.player.resource.remaining = sSavedWebRemaining;
    sSource.web.player.resource.cartridges = sSavedWebCartridges;
}
void spiderman_adapter_set_selected(int selected) {
    sWheelMode = 1;
    if (!selected) spiderman_adapter_suspend();
    sSelected = !!selected;
}
const char *spiderman_adapter_switch_reason(void) {
    if (!sOwner) return NULL;
    if (sFailed || sRenderWaiting) return "Wait for the Spider-Man renderer";
    if (sSource.actor.held_object || sSource.combat.carry.held_actor || sSource.combat.carry.pickup_actor)
        return "Release the carried object first";
    if (sSource.combat.damage.health <= 0 || sGameoverRequested)
        return "Cannot switch during a death sequence";
    if (sSource.actor.adhered || !(sSource.actor.collision & 2) ||
        (sSource.actor.state != 1 && sSource.actor.state != 2))
        return "Finish the action and stand on the ground";
    return NULL;
}
void spiderman_adapter_suspend(void){
    if (sOwner) {
        host_health_into_source(sOwner);
        remember_shared_inventory(sOwner);
    }
    restore_visibility();spiderman_runtime_suspend();spiderman_web_scene_suspend();spiderman_trail_scene_suspend();spiderman_effect_scene_suspend();spiderman_dome_scene_suspend();spiderman_dome_host_reset();spiderman_web_attack_host_reset();spiderman_combat_host_reset_actor();sOwner=NULL;sArea=NULL;sHavePosition=sFailed=sGameoverRequested=sRenderWaiting=0;
    sUnavailableCommands=sUnavailableUntil=0;memset(&sSource,0,sizeof sSource);sStatus="Original Spider-Man inactive";
}
int spiderman_adapter_snapshot(SpidermanBehaviorSnapshot *out){
    if(!out||!sOwner)return 0;
    out->state=sSource.actor.state;out->ticks=sSource.ticks;out->adhered=sSource.actor.adhered;
    out->wall=sSource.actor.wall_class;out->ceiling=sSource.actor.ceiling_class;
    out->clip=sSource.actor.anim.animation;out->frame=sSource.actor.anim.frame;
    out->health=sSource.combat.damage.health;out->maximum_health=sSource.maximum_health;out->web_remaining=sSource.web.player.resource.remaining;out->web_cartridges=sSource.web.player.resource.cartridges;
    out->glove_hits=(int32_t)sSource.combat.web.ability.glove_hits;out->combat_target=sSource.combat.owner.target_actor;
    memcpy(out->native_position,sSource.actor.position,sizeof out->native_position);
    out->collision=sSource.actor.collision;out->side_present=sSource.actor.side.present;out->approach_ticks=sSource.actor.approach_ticks;
    for(int i=0;i<3;++i){out->side_normal[i]=sSource.actor.side.normal[i];out->forward[i]=sSource.actor.basis.forward[i];}return 1;
}
static int supported_action(u32 action){
    switch(action){
        case ACT_IDLE:case ACT_WALKING:case ACT_DECELERATING:case ACT_BRAKING:
        case ACT_BRAKING_STOP:case ACT_TURNING_AROUND:case ACT_FINISH_TURNING_AROUND:
        case ACT_FREEFALL:case ACT_FREEFALL_LAND:return 1;
        default:return 0;
    }
}
static int native_position(const float host[3],int32_t native[3]){
    double v[3]={host[0],-((double)host[1]+96),-(double)host[2]};
    for(int k=0;k<3;++k){double n=round(v[k]*4096);if(!isfinite(n)||n<INT32_MIN||n>INT32_MAX)return 0;native[k]=(int32_t)n;}return 1;
}
static int pose(void *context,int clip,int frame,int16_t out[216]){
    (void)context;return spiderman_runtime_pose_s16(clip,frame,out,216);
}
static int trace(void *context,const SmN64ClimbQuery *q,SmN64ClimbHit *out){
    (void)context;if(!q||!out||q->line_byte_88||q->arg2||q->arg3||q->arg4!=1)return -3;
    SpidermanWorldHit h;int rc=spiderman_world_trace_ex(q->start,q->end,q->arg1,&h);if(rc!=1)return rc;
    memset(out,0,sizeof *out);out->present=h.present;
    if(h.present){out->has_surface=1;out->distance=h.distance;out->surface_flags=h.source_flags;
        memcpy(out->position,h.position,sizeof out->position);memcpy(out->normal,h.normal,sizeof out->normal);}
    return 1;
}
static int free_trace(void *context,const SmN64FreeQuery *q,SmN64FreeHit *out){
    if(!q||!out)return -3;
    memset(out,0,sizeof *out);int rc=trace(context,&q->ray,&out->hit);if(rc!=1)return rc;
    if(q->kind==SMN64_FREE_DOWN&&out->hit.present){
        /* Explicit host metadata record, NOT Neversoft triangle/shadow layout.
         * The recovered contact owner only retains these opaque15words. */
        SpidermanWorldHit h;rc=spiderman_world_trace_ex(q->ray.start,q->ray.end,q->ray.arg1,&h);if(rc!=1||!h.surface)return -3;
        out->ground_record[0]=1;
        const s16 *v[3]={h.surface->vertex1,h.surface->vertex2,h.surface->vertex3};
        for(int i=0;i<3;++i)for(int k=0;k<3;++k)out->ground_record[1+i*3+k]=(uint32_t)((int32_t)v[i][k]*(k? -4096:4096));
        for(int k=0;k<3;++k)out->ground_record[10+k]=(uint32_t)(int32_t)h.normal[k];
        out->ground_record[13]=h.source_flags;out->ground_record[14]=(uint16_t)h.surface->type;out->has_ground_record=1;
    }return 1;
}
static int incoming_damage(void *context,SmN64Character *c,SmN64CombatOwnerFrame *frame){
    (void)context;
    for(size_t i=0;i<16;++i){SmN64CombatHit hit;int rc=spiderman_combat_host_incoming(i,&hit);if(rc<0)return rc;if(!rc)return 1;
        rc=smn64_character_combat_damage(&c->combat,frame,&hit);if(rc<0)return rc;
        if(spiderman_combat_host_incoming_result(i,rc)!=1)return -21;
    }
    SmN64CombatHit extra;return spiderman_combat_host_incoming(16,&extra)==0?1:-21;
}
static int retain_trails(void *context,const SmN64ClimbState *a,const SmN64Marker *markers,size_t n,const int16_t pose[216],const int32_t body[3],uint32_t tick){
    (void)context;return spiderman_combat_host_trails_retain(a,markers,n,pose,body,tick);
}
static int step_trails(void *context,uint32_t tick){(void)context;return spiderman_combat_host_trails_effects(tick);}
typedef struct DomeEffectFrame { SmN64CombatOwnerFrame *frame;SmN64CharacterCombat *combat; } DomeEffectFrame;
static int dome_graphics(void *context,uint64_t *clock){
    DomeEffectFrame *f=context;if(!f||clock!=f->frame->graphical_clock)return -2;
    return spiderman_dome_host_graphics(f->frame,f->combat);
}
static int step_attacks(void *context,SmN64CombatOwnerFrame *frame,SmN64CharacterCombat *combat){
    (void)context;DomeEffectFrame f={frame,combat};
    if(spiderman_dome_host_misc(frame,combat)!=1)return -2;
    if(spiderman_combat_host_carry_tick(frame,combat)!=1)return -2;
    return spiderman_web_attack_host_effects_interleaved(frame,combat,dome_graphics,&f);
}
static SmN64CharacterServices services={NULL,free_trace,trace,pose,NULL,NULL,incoming_damage,retain_trails,step_trails,step_attacks};
static uint32_t buttons(u16 mask){
    return ((mask&U_CBUTTONS)?SMN64_BUTTON_WEB:0)|((mask&L_CBUTTONS)?SMN64_BUTTON_KICK:0)|
        ((mask&D_CBUTTONS)?SMN64_BUTTON_PUNCH:0)|((mask&A_BUTTON)?SMN64_BUTTON_JUMP:0)|
        ((mask&(L_TRIG|Z_TRIG))?SMN64_BUTTON_AIM:0)|((mask&B_BUTTON)?SMN64_BUTTON_ZIP:0)|
        ((mask&R_TRIG)?SMN64_BUTTON_SWING:0);
}
static int submit(struct MarioState *m){
    SpidermanRenderSnapshot s={0};s.position[0]=sSource.actor.position[0]/4096.0f;
    s.position[1]=-(float)sSource.actor.position[1]/4096.0f;s.position[2]=-(float)sSource.actor.position[2]/4096.0f;
    s.host_scale=2.25f;s.clip_slot=sSource.actor.anim.animation;s.frame_index=sSource.actor.anim.frame;s.ticks=sSource.ticks;
    s.orientation_mode=SPIDERMAN_ORIENTATION_NATIVE_BASIS;memcpy(s.native_body_matrix,sSource.actor.basis.matrix,sizeof s.native_body_matrix);
    SmN64Trail trails[SPIDERMAN_HOST_TRAIL_CAPACITY];size_t trail_count=0;
    if(spiderman_combat_host_trails_snapshot(trails,SPIDERMAN_HOST_TRAIL_CAPACITY,&trail_count)!=1||
       !spiderman_trail_scene_submit(trails,trail_count,sSource.ticks))return 0;
    if(!spiderman_web_scene_submit(&sSource.visuals,sSource.ticks))return 0;
    SmN64WebAttackObject attacks[SMN64_WEB_ATTACK_CAPACITY];size_t attack_count=0;
    if(spiderman_web_attack_host_snapshot(attacks,SMN64_WEB_ATTACK_CAPACITY,&attack_count)!=1||
       !spiderman_effect_scene_submit(&sSource.visuals,attacks,attack_count,trails,trail_count,sSource.ticks,sSource.graphical_clock))return 0;
    SpidermanDomeInstance dome[SMN64_DOME_BODY_CAPACITY];size_t dome_count=0;
    SmN64DomeShatterFragment shards[SPIDERMAN_DOME_SHARD_CAPACITY];size_t shard_count=0;
    if(spiderman_dome_host_snapshot(dome,SMN64_DOME_BODY_CAPACITY,&dome_count)!=1||
       !spiderman_dome_scene_submit(dome,dome_count,sSource.ticks)||
       spiderman_dome_host_shards(shards,SPIDERMAN_DOME_SHARD_CAPACITY,&shard_count)!=1||
       !spiderman_effect_scene_submit_dome_shards(shards,shard_count,sSource.ticks))return 0;
    if(!spiderman_runtime_submit(&s))return 0;
    /* A validated exact source snapshot owns this same render frame, including
     * the first frame after warp. Waiting for last frame's draw overlaps Mario
     * with Spider-Man for one frame on each fresh ownership handoff. */
    m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;return 1;
}
static int stop_source(struct MarioState *m,const char *reason){
    if(!sFailed)fprintf(stderr,"Original Spider-Man paused safely: %s\n",reason);
    sFailed=1;sSource.stable_updates=0;sStatus=reason;vec3f_set(m->vel,0,0,0);m->forwardVel=m->slideVelX=m->slideVelZ=0;
    if(sOwnHide||spiderman_runtime_visible()){m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;}else restore_visibility();return 1;
}
/* Render submission and gameplay are one ownership interval. A draw/camera
 * failure cannot allow another source tick or fall back to Mario simulation.
 * Keep the last committed source state; recovery requires explicit suspend via
 * a genuine host handoff/restart. Inactive scenes do not report failures. */
int spiderman_adapter_render_required(void){return sOwner&&sOwnHide;}
void spiderman_adapter_render_wait(void){
    if(!spiderman_adapter_render_required()||sFailed)return;
    sRenderWaiting=1;sSource.stable_updates=0;
    vec3f_set(sOwner->vel,0,0,0);sOwner->forwardVel=sOwner->slideVelX=sOwner->slideVelZ=0;
    sOwner->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;
    sStatus="Original Spider-Man waiting for a usable host camera";
}
void spiderman_adapter_render_success(void){
    if(!spiderman_adapter_render_required()||sFailed)return;
    if(sRenderWaiting)sStatus="Original Spider-Man traversal controller active";
    sRenderWaiting=0;
}
void spiderman_adapter_render_failure(const char *reason){
    if(!spiderman_adapter_render_required()||sFailed)return;
    snprintf(sFailure,sizeof sFailure,"source rendering stopped: %s",reason?reason:"unspecified renderer failure");
    (void)stop_source(sOwner,sFailure);
}
static void committed_gameflow(struct MarioState *m){
    if(sGameoverRequested)return;
    for(uint32_t i=0;i<sSource.combat.effect_count;++i){
        if(sSource.combat.effects[i].kind==SMN64_COMBAT_GAMEOVER){
            /* Source death owner already waited its original120 ticks. The
             * cross-game destination is the host's existing death warp; native
             * death-exit code owns its usual life decrement. No extra timer. */
            sGameoverRequested=1;level_trigger_warp(m,WARP_OP_DEATH);return;
        }
    }
}
int spiderman_adapter_enemy_contact(struct MarioState *m,struct Object *object){
    if(!sSelected||!m||!object||sOwner!=m||!spiderman_runtime_enabled())return 0;
    if(sFailed||sRenderWaiting)return 1;
    int rc=spiderman_combat_host_queue_incoming(m,object);
    if(rc<0){stop_source(m,spiderman_combat_host_error());return 1;}
    return rc==1;
}
int spiderman_adapter_process_interaction(struct MarioState *m,struct Object *object){
    if(!sSelected||!m||!object||sOwner!=m||!spiderman_runtime_enabled()||!spiderman_combat_host_owns_interaction(object))return 0;
    if(sFailed||sRenderWaiting)return 1;
    /* Vetted enemy/carry roles use source attacks, never Mario's implicit
     * bounce-top punch or B-button grab. Positive native contact damage is an
     * explicit queued source packet; actual recipient writes wait for commit. */
    (void)spiderman_adapter_enemy_contact(m,object);return 1;
}
int spiderman_adapter_prepare_interactions(struct MarioState *m){
    if(!m||m->playerIndex!=0)return 0;
    if(!sSelected||!spiderman_runtime_enabled()||!m->marioObj||!m->controller||!m->area||!m->floor){spiderman_adapter_suspend();return 0;}
    if(sOwner&&(sOwner!=m||sArea!=m->area||sLevel!=gCurrLevelNum))spiderman_adapter_suspend();
    if(sHavePosition){float x=m->pos[0]-sLastPosition[0],y=m->pos[1]-sLastPosition[1],z=m->pos[2]-sLastPosition[2];if(x*x+y*y+z*z>200*200)spiderman_adapter_suspend();}
    if(!supported_action(m->action)||m->health<0x100||m->heldObj||m->riddenObj||m->heldByObj||m->quicksandDepth>1||
        (m->action!=ACT_FREEFALL&&SURFACE_IS_QUICKSAND(m->floor->type))||(m->input&INPUT_SQUISHED)){spiderman_adapter_suspend();return 0;}
    if(m->pos[1]<m->waterLevel-100){spiderman_adapter_suspend();set_water_plunge_action(m);return 0;}
    services.combat=spiderman_dome_host_services();
    if(!sOwner){
        int32_t position[3];SmN64Marker marker[9];if(!native_position(m->pos,position)||spiderman_runtime_marker_count()!=9)return 0;
        for(int i=0;i<300;++i){int n=spiderman_runtime_frame_count(i);if(n<1||n>65535)return 0;sCounts[i]=(uint16_t)n;}
        for(int i=0;i<9;++i)if(!spiderman_runtime_marker_record(i,marker[i].xyz,&marker[i].joint))return 0;
        uint16_t yaw=(uint16_t)(-(int)((u16)m->faceAngle[1]>>4))&4095;
        if(smn64_character_init(&sSource,position,yaw,0x534D3634u,marker,sCounts,300,&services)<0)return 0;
        restore_shared_inventory(m);
        host_health_into_source(m);
        sOwner=m;sArea=m->area;sLevel=gCurrLevelNum;
        if(m->action==ACT_FREEFALL){
            sSource.actor.collision=0;sSource.actor.ground_grace=0;sSource.actor.state=4;
            smn64_anim_run(&sSource.actor.anim,212,sCounts[212],0,-1);
            for(int k=0;k<3;++k){double v=round((double)m->vel[k]*(k?-4096:4096));sSource.actor.velocity[k]=isfinite(v)&&v>=INT32_MIN&&v<=INT32_MAX?(int32_t)v:0;}
            smn64_bridge_actor_to_ground(&sSource.actor,&sSource.ground);
        }
    }
    host_health_into_source(m);
    return 1;
}
int spiderman_adapter_update(struct MarioState *m){
    if(!spiderman_adapter_prepare_interactions(m))return 0;
    if(sFailed)return stop_source(m,sStatus);
    if(sRenderWaiting){spiderman_adapter_render_wait();return 1;}
    if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED){sSource.stable_updates=0;if(spiderman_runtime_visible()){m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;}return 1;}
    SmN64CharacterInput input={0};int x=m->controller->rawStickX,y=m->controller->rawStickY;int8_t logical[2];
    smn64_input_axes((int8_t)(x< -128?-128:x>127?127:x),(int8_t)(y< -128?-128:y>127?127:y),logical);
    input.motion.stick_x=logical[0];input.motion.stick_y=logical[1];input.pressed=buttons(m->controller->buttonPressed);input.held=buttons(m->controller->buttonDown);
    s16 camera=m->area->camera?m->area->camera->yaw:m->faceAngle[1];
    if(gLakituState.mode==CAMERA_MODE_NEWCAM)camera=get_first_person_enabled()?gLakituState.yaw:-gNewCamera.yaw+0x4000;
    input.motion.camera_yaw=(uint16_t)(2048-((u16)camera>>4))&4095;input.motion.elapsed_ticks=2;
    input.effect_seconds=2.0f*(1.0f/60.0f);
    for(int k=0;k<3;++k)input.camera_position[k]=(int32_t)(gLakituState.pos[k]*(k?-1.0f:1.0f));
    SmN64Character pending=sSource;
    if(spiderman_combat_host_begin(m,&sSource.actor)!=1)return stop_source(m,spiderman_combat_host_error());
    if(spiderman_web_attack_host_begin()!=1){spiderman_combat_host_abort();return stop_source(m,"original attack registry begin rejected");}
    if(spiderman_dome_host_begin()!=1){spiderman_web_attack_host_abort();spiderman_combat_host_abort();return stop_source(m,"original dome registry begin rejected");}
    int rc=smn64_character_tick(&pending,&input,sCounts,300,&services);
    if(rc<0){spiderman_dome_host_abort();spiderman_web_attack_host_abort();spiderman_combat_host_abort();}
    else if(spiderman_dome_host_validate()!=1||spiderman_web_attack_host_validate()!=1){spiderman_dome_host_abort();spiderman_web_attack_host_abort();spiderman_combat_host_abort();return stop_source(m,"original effect registries validation rejected");}
    else if(spiderman_combat_host_commit()!=1){spiderman_dome_host_abort();spiderman_web_attack_host_abort();spiderman_combat_host_abort();return stop_source(m,spiderman_combat_host_error());}
    else {spiderman_dome_host_finalize();spiderman_web_attack_host_finalize();sSource=pending;
        source_health_into_host(m);
        /* Source shake-kind1 maps explicitly to SM64's point-small camera
         * shake; native camera owns duration/amplitude, not original N64 code.
         * Publish only after every state/recipient transaction commits. */
        SpidermanDomeShake shakes[SPIDERMAN_DOME_SHAKE_CAPACITY];size_t n=0;
        if(spiderman_dome_host_shakes(shakes,SPIDERMAN_DOME_SHAKE_CAPACITY,&n)==1)
            for(size_t i=0;i<n;i++)set_camera_shake_from_point(SHAKE_POS_SMALL,shakes[i].position[0]/4096.f,-shakes[i].position[1]/4096.f,-shakes[i].position[2]/4096.f);
        if(sSource.combat.unavailable_requested){sUnavailableCommands=sSource.combat.unavailable_requested;sUnavailableUntil=sSource.ticks+120;}
        committed_gameflow(m);}
    if(rc<0){snprintf(sFailure,sizeof sFailure,"source owner rejected %d (state 0x%x, clip %u)",rc,sSource.actor.state,sSource.actor.anim.animation);return stop_source(m,sFailure);}
    m->pos[0]=sSource.actor.position[0]/4096.0f;m->pos[1]=-(float)sSource.actor.position[1]/4096.0f-96;m->pos[2]=-(float)sSource.actor.position[2]/4096.0f;
    struct Surface *floor=NULL;float height=find_floor(m->pos[0],m->pos[1]+100,m->pos[2],&floor);if(floor){m->floor=floor;m->floorHeight=height;}
    m->prevAction=m->action;m->action=sSource.actor.adhered?ACT_FREEFALL:((sSource.actor.collision&2)?((logical[0]||logical[1])?ACT_WALKING:ACT_IDLE):ACT_FREEFALL);
    m->faceAngle[1]=(s16)(u16)(-(int)(sSource.actor.yaw&4095)*16);
    m->vel[0]=sSource.actor.velocity[0]/4096.0f;m->vel[1]=-(float)sSource.actor.velocity[1]/4096.0f;m->vel[2]=-(float)sSource.actor.velocity[2]/4096.0f;
    m->forwardVel=hypotf(m->vel[0],m->vel[2]);m->slideVelX=m->vel[0];m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    if(!submit(m))return stop_source(m,"original pose submission rejected");
    vec3f_copy(sLastPosition,m->pos);sHavePosition=1;sStatus="Original Spider-Man traversal controller active";return 1;
}
