#ifndef SMN64_CHARACTER_COMBAT_OWNER_H
#define SMN64_CHARACTER_COMBAT_OWNER_H
#include "../climbing/climbing_n64.h"
#include "../web/lifecycle_n64.h"
#include "../combat/entry_n64.h"
#include "../combat/combo_owner_n64.h"
#include "../combat/root_motion_n64.h"
#include "../combat/bone_world_n64.h"
#include "../combat/surface_attack_n64.h"
#include "../combat/air_interrupt_n64.h"
#include "../combat/web_action_n64.h"
#include "../combat/carry_n64.h"
#include "../combat/mounted_n64.h"
#include "../combat/damage_n64.h"
#include "../combat/hurt_owner_n64.h"
#include "../combat/effects_n64.h"

#define SMN64_COMBAT_EFFECT_CAPACITY 32u
#define SMN64_COMBAT_ACTOR_CAPACITY 256u
/* Entry-only result: consumed unavailable semantic request, no transition.
 * Caller must end this request group, then continue ordinary movement. */
#define SMN64_COMBAT_ENTRY_DECLINED 2
/* Immutable, already RNG-resolved native presentation commands. The parent
 * drains this bounded queue ONLY after its actor/world transaction commits.
 * Never reroll during rendering. Queue overflow fails the whole frame. */
typedef enum SmN64CombatEffectKind {
    SMN64_COMBAT_SOUND=1, SMN64_COMBAT_HIT_EFFECTS, SMN64_COMBAT_RESOURCE,
    SMN64_COMBAT_GAMEOVER
} SmN64CombatEffectKind;
typedef struct SmN64CombatEffect {
    SmN64CombatEffectKind kind;
    int32_t position[3];
    uint32_t sound;
    SmN64HitEffects hit;
    SmN64WebResourceEvent resource;
} SmN64CombatEffect;
typedef struct SmN64CharacterCombat {
    SmN64Combo combo;
    SmN64ComboOwner owner; /* target_actor is the shared selected+10C4 handle */
    SmN64CombatEntry entry;
    SmN64CombatRoot root;
    SmN64AirAttack air;
    SmN64WebAction web;
    SmN64Carry carry;
    SmN64MountedOwner mounted;
    SmN64DamageState damage;
    SmN64HurtOwner hurt;
    int32_t death_wait;
    uint32_t movement_enabled; /*shared sourceD1C, feed parent post-AI tail*/
    uint8_t suit;
    uint16_t graphic_hand;
    uint8_t loop_sound;
    uint32_t effect_count;
    SmN64CombatEffect effects[SMN64_COMBAT_EFFECT_CAPACITY];
    /* Presentation-only command declines for THIS committed frame. Not an
     * effect queue, source state, resource mutation or successful ability. */
    uint32_t unavailable_requested;
} SmN64CharacterCombat;

struct SmN64CombatOwnerFrame;
typedef struct SmN64CombatOwnerServices {
    const SmN64CombatBank *bank;
    SmN64EntryHost entry;
    SmN64ComboOwnerHost combo;
    SmN64RootHost root; /* refresh_pose/marker are supplied by this owner */
    SmN64CombatHost melee; /* bone/apply are wrapped by this owner */
    SmN64AirHost air;
    SmN64WebActionHost web; /* fire is composed here when fire_event is set */
    SmN64FireHost fire; /* bone is supplied by this owner */
    SmN64CarryHost carry;
    SmN64MountedHost mounted;
    SmN64DamageHost damage;
    SmN64HurtHost hurt;
    void *context;
    void (*bind_frame)(void *,const struct SmN64CombatOwnerFrame *); /*NULL on exit*/
    int (*release_stun)(void *); /*source78C68 actual+114 graphic destruction*/
    int (*hold_actor)(void *,uint32_t actor,const int32_t position[3],uint16_t yaw);
    int (*pose)(void *,int clip,int frame,int16_t out[216]);
    /* All actor arrays are actual source-order snapshots, never invented hits.
     * Must return1 and count<=capacity; overflow/unavailable is a hard failure. */
    int (*actors)(void *,SmN64CombatActor *out,size_t capacity,size_t *count);
    /* *id is the ALREADY selected+10C4 handle on entry. Resolve lifetime/type/
     * position only, never reselect. 1 found,0 expired,negative unavailable. */
    int (*target)(void *,uint32_t *id,uint16_t *type,int32_t position[3]);
    /* Apply source facing BEFORE combo-start RNG on every begin. Event heading
     * is absolute. Other entry requests are descriptive, not fake acceptance. */
    int (*entry_event)(void *,SmN64ClimbState *,const SmN64EntryEvent *);
    /* Entry face_target callbacks see PRE-entry actor.state/anim/adhered.
     * For surface entry: flat-normal basis first, clear adhered, then timed
     * target turn. New action state/clip commit afterward. Invalid basis must
     * fail explicitly. Pursuit/landing callbacks see current state as usual.
     * Swing lifecycle is already composed here; stage remaining actual effects. */
    int (*air_event)(void *,SmN64ClimbState *,SmN64AirAttack *,const SmN64AirEvent *);
    /* Actual dome allocation/release and grab/target semantics. Resource/audio
     * events are queued here; ability_event must handle gameplay flags only. */
    int (*ability_event)(void *,SmN64ClimbState *,SmN64CharacterCombat *,const SmN64WebAbilityEvent *);
    /* Synchronous actual FireWeb actor/graphic/effect boundary. Must enact
     * rejected-yank sheet release BEFORE graphic release, honor real recipients,
     * and consume any source actor-effect RNG through the supplied shared rng.
     * This never equates actor message5/6 with generic damage or stun. */
    int (*fire_event)(void *,SmN64WebAction *,const SmN64FireEvent *,uint32_t rng[3]);
    int (*damage_event)(void *,SmN64ClimbState *,SmN64CharacterCombat *,const SmN64DamageEvent *);
    /* Additive services: keep all previously shipped member offsets stable. */
    SmN64AirInterruptHost air_interrupt; /*exact START/ALIGN_FLAT/STOP phases*/
    /* Actual first source6B4 handle presence in the pending trail registry.
     * Read-only; exactly1 success and *present must be0/1. Queried only when the
     * recent-damage prelude needs it, never replaced by a synthetic flag. */
    int (*first_trail_present)(void *,uint32_t *present);
    /* Explicit implementation capability policy, constant during an action.
     * Zero preserves all original APIs. Never infer unavailable from a host
     * error, failed resource check, geometry query or missing actor. */
    uint32_t unavailable_commands;
    /* Source9ACB0 lost-ground Drop or91F7C obstacle Smash. The supplied
     * velocity is the exact source release vector (unused for smash). Return1
     * only after staging the real object operation in the pending transaction.
     * An expired handle is resolved and cleared before this callback. */
    int (*carry_release)(void *,uint32_t actor,const int32_t velocity[3],int smash);
    /* Optional source-ordered Throw/Drop body before its one spin RNG draw.
     * Uses carry.context, and the actual shared actor RNG. */
    SmN64CarryOrderedThrow carry_throw_ordered;
} SmN64CombatOwnerServices;

typedef struct SmN64CombatOwnerFrame {
    SmN64ClimbState *actor;
    SmN64WebResource *resource; /* MUST be &traversal->player.resource */
    SmN64WebRuntime *traversal; /*SAME pending shared fields660/664/111C/A38/A44*/
    SmN64WebVisuals *traversal_visuals; /*SAME pending actual swing/zip registry*/
    uint32_t *pressed; /* source logical bits0..3; consumed in source order */
    SmN64WebButtons held;
    uint32_t tick;
    uint16_t completed_animation; /* generic successor result, or0xffff */
    const uint16_t *counts;size_t count;
    const SmN64Marker *markers;size_t marker_count;
    int16_t *retained_pose; /*216 original signed16 cells, updated by root owner*/
    int32_t *body_translation; /*3 retained native whole units*/
    int32_t previous_position[3]; /*before this frame's source physics*/
    const SmN64CombatOwnerServices *services;
    uint64_t *graphical_clock; /* same pending character allocation clock */
} SmN64CombatOwnerFrame;

void smn64_character_combat_init(SmN64CharacterCombat *,int16_t health,uint8_t suit);
void smn64_character_combat_begin_frame(SmN64CharacterCombat *); /*clear queue once*/
/* Original8CBB0–8CC14 invulnerability/display/stun clocks: once at player-AI
 * start, before inventory. Expired stun graphic destruction is synchronous. */
int smn64_character_combat_prepare(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
/* Original9E3AC plus8D4DC fall timer: after basis/drag, BEFORE input/ramp
 * and generic successor. Aiming preserves target but still ticks fall timer. */
int smn64_character_combat_select_target(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
int smn64_character_combat_active(uint32_t state);
/* Verified combat/hurt subset of sorted originalF74AC. Return completed clip,
 * 65535 unchanged, negative invalid. Run once BEFORE state dispatch, including
 * recovery clips whose current state has already become ordinary. */
int smn64_character_combat_successor(SmN64ClimbState *,const uint16_t *,size_t);
/* No animation, physics, input, successor or AI-tail pass is duplicated here.
 * Caller supplies a pending WHOLE-character transaction. Every mutating host
 * callback must stage changes against the same transaction, with actual actor
 * acceptance/allocation replies. Negative return requires ABORT of all staged
 * actors, inventory, graphics, RNG, pose and effect queue; never local retry.
 * Host enumeration itself must be finite (max256 actors is owner policy).
 * Missing host support fails explicitly before successful publication. */
int smn64_character_combat_entry(SmN64CharacterCombat *,SmN64CombatOwnerFrame *,uint32_t source_address);
int smn64_character_combat_dispatch(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
/* Source92DAC air contacts: AFTER common AI tail, BEFORE final pose refresh. */
int smn64_character_combat_post_tail(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
/* Run AFTER final source AI-tail body/pose refresh. Does not refresh itself. */
int smn64_character_combat_retain(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
int smn64_character_combat_damage(SmN64CharacterCombat *,SmN64CombatOwnerFrame *,const SmN64CombatHit *);
int smn64_character_combat_die(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
int smn64_character_combat_land(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
/* Original939CC: after finalpose, root retain, and web endpoint attachments. */
int smn64_character_combat_hold(SmN64CharacterCombat *,SmN64CombatOwnerFrame *);
/* Resolve the authoritative held handle through carry.object: 1 live object,
 * 0 no/expired object, negative unavailable or malformed. Expiry clears only
 * owning aliases, with no invented throw/drop physics. Every call binds the
 * pending frame just like the other public combat boundaries. */
int smn64_character_combat_carry_object(SmN64CharacterCombat *,SmN64CombatOwnerFrame *,SmN64CarryObject *);
int smn64_character_combat_release_held(SmN64CharacterCombat *,SmN64CombatOwnerFrame *,const int32_t velocity[3],int smash);
#endif
