#include "character_n64.h"
#include "../climbing/clearance_n64.h"
#include "../web/swing_contact_n64.h"
#include "../web/zip_physics_n64.h"
#include "../web/fall_exit_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#define COPY(d,s) memcpy((d),(s),sizeof(d))
typedef struct Frame {
    SmN64Character *c;SmN64CharacterInput *input;
    const uint16_t *counts;size_t count;const SmN64CharacterServices *services;
    uint16_t completed;int32_t previous_position[3];int target_selected,command_declined;SmN64WebRuntime *active_web;
} Frame;
static int32_t sar12(int32_t x){return x>=0?x/4096:-1-(int32_t)((uint32_t)(-1-x)/4096);}
static int refresh(Frame *f,const SmN64ClimbState *a) {
    int16_t pose[216];
    if(!f->services->pose || f->services->pose(f->services->context,a->anim.animation,a->anim.frame,pose)!=1)return -11;
    COPY(f->c->retained_pose,pose);
    for(int k=0;k<3;++k)f->c->body_translation[k]=sar12(a->position[k]);
    return 1;
}
static int marker(void *context,const SmN64ClimbState *a,unsigned index,int update,int32_t out[3]) {
    Frame *f=context;if(update && refresh(f,a)<0)return -11;
    return smn64_marker_world(f->c->markers,9,index,f->c->retained_pose,18,
        a->basis.matrix,f->c->body_translation,a->position,0,out)?1:-11;
}
static int trace(void *context,const SmN64ClimbQuery *q,SmN64ClimbHit *h){
    Frame *f=context;return f->services->trace?f->services->trace(f->services->context,q,h):-3;
}
static int free_trace(void *context,const SmN64FreeQuery *q,SmN64FreeHit *h){
    Frame *f=context;return f->services->free_trace?f->services->free_trace(f->services->context,q,h):-3;
}
static int clearance(void *context,int32_t position[3],const SmN64ClimbBasis *basis,int32_t distance){
    return smn64_climb_clearance(position,basis,distance,trace,context);
}
static SmN64WebInput web_input(const Frame *f){
    const SmN64CharacterInput *i=f->input;SmN64WebInput w={0};
    w.zip_held=(i->held&SMN64_BUTTON_ZIP)!=0;w.swing_held=(i->held&SMN64_BUTTON_SWING)!=0;
    w.jump_held=(i->held&SMN64_BUTTON_JUMP)!=0;w.jump_pressed=(i->pressed&SMN64_BUTTON_JUMP)!=0;
    w.web_pressed=(i->pressed&SMN64_BUTTON_WEB)!=0;w.punch_pressed=(i->pressed&SMN64_BUTTON_PUNCH)!=0;w.kick_pressed=(i->pressed&SMN64_BUTTON_KICK)!=0;
    w.stable_updates=f->c->stable_updates;
    return w;
}
static int web_ray(void *context,SmN64WebQueryKind kind,const int32_t from[3],const int32_t to[3],SmN64WebLine *h){
    (void)kind;SmN64ClimbQuery q;memset(&q,0,sizeof q);COPY(q.start,from);COPY(q.end,to);q.arg1=q.arg4=1;
    SmN64ClimbHit hit;memset(&hit,0,sizeof hit);int rc=trace(context,&q,&hit);if(rc!=1)return rc;
    h->hit=(uint32_t)hit.present;h->surface=hit.has_surface;h->distance=hit.distance;
    COPY(h->position,hit.position);COPY(h->normal,hit.normal);h->surface_flags=hit.surface_flags;return 1;
}
static int web_marker(void *context,const SmN64WebRuntime *w,unsigned index,int32_t out[3]){
    Frame *f=context;return smn64_marker_world(f->c->markers,9,index,f->c->retained_pose,18,
        w->basis.matrix,f->c->body_translation,w->player.position,0,out)?1:-11;
}
static int visual_marker(void *context,const SmN64WebPlayer *p,unsigned index,int32_t out[3]){
    Frame *f=context;return smn64_marker_world(f->c->markers,9,index,f->c->retained_pose,18,
        f->c->actor.basis.matrix,f->c->body_translation,p->position,0,out)?1:-11;
}
static int web_floor(void *context,const int32_t p[3],int32_t above,int32_t below,int32_t objects,int32_t *height){
    return smn64_climb_floor_query(p,above,below,objects,trace,context,height);
}
static int lifecycle(void *context,SmN64WebEventKind kind,SmN64WebPlayer *p,const SmN64Swinger *s){
    Frame *f=context;return smn64_web_visual_lifecycle_ordered(&f->c->visuals,kind,p,s,&f->c->graphical_clock);
}
static int web_entry(Frame *f,SmN64ClimbState *a,uint32_t address){
    if(address!=0x8009a058&&address!=0x80099ab8&&address!=0x80099d54)return 0;
    SmN64WebRuntime *w=&f->c->web;smn64_bridge_actor_to_web(a,w,&f->c->web_owner);
    SmN64WebInput in=web_input(f);int rc=0;
    if(address==0x8009a058)rc=smn64_web_try_swing(&w->player,&in,web_ray,f,f->counts,f->count,&f->c->web_events);
    else if(address==0x80099ab8)rc=smn64_web_try_zip_b_visual(&w->player,&in,web_ray,f,f->counts,f->count,&f->c->web_events,lifecycle);
    else rc=smn64_web_try_zip_r_visual(&w->player,&in,web_ray,f,f->counts,f->count,&f->c->web_events,lifecycle);
    if(rc>=0)smn64_bridge_web_to_actor(w,&f->c->web_owner,a);
    return rc;
}
static SmN64CombatOwnerFrame combat_frame(Frame *f,SmN64ClimbState *a){
    SmN64CombatOwnerFrame q;memset(&q,0,sizeof q);q.actor=a;q.traversal=f->active_web?f->active_web:&f->c->web;q.traversal_visuals=&f->c->visuals;q.resource=&q.traversal->player.resource;q.pressed=&f->input->pressed;
    q.held.web=(f->input->held&SMN64_BUTTON_WEB)!=0;q.held.punch=(f->input->held&SMN64_BUTTON_PUNCH)!=0;
    q.held.kick=(f->input->held&SMN64_BUTTON_KICK)!=0;q.held.jump=(f->input->held&SMN64_BUTTON_JUMP)!=0;
    q.tick=f->c->ticks;q.completed_animation=f->completed;q.counts=f->counts;q.count=f->count;
    q.markers=f->c->markers;q.marker_count=9;q.retained_pose=f->c->retained_pose;q.body_translation=f->c->body_translation;
    COPY(q.previous_position,f->previous_position);q.services=f->services->combat;q.graphical_clock=&f->c->graphical_clock;return q;
}
static int carried_object(Frame *f,SmN64ClimbState *a,SmN64CarryObject *object){
    memset(object,0,sizeof(*object));if(!a->held_object)return 0;
    if(!f->services->combat)return -20;
    SmN64CombatOwnerFrame q=combat_frame(f,a);
    return smn64_character_combat_carry_object(&f->c->combat,&q,object);
}
static int post_ai_tail(Frame *f,SmN64ClimbState *a,uint16_t camera){
    SmN64CarryObject object;int rc=carried_object(f,a,&object);if(rc<0)return rc;
    return rc?smn64_climb_post_ai_tail_carrying(a,camera,f->c->combat.movement_enabled!=0,object.flags_10c):
        smn64_climb_post_ai_tail(a,camera,f->c->combat.movement_enabled!=0);
}
static int select_target(void *context,SmN64ClimbState *a){
    Frame *f=context;if(f->target_selected||!f->services->combat)return 1;
    SmN64CombatOwnerFrame q=combat_frame(f,a);int rc=smn64_character_combat_select_target(&f->c->combat,&q);
    if(rc<0)return rc;
    f->target_selected=1;return 1;
}
static int external_action(Frame *f,SmN64ClimbState *a,uint32_t address){
    if(f->command_declined)return 0;
    int rc=web_entry(f,a,address);if(rc)return rc;
    if(f->services->combat){SmN64CombatOwnerFrame q=combat_frame(f,a);rc=smn64_character_combat_entry(&f->c->combat,&q,address);
        if(rc==SMN64_COMBAT_ENTRY_DECLINED){f->command_declined=1;return 0;}if(rc)return rc;}
    if(!f->services->action)return 0;
    return f->services->action(f->services->context,f->c,a,address,f->input,f->counts,f->count);
}
static int climb_action(void *context,SmN64ClimbState *a,uint32_t address){return external_action(context,a,address);}
static int actions(Frame *f,SmN64ClimbState *a,const uint32_t *addresses,size_t count){
    for(size_t i=0;i<count;++i){int r=external_action(f,a,addresses[i]);if(f->command_declined)return 0;if(r)return r;}return 0;
}
static int ground_basis(void *context,SmN64Locomotion *m){
    Frame *f=context;int rc=smn64_bridge_ground_basis(&f->c->actor,m);if(rc<0)return rc;
    return select_target(f,&f->c->actor);
}
static int ground_route(void *context,SmN64Player *g,SmN64PlayerActionStage stage){
    Frame *f=context;SmN64ClimbState *a=&f->c->actor;smn64_bridge_ground_to_actor(g,a);
    static const uint32_t idle[]={0x80099444,0x80099058,0x8009996c,0x8009a058,0x80099ab8,0x80099d54};
    static const uint32_t run0[]={0x80099444},run1[]={0x8009996c,0x80099058,0x80099444};
    static const uint32_t air[]={0x8009a7d8,0x8009a058,0x80099ab8,0x80099d54};
    static const uint32_t web[]={0x8009a058,0x80099ab8,0x80099d54};
    static const uint32_t land[]={0x80099058,0x8009a058,0x80099ab8,0x80099d54};
    int r=0;
    switch(stage){
        case SMN64_ROUTE_CARRY_DROP:
        case SMN64_ROUTE_CARRY_SMASH:{
            int32_t velocity[3]={0,0,0};
            if(!f->services->combat)return -20;
            if(stage==SMN64_ROUTE_CARRY_SMASH){
                /* Original91F7C starts ordinary run21 before Smash7E7D8. */
                if(f->count<=21||!f->counts[21])return -1;
                smn64_anim_run(&a->anim,21,f->counts[21],0,-1);
            }else for(int k=0;k<3;++k)velocity[k]=4*a->basis.outward[k];
            SmN64CombatOwnerFrame q=combat_frame(f,a);
            r=smn64_character_combat_release_held(&f->c->combat,&q,velocity,stage==SMN64_ROUTE_CARRY_SMASH);
            if(r<0)return r;
            if(stage==SMN64_ROUTE_CARRY_DROP)a->d20=a->d24=0;
            /* Release is an in-handler operation. Continue the source ground
             * handler with the now-unheld actor, rather than taking it over. */
            smn64_bridge_actor_to_ground(a,g);return 0;
        }
        case SMN64_ROUTE_FALLING_LAND:
            if(f->services->combat){SmN64CombatOwnerFrame q=combat_frame(f,a);r=smn64_character_combat_land(&f->c->combat,&q);
                if(r>0&&f->c->combat.damage.health<=0)r=smn64_character_combat_die(&f->c->combat,&q);}
            break;
        case SMN64_ROUTE_IDLE_BEFORE_JUMP:r=actions(f,a,idle,6);break;
        case SMN64_ROUTE_RUN_BEFORE_JUMP:r=actions(f,a,run0,1);break;
        case SMN64_ROUTE_RUN_AFTER_JUMP:
            r=actions(f,a,run1,3);if(r)break;
            r=smn64_climb_approach(a,&a->side,f->counts,f->count);if(r)break;
            if(!a->adhered)r=actions(f,a,web,3);
            break;
        case SMN64_ROUTE_FALLING:
            r=smn64_climb_attach_wall(a,&a->side,trace,f,f->counts,f->count);if(r)break;
            /* Deliberate fall-through: source wall acquisition precedes the
             * shared ceiling/air-attack/swing/zip group. */
            /* fall through */
        case SMN64_ROUTE_RISING:
            r=smn64_climb_attach_ceiling(a,&f->c->contact.ceiling,f->counts,f->count);if(r)break;
            r=actions(f,a,air,4);break;
        case SMN64_ROUTE_LANDING_AFTER_JUMP:r=actions(f,a,land,4);break;
        default:return -1;
    }
    if(r<0)return r;
    if(!r && stage==SMN64_ROUTE_FALLING){
        SmN64FallExit fall={a->anim,a->position[1],a->velocity[1],g->previous_velocity_y,
            f->c->web.player.launch_flag,a->falling_origin_y,f->c->falling_tick,f->c->ticks,a->d20,a->d24};
        r=smn64_web_fall_exit(&fall,f->counts,f->count);if(r<0)return r;
        a->anim=fall.anim;f->c->web.player.launch_flag=fall.launch_flag;a->falling_origin_y=fall.falling_origin_y;
        f->c->falling_tick=fall.falling_tick;a->d20=fall.d20;a->d24=fall.d24;
        /* The full source fall block was handled. Return takeover so the
         * bounded flat block cannot run again; caller runs common tail once. */
    }
    /* Rejected original requests may still update shared RNG or target-facing
     * fields. Preserve them before the ordinary source handler continues. */
    smn64_bridge_actor_to_ground(a,g);return r;
}
static void contact_from_actor(const SmN64ClimbState *a,SmN64FreeContact *c){
    COPY(c->position,a->position);COPY(c->velocity,a->velocity);COPY(c->acceleration,a->acceleration);COPY(c->drag,a->drag);
    c->elapsed_ticks=a->anim.elapsed_ticks;c->state=a->state;c->animation=a->anim.animation;c->frame=a->anim.frame;
    c->adhered=a->adhered;c->held_object=a->held_object;c->body_offset=a->body_offset;c->body_radius=70;
    c->angles[1]=a->yaw;COPY(c->normal,a->basis.normal);c->ground_grace=a->ground_grace;c->side=a->side;
    COPY(c->random_state,a->random_state);
}
static void actor_from_contact(const SmN64FreeContact *c,SmN64ClimbState *a){
    COPY(a->position,c->position);COPY(a->velocity,c->velocity);COPY(a->basis.normal,c->normal);
    a->yaw=c->angles[1];a->collision=c->collision;a->ground_grace=c->ground_grace;a->side=c->side;a->d48=c->d48;
    COPY(a->contact_position,c->contact_position);COPY(a->random_state,c->random_state);a->lighting_target=c->lighting_target;
}
static int web_action(void *context,SmN64WebStateAction action,SmN64WebRuntime *w,SmN64WebEvents *events){
    (void)events;Frame *f=context;SmN64ClimbState a=f->c->actor;smn64_bridge_web_to_actor(w,&f->c->web_owner,&a);
    int rc=0;
    if(action==SMN64_WEB_ACTION_JUMP)rc=smn64_climb_jump_detach(&a,f->counts,f->count);
    else if(action==SMN64_WEB_ACTION_CEILING)rc=smn64_climb_attach_ceiling(&a,&f->c->contact.ceiling,f->counts,f->count);
    else if(action==SMN64_WEB_ACTION_AIR_ATTACK){
        /* State step owns a transactional runtime copy. Bind shared combat
         * aliases to THAT copy, or its commit would restore stale660/resources. */
        SmN64WebRuntime *saved=f->active_web;f->active_web=w;
        rc=external_action(f,&a,0x8009a7d8);f->active_web=saved;
    }
    else if(action==SMN64_WEB_ACTION_STOP)rc=smn64_climb_stop(&a,f->counts,f->count);
    else return -4;
    if(rc>=0)smn64_bridge_actor_to_web(&a,w,&f->c->web_owner);
    return rc;
}
static int physics_marker(void *context,const SmN64ClimbState *a,unsigned index,int32_t out[3]){
    return marker(context,a,index,0,out);
}
static int swing_marker(void *context,const SmN64SwingContact *s,unsigned index,int32_t out[3]){
    Frame *f=context;return smn64_marker_world(f->c->markers,9,index,f->c->retained_pose,18,
        f->c->actor.basis.matrix,f->c->body_translation,s->position,0,out)?1:-11;
}
static int swing_trace(void *context,const SmN64SwingContactQuery *q,SmN64WebLine *h){
    return web_ray(context,SMN64_WEB_SWING_EXIT,q->start,q->end,h);
}
static int32_t wrapped_add(int32_t a,int32_t b){uint32_t x=(uint32_t)a+(uint32_t)b;return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int shared_physics_input(Frame *f){
    SmN64Character *c=f->c;SmN64ClimbState *a=&c->actor;SmN64WebRuntime *w=&c->web;
    int rc;a->collision=0;a->d48=0;a->side.present=0;c->contact.ceiling.present=0;
    if(w->player.swinger_present){
        SmN64SwingContact s;memset(&s,0,sizeof s);COPY(s.position,a->position);COPY(s.velocity,a->velocity);s.body_offset=a->body_offset;s.line=w->side;
        rc=smn64_swing_contact_run(&s,&w->swinger,swing_marker,swing_trace,f);if(rc<0)return rc;
        COPY(a->position,s.position);COPY(a->velocity,s.velocity);a->collision=s.collision;
        a->side.present=s.line.hit!=0;a->side.has_surface=s.line.surface!=0;a->side.distance=s.line.distance;
        COPY(a->side.position,s.line.position);COPY(a->side.normal,s.line.normal);a->side.surface_flags=s.line.surface_flags;
    }else if(a->adhered){
        rc=smn64_climb_physics(a,a->acceleration,a->drag,physics_marker,trace,f,0,0,c->source_level);if(rc<0)return rc;
    }else if(smn64_zip_physics_active(a->state,a->anim.animation,a->anim.frame)){
        SmN64ZipPhysics z;smn64_zip_physics_step(a->velocity,a->acceleration,a->drag,a->anim.elapsed_ticks,&z);
        COPY(a->velocity,z.velocity);for(int k=0;k<3;++k)a->position[k]=wrapped_add(a->position[k],z.displacement[k]);
    }else{
        contact_from_actor(a,&c->contact);rc=smn64_free_contact_run(&c->contact,free_trace,f,0,0);if(rc<0)return rc;actor_from_contact(&c->contact,a);
    }
    smn64_bridge_actor_to_web(a,w,&c->web_owner);
    rc=smn64_web_owner_basis_drag(w,&c->web_owner);if(rc<0)return rc;
    smn64_bridge_web_to_actor(w,&c->web_owner,a);
    rc=select_target(f,a);if(rc<0)return rc;
    rc=smn64_climb_input(a,f->input->motion.stick_x,f->input->motion.stick_y);if(rc<0)return rc;
    if(a->analog_x||a->analog_y){a->run_ramp+=a->anim.elapsed_ticks;if(a->run_ramp>16)a->run_ramp=16;}
    else if(a->run_ramp){a->run_ramp-=a->anim.elapsed_ticks;if(a->run_ramp<0)a->run_ramp=0;}
    smn64_bridge_actor_to_web(a,w,&c->web_owner);return 1;
}
static int web_tick(Frame *f){
    SmN64Character *c=f->c;SmN64ClimbState *a=&c->actor;SmN64WebRuntime *w=&c->web;
    int rc=shared_physics_input(f);if(rc<0)return rc;
    rc=smn64_web_owner_successor(w,f->counts,f->count);if(rc<0)return rc;
    SmN64WebInput input=web_input(f);SmN64WebServices q={f,web_ray,web_marker,web_floor,web_action,lifecycle};
    rc=smn64_web_state_step(w,&input,&q,f->counts,f->count,&c->web_events);if(rc<0)return rc;
    smn64_bridge_web_to_actor(w,&c->web_owner,a);
    rc=post_ai_tail(f,a,f->input->motion.camera_yaw);if(rc<0)return rc;
    smn64_bridge_actor_to_ground(a,&c->ground);c->ground.ticks=c->ticks;return 1;
}
static int combat_tick(Frame *f){
    if(!f->services->combat)return -20;
    int rc=shared_physics_input(f);if(rc<0)return rc;
    rc=smn64_character_combat_successor(&f->c->actor,f->counts,f->count);if(rc<0)return rc;f->completed=(uint16_t)rc;
    SmN64CombatOwnerFrame q=combat_frame(f,&f->c->actor);
    rc=smn64_character_combat_dispatch(&f->c->combat,&q);if(rc<0)return rc;
    rc=post_ai_tail(f,&f->c->actor,f->input->motion.camera_yaw);if(rc<0)return rc;
    smn64_bridge_actor_to_ground(&f->c->actor,&f->c->ground);f->c->ground.ticks=f->c->ticks;return 1;
}
int smn64_character_init(SmN64Character *out,const int32_t position[3],uint16_t yaw,uint32_t seed,
    const SmN64Marker markers[9],const uint16_t *counts,size_t count,const SmN64CharacterServices *services){
    if(!out||!position||!markers||!counts||count<300||!services||!services->pose)return -1;
    for(size_t i=0;i<300;++i)if(!counts[i])return -1;
    for(size_t i=0;i<9;++i)if(markers[i].joint>=18)return -1;
    SmN64Character c;memset(&c,0,sizeof c);SmN64ClimbState *a=&c.actor;
    if(smn64_player_init(&c.ground,position,yaw,seed,counts,count)<0)return -1;
    smn64_bridge_ground_to_actor(&c.ground,a);a->body_offset=96;a->air_turn_factor=1;a->acceleration[1]=40960;
    a->basis.normal[1]=-4096;a->basis.matrix[0]=a->basis.matrix[4]=a->basis.matrix[8]=4096;a->basis.yaw_delta=yaw;
    if(!smn64_climb_basis(&a->basis,NULL))return -5;
    a->basis.yaw_delta=0;
    smn64_bridge_actor_to_ground(a,&c.ground);smn64_web_visuals_init(&c.visuals,NULL,NULL);COPY(c.markers,markers);c.source_level=0x100;
    SmN64WebStartInventory initial;if(smn64_web_inventory_start(2,0,0x100,0,0,&initial)!=1)return -1;
    c.web.player.resource.remaining=initial.remaining;c.web.player.resource.cartridges=initial.cartridges;c.web.player.resource.difficulty=2;
    c.web_owner.health=c.maximum_health=initial.health;smn64_character_combat_init(&c.combat,initial.health,0);
    /* Original zero allocation leaves scripted fall thresholds disabled;
     * constructor94A3C copies maximum health into fall-damage scale11E8. */
    c.combat.hurt.fall_damage_scale=initial.health;
    smn64_bridge_actor_to_web(a,&c.web,&c.web_owner);
    Frame f={.c=&c,.counts=counts,.count=count,.services=services,.completed=65535};if(refresh(&f,a)<0)return -11;*out=c;return 1;
}
int smn64_character_tick(SmN64Character *out,const SmN64CharacterInput *input,const uint16_t *counts,size_t count,const SmN64CharacterServices *services){
    if(!out||!input||!counts||count<300||!services||!services->pose||!services->trace||!services->free_trace||input->motion.elapsed_ticks<1||input->motion.elapsed_ticks>6)return -1;
    if((input->pressed|input->held)&~127u)return -1;
    if(services->combat&&(services->combat->unavailable_commands&~SMN64_COMMAND_ALL))return -1;
    if(((input->pressed|input->held)&SMN64_BUTTON_AIM)&&!services->action&&
       (!services->combat||!(services->combat->unavailable_commands&SMN64_COMMAND_AIM)))return -20;
    if(((input->pressed|input->held)&(SMN64_BUTTON_WEB|SMN64_BUTTON_PUNCH|SMN64_BUTTON_KICK))&&!services->action&&!services->combat)return -20;
    if(!isfinite(input->effect_seconds)||input->effect_seconds<0.0f||input->effect_seconds>1.0f)return -1;
    SmN64Character c=*out;SmN64ClimbState *a=&c.actor;SmN64CharacterInput mutable_input=*input;input=&mutable_input;
    Frame f={.c=&c,.input=&mutable_input,.counts=counts,.count=count,.services=services,.completed=65535};COPY(f.previous_position,a->position);
    SmN64CarryObject carried;int carry_rc=carried_object(&f,a,&carried);if(carry_rc<0)return carry_rc;
    smn64_character_combat_begin_frame(&c.combat);
    if(((mutable_input.pressed|mutable_input.held)&SMN64_BUTTON_AIM)&&services->combat&&
       (services->combat->unavailable_commands&SMN64_COMMAND_AIM)){
        mutable_input.pressed&=~SMN64_BUTTON_AIM;mutable_input.held&=~SMN64_BUTTON_AIM;
        c.combat.unavailable_requested|=SMN64_COMMAND_AIM;
    }
    c.ticks+=input->motion.elapsed_ticks;c.web.now=c.ticks;
    c.visuals.marker=visual_marker;c.visuals.marker_context=&f;COPY(c.visuals.camera,input->camera_position);
    memset(&c.web_events,0,sizeof c.web_events);
    a->jump_pressed=(input->pressed&SMN64_BUTTON_JUMP)!=0;a->jump_held=(input->held&SMN64_BUTTON_JUMP)!=0;a->aim_held=(input->held&SMN64_BUTTON_AIM)!=0;
    memset(&c.climb_event,0,sizeof c.climb_event);
    /* Original82280 animation(old rate), existing zip actor clock, then
     * player AI resource/swinger/contact prefix. All copies commit together. */
    a->anim.elapsed_ticks=input->motion.elapsed_ticks;
    if(services->combat&&services->incoming){SmN64CombatOwnerFrame q=combat_frame(&f,a);int incoming=services->incoming(services->context,&c,&q);if(incoming!=1)return incoming<0?incoming:-21;}
    smn64_anim_advance(&a->anim);
    smn64_bridge_actor_to_web(a,&c.web,&c.web_owner);
    int rc=smn64_web_visuals_zip_actors(&c.visuals,c.ticks);if(rc<0)return rc;
    if(services->combat){SmN64CombatOwnerFrame q=combat_frame(&f,a);rc=smn64_character_combat_prepare(&c.combat,&q);if(rc<0)return rc;}
    c.web_owner.health=c.combat.damage.health;c.web.damage_tick=c.combat.damage.damage_tick;c.web.damage_flags=c.combat.damage.prior_damage_state;
    /* Source8D2B8 skips841D0 entirely for authored transition states. The
     * numeric web prepass includes its represented contact-prefix fields, so
     * preserve those fields while retaining resource/swinger/rate preparation. */
    uint16_t retained_collision=c.web.collision;
    uint32_t retained_side_hit=c.web.side.hit;
    int32_t retained_platform=c.web.platform_present;
    rc=smn64_web_owner_after_animation(&c.web,&c.web_owner,input->motion.elapsed_ticks);if(rc<0)return rc;
    if(a->state&0x83000u){c.web.collision=retained_collision;c.web.side.hit=retained_side_hit;c.web.platform_present=retained_platform;}
    if(c.web.player.swinger_present){rc=smn64_web_visuals_swing_endpoint(&c.visuals,&c.web.swinger);if(rc<0)return rc;}
    if(!a->jump_held)c.web.player.kid_jump_gate=1;
    if(!(input->held&SMN64_BUTTON_SWING))c.web.player.lock=0;
    smn64_bridge_web_to_actor(&c.web,&c.web_owner,a);
    SmN64ClimbTransitionEnv env={marker,clearance,trace,&f,0,climb_action};
    if(a->state==0x100||a->state==0x200||a->state==0x400||a->state==0x40000){
        rc=web_tick(&f);if(rc<0)return rc;
    }else if(smn64_character_combat_active(a->state)){
        rc=combat_tick(&f);if(rc<0)return rc;
    }else if(a->adhered || (a->state&0x83000u)){
        SmN64ClimbInput in={input->motion.stick_x,input->motion.stick_y,a->jump_pressed,a->jump_held,a->aim_held,input->motion.camera_yaw,input->motion.elapsed_ticks,(input->pressed|input->held)&~SMN64_BUTTON_JUMP};
        /* Source generic841D0 prefix precedes the adhered owner. */
        if(!(a->state&0x83000u)){a->d48=0;a->side.present=0;c.contact.ceiling.present=0;}
        rc=smn64_climb_tick_after_animation_hooked(a,&in,&env,&c.climb_event,counts,count,0,0,c.source_level,select_target,&f,&c.combat.movement_enabled);if(rc<0)return rc;
        smn64_bridge_actor_to_ground(a,&c.ground);c.ground.ticks=c.ticks;
    }else{
        uint32_t state=a->state;if(state!=1&&state!=2&&state!=4&&state!=8&&state!=0x10&&state!=0x40&&state!=0x400000)return -30;
        smn64_bridge_actor_to_ground(a,&c.ground);SmN64Input in=input->motion;in.jump_pressed=a->jump_pressed;in.jump_held=a->jump_held;
        rc=smn64_player_prepare_after_animation(&c.ground,&in);if(rc<0)return rc;
        smn64_bridge_ground_to_actor(&c.ground,a);a->anim.rate=65536;
        contact_from_actor(a,&c.contact);rc=smn64_free_contact_run(&c.contact,free_trace,&f,0,0);if(rc<0)return rc;
        actor_from_contact(&c.contact,a);smn64_bridge_actor_to_ground(a,&c.ground);
        SmN64Contact contact;memset(&contact,0,sizeof contact);COPY(contact.position,a->position);
        contact.grounded=(a->collision&2)!=0;contact.wall=(a->collision&1)!=0;contact.ceiling=(a->collision&0x100)!=0;
        carry_rc=carried_object(&f,a,&carried);if(carry_rc<0)return carry_rc;
        rc=carry_rc?smn64_player_finish_carrying(&c.ground,&in,&contact,counts,count,ground_route,ground_basis,&f,carried.flags_10c):
            smn64_player_finish_with_hooks(&c.ground,&in,&contact,counts,count,ground_route,ground_basis,&f);if(rc<0)return rc;
        smn64_bridge_ground_to_actor(&c.ground,a);
        if(rc==2){rc=post_ai_tail(&f,a,in.camera_yaw);if(rc<0)return rc;smn64_bridge_actor_to_ground(a,&c.ground);}
        else {a->acceleration[0]=a->acceleration[2]=0;a->acceleration[1]=40960;}
    }
    if(services->combat){SmN64CombatOwnerFrame q=combat_frame(&f,a);rc=smn64_character_combat_post_tail(&c.combat,&q);if(rc<0)return rc;}
    smn64_bridge_actor_to_web(a,&c.web,&c.web_owner);
    if(refresh(&f,a)<0)return -11;
    if(services->combat){SmN64CombatOwnerFrame q=combat_frame(&f,a);rc=smn64_character_combat_retain(&c.combat,&q);if(rc<0)return rc;}
    if(services->trails_retain){rc=services->trails_retain(services->context,a,c.markers,9,c.retained_pose,c.body_translation,c.ticks);if(rc!=1)return rc<0?rc:-22;}
    rc=smn64_web_visuals_attach(&c.visuals,&c.web.player);if(rc<0)return rc;
    if(services->combat){SmN64CombatOwnerFrame q=combat_frame(&f,a);rc=smn64_character_combat_hold(&c.combat,&q);if(rc<0)return rc;}
    if(services->attack_effects){SmN64CombatOwnerFrame q=combat_frame(&f,a);rc=services->attack_effects(services->context,&q,&c.combat);if(rc!=1)return rc<0?rc:-22;}
    rc=smn64_web_visuals_effects(&c.visuals,a->position,c.ticks,input->effect_seconds,a->random_state);if(rc<0)return rc;
    if(services->trails_effects){rc=services->trails_effects(services->context,c.ticks);if(rc!=1)return rc<0?rc:-22;}
    COPY(c.web.player.random_state,a->random_state);COPY(c.ground.random_state,a->random_state);
    c.visuals.marker=NULL;c.visuals.marker_context=NULL;
    c.stable_updates=wrapped_add(c.stable_updates,1);
    *out=c;return 1;
}
#undef COPY
