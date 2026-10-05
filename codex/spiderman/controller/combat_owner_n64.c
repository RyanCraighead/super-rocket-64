#include "combat_owner_n64.h"
#include <limits.h>
#include <string.h>
#define COPY(d,s) memcpy((d),(s),sizeof(d))
typedef struct Call { SmN64CharacterCombat *c;
    SmN64CombatOwnerFrame *f;
    int root_refreshed;
} Call;
static int32_t sar12(int32_t x){return x>=0?x/4096:-1-(int32_t)((uint32_t)(-1-x)/4096);
}

static int valid(const SmN64CharacterCombat *c,const SmN64CombatOwnerFrame *f){
    return c&&f&&f->actor&&f->traversal&&f->traversal_visuals&&f->resource==&f->traversal->player.resource&&f->pressed&&f->counts&&f->count>=300&&
    f->markers&&f->marker_count>=9&&f->retained_pose&&f->body_translation&&f->services&&f->services->bank&&!(f->services->unavailable_commands&~SMN64_COMMAND_ALL);
}

static int admit_command(void *context,SmN64CombatCommand command){
    Call *q=context;
    if(!command||((uint32_t)command&~SMN64_COMMAND_ALL))return -1;
    if(!(q->f->services->unavailable_commands&(uint32_t)command))return 1;
    q->c->unavailable_requested|=(uint32_t)command;return 0;
}
static SmN64CombatCommand state_command(uint32_t state){
    switch(state){
        case 0x4000:return SMN64_COMMAND_TRAP;
        case 0x8000:case 0x20000:return SMN64_COMMAND_YANK;
        case 0x10000:return SMN64_COMMAND_IMPACT;
        case 0x20000000:return SMN64_COMMAND_DOME;
        case 0x2000000:return SMN64_COMMAND_GRAB;
        case 0x100000:case 0x200000:return SMN64_COMMAND_CARRY;
        case 0x4000000:case 0x8000000:return SMN64_COMMAND_MOUNTED;
        default:return SMN64_COMMAND_NONE;
    }
}

void smn64_character_combat_init(SmN64CharacterCombat *c,int16_t health,uint8_t suit){
    if(!c)return;
    memset(c,0,sizeof(*c));
    c->suit=suit;
    c->movement_enabled=1;
    c->damage.health=health;
    c->damage.suit=suit;
}

void smn64_character_combat_begin_frame(SmN64CharacterCombat *c){if(c){c->effect_count=0;c->unavailable_requested=0;}
}

int smn64_character_combat_active(uint32_t state){
    switch(state){case 0x800:case 0x4000:case 0x8000:case 0x10000:case 0x20000:
        case 0x100000:case 0x200000:case 0x800000:case 0x1000000:case 0x2000000:
        case 0x4000000:case 0x8000000:case 0x20000000:case 0x80:return 1;
        default:return 0;
    }
}

static SmN64CombatEffect *effect(Call *q,SmN64CombatEffectKind kind,const int32_t pos[3]){
    SmN64CombatEffect *e;
    if(q->c->effect_count>=SMN64_COMBAT_EFFECT_CAPACITY){return NULL;}
    e=&q->c->effects[q->c->effect_count++];
    memset(e,0,sizeof(*e));
    e->kind=kind;
    COPY(e->position,pos);
    return e;
}

static int sound(Call *q,uint32_t id,const int32_t pos[3]){SmN64CombatEffect *e=effect(q,SMN64_COMBAT_SOUND,pos);
    if(!e){return -2;}
    e->sound=id;
    return 1;
}

static int resource(Call *q,const SmN64WebResourceEvent *r){
    static const SmN64WebResourceEvent zero={0};
    SmN64CombatEffect *e;
    if(!memcmp(r,&zero,sizeof(*r))){return 1;}
    e=effect(q,SMN64_COMBAT_RESOURCE,q->f->actor->position);
    if(!e){return -2;}
    e->resource=*r;
    return 1;
}

static void pull_owner(Call *q){
    SmN64ClimbState *a=q->f->actor;
    SmN64ComboOwner *s=&q->c->owner;
    s->anim=a->anim;
    s->state=a->state;
    COPY(s->position,a->position);
    COPY(s->forward,a->basis.forward);
    s->axis_1123=a->analog_x;
    s->axis_1124=a->analog_y;
    s->heading_658=(uint32_t)a->basis.yaw_delta;
    s->look_active=(uint32_t)a->turn_ticks;
    s->motion_speed=a->idle_ticks;
    s->attack_mode=q->f->traversal->player.web_mode;
    s->aim_snapshot=(uint32_t)q->f->traversal->player.auxiliary_a38;
}

static void push_owner(Call *q){SmN64ClimbState *a=q->f->actor;
    SmN64ComboOwner *s=&q->c->owner;
    a->anim=s->anim;
    a->state=s->state;
    COPY(a->position,s->position);
    COPY(a->basis.forward,s->forward);
    a->basis.yaw_delta=(int32_t)s->heading_658;
    a->turn_ticks=(int32_t)s->look_active;
    a->idle_ticks=s->motion_speed;
    q->f->traversal->player.web_mode=s->attack_mode;
    q->f->traversal->player.auxiliary_a38=(int32_t)s->aim_snapshot;
}

static void web_snapshot(const Call *q,SmN64WebAction *w){
    SmN64ClimbState *a=q->f->actor;
    SmN64WebAbility *s=&w->ability;
    s->anim=a->anim;
    s->state=a->state;
    s->holding_object=(uint32_t)a->held_object;
    s->surface_mode=(uint32_t)a->adhered;
    s->aiming=(uint32_t)a->aiming;
    s->ceiling_orientation=(uint32_t)a->ceiling_class;
    s->run_ramp=a->run_ramp;
    s->axis_1123=a->analog_x;
    s->axis_1124=a->analog_y;
    w->target=q->c->owner.target_actor;
    s->attack_mode=q->f->traversal->player.web_mode;
    s->aim_snapshot=(uint32_t)q->f->traversal->player.auxiliary_a38;
    w->phase=(uint32_t)q->f->traversal->player.substate;
    COPY(w->explicit_target,q->f->traversal->player.target);
    for(unsigned k=0;k<3;k++)w->target_normal[k]=(int16_t)q->f->traversal->player.target_normal[k];
    COPY(w->position,a->position);
    COPY(w->forward,a->basis.forward);
    COPY(w->right,a->basis.right);
    COPY(w->up,a->basis.outward);
}

static void pull_web(Call *q){web_snapshot(q,&q->c->web);}

static void push_web(Call *q){
    SmN64ClimbState *a=q->f->actor;
    SmN64WebAction *w=&q->c->web;
    SmN64WebAbility *s=&w->ability;
    q->c->owner.target_actor=w->target;
    q->f->traversal->player.web_mode=s->attack_mode;
    q->f->traversal->player.auxiliary_a38=(int32_t)s->aim_snapshot;
    q->f->traversal->player.substate=(int32_t)w->phase;
    COPY(q->f->traversal->player.target,w->explicit_target);
    for(unsigned k=0;k<3;k++)q->f->traversal->player.target_normal[k]=w->target_normal[k];
    a->anim=s->anim;
    a->state=s->state;
    a->held_object=(int32_t)s->holding_object;
    a->adhered=(int32_t)s->surface_mode;
    a->aiming=(int32_t)s->aiming;
    a->ceiling_class=(int32_t)s->ceiling_orientation;
    COPY(a->position,w->position);
    COPY(a->basis.forward,w->forward);
}

static void pull_root(Call *q){SmN64CombatRoot *r=&q->c->root;
    SmN64ClimbState *a=q->f->actor;
    COPY(r->position,a->position);
    COPY(r->forward,a->basis.forward);
    COPY(r->right,a->basis.right);
    COPY(r->up,a->basis.outward);
}

static int refresh(void *ctx,const SmN64Combo *combo){
    Call *q=ctx;
    SmN64CombatOwnerFrame *f=q->f;
    int16_t pose[216];
    if(!f->services->pose||f->services->pose(f->services->context,combo->anim.animation,combo->anim.frame,pose)!=1){return -2;}
    q->root_refreshed=1;
    memcpy(f->retained_pose,pose,sizeof(pose));
    for(unsigned k=0;k<3;k++)f->body_translation[k]=sar12(f->actor->position[k]);
    return 1;
}

static int marker(void *ctx,uint8_t id,int32_t out[3]){
    Call *q=ctx;
    SmN64CombatOwnerFrame *f=q->f;
    return smn64_marker_world(f->markers,f->marker_count,id,f->retained_pose,18,
    f->actor->basis.matrix,f->body_translation,f->actor->position,0,out)?1:-2;
}

static int bone(void *ctx,uint8_t id,int32_t out[3]){
    Call *q=ctx;
    SmN64CombatOwnerFrame *f=q->f;
    return smn64_combat_bone_world(id,f->retained_pose,18,f->actor->basis.matrix,f->body_translation,f->actor->position,out)?1:-2;
}

static int root_actor(void *ctx,const int32_t from[3],const int32_t to[3],int32_t radius){Call *q=ctx;
    const SmN64RootHost *h=&q->f->services->root;
    return h->actor_sweep?h->actor_sweep(h->context,from,to,radius):-2;
}

static int root_world(void *ctx,const int32_t from[3],const int32_t to[3]){Call *q=ctx;
    const SmN64RootHost *h=&q->f->services->root;
    return h->world_trace?h->world_trace(h->context,from,to):-2;
}

static SmN64RootHost root_host(Call *q){SmN64RootHost h={q,refresh,root_actor,root_world,marker};
    return h;
}

static int resolve(void *ctx,const SmN64Combo *combo,const SmN64CombatMotion *motion){
    Call *q=ctx;
    SmN64RootHost h=root_host(q);
    pull_root(q);
    int rc=smn64_combo_root_resolve(&q->c->root,combo,motion,&h);
    if(rc>=0){COPY(q->f->actor->position,q->c->root.position);}
    return rc;
}

static int sweep(void *ctx,uint32_t actor,const int32_t from[3],const int32_t to[3],int32_t radius,uint8_t *part,int32_t position[3]){
    Call *q=ctx;
    const SmN64CombatHost *h=&q->f->services->melee;
    return h->sweep?h->sweep(h->context,actor,from,to,radius,part,position):-2;
}

static int melee_apply(void *ctx,const SmN64CombatHit *hit){
    Call *q=ctx;
    const SmN64CombatHost *h=&q->f->services->melee;
    int rc;
    if(!h->apply){return -2;}
    rc=h->apply(h->context,hit);
    if(rc<0||rc>1){return -2;}
    if(rc){SmN64CombatEffect *e=effect(q,SMN64_COMBAT_HIT_EFFECTS,hit->position);
        if(!e){return -2;}
        if(smn64_combat_hit_effects(q->c->combo.anim.animation,(uint32_t)q->f->resource->web_type,q->c->combo.glove_hits,q->f->actor->random_state,&e->hit)!=1){return -2;}
    }
    return rc;
}

static int apply_entry(Call *q,const SmN64EntryEvent *e){
    if(e->face_actor||e->pickup_requested||e->throw_requested||e->interact_requested){
        if(!q->f->services->entry_event||q->f->services->entry_event(q->f->services->context,q->f->actor,e)!=1){return -2;}
    }
    if(e->began_combo){return sound(q,smn64_combo_start_sound(q->f->actor->random_state),q->f->actor->position);}
    return 1;
}

static int actor_at(void *ctx,uint32_t kind,size_t index,SmN64EntryActor *a){
    Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    int rc;
    if(!h->actor_at){return -2;}
    rc=h->actor_at(h->context,kind,index,a);
    if(rc<0||rc>1||(rc==1&&(!a->id||index>=SMN64_COMBAT_ACTOR_CAPACITY))){return -2;}
    return rc;
}

static int actor_id(void *ctx,uint32_t id,SmN64EntryActor *a){Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    return h->actor_by_id?h->actor_by_id(h->context,id,a):-2;
}

static int line(void *ctx,const int32_t from[3],const int32_t to[3]){Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    return h->line_clear?h->line_clear(h->context,from,to):-2;
}

static int pickup_trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64PickupTrace *out){Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    return h->pickup_trace?h->pickup_trace(h->context,from,to,out):-2;
}

static int pickup_eligible(void *ctx,uint32_t id){Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    return h->pickup_eligible?h->pickup_eligible(h->context,id):-2;
}

static int interactable(void *ctx,uint32_t *id){Call *q=ctx;
    const SmN64EntryHost *h=&q->f->services->entry;
    return h->interactable?h->interactable(h->context,id):-2;
}

static SmN64EntryHost entry_host(Call *q){SmN64EntryHost h={q,actor_at,actor_id,line,pickup_trace,pickup_eligible,interactable};
    return h;
}

static int interpret(void *ctx,SmN64ComboOwner *s,SmN64CombatInput *in,uint32_t tick,int32_t *status){
    Call *q=ctx;
    SmN64CombatOwnerFrame *f=q->f;
    SmN64ComboResult result;
    SmN64Combo *combo=&q->c->combo;
    int rc;
    combo->anim=s->anim;
    combo->glove_hits=q->c->web.ability.glove_hits;
    rc=smn64_combo_tick_admitted(combo,f->services->bank,in,tick,f->counts,f->count,&result,resolve,q,admit_command,q);
    if(rc<0){return rc;}
    s->anim=combo->anim;
    s->combo_id=combo->id;
    COPY(s->position,f->actor->position);
    f->actor->anim=combo->anim;
    if(result.began){SmN64EntryEvent event={0};
        SmN64EntryHost h=entry_host(q);
        if(smn64_combat_combo_target(f->actor->position,&h,&event)<0){return -2;}
        event.began_combo=1;
        if(apply_entry(q,&event)<0){return -2;}
        COPY(s->forward,f->actor->basis.forward);
        s->heading_658=(uint32_t)f->actor->basis.yaw_delta;
        s->look_active=(uint32_t)f->actor->turn_ticks;
    }else if(result.sample_bones){
        SmN64CombatActor actors[SMN64_COMBAT_ACTOR_CAPACITY];
        size_t count=0;
        SmN64CombatHost host={q,bone,sweep,melee_apply};
        if(!f->services->actors||f->services->actors(f->services->context,actors,SMN64_COMBAT_ACTOR_CAPACITY,&count)!=1||count>SMN64_COMBAT_ACTOR_CAPACITY){return -2;}
        /* Original root path already refreshed BEFORE displacement. Retain its
        * body translation; A09C0 refreshes only when root did not do so. */
        if(!q->root_refreshed&&refresh(q,combo)!=1){return -2;}
        rc=smn64_combo_contacts(combo,f->services->bank,tick,actors,count,q->c->suit,f->resource->difficulty,&host);
        if(rc<0){return rc;}
    }
    q->c->web.ability.glove_hits=combo->glove_hits;
    *status=result.status;
    return 1;
}

#define OWNER_FORWARD(name,field) static int name(void *ctx,SmN64ComboOwner *s){Call *q=ctx;const SmN64ComboOwnerHost *h=&q->f->services->combo;return h->field?h->field(h->context,s):-2;}
OWNER_FORWARD(owner_lost,lost_ground)
OWNER_FORWARD(owner_jump,jump)
OWNER_FORWARD(owner_stop,stop)
OWNER_FORWARD(owner_move,move_grab)
static int owner_select(void *ctx,SmN64ComboOwner *s,int32_t a,int32_t b,int32_t c,int32_t d,uint32_t *id){Call *q=ctx;
    const SmN64ComboOwnerHost *h=&q->f->services->combo;
    return h->select_target?h->select_target(h->context,s,a,b,c,d,id):-2;
}

static int owner_face(void *ctx,SmN64ComboOwner *s,uint32_t id){Call *q=ctx;
    const SmN64ComboOwnerHost *h=&q->f->services->combo;
    return h->face_actor?h->face_actor(h->context,s,id):-2;
}

static int owner_grab_actor(void *ctx,uint32_t id,SmN64GrabActor *a){Call *q=ctx;
    const SmN64ComboOwnerHost *h=&q->f->services->combo;
    return h->grab_actor?h->grab_actor(h->context,id,a):-2;
}

static int owner_grab(void *ctx,SmN64ComboOwner *s,uint32_t id,const int32_t p[3]){Call *q=ctx;
    const SmN64ComboOwnerHost *h=&q->f->services->combo;
    return h->grab_request?h->grab_request(h->context,s,id,p):-2;
}

static int owner_release(void *ctx,uint32_t id){Call *q=ctx;
    const SmN64ComboOwnerHost *h=&q->f->services->combo;
    return h->release_grab?h->release_grab(h->context,id):-2;
}

static SmN64ComboOwnerHost combo_host(Call *q){SmN64ComboOwnerHost h={q,owner_lost,owner_jump,interpret,owner_select,owner_face,owner_stop,owner_grab_actor,owner_grab,owner_release,owner_move};
    return h;
}

static void pull_air(Call *q){SmN64AirAttack *s=&q->c->air;
    SmN64ClimbState *a=q->f->actor;
    s->anim=a->anim;
    s->state=a->state;
    s->aiming=(uint32_t)a->aiming;
    COPY(s->position,a->position);
    COPY(s->previous_position,q->f->previous_position);
    COPY(s->velocity,a->velocity);
    COPY(s->forward,a->basis.forward);
    for(unsigned k=0;k<3;k++)s->normal[k]=a->basis.normal[k];
    s->turn_a=(uint32_t)a->field65c;
    s->turn_b=(uint32_t)q->f->traversal->player.launch_flag;
    s->movement_blocked=(uint32_t)a->d20;
    s->movement_enabled=q->c->movement_enabled;
}

static void push_air(Call *q){SmN64AirAttack *s=&q->c->air;
    SmN64ClimbState *a=q->f->actor;
    a->anim=s->anim;
    a->state=s->state;
    COPY(a->position,s->position);
    COPY(a->velocity,s->velocity);
    COPY(a->basis.forward,s->forward);
    for(unsigned k=0;k<3;k++)a->basis.normal[k]=(int16_t)s->normal[k];
    a->field65c=(int32_t)s->turn_a;
    q->f->traversal->player.launch_flag=(int32_t)s->turn_b;
    a->d20=(int32_t)s->movement_blocked;
    q->c->movement_enabled=s->movement_enabled;
}

/* These are the existing source graphical lifetimes, operating on the SAME
 * pending visual registry and shared source RNG as the enclosing character. */
static int visual_lifecycle(Call *q,SmN64WebEventKind event){
    SmN64WebRuntime *w=q->f->traversal;SmN64WebVisuals *v=q->f->traversal_visuals;
    if(q->f->graphical_clock)return smn64_web_visual_lifecycle_ordered(v,event,&w->player,&w->swinger,q->f->graphical_clock);
    return smn64_web_visual_lifecycle(v,event,&w->player,&w->swinger);
}
static int release_swing(Call *q,int detach,uint32_t expected_handle){
    SmN64WebRuntime *w=q->f->traversal;SmN64WebVisuals *v=q->f->traversal_visuals;
    if(!w->player.swinger_present){return expected_handle?-2:1;}
    if(v->active_swing<0||v->active_swing>=SMN64_WEB_VISUAL_CAPACITY||!v->strands[v->active_swing].alive){return -2;}
    if(expected_handle&&expected_handle!=(uint32_t)v->active_swing+1){return -2;}
    COPY(w->player.position,q->f->actor->position);COPY(w->player.random_state,q->f->actor->random_state);
    if(detach&&visual_lifecycle(q,SMN64_WEB_DETACH_STRAND)!=1){return -2;}
    if(visual_lifecycle(q,SMN64_WEB_DELETE_SWINGER)!=1){return -2;}
    w->player.swinger_present=0;COPY(q->f->actor->random_state,w->player.random_state);return 1;
}
static int air_event(Call *q,const SmN64AirEvent *event){
    SmN64AirEvent e=*event;
    if(e.detach_swing){if(release_swing(q,1,0)<0){return -2;}e.detach_swing=0;}
    if(!e.face_target&&!e.align_normal&&!e.start_trails&&!e.stop_trails){return 1;}
    return q->f->services->air_event?q->f->services->air_event(q->f->services->context,q->f->actor,&q->c->air,&e):-2;
}
static int ability_event(Call *q,const SmN64WebAbilityEvent *e){
    if(resource(q,&e->resource)<0){return -2;}
    if(e->glove_sound&&sound(q,e->glove_sound,q->f->actor->position)<0){return -2;}
    if(e->create_dome||e->destroy_dome||e->grab_target_query||e->face_target||e->fire_amount||e->release_web){
        if(!q->f->services->ability_event||q->f->services->ability_event(q->f->services->context,q->f->actor,q->c,e)!=1){return -2;}
    }
    return 1;
}

static int combat_entry(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,uint32_t address){
    Call q={c,f,0};
    int rc;
    if(!valid(c,f)){return -1;}
    if(address==0x80099058u){
        SmN64CombatEntry pending_entry=c->entry,*s=&pending_entry;
        SmN64Combo pending_combo=c->combo;
        SmN64ClimbState *a=f->actor;
        SmN64CombatInput in={*f->pressed,f->held.web};
        SmN64EntryEvent e;
        SmN64EntryHost h=entry_host(&q);
        s->anim=a->anim;
        s->state=a->state;
        s->surface_mode=(uint32_t)a->adhered;
        s->aiming=(uint32_t)a->aiming;
        s->held_actor=(uint32_t)a->held_object;
        s->target_actor=c->owner.target_actor;
        s->phase=(uint32_t)f->traversal->player.substate;
        COPY(s->position,a->position);
        COPY(s->forward,a->basis.forward);
        rc=smn64_combat_entry(s,&pending_combo,f->services->bank,&in,f->tick,f->counts,f->count,&h,&e);
        if(rc<0){return rc;}
        if((e.pickup_requested||e.throw_requested)&&!admit_command(&q,SMN64_COMMAND_CARRY))return SMN64_COMBAT_ENTRY_DECLINED;
        if(e.interact_requested&&!admit_command(&q,SMN64_COMMAND_INTERACT))return SMN64_COMBAT_ENTRY_DECLINED;
        c->entry=pending_entry;c->combo=pending_combo;
        *f->pressed=in.pressed;
        a->anim=s->anim;
        a->state=s->state;
        f->traversal->player.substate=(int32_t)s->phase;
        c->owner.entry_tick=s->entry_tick;
        c->owner.combo_id=c->combo.id;
        c->carry.pickup_actor=s->pickup_actor;
        c->carry.target_actor=s->target_actor;
        if(rc&&apply_entry(&q,&e)<0){return -2;}
        return rc;
    }
    if(address==0x80099444u){SmN64WebAbilityEvent e;
        SmN64WebAbilityPlan plan;SmN64WebAction pending=c->web;
        web_snapshot(&q,&pending);
        rc=smn64_web_ability_plan(&pending.ability,&f->held,f->tick,&plan);
        if(rc<0)return rc;
        if(plan.command&&!admit_command(&q,plan.command))return SMN64_COMBAT_ENTRY_DECLINED;
        c->web=pending;
        rc=smn64_web_ability_request(&c->web.ability,&f->held,f->tick,f->resource,f->actor->random_state,f->counts,f->count,&e);
        if(rc<0){return rc;}
        push_web(&q);
        if(ability_event(&q,&e)<0){return -2;}
        return rc;
    }
    if(address==0x8009996cu||address==0x8009a7d8u){
        uint32_t id=c->owner.target_actor;
        uint16_t type=0;
        int32_t position[3]={0};
        SmN64AirEvent e;
        uint32_t surface=(uint32_t)f->actor->adhered;
        uint32_t prior_state=f->actor->state;
        int32_t prior_surface=f->actor->adhered;
        SmN64Anim prior_animation=f->actor->anim;
        if(!(*f->pressed&6u)||f->actor->aiming){return 0;}
        if(address==0x8009996cu&&!f->actor->wall_class&&!f->actor->ceiling_class){return 0;}
        if(!id){return 0;}
        if(!f->services->target){return -2;}
        rc=f->services->target(f->services->context,&id,&type,position);
        if(rc<0||rc>1||(rc&&id!=c->owner.target_actor)){return -2;}
        if(!rc){return 0;}
        c->owner.target_actor=id;
        pull_air(&q);
        if(address==0x8009996cu)rc=smn64_surface_attack_request(&c->air,&surface,(uint32_t)f->actor->wall_class,(uint32_t)f->actor->ceiling_class,id,position,!!(*f->pressed&4),!!(*f->pressed&2),f->tick,f->counts,f->count,&e);
        else rc=smn64_air_attack_request(&c->air,id,type,position,!!(*f->pressed&4),!!(*f->pressed&2),f->tick,f->counts,f->count,&e);
        if(rc<0){return rc;}
        if(rc){
            int event_rc;push_air(&q);
            /* Original timed A201C sees the PRE-entry state/clip. Surface
             * alignment also sees old CF0; callback clears it after basis and
             * before timed facing. Invalid basis must fail, not run a fallback
             * stop after the new action has already been committed. */
            f->actor->state=prior_state;f->actor->anim=prior_animation;
            f->actor->adhered=prior_surface;
            event_rc=air_event(&q,&e);
            f->actor->state=c->air.state;f->actor->anim=c->air.anim;
            f->actor->adhered=(int32_t)surface;
            if(event_rc!=1){return -2;}
        }return rc;
    }
    return 0;
}

static int combat_retain(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};
    if(!valid(c,f)){return -1;}
    if(!c->combo.active){return 0;}
    SmN64RootHost h=root_host(&q);
    pull_root(&q);
    return smn64_combo_root_retain(&c->root,&h);
}

static int fire_actor(void *ctx,uint32_t id,SmN64FireActor *a){Call *q=ctx;
    const SmN64FireHost *h=&q->f->services->fire;
    int rc=h->actor?h->actor(h->context,id,a):-2;
    if(rc==1&&id==q->c->web.target)q->c->web.target_type=a->type;
    return rc;
}

static int fire_target(void *ctx,int32_t p[3],uint32_t *id){Call *q=ctx;
    const SmN64FireHost *h=&q->f->services->fire;
    return h->special_target?h->special_target(h->context,p,id):-2;
}

static int fire_trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64FireSurface *p){Call *q=ctx;
    const SmN64FireHost *h=&q->f->services->fire;
    return h->trace?h->trace(h->context,from,to,p):-2;
}

static int web_fire(void *ctx,SmN64WebAction *s,uint8_t auto_target,int32_t amount,uint8_t surface,const int16_t normal[3],uint32_t *flags){
    Call *q=ctx;
    const SmN64CombatOwnerServices *h=q->f->services;
    SmN64FireWeb fire;
    SmN64FireEvent e;
    int rc;
    if(!h->fire_event){return h->web.fire?h->web.fire(h->web.context,s,auto_target,amount,surface,normal,flags):-2;}
    memset(&fire,0,sizeof(fire));
    COPY(fire.position,s->position);
    COPY(fire.forward,s->forward);
    fire.target_id=s->target;
    fire.graphic_id=s->graphic;
    fire.graphic_hand=q->c->graphic_hand;
    fire.animation=s->ability.anim.animation;
    fire.attack_mode=s->ability.attack_mode;
    fire.loop_sound=q->c->loop_sound;
    SmN64FireHost host={q,fire_actor,marker,fire_target,fire_trace};
    rc=smn64_fireweb(&fire,auto_target,amount,s->explicit_target,surface,normal,q->f->resource,q->f->actor->random_state,&host,&e);
    if(rc<0){return rc;}
    /* Native event handler stages real recipient/graphic work and its source RNG
    * synchronously, not when the delayed presentation queue is drained. */
    if(h->fire_event(h->context,s,&e,q->f->actor->random_state)!=1){return -2;}
    if(resource(q,&e.resource)<0){return -2;}
    if(e.sound&&sound(q,e.sound,e.sound_positional?s->position:q->f->actor->position)<0){return -2;}
    s->graphic=fire.graphic_id;
    q->c->loop_sound=fire.loop_sound;
    *flags=e.result_flags;
    return 1;
}

#define WEB_FORWARD(name,field) static int name(void *ctx,SmN64WebAction *s){Call *q=ctx;const SmN64WebActionHost *h=&q->f->services->web;return h->field?h->field(h->context,s):-2;}
WEB_FORWARD(web_jump,jump)
WEB_FORWARD(web_stop,stop)
WEB_FORWARD(web_grab,grab)
static int web_create(void *ctx,uint8_t hand,uint32_t type,uint32_t *id){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    int rc=h->create_graphic?h->create_graphic(h->context,hand,type,id):-2;
    if(rc==1)q->c->graphic_hand=hand;
    return rc;
}

static int web_release(void *ctx,uint32_t id,uint8_t immediate){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->release_graphic?h->release_graphic(h->context,id,immediate):-2;
}

static int web_graphic(void *ctx,uint32_t id,uint32_t *actor){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->graphic_target?h->graphic_target(h->context,id,actor):-2;
}

static int web_position(void *ctx,uint32_t actor,int32_t p[3]){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->target_position?h->target_position(h->context,actor,p):-2;
}

static int web_yank(void *ctx,uint32_t id,const int32_t v[3],const SmN64YankCurve *curve){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->yank?h->yank(h->context,id,v,curve):-2;
}

static int web_sparks(void *ctx,uint32_t n){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->impact_sparks?h->impact_sparks(h->context,n):-2;
}

static int web_dome(void *ctx,SmN64WebAction *s,uint32_t type){Call *q=ctx;
    const SmN64WebActionHost *h=&q->f->services->web;
    return h->dome?h->dome(h->context,s,type):-2;
}

static SmN64WebActionHost web_host(Call *q){SmN64WebActionHost h={q,web_jump,web_stop,web_create,web_release,web_graphic,web_position,web_fire,web_yank,web_sparks,web_dome,web_grab};
    return h;
}

static int mounted_actor(void *ctx,uint32_t id,SmN64MountedActor *a){Call *q=ctx;
    const SmN64MountedHost *h=&q->f->services->mounted;
    return h->actor?h->actor(h->context,id,a):-2;
}

static int mounted_damage(void *ctx,SmN64MountedOwner *s,const SmN64MountedHit *hit){Call *q=ctx;
    const SmN64MountedHost *h=&q->f->services->mounted;
    return h->damage?h->damage(h->context,s,hit):-2;
}

static int mounted_stop(void *ctx,SmN64MountedOwner *s){Call *q=ctx;
    const SmN64MountedHost *h=&q->f->services->mounted;
    return h->stop?h->stop(h->context,s):-2;
}

static int mounted_sound(void *ctx,uint32_t id,const int32_t pos[3]){return sound(ctx,id,pos);
}

static int mounted_effects(void *ctx,SmN64MountedOwner *s,const int32_t pos[3]){
    Call *q=ctx;
    SmN64CombatEffect *e=effect(q,SMN64_COMBAT_HIT_EFFECTS,pos);
    if(!e){return -2;}
    return smn64_combat_hit_particles(0,s->player.anim.animation,(uint32_t)q->f->resource->web_type,q->c->web.ability.glove_hits,q->f->actor->random_state,&e->hit);
}

static int pull_damage(Call *q){
    SmN64DamageState *s=&q->c->damage;
    SmN64ClimbState *a=q->f->actor;
    s->anim=a->anim;
    s->state=a->state;
    s->suit=q->c->suit;
    s->input_disabled=(uint32_t)a->control_inhibit;
    s->turn_lock=(uint32_t)a->field65c;
    s->movement_blocked=(uint32_t)a->d20;
    s->scripted=(uint32_t)q->f->resource->player_2cc;
    s->surface_mode=(uint32_t)a->adhered;
    s->wall=(uint32_t)a->wall_class;
    s->ceiling=(uint32_t)a->ceiling_class;
    s->aiming=(uint32_t)a->aiming;
    s->held_actor=(uint32_t)a->held_object;
    s->web_graphic=q->c->web.graphic;
    if(q->f->traversal->player.zip_graphic){
        int32_t handle=q->f->traversal_visuals->active_zip;
        if(s->web_graphic||handle<0||handle>=SMN64_WEB_VISUAL_CAPACITY||!q->f->traversal_visuals->strands[handle].alive){return -2;}
        s->web_graphic=(uint32_t)handle+1;
    }
    s->air_landing_wait=q->c->air.landing_wait;
    s->swing_graphic=0;
    if(q->f->traversal->player.swinger_present){
        int32_t handle=q->f->traversal_visuals->active_swing;
        if(handle<0||handle>=SMN64_WEB_VISUAL_CAPACITY||!q->f->traversal_visuals->strands[handle].alive){return -2;}
        s->swing_graphic=(uint32_t)handle+1;
    }
    s->elapsed_ticks=a->anim.elapsed_ticks;
    COPY(s->position,a->position);
    COPY(s->velocity,a->velocity);
    COPY(s->forward,a->basis.forward);
    COPY(s->up,a->basis.outward);
    for(unsigned k=0;k<3;k++)s->normal[k]=a->basis.normal[k];
    return 1;
}

static void push_damage(Call *q){
    SmN64DamageState *s=&q->c->damage;
    SmN64ClimbState *a=q->f->actor;
    a->anim=s->anim;
    a->state=s->state;
    a->adhered=(int32_t)s->surface_mode;
    a->wall_class=(int32_t)s->wall;
    a->ceiling_class=(int32_t)s->ceiling;
    a->aiming=(int32_t)s->aiming;
    a->held_object=(int32_t)s->held_actor;
    a->control_inhibit=(int32_t)s->input_disabled;
    a->field65c=(int32_t)s->turn_lock;
    a->d20=(int32_t)s->movement_blocked;
    q->f->resource->player_2cc=(int32_t)s->scripted;
    if(!q->f->traversal->player.zip_graphic)q->c->web.graphic=s->web_graphic;
    q->c->air.landing_wait=s->air_landing_wait;
    q->f->traversal->damage_tick=s->damage_tick;
    q->f->traversal->damage_flags=s->prior_damage_state;
    COPY(a->position,s->position);
    COPY(a->velocity,s->velocity);
    COPY(a->basis.forward,s->forward);
    for(unsigned k=0;k<3;k++)a->basis.normal[k]=(int16_t)s->normal[k];
}

static int damage_event(Call *q,const SmN64DamageEvent *event){
    SmN64DamageEvent remaining=*event;const SmN64DamageEvent *e=&remaining;
    if(e->release_web){
        SmN64WebRuntime *w=q->f->traversal;SmN64WebVisuals *v=q->f->traversal_visuals;
        if(w->player.zip_graphic){
            if(v->active_zip<0||e->release_web!=(uint32_t)v->active_zip+1){return -2;}
            COPY(w->player.random_state,q->f->actor->random_state);
            if(visual_lifecycle(q,SMN64_WEB_RETRACT_ZIP)!=1){return -2;}
            w->player.zip_graphic=0;COPY(q->f->actor->random_state,w->player.random_state);
        }else if(web_release(q,e->release_web,0)!=1){return -2;}
        remaining.release_web=0;
    }
    if(e->release_swing){if(release_swing(q,0,e->release_swing)<0){return -2;}remaining.release_swing=0;}
    /* Actor/graphics/basis/camera changes must finish before sound publication. */
    if(e->stop_trails||e->drop_actor||e->release_web||e->release_swing||e->exit_aim||e->unlock_camera||e->restore_armor_model||e->clear_armor_ui||e->align_normal||e->stun_effect||e->rumble_kind||e->died){
        if(!q->f->services->damage_event||q->f->services->damage_event(q->f->services->context,q->f->actor,q->c,e)!=1){return -2;}
    }
    if(e->sound&&sound(q,e->sound,q->f->actor->position)<0){return -2;}
    if(e->extra_sound&&sound(q,e->extra_sound,q->f->actor->position)<0){return -2;}
    if(e->voice&&sound(q,e->voice,q->f->actor->position)<0){return -2;}
    return 1;
}

static int combat_damage(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,const SmN64CombatHit *hit){
    Call q={c,f,0};
    SmN64DamageEvent e;
    int rc;
    if(!valid(c,f)||!hit){return -1;}
    if(pull_damage(&q)<0){return -2;}
    rc=smn64_player_damage(&c->damage,hit,f->tick,f->actor->random_state,f->counts,f->count,&f->services->damage,&e);
    if(rc<0){return rc;}
    push_damage(&q);
    if(damage_event(&q,&e)<0){return -2;}
    return rc;
}

static int combat_die(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};
    SmN64DamageEvent e;
    int rc;
    if(!valid(c,f)){return -1;}
    if(pull_damage(&q)<0){return -2;}
    rc=smn64_player_die(&c->damage,f->counts,f->count,&e);
    if(rc<0){return rc;}
    push_damage(&q);
    if(damage_event(&q,&e)<0){return -2;}
    return rc;
}

static void pull_hurt(Call *q){
    SmN64HurtOwner *s=&q->c->hurt;SmN64ClimbState *a=q->f->actor;
    s->anim=a->anim;s->state=a->state;s->collision=a->collision;s->health=q->c->damage.health;
    s->glove_hits=q->c->web.ability.glove_hits;s->glove_display=q->c->web.ability.glove_fade;s->glove_tick=q->c->web.ability.last_glove_tick;
    s->invulnerable_ticks=q->c->damage.invulnerable_ticks;s->cf8=(uint32_t)a->cf8;s->d20=(uint32_t)a->d20;s->jump_variant=(uint32_t)a->jump_variant;
    s->axis_1123=a->analog_x;s->axis_1124=a->analog_y;s->jump_pressed=a->jump_pressed;s->jump_latch_311=a->jump_pressed;
    s->position_y=a->position[1];s->fall_origin_y=a->falling_origin_y;s->motion_speed=a->idle_ticks;
    s->field_660=(uint32_t)q->f->traversal->player.launch_flag;s->fall_latch_664=(uint32_t)q->f->traversal->field664;
}
static void push_hurt(Call *q){
    SmN64HurtOwner *s=&q->c->hurt;SmN64ClimbState *a=q->f->actor;
    a->anim=s->anim;a->state=s->state;a->cf8=(int32_t)s->cf8;a->d20=(int32_t)s->d20;a->jump_variant=(int32_t)s->jump_variant;a->jump_pressed=s->jump_pressed;a->idle_ticks=s->motion_speed;
    q->f->traversal->player.launch_flag=(int32_t)s->field_660;q->f->traversal->field664=(int32_t)s->fall_latch_664;
    q->c->damage.health=s->health;q->c->damage.invulnerable_ticks=s->invulnerable_ticks;
    q->c->web.ability.glove_hits=s->glove_hits;q->c->web.ability.glove_fade=s->glove_display;q->c->web.ability.last_glove_tick=s->glove_tick;
}
static int hurt_stop(void *ctx,SmN64HurtOwner *s){
    Call *q=ctx;const SmN64HurtHost *h=&q->f->services->hurt;int rc;
    if(h->stop){return h->stop(h->context,s);}
    push_hurt(q);pull_owner(q);rc=owner_stop(q,&q->c->owner);
    if(rc==1){push_owner(q);pull_hurt(q);}return rc;
}
static int hurt_die(void *ctx,SmN64HurtOwner *s){
    Call *q=ctx;const SmN64HurtHost *h=&q->f->services->hurt;int rc;
    if(h->die){return h->die(h->context,s);}
    push_hurt(q);rc=combat_die(q->c,q->f);if(rc==1)pull_hurt(q);return rc;
}
static int hurt_fall_damage(void *ctx,SmN64HurtOwner *s,uint16_t damage){
    Call *q=ctx;const SmN64HurtHost *h=&q->f->services->hurt;
    return h->fall_damage?h->fall_damage(h->context,s,damage):-2;
}
static int hurt_event(Call *q,const SmN64HurtEvent *e){
    if(e->sound&&sound(q,e->sound,q->f->actor->position)<0){return -2;}
    if(e->level_state){SmN64CombatEffect *out=effect(q,SMN64_COMBAT_GAMEOVER,q->f->actor->position);if(!out){return -2;}out->sound=e->level_state;}
    /* Rumble is optional native presentation; it never changes source state. */
    return 1;
}
static int combat_land(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};SmN64HurtEvent e;SmN64HurtHost h={&q,hurt_stop,hurt_die,hurt_fall_damage};int rc;
    if(!valid(c,f)){return -1;}pull_hurt(&q);
    rc=smn64_hurt_land(&c->hurt,f->actor->random_state,f->counts,f->count,&h,&e);
    if(rc<0){return rc;}push_hurt(&q);if(hurt_event(&q,&e)<0){return -2;}return rc;
}
static void clear_held(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    f->actor->held_object=0;c->carry.held_actor=0;c->entry.held_actor=0;
    c->damage.held_actor=0;c->web.ability.holding_object=0;
    f->traversal->player.holding=0;
}
static int combat_carry_object(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,SmN64CarryObject *object){
    int rc;uint32_t id;
    if(!valid(c,f)||!object){return -1;}
    memset(object,0,sizeof(*object));id=(uint32_t)f->actor->held_object;
    if(!id){return 0;}
    const SmN64CarryHost *h=&f->services->carry;
    if(!h->object){return -2;}
    rc=h->object(h->context,id,object);
    if(rc<0){return rc;}
    /* A native generation-checked handle can expire between source ticks.
     * This is an explicit host lifetime boundary, not source release physics. */
    if(!rc){clear_held(c,f);return 0;}
    if(rc!=1||object->id!=id){return -2;}
    return 1;
}
static int combat_release_held(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,const int32_t velocity[3],int smash){
    SmN64CarryObject object;int rc;
    if(!velocity||(smash!=0&&smash!=1)){return -1;}
    rc=combat_carry_object(c,f,&object);if(rc<=0){return rc;}
    if(!f->services->carry_release){return -2;}
    /* Source91F90 clears113C before virtual Smash;9ACB0 calls Drop while the
     * pointer remains live and clears it afterward. Whole-frame rollback owns
     * restoration if the external operation rejects this pending transaction. */
    if(smash)clear_held(c,f);
    rc=f->services->carry_release(f->services->context,object.id,velocity,smash);
    if(rc!=1){return rc<0?rc:-2;}
    if(!smash){clear_held(c,f);}return 1;
}
static int combat_hold(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};SmN64CarryObject object;int32_t p0[3],p1[3],out[3];uint16_t yaw;
    int rc=combat_carry_object(c,f,&object);if(rc<=0){return rc;}
    if(!f->services->hold_actor){return -2;}
    if(marker(&q,1,p1)!=1||marker(&q,0,p0)!=1){return -2;}
    if(smn64_carry_position(p1,p0,f->actor->position,f->actor->basis.forward,object.hold_radius,(uint16_t)object.yaw,(uint16_t)f->actor->basis.yaw_delta,out,&yaw)!=1){return -2;}
    return f->services->hold_actor(f->services->context,object.id,out,yaw)==1?1:-2;
}
int smn64_character_combat_successor(SmN64ClimbState *a,const uint16_t *counts,size_t n){
    uint16_t old,next;if(!a||!counts){return -1;}if(!a->anim.finished){return 65535;}
    old=a->anim.animation;
    switch(old){
        case 120:next=123;break;
        case 121:case 122:case 124:case 130:case 134:case 170:case 174:case 180:case 187:case 284:case 285:next=0;break;
        case 123:next=123;break;case 125:case 126:case 128:next=128;break;
        case 172:next=173;break;case 173:next=174;break;
        case 175:case 176:next=176;break;case 177:next=228;break;
        case 178:next=180;break;case 181:next=19;break;
        default:return 65535;
    }
    if(next>=n||!counts[next]||counts[next]>INT16_MAX){return -1;}
    smn64_anim_run(&a->anim,next,counts[next],0,-1);return old;
}
static int air_interrupt_phase(void *context,SmN64AirAttack *air,uint32_t phase){
    Call *q=context;const SmN64AirInterruptHost *host=&q->f->services->air_interrupt;
    /* The typed kernel chooses the exact phase. In particular ALIGN_FLAT sees
     * the old clip/state/velocity, but its already-written flat normal. */
    if(air!=&q->c->air||!host->phase)return -2;
    push_air(q);
    return host->phase(host->context,air,phase)==1?1:-2;
}
static int air_interrupt(Call *q){
    SmN64AirAttack *air=&q->c->air;SmN64CombatOwnerFrame *f=q->f;
    uint32_t first=0,jump=(uint32_t)f->actor->jump_variant;int rc;
    /* Source119E clears before the prelude; the kernel repeats that scalar
     * write as part of its independently verified contract. */
    f->actor->idle_ticks=0;
    if(!air->landing_wait&&(q->c->damage.prior_damage_state&0x1000000u)&&
       (uint32_t)(f->tick-q->c->damage.damage_tick)<6u&&
       (air->anim.animation==129||air->anim.animation==133)&&air->anim.finished){
        if(!f->services->first_trail_present||
           f->services->first_trail_present(f->services->context,&first)!=1||first>1)return -2;
    }
    /* Outside that prelude the first argument is not read by the source helper;
     * no assertion about actual graphic presence is made. */
    SmN64AirInterruptHost host={q,air_interrupt_phase};
    rc=smn64_air_attack_interrupt(air,q->c->damage.prior_damage_state,
        q->c->damage.damage_tick,f->tick,first,&jump,&f->actor->idle_ticks,
        f->counts,f->count,&host);
    if(rc==1)f->actor->jump_variant=(int32_t)jump;
    return rc;
}
static int combat_dispatch(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};
    int rc;
    SmN64ClimbState *a;
    if(!valid(c,f)){return -1;}
    a=f->actor;
    /* An unavailable command cannot be newly entered through this owner.
     * Finding one already active is a state/capability invariant failure, not
     * a user request: never silently reset a live actor/graphic relationship. */
    if(f->services->unavailable_commands&(uint32_t)state_command(a->state))return -2;
    if(a->state==0x800&&c->combo.pending_id>=0&&
       (f->services->unavailable_commands&(uint32_t)smn64_combo_command((uint16_t)c->combo.pending_id)))return -2;
    if(a->state==0x800||a->state==0x2000000){
        SmN64CombatInput in={*f->pressed,f->held.web};
        SmN64ComboOwnerHost h=combo_host(&q);
        pull_owner(&q);
        if(a->state==0x800)rc=smn64_combo_owner_step_admitted(&c->owner,&in,&f->held,f->tick,f->counts,f->count,&h,admit_command,&q);
        else rc=smn64_grab_owner_step_admitted(&c->owner,&in,f->completed_animation,f->tick,f->resource->difficulty,f->counts,f->count,&h,admit_command,&q);
        if(rc<0){return rc;}
        *f->pressed=in.pressed;
        push_owner(&q);
        c->web.ability.attack_mode=c->owner.attack_mode;
        c->web.ability.fired=c->owner.fired;
        c->web.ability.aim_snapshot=c->owner.aim_snapshot;
        c->web.ability.yank_variant=c->owner.yank_variant;
        c->web.ability.actor_flags=c->owner.actor_flags;
        c->web.target=c->owner.target_actor;
        return rc;
    }
    if(a->state==0x1000000){SmN64AirEvent e;
        pull_air(&q);
        rc=air_interrupt(&q);
        if(rc<0){return rc;}
        if(rc){push_air(&q);return 1;}
        rc=smn64_air_attack_tick(&c->air,!!(a->collision&2),f->counts,f->count,&e);
        if(rc<0){return rc;}
        push_air(&q);
        if(air_event(&q,&e)!=1){return -2;}
        return 1;
    }
    if(a->state==0x4000||a->state==0x8000||a->state==0x10000||a->state==0x20000){
        SmN64WebAbilityEvent e;
        SmN64WebActionHost h=web_host(&q);
        pull_web(&q);
        rc=smn64_web_action_step_admitted(&c->web,&f->held,f->tick,f->resource,a->random_state,f->counts,f->count,&h,&e,admit_command,&q);
        if(rc<0){return rc;}
        push_web(&q);
        if(ability_event(&q,&e)<0){return -2;}
        return rc;
    }
    if(a->state==0x800000){
        SmN64HurtEvent e;SmN64HurtHost h={&q,hurt_stop,hurt_die,hurt_fall_damage};pull_hurt(&q);
        rc=smn64_hurt_owner_step(&c->hurt,f->completed_animation,f->tick,f->counts,f->count,&h,&e);
        if(rc<0){return rc;}push_hurt(&q);if(hurt_event(&q,&e)<0){return -2;}return rc;
    }
    if(a->state==0x20000000){SmN64WebAbilityEvent e;
        pull_web(&q);
        rc=smn64_web_dome_tick(&c->web.ability,&f->held,f->completed_animation,f->tick,f->counts,f->count,&e);
        if(rc<0){return rc;}
        push_web(&q);
        if(ability_event(&q,&e)<0){return -2;}
        return 1;
    }
    if(a->state==0x100000||a->state==0x200000){SmN64Carry *s=&c->carry;
        SmN64CarryEvent e;
        if(a->state==0x100000)a->idle_ticks=0; /* original8DEB0 */
        s->anim=a->anim;
        s->state=a->state;
        s->held_actor=(uint32_t)a->held_object;
        s->target_actor=c->owner.target_actor;
        COPY(s->position,a->position);
        COPY(s->forward,a->basis.forward);
        COPY(s->up,a->basis.outward);
        rc=f->services->carry_throw_ordered?
            smn64_carry_step_ordered(s,a->random_state,&f->services->carry,&e,f->services->carry_throw_ordered):
            smn64_carry_step(s,a->random_state,&f->services->carry,&e);
        if(rc<0){return rc;}
        a->anim=s->anim;
        a->state=s->state;
        a->held_object=(int32_t)s->held_actor;
        COPY(a->position,s->position);
        COPY(a->basis.forward,s->forward);
        if(e.camera_reset)a->camera_reset=1;
        if(e.sound&&sound(&q,e.sound,a->position)<0){return -2;}
        return rc;
    }
    if(a->state==0x4000000||a->state==0x8000000){SmN64MountedOwner *s=&c->mounted;
        SmN64CombatInput in={*f->pressed,f->held.web};
        SmN64MountedHost h={&q,mounted_actor,mounted_damage,mounted_effects,mounted_sound,mounted_stop};
        pull_owner(&q);
        s->player=c->owner;
        COPY(s->velocity,a->velocity);
        COPY(s->up,a->basis.outward);
        s->suit=c->suit;
        s->difficulty=f->resource->difficulty;
        s->combo_metric=c->damage.combo_metric;s->display_value=c->damage.display_value;s->display_timer=c->damage.display_timer;
        s->jump_latch_311=a->jump_pressed;
        rc=smn64_mounted_step(s,&in,f->completed_animation,f->tick,f->counts,f->count,&h);
        if(rc<0){return rc;}
        c->owner=s->player;
        c->damage.combo_metric=s->combo_metric;c->damage.display_value=s->display_value;c->damage.display_timer=s->display_timer;
        push_owner(&q);
        COPY(a->velocity,s->velocity);
        a->jump_pressed=s->jump_latch_311;
        *f->pressed=in.pressed;
        return rc;
    }
    if(a->state==0x80){
        rc=smn64_death_wait(&c->death_wait,!!a->anim.finished,a->anim.elapsed_ticks);
        if(rc<0){return rc;}
        if(rc){SmN64CombatEffect *e=effect(&q,SMN64_COMBAT_GAMEOVER,a->position);
            if(!e){return -2;}
            e->sound=2;
        }return 1;
    }
    return 0;
}

static int combat_post_tail(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};int rc;if(!valid(c,f)){return -1;}
    if(f->actor->state!=0x1000000){return 0;}
    pull_air(&q);
    rc=smn64_air_attack_contacts(&c->air,f->services->bank,c->suit,f->resource->difficulty,&f->services->air);
    if(rc<0){return rc;}
    /* Contacts change velocity/hit latch, never repeat state AI or basis work. */
    COPY(f->actor->velocity,c->air.velocity);
    return 1;
}

#undef COPY
static int combat_prepare(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    uint32_t value;if(!valid(c,f)||f->actor->anim.elapsed_ticks<1||f->actor->anim.elapsed_ticks>6){return -1;}
    c->movement_enabled=1;
    c->damage.invulnerable_ticks=smn64_hurt_invulnerability_tick(c->damage.invulnerable_ticks,f->actor->anim.elapsed_ticks);
    c->damage.display_timer=smn64_hurt_invulnerability_tick(c->damage.display_timer,f->actor->anim.elapsed_ticks);
    value=c->damage.stun_ticks;
    if(value){
        value-=(uint32_t)f->actor->anim.elapsed_ticks;
        if(!value||(value&0x80000000u)){
            c->damage.stun_ticks=0;
            if(!f->services->release_stun||f->services->release_stun(f->services->context)!=1){return -2;}
        }else c->damage.stun_ticks=value;
    }
    return 1;
}
static int combat_select_target(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    Call q={c,f,0};uint32_t id=0;int rc;if(!valid(c,f)){return -1;}
    if(!f->actor->aiming){
        pull_owner(&q);c->owner.target_actor=0;
        rc=owner_select(&q,&c->owner,2048,2896,4096,4096,&id);
        if(rc!=1){return -2;}
        c->owner.target_actor=id;c->web.target=id;
    }
    c->hurt.fall_timer_1188=smn64_hurt_fall_timer_tick(c->hurt.fall_timer_1188,f->actor->state,f->actor->collision,f->actor->anim.elapsed_ticks);
    return f->actor->aiming?0:1;
}
/* Public boundaries never retain a borrowed pending-frame pointer. */
static void bind(SmN64CombatOwnerFrame *f,int active){
    if(f&&f->services&&f->services->bind_frame)
    f->services->bind_frame(f->services->context,active?f:NULL);
}

int smn64_character_combat_entry(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,uint32_t address){
    int rc;bind(f,1);rc=combat_entry(c,f,address);bind(f,0);return rc;
}

int smn64_character_combat_dispatch(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_dispatch(c,f);bind(f,0);return rc;
}

int smn64_character_combat_retain(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_retain(c,f);bind(f,0);return rc;
}

int smn64_character_combat_damage(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,const SmN64CombatHit *h){
    int rc;bind(f,1);rc=combat_damage(c,f,h);bind(f,0);return rc;
}

int smn64_character_combat_die(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_die(c,f);bind(f,0);return rc;
}

int smn64_character_combat_post_tail(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_post_tail(c,f);bind(f,0);return rc;
}


int smn64_character_combat_land(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_land(c,f);bind(f,0);return rc;
}
int smn64_character_combat_hold(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_hold(c,f);bind(f,0);return rc;
}
int smn64_character_combat_carry_object(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,SmN64CarryObject *object){
    int rc;bind(f,1);rc=combat_carry_object(c,f,object);bind(f,0);return rc;
}
int smn64_character_combat_release_held(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f,const int32_t velocity[3],int smash){
    int rc;bind(f,1);rc=combat_release_held(c,f,velocity,smash);bind(f,0);return rc;
}

int smn64_character_combat_prepare(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_prepare(c,f);bind(f,0);return rc;
}
int smn64_character_combat_select_target(SmN64CharacterCombat *c,SmN64CombatOwnerFrame *f){
    int rc;bind(f,1);rc=combat_select_target(c,f);bind(f,0);return rc;
}
