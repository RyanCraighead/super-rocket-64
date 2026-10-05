#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "area.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "engine/surface_load.h"
#include "level_update.h"
#include "mario.h"
#include "mario_step.h"
#include "object_fields.h"
#include "thps_adapter.h"
#include "thps_rails.h"
#include "pc/thps_runtime.h"
#include "../../codex/thps/mechanics/thps_skate.h"

/* Explicit Mario-world presentation mapping, not THPS collision dimensions.
 * Rev1 owner0x80026D2C scales posed raw geometry by .5 source-world units;
 * its additional1/16 renderer scale applies equally to body translation.
 * Our asset exporter uses8/36, hence source-to-host=render_scale*(4/9).
 * 1.75 makes the original Hawk+board silhouette approximately 160 host units.
 * Original clip0 wheel-bottom is -15.283079 in those normalized coordinates.
 */
#define THPS_RENDER_SCALE 1.75f
#define THPS_SOURCE_SCALE (THPS_RENDER_SCALE * (4.0f / 9.0f))
#define THPS_BOARD_ORIGIN_OFFSET (15.283079f * THPS_RENDER_SCALE)
#define THPS_BODY_OFFSET_SOURCE (THPS_BOARD_ORIGIN_OFFSET / THPS_SOURCE_SCALE)
/* Legacy launchers select their sole controller by default. The opt-in wheel
 * explicitly assigns one owner after all available assets are preloaded. */
static int sSelected = 1;
static struct MarioState *sOwner;
static struct Area *sArea;
static s16 sLevel;
static ThpsSkateState sSource;
static int16_t sPitchRotations[4096*9],sYawRotations[4096*9];
static Vec3f sLastPosition;
static int sHavePosition, sOwnHide, sFailed, sRenderWaiting;
static int sWindowActive=1;
static char sFailure[192];
static unsigned sUnavailableTicks;
static const char *sUnavailable;
static const char *sStatus="Original THPS1 skating inactive";
static unsigned sTrickLabelTicks;
static const char *sLastTrickLabel;
static int sMappedRails;

const char *thps_adapter_status(void) { return sStatus; }
const char *thps_adapter_unavailable_label(void) {return sOwner && sUnavailableTicks ? sUnavailable : NULL;}
const char *thps_adapter_trick_label(void) {
    if(!sOwner)return NULL;
    if(sSource.bail.active)return "BAIL";
    if(sSource.mode==THPS_SKATE_GRIND) {
        static const char *names[]={"FS BOARDSLIDE","BS BOARDSLIDE","5-0 GRIND","NOSEGRIND","50 50 GRIND","SMITH GRIND","CROOKED GRIND"};
        return sSource.grind.trick>=2 && sSource.grind.trick<=8 ? names[sSource.grind.trick-2] : "GRIND";
    }
    if(sSource.tricks.active) {
        const Thps1TrickRecord *record=thps1_trick_record(sSource.tricks.trick_flags&255u);
        if(record && record->name && *record->name)return record->name;
    }
    return sTrickLabelTicks ? sLastTrickLabel : NULL;
}
const char *thps_adapter_score_name(int32_t index) {
 if(index>=0 && index<36) {
  const Thps1TrickRecord *r=thps1_trick_record((unsigned)index);return r?r->name:NULL;
 }
 if(index>=68 && index<=74) {
  static const char *names[]={"FS BOARDSLIDE","BS BOARDSLIDE","5-0 GRIND","NOSEGRIND","50 50 GRIND","SMITH GRIND","CROOKED GRIND"};
  return names[index-68];
 }
 return NULL;
}
int thps_adapter_score_snapshot(ThpsScoreSnapshot *out) {
 if(!out || !sOwner || sFailed || !sSelected)return 0;
 const Thps1Score *s=&sSource.scoring;memset(out,0,sizeof *out);
 out->total=thps1_score_total(s);out->combo=thps1_score_preview(s,sSource.spin.completed180);
 out->last_award=s->last_award;out->last_bail=s->last_bail;out->best_combo=s->best_combo;
 out->base=thps1_score_base(s);out->multiplier2=thps1_score_multiplier2(s->count,sSource.spin.completed180);
 out->count=s->count;out->last_count=s->last_count;out->last_base=s->last_base;out->last_multiplier2=s->last_multiplier2;
 out->meter=s->meter;out->special=s->special;out->enabled=s->enabled;out->result=s->result;
 out->bank_delay=s->bank_delay;out->boost_ticks=sSource.score_boost_ticks;
 memcpy(out->repetitions,s->repetitions,sizeof out->repetitions);
 memcpy(out->attempt_repetitions,s->attempt_repetitions,sizeof out->attempt_repetitions);
 memcpy(out->entry_points,s->entry_points,sizeof out->entry_points);memcpy(out->entry_index,s->entry_index,sizeof out->entry_index);
 memcpy(out->entry_spin_degrees,s->entry_spin_degrees,sizeof out->entry_spin_degrees);
 return 1;
}
int thps_adapter_balance_percent(int *out) {
    if(!out || !sOwner || sSource.mode!=THPS_SKATE_GRIND)return 0;
    *out=sSource.grind.balance/40;return 1;
}
void thps_adapter_set_window_active(int active) {
    sWindowActive=!!active;
    if(!sWindowActive)thps_adapter_pause_inputs();
}
static void restore_visibility(void) {
    if(sOwnHide && sOwner && sOwner->marioObj)
        sOwner->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
    sOwnHide=0;
}
void thps_adapter_set_selected(int selected) {
    if (!selected) thps_adapter_suspend();
    sSelected = !!selected;
}
void thps_adapter_suspend(void) {
    restore_visibility();thps_runtime_suspend();
    sOwner=NULL;sArea=NULL;sHavePosition=sFailed=sRenderWaiting=0;
    sUnavailableTicks=0;sUnavailable=NULL;
    sTrickLabelTicks=0;sLastTrickLabel=NULL;sMappedRails=0;
    memset(&sSource,0,sizeof sSource);sStatus="Original THPS1 skating inactive";
}
const char *thps_adapter_switch_reason(void) {
    if (!sOwner) return NULL;
    if (sFailed || sRenderWaiting) return "Wait for the skater renderer";
    if (sSource.mode != THPS_SKATE_GROUND || sSource.bail.active || sSource.tricks.active)
        return "Finish the trick or bail and land first";
    return NULL;
}
static uint8_t sGamepadSpin;
int thps_adapter_controller_active(void) { return sSelected && sOwner && !sFailed && thps_runtime_enabled(); }
void thps_adapter_set_gamepad_spin(uint8_t mask) { sGamepadSpin=mask&15; }
uint16_t thps_adapter_reserved_buttons(void) {
    return sSelected && sOwner && !sFailed && thps_runtime_enabled() && sCurrPlayMode!=PLAY_MODE_PAUSED
        ? U_CBUTTONS|D_CBUTTONS|L_CBUTTONS|R_CBUTTONS|L_TRIG|R_TRIG|U_JPAD|D_JPAD|L_JPAD|R_JPAD : 0;
}
static int stop_source(struct MarioState *m,const char *reason) {
    if(!sFailed)fprintf(stderr,"THPS1 skating stopped safely: %s\n",reason);
    sFailed=1;sStatus=reason;
    vec3f_set(m->vel,0,0,0);m->forwardVel=m->slideVelX=m->slideVelZ=0;
    if(sOwnHide)m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;
    return 1;
}
int thps_adapter_render_required(void) { return sOwner && sOwnHide; }
void thps_adapter_render_wait(void) {
    if(!thps_adapter_render_required() || sFailed)return;
    sRenderWaiting=1;
    vec3f_set(sOwner->vel,0,0,0);sOwner->forwardVel=sOwner->slideVelX=sOwner->slideVelZ=0;
    sOwner->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;
    sStatus="THPS1 waiting for a usable host camera";
}
void thps_adapter_render_success(void) {
    if(!thps_adapter_render_required() || sFailed)return;
    sRenderWaiting=0;sStatus="Original THPS1 skating slice active";
}
void thps_adapter_render_failure(const char *reason) {
    if(!thps_adapter_render_required() || sFailed)return;
    snprintf(sFailure,sizeof sFailure,"original skater rendering failed: %s",reason?reason:"unknown error");
    stop_source(sOwner,sFailure);
}
static int supported(u32 action) {
    switch(action) {
        case ACT_IDLE:case ACT_WALKING:case ACT_DECELERATING:case ACT_BRAKING:
        case ACT_BRAKING_STOP:case ACT_TURNING_AROUND:case ACT_FINISH_TURNING_AROUND:
        case ACT_FREEFALL:case ACT_FREEFALL_LAND:return 1;
        default:return 0;
    }
}
static int valid_position(const float p[3]) {
    for(int i=0;i<3;i++)if(!isfinite(p[i]) || fabsf(p[i])>1e7f)return 0;
    return 1;
}
static int rail_geometry_ready(const struct MarioState *m) {
    if(!m || !m->area || !m->area->terrainData ||
       !thps_rails_for_level(gCurrLevelNum,m->area->index,NULL))return 0;
    u32 words=get_area_terrain_size(m->area->terrainData);
    return thps_rails_collision_matches(gCurrLevelNum,m->area->index,m->area->terrainData,words);
}
static int source_point(const float host[3],int32_t out[3]) {
    for(int k=0;k<3;k++) {
        double value=round((double)host[k]*(4096.0/THPS_SOURCE_SCALE)*(k==1?-1:1));
        if(!isfinite(value) || value<INT32_MIN || value>INT32_MAX)return 0;
        out[k]=(int32_t)value;
    }
    return 1;
}
static void host_point(const int32_t source[3],float out[3]) {
    for(int k=0;k<3;k++)out[k]=(float)source[k]*(THPS_SOURCE_SCALE/4096.f)*(k==1?-1:1);
}
static int lookup_rail(void *context,int32_t id,Thps1GrindRail *out) {
    const struct MarioState *m=context;
    if(!out || id<=0 || !rail_geometry_ready(m))return 0;
    const ThpsHostRail *rail=thps_rail_by_id(gCurrLevelNum,m->area->index,(uint32_t)id);
    if(!rail)return 0;
    Thps1GrindRail mapped={0};mapped.id=(int32_t)rail->id;mapped.chain_id=mapped.id;
    mapped.previous_id=rail->previous_id?(int32_t)rail->previous_id:THPS1_GRIND_NO_RAIL;
    mapped.next_id=rail->next_id?(int32_t)rail->next_id:THPS1_GRIND_NO_RAIL;
    if(!source_point(rail->start,mapped.a)||!source_point(rail->end,mapped.b))return 0;
    *out=mapped;return 1;
}
static int prepare_rail_query(struct MarioState *m,const ThpsSkateState *state,const ThpsSkateInput *input,
                              Thps1GrindEntry *entry,ThpsSkateHost *host) {
    memset(entry,0,sizeof *entry);memset(host,0,sizeof *host);
    sMappedRails=rail_geometry_ready(m);
    if(state->mode==THPS_SKATE_GRIND&&!sMappedRails)return 0;
    if(!sMappedRails)return 1;
    host->rail_lookup=lookup_rail;host->rail_context=m;
    if(!input->grind || !state->body_offset_bound)return 1;
    ThpsHostRailQuery query={0};ThpsHostRailHit hit={0};
    /* Original68cf4 queries previous/current BODY sweep. The closest point may
     * precede the current body; don't invent an additional current-only gate. */
    host_point(state->source_previous_position,query.previous);
    host_point(state->source_body_position,query.current);
    memcpy(query.motion,state->velocity,sizeof query.motion);
    query.requested=1;query.radius=100.f*THPS_SOURCE_SCALE;query.minimum_alignment=0;
    if(!thps_rails_trace(gCurrLevelNum,m->area->index,&query,&hit))return 1;
    if(!lookup_rail(m,(int32_t)hit.rail->id,&entry->rail) || !source_point(hit.point,entry->closest))return 0;
    memcpy(entry->actor_position,state->source_body_position,sizeof entry->actor_position);
    entry->distance=(int32_t)(hit.distance/THPS_SOURCE_SCALE);entry->candidate_valid=1;
    /* ready is ORIGINAL+858 trick-sequence state, not geometry. The canonical
     * source wrapper overrides it from its recovered latch. */
    entry->ready=0;host->grind_candidate=entry;
    return 1;
}
static int rail_motion_clear(const ThpsSkateState *before,const ThpsSkateState *after) {
    float a[3],b[3];host_point(before->source_body_position,a);host_point(after->source_body_position,b);
    Vec3f ray={b[0]-a[0],b[1]-a[1],b[2]-a[2]},hit;struct Surface *surface=NULL;
    if(ray[0]*ray[0]+ray[1]*ray[1]+ray[2]*ray[2]<.000001f)return 1;
    find_surface_on_ray(a,ray,&surface,hit,1.f);
    /* Bounded host safety gate only; original grind obstacle/wall collision is
     * not substituted with a guessed bail. A newly blocked segment fails safe. */
    return surface==NULL;
}
static int publish(struct MarioState *m,const ThpsSkateState *state) {
    if(!isfinite(state->animation_frame) || state->animation_frame<0 || state->animation_frame>65535 || !valid_position(m->pos))return 0;
    int frame=(int)state->animation_frame;
    if(frame>=thps_runtime_frame_count(state->animation))return 0;
    ThpsRenderSnapshot render={0};vec3f_copy(render.position,m->pos);
    /* Preserve the exact source physical basis rotations. The initial floor
     * basis is a documented host mapping; source stance bit2 reflects localX.
     * NativeN columns[-D*S,-D*U,D*F], D=diag(1,1,-1), maintain the established
     * mesh reflection while keeping original F opposite the travel direction. */
    render.use_body_basis=1;
    for(int row=0;row<3;row++) {
        int d=row==2?-1:1;
        int32_t values[3]={-d*state->source_side[row],-d*state->source_up[row],d*state->source_forward[row]};
        if(state->stance_flags&2u)values[0]=-values[0];
        for(int col=0;col<3;col++) {
            if(values[col]<INT16_MIN || values[col]>INT16_MAX)return 0;
            render.body_basis[row*3+col]=(int16_t)values[col];
        }
    }
    if(state->source_body_valid)for(int k=0;k<3;k++)
        render.position[k]=(float)state->source_body_position[k]*(THPS_SOURCE_SCALE/4096.0f)*(k==1?-1:1);
    else for(int k=0;k<3;k++)render.position[k]+=state->floor_normal[k]*THPS_BOARD_ORIGIN_OFFSET;
    render.hidden_joints=state->bail.hidden_joint_mask;
    render.yaw_degrees=state->yaw*(360.0f/65536.0f);
    render.host_scale=THPS_RENDER_SCALE;render.clip_slot=state->animation;
    render.frame_index=frame;render.ticks=state->ticks;
    if(!thps_runtime_submit(&render))return 0;
    m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;sOwnHide=1;
    return 1;
}
void thps_adapter_pause_inputs(void) {
    if(!sOwner || sFailed)return;
    uint16_t priorClip=sSource.animation;
    int32_t priorFrame=sSource.anim_frame_i;
    thps_skate_cancel_inputs(&sSource);
    /* Host focus cancellation changes crouch to roll without advancing time.
     * Publish that same frozen-tick state rather than retaining a stale pose.
     * The input-priority hook also runs after drawing the actor: an identical
     * cancellation must not invalidate the completed draw's visibility. */
    if((priorClip!=sSource.animation || priorFrame!=sSource.anim_frame_i) &&
       !publish(sOwner,&sSource))stop_source(sOwner,"cancelled original pose unavailable");
}
int thps_adapter_snapshot(ThpsBehaviorSnapshot *out) {
    if(!out || !sOwner)return 0;
    memset(out,0,sizeof *out);out->ticks=sSource.ticks;
    memcpy(out->velocity,sSource.velocity,sizeof out->velocity);
    out->speed=hypotf(sSource.velocity[0],sSource.velocity[2]);
    out->events=sSource.events;out->state=sSource.mode;
    out->grounded=sSource.mode==THPS_SKATE_GROUND;
    out->charge_ticks=sSource.charge_ticks;out->clip_slot=sSource.animation;
    out->frame_index=(int)sSource.animation_frame;
    out->spin_queued180=sSource.spin.queued180;out->spin_yaw_rate=sSource.spin.yaw_rate;out->gamepad_spin=sGamepadSpin;
    out->source_yaw=sSource.yaw;out->source_state=sSource.source_state;
    out->bail_active=sSource.bail.active;out->bail_phase=sSource.bail.phase;out->landing_reason=sSource.landing_reason;
    out->trick_active=sSource.tricks.active;out->trick_index=(int)(sSource.tricks.trick_flags&255u);
    out->trick_interruptible=sSource.tricks.interruptible;out->completed_spins=sSource.spin.completed180;
    out->grinding=sSource.mode==THPS_SKATE_GRIND;out->rail_id=sSource.grind.rail_id;out->balance=sSource.grind.balance;
    out->hidden_joints=sSource.bail.hidden_joint_mask;out->stance_flags=sSource.stance_flags;out->mapped_rails=sMappedRails;
    memcpy(out->source_body_position,sSource.source_body_position,sizeof out->source_body_position);
    return 1;
}
static int host_contact(struct MarioState *m,const ThpsSkateState *pending,ThpsSkateContact *contact) {
    Vec3f before;vec3f_copy(before,m->pos);
    for(int k=0;k<3;k++)m->vel[k]=pending->delta[k];
    m->faceAngle[1]=(s16)pending->yaw;
    int result;
    if(pending->mode==THPS_SKATE_GROUND) {
        /* Ground-step projects horizontal displacement by floor normal Y.
         * Undo that host speed adjustment: THPS owns the requested delta. */
        if(m->floor && m->floor->normal.y>.01f) {
            m->vel[0]/=m->floor->normal.y;m->vel[2]/=m->floor->normal.y;
        }
        result=perform_ground_step(m);
        contact->grounded=result!=GROUND_STEP_LEFT_GROUND;
        contact->hit_wall=result==GROUND_STEP_HIT_WALL || result==GROUND_STEP_HIT_WALL_STOP_QSTEPS;
    } else {
        float requestedY=m->vel[1];
        result=perform_air_step(m,0);
        /* Native quarter-steps supply collision only. Discard the Mario
         * gravity and vertical-wind update that follows those collisions. */
        m->vel[1]=requestedY;
        contact->grounded=result==AIR_STEP_LANDED;
        contact->hit_wall=result==AIR_STEP_HIT_WALL || result==AIR_STEP_HIT_LAVA_WALL;
        if(requestedY>0 && !contact->grounded && m->pos[1]-before[1]<requestedY-.001f) {
            for(int q=0;q<=4 && !contact->hit_ceiling;q++) {
                float fraction=q*.25f;
                Vec3f probe={before[0]+pending->delta[0]*fraction,
                    before[1]+requestedY*fraction,before[2]+pending->delta[2]*fraction};
                struct Surface *ceiling=NULL;float height=vec3f_mario_ceil(probe,m->floorHeight,&ceiling);
                if(ceiling && probe[1]+m->marioObj->hitboxHeight>=height-.001f)contact->hit_ceiling=1;
            }
        }
    }
    vec3f_copy(contact->position,m->pos);
    if(m->floor) {
        contact->floor_normal[0]=m->floor->normal.x;
        contact->floor_normal[1]=m->floor->normal.y;
        contact->floor_normal[2]=m->floor->normal.z;
    }
    if(contact->hit_wall && m->wall) {
        contact->wall_normal[0]=m->wall->normal.x;
        contact->wall_normal[1]=m->wall->normal.y;
        contact->wall_normal[2]=m->wall->normal.z;
    }
    contact->valid=valid_position(contact->position);
    return pending->mode!=THPS_SKATE_GROUND && result==AIR_STEP_HIT_LAVA_WALL;
}
int thps_adapter_update(struct MarioState *m) {
    if(!m || m->playerIndex!=0)return 0;
    if(!sSelected || !thps_runtime_enabled() || !m->marioObj || !m->controller || !m->area || !m->floor) {
        thps_adapter_suspend();return 0;
    }
    if(sOwner && (sOwner!=m || sArea!=m->area || sLevel!=gCurrLevelNum))thps_adapter_suspend();
    if(sHavePosition) {
        float x=m->pos[0]-sLastPosition[0],y=m->pos[1]-sLastPosition[1],z=m->pos[2]-sLastPosition[2];
        if(x*x+y*y+z*z>200.0f*200.0f)thps_adapter_suspend();
    }
    if(!supported(m->action) || m->health<0x100 || m->heldObj || m->riddenObj || m->heldByObj ||
       m->quicksandDepth>1 || (m->action!=ACT_FREEFALL && SURFACE_IS_QUICKSAND(m->floor->type)) ||
       (m->input&INPUT_SQUISHED)) {thps_adapter_suspend();return 0;}
    if(m->pos[1]<m->waterLevel-100) {thps_adapter_suspend();set_water_plunge_action(m);return 0;}
    if(!sOwner) {
        if(!thps_skate_reset(&sSource,m->pos[0],m->pos[1],m->pos[2],(u16)m->faceAngle[1],THPS_SOURCE_SCALE))return 0;
        sOwner=m;sArea=m->area;sLevel=gCurrLevelNum;
        uint16_t counts[78];
        for(int i=0;i<78;i++) {
            int count=thps_runtime_frame_count(i);
            if(count<1 || count>UINT16_MAX)return stop_source(m,"original clip counts unavailable");
            counts[i]=(uint16_t)count;
        }
        if(thps_skate_bind_clip_counts(&sSource,counts,78)!=1)return stop_source(m,"original clip count binding rejected");
        sSource.floor_normal[0]=m->floor->normal.x;
        sSource.floor_normal[1]=m->floor->normal.y;
        sSource.floor_normal[2]=m->floor->normal.z;
        if(!thps_runtime_rotation_tables(sPitchRotations,sYawRotations,4096*9) ||
           !thps_skate_bind_rotation_tables(&sSource,sPitchRotations,sYawRotations,4096) ||
           !thps_skate_bind_body_offset(&sSource,THPS_BODY_OFFSET_SOURCE))
            return stop_source(m,"original rotation tables or model body anchor unavailable");
        if(m->action==ACT_FREEFALL) {
            sSource.mode=THPS_SKATE_AIR;sSource.source_state=1;
            /* Host velocity is displacement per30Hz frame, matching the
             * recovered nominal source tick. Only the Y axis is reflected. */
            for(int k=0;k<3;k++) {
                double q=round((double)m->vel[k]/THPS_SOURCE_SCALE*4096.0*(k==1?-1:1));
                if(!isfinite(q) || q<INT32_MIN || q>INT32_MAX)return stop_source(m,"invalid host momentum at skating handoff");
                sSource.source_velocity[k]=(int32_t)q;sSource.velocity[k]=m->vel[k];
            }
            double speed=hypot((double)sSource.source_velocity[0],(double)sSource.source_velocity[2]);
            if(speed>INT32_MAX)return stop_source(m,"host momentum exceeds source speed range");
            sSource.source_speed=(int32_t)speed;
        }
        if(!publish(m,&sSource))return stop_source(m,"original initial pose unavailable");
        vec3f_copy(sLastPosition,m->pos);sHavePosition=1;sRenderWaiting=1;
        return 1;
    }
    if(sOwnHide)m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;
    if(sFailed)return stop_source(m,sStatus);
    if(sRenderWaiting){thps_adapter_render_wait();return 1;}
    if(m->freeze || sCurrPlayMode==PLAY_MODE_PAUSED || !sWindowActive || 0) {
        thps_adapter_pause_inputs();
        vec3f_set(m->vel,0,0,0);m->forwardVel=m->slideVelX=m->slideVelZ=0;
        return 1;
    }
    ThpsSkateInput input={0};
    input.steer=fmaxf(-1,fminf(1,m->controller->rawStickX/80.0f));
    input.forward=fmaxf(-1,fminf(1,m->controller->rawStickY/80.0f));
    u16 held=m->controller->buttonDown;
    if(sUnavailableTicks)--sUnavailableTicks;
    if(sTrickLabelTicks)--sTrickLabelTicks;
    input.ollie=!!(held&D_CBUTTONS);input.flip=!!(held&L_CBUTTONS);
    input.grab=!!(held&R_CBUTTONS);input.grind=!!(held&U_CBUTTONS);
    input.spin_left=!!(held&L_TRIG);input.spin_right=!!(held&R_TRIG);
    input.spin=(int8_t)(input.spin_right-input.spin_left);
    input.spin_continuous_left=!!(sGamepadSpin&1);input.spin_continuous_right=!!(sGamepadSpin&2);
    input.spin_180_left=!!(sGamepadSpin&4);input.spin_180_right=!!(sGamepadSpin&8);
    input.up=!!(held&U_JPAD);input.down=!!(held&D_JPAD);
    input.left=!!(held&L_JPAD);input.right=!!(held&R_JPAD);
    if((m->controller->buttonPressed&U_CBUTTONS)&&!rail_geometry_ready(m)) {
        sUnavailable="NO MAPPED RAILS HERE";sUnavailableTicks=90;
    }
    /* Two-phase source state publication; a rejected collision or source update
     * cannot leave one substep committed. Native collision observes the same
     * world each time, at the exact source timestep declared by the kernel. */
    struct MarioState saved=*m;
    Vec3f oldGfx;Vec3s oldAngle;
    vec3f_copy(oldGfx,m->marioObj->header.gfx.pos);vec3s_copy(oldAngle,m->marioObj->header.gfx.angle);
    ThpsSkateState working=sSource;
    uint32_t frameEvents=0;
    for(int tick=0;tick<THPS_SKATE_TICKS_PER_HOST_FRAME;tick++) {
        ThpsSkateState pending;ThpsSkateContact contact={0};
        Thps1GrindEntry entry;ThpsSkateHost host;
        if(!prepare_rail_query(m,&working,&input,&entry,&host))goto reject;
        if(thps_skate_begin_step_with_host(&working,&input,&host,&pending)!=1 || !valid_position(pending.delta))goto reject;
        if(pending.step_skip_host_collision) {
            if(!rail_motion_clear(&working,&pending))goto reject;
            /* Source owns rail height; ordinary ground stepping would snap its
             * body-derived foot anchor onto the unrelated floor underneath. */
            vec3f_copy(contact.position,pending.position);
            memcpy(contact.floor_normal,pending.floor_normal,sizeof contact.floor_normal);
            contact.valid=valid_position(contact.position);
        } else if(host_contact(m,&pending,&contact)) {
            thps_adapter_suspend();set_mario_action(m,ACT_LAVA_BOOST,0);return 0;
        }
        if(thps_skate_resolve(&pending,&contact)!=1)goto reject;
        /* Rail-owned foot conversion occurs transactionally in resolve. Never
         * publish or feed the camera the old pre-resolve position. */
        vec3f_copy(m->pos,pending.position);
        if(pending.step_skip_host_collision) {
            struct Surface *floor=NULL;float height=find_floor(m->pos[0],m->pos[1]+100,m->pos[2],&floor);
            if(floor){m->floor=floor;m->floorHeight=height;}
        }
        working=pending;frameEvents|=pending.events;
    }
    working.events=frameEvents;
    if(!publish(m,&working))goto reject;
    sSource=working;
    if(sSource.events&(THPS_EVENT_FLIP|THPS_EVENT_GRAB)) {
        const Thps1TrickRecord *record=thps1_trick_record(sSource.tricks.trick_flags&255u);
        if(record){sLastTrickLabel=record->name;sTrickLabelTicks=90;}
    }
    if(sSource.bail.active){sTrickLabelTicks=0;sLastTrickLabel=NULL;}
    m->prevAction=m->action;m->action=sSource.mode==THPS_SKATE_GROUND
        ?(hypotf(sSource.velocity[0],sSource.velocity[2])>.01f?ACT_WALKING:ACT_IDLE):ACT_FREEFALL;
    m->faceAngle[1]=(s16)sSource.yaw;
    for(int k=0;k<3;k++)m->vel[k]=sSource.velocity[k]*THPS_SKATE_TICKS_PER_HOST_FRAME;
    /* Keep the Mario camera on the physical travel heading while the original
     * board/body independently rotates. This is explicitly host camera policy. */
    if(sSource.mode!=THPS_SKATE_GROUND && hypotf(m->vel[0],m->vel[2])>1.f)
        m->faceAngle[1]=atan2s(m->vel[2],m->vel[0]);
    m->forwardVel=hypotf(m->vel[0],m->vel[2]);m->slideVelX=m->vel[0];m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    vec3f_copy(sLastPosition,m->pos);sHavePosition=1;sStatus="Original THPS1 skating slice active";
    return 1;
reject:
    *m=saved;vec3f_copy(m->marioObj->header.gfx.pos,oldGfx);vec3s_copy(m->marioObj->header.gfx.angle,oldAngle);
    return stop_source(m,"source update, host contact or original frame rejected");
}
