#ifndef SM64_SPIDERMAN_COMBAT_HOST_H
#define SM64_SPIDERMAN_COMBAT_HOST_H
#include "../../codex/spiderman/controller/combat_owner_n64.h"
#include "../../codex/spiderman/combat/throwable_n64.h"
#include "../../codex/spiderman/combat/trails_n64.h"
#include "../../codex/spiderman/combat/dome_pulse_n64.h"
struct MarioState;
struct Object;
/* The private directory must contain exact, revision/hash-verified combat.bin.
 * No ROM tables are embedded. Initialization performs bounded regular-file I/O. */
typedef struct SpidermanCarrySound { uint32_t sound;int32_t position[3]; } SpidermanCarrySound;
typedef struct SpidermanCarrySnapshot {
    uint32_t held_actor,flight_count,sound_count;
    int32_t held_position[3];uint8_t alpha;
} SpidermanCarrySnapshot;
/* Bounded explicit SM64 small-box profile, only after presentation is installed. */
int spiderman_combat_host_carry_enable(void);
int spiderman_combat_host_carry_tick(SmN64CombatOwnerFrame *,SmN64CharacterCombat *);
int spiderman_combat_host_carry_snapshot(SpidermanCarrySnapshot *);
int spiderman_combat_host_object_alpha(const struct Object *,uint8_t *);
int spiderman_combat_host_init(const char *asset_directory);
void spiderman_combat_host_shutdown(void);
/* Clear actor/level epoch and release any captured native box, keeping the
 * verified immutable bank and throwable type table loaded. */
void spiderman_combat_host_reset_actor(void);
const char *spiderman_combat_host_error(void);
/* One main-thread host transaction for the WHOLE source tick. begin snapshots
 * real object-list order; no callback mutates native actors. bind must receive
 * the CURRENT pending source frame before each owner call (never a committed
 * actor pointer). commit validates EVERY target before publishing ANY write.
 * On any failure abort both source and host transaction. No local retry. */
int spiderman_combat_host_begin(struct MarioState *,const SmN64ClimbState *);
int spiderman_combat_host_bind(const SmN64CombatOwnerFrame *);
const SmN64CombatOwnerServices *spiderman_combat_host_services(void);
int spiderman_combat_host_commit(void);
/* Explicit selection utility only. services.target receives the existing
 * source-maintained player10C4 handle in *id and performs lifetime lookup. */
int spiderman_combat_host_select_target(const SmN64ComboOwner *,int32_t max_distance,
    int32_t min_facing,int32_t distance_weight,int32_t facing_weight,uint32_t *actor);
void spiderman_combat_host_abort(void);
/* Explicit cross-game recipient contract. Source packet is retained unchanged;
 * accepted packets enqueue vetted SM64 behavior-mailbox consequences. Unsupported
 * behavior/expired handle/immune actor returns0, unavailable transaction <0.
 * Neversoft message5/6 are NEVER passed to this damage operation. */
int spiderman_combat_host_damage(const SmN64CombatHit *);
/* Ordinary dome recipient mapping: native vetted enemies in source list0,
 * native small boxes in list1, preserving their captured native list order.
 * 0x10 is current native eligibility; 0x100 is the source pulse's once-hit bit.
 * Marks are lifetime-keyed, staged with the complete combat transaction and
 * reset by original pulse initialization. Enumeration never invents actors. */
int spiderman_combat_host_dome_actor_at(uint32_t list,size_t index,SmN64DomeActor *);
int spiderman_combat_host_dome_mark_hit(uint32_t actor,uint8_t set);
/* Original pulse initialization clears both complete actor lists. The native
 * epoch reset additionally covers extant dormant/off-room lifetimes omitted
 * from collision enumeration. Must run once BEFORE each new pulse starts. */
int spiderman_combat_host_dome_reset_hits(void);
/* AA8DC/49B40 impact-only policy: actual native hurt-cylinder intersections,
 * radius4096, then minimum source float32 projected actor-center distance.
 * Equal projections replace earlier actors. No world-visibility query. Native
 * intangible/held/explosion roles map to source geometric exclusion; staged
 * damage eligibility/interaction status never makes a real collider disappear.
 * Returns1 geometric actor,0 none,negative unavailable; no target selection. */
int spiderman_combat_host_impact_sweep(const int32_t from[3],const int32_t to[3],uint32_t *actor);
int spiderman_combat_host_web_message(uint32_t actor,uint32_t message);
/* Incoming contact bridge, called only while Spider-Man owns this player,
 * BEFORE Mario damage/action mutation. Explicit cross-game policy: one native
 * health wedge maps to ceil(100/8)=13 source health; multi-wedge packets map
 * ceil(wedges*100/8), capped100. Source suit/armor/invulnerability still decides.
 * 1 queued/deduplicated;0unsupported actor;negative malformed/capacity. */
int spiderman_combat_host_queue_incoming(struct MarioState *,struct Object *);
/* Read-only native interaction arbitration for the same vetted actor roles.
 * While Spider-Man owns the player, suppress Mario stomp/grab/attack decisions
 * for these actors and let the source owner decide. Coins/doors/stars and all
 * unknown behaviors remain outside this predicate. No query creates a target. */
int spiderman_combat_host_owns_interaction(const struct Object *);
/* During pending tick enumerate queued packets then run source damage intake.
 * Report actual intake result through incoming_result, which stages native
 * ATTACKED_MARIO consequence only for source acceptance. Queue clears on commit,
 * remains on abort, and clears on shutdown. No direct Mario health/action writes. */
int spiderman_combat_host_incoming(size_t index,SmN64CombatHit *);
int spiderman_combat_host_incoming_result(size_t index,int accepted);
#define SPIDERMAN_HOST_TRAIL_CAPACITY 32u
/* Enable only after the actual source-material renderer is ready. The color is
 * the explicit source player+6B0 value; no substitute white color is invented.
 * Default initialization keeps trails gated. Epoch reset keeps this capability
 * while clearing every owned active/released graphical lifetime. */
int spiderman_combat_host_trails_enable(uint32_t source_player_color);
/* Source93344 final-pose point: AFTER retained melee anchors, BEFORE web endpoint
 * and held-object attachment. Samples authored markers5/6 without refreshing or
 * advancing pose. Must be called on the same pending whole-character frame. */
int spiderman_combat_host_trails_retain(const SmN64ClimbState *,
    const SmN64Marker *,size_t marker_count,const int16_t retained_pose[216],
    const int32_t body_translation[3],uint32_t source_tick);
/* One source7D518 effect update, then7D520 cleanup, after source actor updates.
 * Independent of render refresh and player elapsed; never consumes RNG. */
int spiderman_combat_host_trails_effects(uint32_t source_tick);
/* Immutable committed source objects, newest allocation first. No stepping,
 * attachment, random draws or native writes occur while taking a snapshot. */
int spiderman_combat_host_trails_snapshot(SmN64Trail *,size_t capacity,size_t *count);
/* Original throwable motion can be composed without enabling incomplete pickup
 * entry. Caller supplies SOURCE-backed object constructor fields and actual
 * synchronous source effect/script callbacks, including shared-RNG ordering.
 * Missing callbacks reject launch before the native actor is captured. */
typedef struct SpidermanThrowableEffects {
    void *context;
    int (*first_world_impact)(void *,const SmN64Throwable *);
    int (*impact)(void *,const SmN64Throwable *,const int16_t normal[3],uint32_t rng[3]);
} SpidermanThrowableEffects;
int spiderman_combat_host_throw_begin(const SmN64ThrowRequest *,
    const SmN64Throwable *source_constructor,const SpidermanThrowableEffects *);
int spiderman_combat_host_throwables_tick(int32_t elapsed,uint32_t shared_rng[3],
    const SpidermanThrowableEffects *);
/* Native boxes remain HELD_HELD while original physics owns them; invoke after
 * native object updates, before scene traversal, to show the original host mesh
 * at committed source coordinates. Never advances simulation or RNG. */
void spiderman_combat_host_present_objects(void);
/* Last committed accepted/rejected geometric source packet, for diagnostics. */
int spiderman_combat_host_last_hit(SmN64CombatHit *,int *accepted);
#endif
