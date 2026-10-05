#ifndef SMN64_WEB_ATTACK_EFFECTS_H
#define SMN64_WEB_ATTACK_EFFECTS_H
#include "../combat/fireweb_n64.h"
#include "../combat/web_debris_n64.h"
/* Bounded host storage, not the source global allocator's exhaustion behavior.
 * No pointers: copy this registry with the WHOLE pending character transaction.
 * Allocation failure aborts owner/RNG/actor effects, never silently drops them. */
#define SMN64_WEB_ATTACK_CAPACITY 512u
typedef struct SmN64ImpactBurst {
    int32_t position[3];
    int16_t size;
    uint8_t rgb[3],alive;
} SmN64ImpactBurst;
typedef struct SmN64ImpactDecal {
    int32_t corners[4][3];
    int16_t age;
    uint8_t phase,rgb[3],alive;
} SmN64ImpactDecal;
typedef struct SmN64ImpactSpark {
    int32_t position[3],previous[3],velocity[3];
    uint32_t previous_tick;
    uint16_t life;
    uint8_t alive;
} SmN64ImpactSpark;
typedef enum SmN64WebAttackKind {
    SMN64_ATTACK_PROJECTILE=1,SMN64_ATTACK_BURST,SMN64_ATTACK_DECAL,
    SMN64_ATTACK_FRAGMENT,SMN64_ATTACK_SPARK
} SmN64WebAttackKind;
typedef struct SmN64WebAttackObject {
    uint32_t id;
    SmN64WebAttackKind kind;
    union {
        SmN64ImpactWeb projectile;
        SmN64ImpactBurst burst;
        SmN64ImpactDecal decal;
        SmN64WebDebris fragment;
        SmN64ImpactSpark spark;
    } state;
    /* Actual constructor serial, not object ID or snapshot index. A fragment
     * reserves two: its parent uses this and its child uses this+1. */
    uint64_t graphical_serial;
} SmN64WebAttackObject;
typedef struct SmN64WebAttackEffects {
    uint32_t next_id,count;
    SmN64WebAttackObject objects[SMN64_WEB_ATTACK_CAPACITY]; /*newest first*/
    uint64_t graphical_clock; /* standalone fallback; shared by ordered APIs */
} SmN64WebAttackEffects;
typedef struct SmN64WebAttackHost {
    SmN64ImpactHost impact;
    void *context;
    /* B26F0 floor line: from burst point to +5000 fixed12 units on Y;
     * source environment query includes dynamic geometry. Return1 query made,
     * even on miss; hit.distance is unused. Missing query is a hard failure. */
    int (*fragment_floor)(void *,const int32_t from[3],const int32_t to[3],SmN64ImpactWorldHit *);
    /* Synchronous source sound boundary, staged in the same outer transaction.
     * Effect simulation must never queue unresolved/randomized commands. */
    int (*sound)(void *,uint32_t sound,const int32_t position[3]);
} SmN64WebAttackHost;
void smn64_web_attack_effects_init(SmN64WebAttackEffects *);
/* Enacts ONLY a normal impact FireWeb event. Resource failure is a real no-op;
 * unsupported special/trap/yank events return-2, never simulated acceptance.
 * Source FireWeb already consumed the finite shared resource before this call. */
int smn64_web_attack_fire(SmN64WebAttackEffects *,const SmN64FireEvent *,uint32_t tick,
    uint8_t suit,int32_t difficulty,uint32_t rng[3],const SmN64WebAttackHost *);
/* 9F780 source count4: marker1 only,4 particles. count2: marker1 then marker0,
 * 2 particles each. Positions are authored markers supplied by current owner;
 * forward is source F64, not display heading or a chosen aiming direction. */
int smn64_web_attack_sparks(SmN64WebAttackEffects *,uint32_t count,
    const int32_t hand0[3],const int32_t hand1[3],const int32_t forward[3],uint32_t tick,uint32_t rng[3]);
/* One source6701C graphical update then cleanup: sprites5534 first (new bursts
 * inserted there wait), then decals5540 (new decals update same pass), then
 * fragments/sparks5560 (new fragments update same pass). All are newest-first.
 * Projectile elapsed clock differs from once-per-effect burst/decal updates. */
int smn64_web_attack_effects_tick(SmN64WebAttackEffects *,uint32_t tick,uint32_t rng[3],const SmN64WebAttackHost *);
/* Import the shared clock synchronously, export only on success. No pointer is
 * retained. Failure can mutate the pending registry and RNG: discard the WHOLE
 * pending owner, effects, external host stages and clock together. */
int smn64_web_attack_fire_ordered(SmN64WebAttackEffects *,const SmN64FireEvent *,uint32_t tick,
    uint8_t suit,int32_t difficulty,uint32_t rng[3],const SmN64WebAttackHost *,uint64_t *shared_clock);
int smn64_web_attack_sparks_ordered(SmN64WebAttackEffects *,uint32_t count,
    const int32_t hand0[3],const int32_t hand1[3],const int32_t forward[3],uint32_t tick,
    uint32_t rng[3],uint64_t *shared_clock);
int smn64_web_attack_effects_tick_ordered(SmN64WebAttackEffects *,uint32_t tick,uint32_t rng[3],
    const SmN64WebAttackHost *,uint64_t *shared_clock);
/* Source6701C runs other graphical lists between5534 sprites and5540
 * polygons. The optional synchronous hook owns precisely that gap. It may
 * advance the shared allocation clock and RNG through its context, but must
 * not mutate this attack registry. Failure discards the entire pending frame.
 * The legacy wrapper above is exactly this API with no additional lists. */
typedef int (*SmN64WebAttackBetweenPasses)(void *context,uint64_t *shared_clock);
int smn64_web_attack_effects_tick_interleaved(SmN64WebAttackEffects *,uint32_t tick,
    uint32_t rng[3],const SmN64WebAttackHost *,uint64_t *shared_clock,
    SmN64WebAttackBetweenPasses,void *context);
/* Copy immutable live objects; renderer must not update, randomize or attach. */
int smn64_web_attack_snapshot(const SmN64WebAttackEffects *,SmN64WebAttackObject *,size_t capacity,size_t *count);
/* Independent complete numeric source owners; exposed for MIPS verification. */
int smn64_impact_burst_init(SmN64ImpactBurst *,const int32_t position[3]);
int smn64_impact_burst_tick(SmN64ImpactBurst *);
int smn64_impact_decal_init(SmN64ImpactDecal *,const int32_t position[3],const int16_t normal[3]);
int smn64_impact_decal_tick(SmN64ImpactDecal *);
int smn64_impact_spark_init(SmN64ImpactSpark *,const int32_t position[3],const int32_t forward[3],uint32_t tick,uint32_t rng[3]);
int smn64_impact_spark_tick(SmN64ImpactSpark *,uint32_t tick);
#endif
