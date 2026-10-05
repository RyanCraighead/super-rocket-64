#ifndef SMN64_DOME_ACTOR_N64_H
#define SMN64_DOME_ACTOR_N64_H
#include "dome_pulse_n64.h"
#include <stddef.h>
#include <stdint.h>

/* Source vertex pool: xyz,s16 flag,st,RGBA (16 bytes). Mutable ring vertices
 * must be the actual shared 51174 pool, not a substitute circle. */
typedef struct SmN64DomeVertex { int16_t x,y,z,flag,s,t; uint8_t rgba[4]; } SmN64DomeVertex;
typedef struct SmN64DomeMesh { SmN64DomeVertex *vertices; size_t count; } SmN64DomeMesh;
typedef struct SmN64DomeBody {
    uint32_t id,render,rgb,body_flags;
    int32_t position[3];
    uint16_t flags,model;
    int16_t scale[3];
    uint8_t alive,poisoned,render_bc,render_bd;
} SmN64DomeBody;
typedef struct SmN64DomePlayer {
    uint32_t id; int32_t position[3]; uint16_t offset_11a0; int16_t frame;
} SmN64DomePlayer;
typedef struct SmN64DomePiece { SmN64DomeBody body; int32_t delay,fade_step; } SmN64DomePiece;
typedef struct SmN64HeldDome {
    SmN64DomeBody body; uint32_t player,generation,web_type; int32_t phase;
    SmN64DomeMesh mesh; SmN64DomePiece pieces[5];
} SmN64HeldDome;
#define SMN64_DOME_RING_CAPACITY 512
typedef struct SmN64DomeRing {
    SmN64DomeBody body; uint32_t web_type; int32_t speed,fade_step;
    SmN64DomeMesh mesh; size_t saved_count;
    int16_t saved[SMN64_DOME_RING_CAPACITY][3]; /* x, original alpha, z */
    int16_t velocity[SMN64_DOME_RING_CAPACITY][3];
} SmN64DomeRing;
typedef struct SmN64DomeWorld {
    uint32_t dome_count,fire_count; SmN64DomeRing *ring;
} SmN64DomeWorld;

/* Each callback is a synchronous service boundary, in the listed source order.
 * Return exactly 1 or the owner becomes poisoned and MUST NOT be retried.
 * Whole-owner rollback is the host's responsibility. No default renderer,
 * fake actor, deferred RNG, silent allocation omission or no-op effect exists.
 * ALLOC_BODY supplies body.id; BIND_RENDER supplies body.render.
 * MAKE_HANDLE supplies args[0:2]; GET_MESH supplies mesh. */
enum SmN64DomeOperation {
    SMN64_DOME_ALLOC_BODY=1, SMN64_DOME_MAKE_HANDLE, SMN64_DOME_INIT_ITEM,
    SMN64_DOME_LOOKUP_BUNDLE, SMN64_DOME_LOOKUP_MODEL, SMN64_DOME_BIND_RENDER,
    SMN64_DOME_ATTACH_MISC, SMN64_DOME_GET_MESH, SMN64_DOME_FIRE_SCRIPT,
    SMN64_DOME_FADE_30, SMN64_DOME_ALLOC_PULSE, SMN64_DOME_SHAKE,
    SMN64_DOME_BODY_DIE, SMN64_DOME_RELEASE_RENDERS, SMN64_DOME_DETACH_MISC,
    SMN64_DOME_BASE_DESTROY, SMN64_DOME_FREE_BODY, SMN64_DOME_ALLOC_BUFFER,
    SMN64_DOME_FREE_BUFFER, SMN64_DOME_ALLOC_IMPACT, SMN64_DOME_PUBLISH_IMPACT
};
typedef struct SmN64DomeCall {
    uint32_t source,operation,args[6]; const char *name;
    int32_t position[3]; SmN64DomeMesh mesh;
} SmN64DomeCall;
typedef struct SmN64DomeActorHost {
    void *context;
    int (*service)(void *,SmN64DomeBody *,SmN64DomeCall *);
    /* Exact generation-checked 7FB08 resolution. Return0 for genuinely dead,
     * -1 for unavailable; a missing service is NOT a lost-player event. */
    int (*player)(void *,uint32_t,uint32_t,SmN64DomePlayer *);
    /* Global actor 800E8744 +8 minus +EC, sampled at the call site. */
    int (*vertical_delta)(void *,int32_t *);
} SmN64DomeActorHost;
int smn64_dome_actor_init(SmN64HeldDome *,SmN64DomeWorld *,const SmN64DomePlayer *,uint32_t,const SmN64DomeActorHost *);
/* Tick returns2 if the source's lost-player release must run now. Call release
 * immediately in the same owner transaction, before another actor/RNG call. */
int smn64_dome_actor_tick(SmN64HeldDome *,uint32_t rng[3],const SmN64DomeActorHost *);
int smn64_dome_piece_tick(SmN64DomePiece *,const SmN64DomeActorHost *);
int smn64_dome_actor_release(SmN64HeldDome *,SmN64DomeWorld *,SmN64DomeRing *,SmN64DomePulse *,uint32_t rng[3],const int32_t player_position[3],const SmN64DomeHost *,const SmN64DomeActorHost *);
int smn64_dome_actor_destroy(SmN64HeldDome *,SmN64DomeWorld *,uint8_t free_storage,const SmN64DomeActorHost *);
int smn64_dome_ring_init(SmN64DomeRing *,SmN64DomeWorld *,const int32_t position[3],uint32_t,const SmN64DomeActorHost *);
int smn64_dome_ring_tick(SmN64DomeRing *,const SmN64DomeActorHost *);
int smn64_dome_ring_destroy(SmN64DomeRing *,SmN64DomeWorld *,uint8_t free_storage,const SmN64DomeActorHost *);
/* Exposed numeric kernels permit unchanged-ROM checks with actual vertex pools. */
int smn64_dome_ring_capture(SmN64DomeRing *);
int smn64_dome_ring_restore(SmN64DomeRing *);

/* Exact dome branch of 72298: 5 spokes =>10 radial samples. Ten Rnd(0)
 * calls are required even though all radii are70. Source64454 attaches this to F554C. N64 69124 is a no-op: packed
 * center/point RGB stays zero although gradient fields contain128. The first
 * ordinary74CA0 tick marks it dead. Do not add a made-up visible spark. */
typedef struct SmN64DomeImpact {
    int32_t position[3],radius[10],outer_radius[10];
    uint32_t point_rgb[10],outer_rgb[10],center_rgb; float bound;
    uint8_t gradient_rgb[6],texture_alpha;
    uint16_t angle_step; uint8_t alive;
} SmN64DomeImpact;
int smn64_dome_impact_init(SmN64DomeImpact *,const int32_t position[3],uint32_t rng[3]);
int smn64_dome_impact_tick(SmN64DomeImpact *);
#endif
