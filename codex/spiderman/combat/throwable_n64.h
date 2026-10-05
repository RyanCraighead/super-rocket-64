#ifndef SMN64_THROWABLE_H
#define SMN64_THROWABLE_H
#include "combat_n64.h"

/* Original 8007EFF8, called by 8007F594 only while flags_10c bit 0 is set.
 * Positions/velocities are native fixed12, Y down. elapsed is source +84,
 * not a host-frame multiplier. All overflow follows MIPS word/halfword wrap. */
typedef struct SmN64Throwable {
    uint32_t id, flags_10c;
    int32_t position[3], velocity[3], acceleration[3];
    uint8_t drag[3], destroyed;
    int16_t angles[3], spin[3];
    uint16_t impact_count;
    int32_t elapsed;
    uint32_t path_handle, path_count, path_index;
    const int32_t (*path)[3];
    size_t path_capacity;
} SmN64Throwable;

/* Derived dispatch from private source table 80020650, types 0130..0144.
 * No authored dispatch table is bundled. Load only from an independently
 * revision/hash-verified 995056-byte boot. This loader checks layout/targets,
 * not cryptographic provenance; failure leaves the output unchanged. */
typedef struct SmN64ThrowableTypes { uint8_t damage[21], loaded; } SmN64ThrowableTypes;
int smn64_throwable_types_load(SmN64ThrowableTypes *,const void *boot,size_t bytes);

typedef struct SmN64ThrowableActor { uint32_t id; uint16_t type; } SmN64ThrowableActor;
typedef struct SmN64ThrowableWorldHit {
    uint32_t hit;
    int32_t position[3];
    int16_t normal[3];
} SmN64ThrowableWorldHit;
typedef struct SmN64ThrowableHost {
    void *context;
    /* Release exactly this path allocation at the source boundary. */
    int (*release_path)(void *,uint32_t path_handle);
    /* Original 49B40: ordered enemy list, flags=0, radius=6144. Return
     * 0 no contact, 1 actual contact, -1 unavailable. The first returned actor
     * ends this query even if its source type does not accept this packet. */
    int (*actor_sweep)(void *,const int32_t from[3],const int32_t to[3],
                       int32_t radius,SmN64ThrowableActor *);
    /* Original 4C0B0: environment=1, exclude owner id, actor-list=0,
     * final flag=1. Fill actual world result; return exactly 1 on success. */
    int (*world_sweep)(void *,uint32_t owner,const int32_t from[3],
                       const int32_t to[3],SmN64ThrowableWorldHit *);
    /* Source-defined packet fields are kind/flags/damage/direction/impulse/
     * duration. Position and hit_part are not populated by this source owner.
     * Return 0 rejected, 1 accepted, -1 unavailable; both 0 and 1 impact. */
    int (*apply)(void *,const SmN64CombatHit *);
    /* Source 6CCB8(global 800F5598, object position, 2), after spin draws and
     * only when impact_count==1. Must execute synchronously in this position. */
    int (*first_world_impact)(void *,const SmN64Throwable *);
    /* Exact 7E880 boundary. Its sound, ground query, radial explosion, debris,
     * shared RNG and script body remain host responsibilities. Do not report
     * success until these dependencies are genuinely handled. A successful
     * impact is followed immediately by the object's virtual destruction. */
    int (*impact)(void *,const SmN64Throwable *,const int16_t normal[3]);
    int (*destroy)(void *,uint32_t owner);
} SmN64ThrowableHost;
typedef struct SmN64ThrowableEvent {
    uint32_t contact; /* 0 none, 1 damageable actor, 2 world */
    uint32_t actor, path_released;
    int32_t direction[3], sweep_end[3];
    int16_t normal[3];
} SmN64ThrowableEvent;

/* Return 1 ongoing/inactive, 0 destroyed, -1 invalid input, -2 boundary failure.
 * A negative result is fail-stop: earlier callbacks, RNG and scalar state may
 * have committed. Do not retry without whole-owner/host rollback. Missing
 * queries never silently become no-hit results. */
int smn64_throwable_tick(SmN64Throwable *,const SmN64ThrowableTypes *,
    uint32_t rng[3],const SmN64ThrowableHost *,SmN64ThrowableEvent *);
#endif
