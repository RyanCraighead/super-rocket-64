#include "actor_bridge.h"
#include <string.h>
#define COPY(d,s) memcpy((d),(s),sizeof(d))
void smn64_bridge_ground_to_actor(const SmN64Player *g,SmN64ClimbState *a) {
    const SmN64Locomotion *m=&g->motion;
    a->anim=m->anim;a->state=m->state;COPY(a->position,g->position);
    a->velocity[0]=m->velocity_x;a->velocity[1]=m->velocity_y;a->velocity[2]=m->velocity_z;
    a->basis.yaw_delta=m->yaw_delta;a->turn_target=m->turn_target;a->turn_step=m->turn_step;a->turn_ticks=m->turn_ticks;
    a->input_ramp=m->input_ramp;a->run_ramp=m->run_ramp;a->yaw=m->yaw;
    a->input_angle=(int16_t)m->input_angle;a->input_base=m->input_base;a->idle_ticks=m->idle_ticks;
    a->analog_x=m->analog_x;a->analog_y=m->analog_y;a->previous_x=m->previous_x;a->previous_y=m->previous_y;
    a->launch_velocity=g->launch_velocity;a->base_timer=g->jump_base_timer;a->hold_timer=g->jump_hold_timer;
    a->jump_variant=g->jump_variant;a->falling_origin_y=g->falling_origin_y;
    a->collision=g->collision;a->ground_grace=g->ground_grace;COPY(a->drag,g->drag);COPY(a->random_state,g->random_state);
}
void smn64_bridge_actor_to_ground(const SmN64ClimbState *a,SmN64Player *g) {
    SmN64Locomotion *m=&g->motion;
    m->anim=a->anim;m->state=a->state;COPY(g->position,a->position);
    m->velocity_x=a->velocity[0];m->velocity_y=a->velocity[1];m->velocity_z=a->velocity[2];
    m->forward_x=a->basis.forward[0];m->forward_z=a->basis.forward[2];m->yaw_delta=a->basis.yaw_delta;
    m->turn_target=a->turn_target;m->turn_step=a->turn_step;m->turn_ticks=a->turn_ticks;
    m->input_ramp=a->input_ramp;m->run_ramp=a->run_ramp;m->yaw=a->yaw;
    m->input_angle=(uint16_t)a->input_angle;m->input_base=a->input_base;m->idle_ticks=a->idle_ticks;
    m->analog_x=a->analog_x;m->analog_y=a->analog_y;m->previous_x=a->previous_x;m->previous_y=a->previous_y;
    g->launch_velocity=a->launch_velocity;g->jump_base_timer=a->base_timer;g->jump_hold_timer=a->hold_timer;
    g->jump_variant=a->jump_variant;g->falling_origin_y=a->falling_origin_y;
    g->collision=a->collision;g->ground_grace=a->ground_grace;COPY(g->drag,a->drag);COPY(g->random_state,a->random_state);
}
void smn64_bridge_actor_to_web(const SmN64ClimbState *a,SmN64WebRuntime *w,SmN64WebOwner *o) {
    SmN64WebPlayer *p=&w->player;p->anim=a->anim;p->state=a->state;
    COPY(p->position,a->position);COPY(p->velocity,a->velocity);COPY(p->forward,a->basis.forward);
    COPY(p->right,a->basis.right);COPY(p->outward,a->basis.outward);COPY(p->normal,a->basis.normal);
    p->body_offset=a->body_offset;p->aiming=a->aiming;p->holding=a->held_object;p->adhered=a->adhered;
    p->wall_orientation=a->wall_class;p->ceiling_orientation=a->ceiling_class;p->turn_ticks=a->turn_ticks;
    p->airborne_owner=a->field65c;p->kid_jump_gate=a->field1184;p->idle_ticks=a->idle_ticks;
    COPY(p->random_state,a->random_state);w->basis=a->basis;w->launch_velocity=a->launch_velocity;
    w->base_timer=a->base_timer;w->hold_timer=a->hold_timer;w->field_d20=a->d20;
    COPY(w->retained_forward,a->retained_forward);w->analog_x=a->analog_x;w->analog_y=a->analog_y;w->run_ramp=a->run_ramp;w->collision=a->collision;
    w->side.hit=(uint32_t)a->side.present;w->side.surface=a->side.has_surface;w->side.distance=a->side.distance;
    COPY(w->side.position,a->side.position);COPY(w->side.normal,a->side.normal);w->side.surface_flags=a->side.surface_flags;
    COPY(o->acceleration,a->acceleration);COPY(o->drag,a->drag);
}
void smn64_bridge_web_to_actor(const SmN64WebRuntime *w,const SmN64WebOwner *o,SmN64ClimbState *a) {
    const SmN64WebPlayer *p=&w->player;a->anim=p->anim;a->state=p->state;
    COPY(a->position,p->position);COPY(a->velocity,p->velocity);a->basis=w->basis;
    /* Late state handlers can change normal without immediately rebuilding the
     * source matrix. Preserve that distinct source timing, not a fresh basis. */
    COPY(a->basis.normal,p->normal);COPY(a->basis.forward,p->forward);COPY(a->basis.right,p->right);COPY(a->basis.outward,p->outward);
    a->body_offset=p->body_offset;a->aiming=p->aiming;a->held_object=p->holding;a->adhered=p->adhered;
    a->wall_class=p->wall_orientation;a->ceiling_class=p->ceiling_orientation;a->turn_ticks=p->turn_ticks;
    a->field65c=p->airborne_owner;a->field1184=p->kid_jump_gate;a->idle_ticks=p->idle_ticks;
    COPY(a->random_state,p->random_state);a->launch_velocity=w->launch_velocity;a->base_timer=w->base_timer;a->hold_timer=w->hold_timer;
    a->d20=w->field_d20;COPY(a->retained_forward,w->retained_forward);a->analog_x=w->analog_x;a->analog_y=w->analog_y;a->run_ramp=w->run_ramp;a->collision=w->collision;
    a->side.present=w->side.hit!=0;a->side.has_surface=w->side.surface!=0;a->side.distance=w->side.distance;
    COPY(a->side.position,w->side.position);COPY(a->side.normal,w->side.normal);a->side.surface_flags=w->side.surface_flags;
    COPY(a->acceleration,o->acceleration);COPY(a->drag,o->drag);
}
int smn64_bridge_ground_basis(void *context,SmN64Locomotion *m) {
    if(!context||!m)return -5;
    SmN64ClimbState *a=context;SmN64ClimbBasis b=a->basis;b.yaw_delta=m->yaw_delta;
    if(!smn64_climb_basis(&b,b.normal[1]>=3401?a->retained_forward:NULL))return -5;
    a->basis=b;smn64_climb_classify(a);m->forward_x=b.forward[0];m->forward_z=b.forward[2];return 1;
}
#undef COPY
