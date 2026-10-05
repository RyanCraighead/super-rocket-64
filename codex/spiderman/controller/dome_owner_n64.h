#ifndef SMN64_DOME_OWNER_H
#define SMN64_DOME_OWNER_H
#include "../combat/dome_actor_n64.h"
#define SMN64_DOME_HELD_CAPACITY 4u
#define SMN64_DOME_PULSE_CAPACITY 16u
#define SMN64_DOME_IMPACT_CAPACITY 256u
#define SMN64_DOME_BODY_CAPACITY 32u
#define SMN64_DOME_POOL_COUNT 7u
/* 226/0,248/0,249/0..4. Pools must be decoded from verified original assets.
 * All bodies bound to a model/node share its one authoritative mutable pool. */
typedef struct SmN64DomeOwnerPool {
    uint16_t slot,node; size_t count;
    SmN64DomeVertex vertices[SMN64_DOME_RING_CAPACITY];
} SmN64DomeOwnerPool;
typedef struct SmN64DomeOwnerRecord {
    uint32_t id; uint16_t slot,node,render_slot; uint8_t attached,buffers;
    uint64_t misc_serial,render_serial;
    float render_translation[3];
} SmN64DomeOwnerRecord;
typedef struct SmN64DomeOwnerGraphic {
    uint32_t id; uint64_t serial; SmN64DomePulse pulse;
} SmN64DomeOwnerGraphic;
typedef struct SmN64DomeOwnerImpact {
    uint32_t id; uint64_t serial; SmN64DomeImpact impact;
} SmN64DomeOwnerImpact;
typedef struct SmN64DomeOwner {
    SmN64HeldDome held[SMN64_DOME_HELD_CAPACITY];
    SmN64DomeRing rings[2]; SmN64DomeWorld world;
    SmN64DomeOwnerRecord records[SMN64_DOME_BODY_CAPACITY];
    SmN64DomeOwnerGraphic pulses[SMN64_DOME_PULSE_CAPACITY];
    SmN64DomeOwnerImpact impacts[SMN64_DOME_IMPACT_CAPACITY];
    SmN64DomeOwnerPool pools[SMN64_DOME_POOL_COUNT];
    uint32_t next_id,player_generation; uint64_t misc_clock;
    uint16_t render_count,render_free_count,render_free[SMN64_DOME_BODY_CAPACITY];
    uint8_t poisoned;
} SmN64DomeOwner;
typedef struct SmN64DomeOwnerHost {
    void *context;
    int (*actor_at)(void *,uint32_t list,size_t,SmN64DomeActor *);
    int (*mark_hit)(void *,uint32_t actor,uint8_t set);
    int (*begin_pulse)(void *); /* native dormant-lifetime once-hit reset */
    int (*apply)(void *,const SmN64CombatHit *);
    /* Exact82F04->B4B6C/58560 triangle shatter. This is NOT a fade/no-op.
     * Caller stages original geometry, floor query, effects and shared RNG. */
    int (*shatter)(void *,const SmN64DomeBody *,uint16_t slot,uint16_t node,
        const float cached_render_translation[3],uint32_t rng[3],uint64_t *graphical_clock);
    /* Explicit native camera adaptation, staged; never publish precommit. */
    int (*shake)(void *,uint32_t kind,const int32_t position[3]);
} SmN64DomeOwnerHost;
typedef struct SmN64DomeOwnerInstance {
    SmN64DomeBody body; uint16_t model_slot,node; uint64_t graphical_serial;
    const SmN64DomeVertex *current_pool; size_t current_count;
} SmN64DomeOwnerInstance;
int smn64_dome_owner_init(SmN64DomeOwner *,const SmN64DomeOwnerPool pools[SMN64_DOME_POOL_COUNT]);
/* REQUIRED for whole-owner copies: rebind world.ring and shared mutable pools.
 * Source/destination may alias. Never retain a pending pointer after commit. */
int smn64_dome_owner_copy(SmN64DomeOwner *,const SmN64DomeOwner *);
int smn64_dome_owner_create(SmN64DomeOwner *,const SmN64DomePlayer *,uint32_t web_type,
    uint32_t rng[3],uint64_t *,const SmN64DomeOwnerHost *);
int smn64_dome_owner_release(SmN64DomeOwner *,const SmN64DomePlayer *,int32_t vertical_delta,
    uint32_t rng[3],uint64_t *,const SmN64DomeOwnerHost *);
/*823D4 misc list pass after player AI, before6701C. Head insertion during a
 * callback waits next pass. Death40 promotes80 next pass, destroys the next.
 * present0 resolves a genuinely lost generation and releases immediately.
 * interrupted1 releases the held dome as an explicit native interrupt policy. */
int smn64_dome_owner_misc(SmN64DomeOwner *,const SmN64DomePlayer *,int present,int interrupted,
    int32_t vertical_delta,uint32_t rng[3],uint64_t *,const SmN64DomeOwnerHost *);
/*6701C lists554C then5530. Call AFTER sprites5534, BEFORE decals5540.
 * Pulse-created impacts enter already-visited554C and wait until next frame. */
int smn64_dome_owner_graphics(SmN64DomeOwner *,uint32_t rng[3],uint64_t *,const SmN64DomeOwnerHost *);
/* Original57818 cache update at committed scene preparation, not simulation. */
void smn64_dome_owner_render_cache(SmN64DomeOwner *);
int smn64_dome_owner_validate(const SmN64DomeOwner *);
/* Source render table scans ascending fixed slots. Free slots are reused LIFO;
 * graphical_serial records construction identity and is NOT the draw order. */
int smn64_dome_owner_snapshot(const SmN64DomeOwner *,SmN64DomeOwnerInstance *,size_t,size_t *);
#endif
