#ifndef SMN64_WEB_LIFECYCLE_H
#define SMN64_WEB_LIFECYCLE_H
#include "state_n64.h"
#include "strand_n64.h"
#define SMN64_WEB_VISUAL_CAPACITY 32
/* Source ordinary graphical actors. Capacity is explicit host storage policy;
 * exhaustion returns an error, never drops a source RNG consumer silently. */
typedef struct SmN64WebVisualStrand {
    SmN64Strand geometry;
    int32_t from[3],to[3],attachment_offset[3];
    int32_t released_close;
    uint16_t timer,age;
    uint8_t alive,kind,phase,has_geometry;
    /* line_rgb is actual vertex RGB; particle_rgb is source logical RGB.
     * Knot GPU RGB=min(255,2*logical), alpha is stored separately. */
    uint8_t line_rgb[3],particle_rgb[3],particle_alpha;
    uint32_t last_tick;
    uint8_t first_update;
} SmN64WebVisualStrand;
typedef struct SmN64WebSplat {
    int32_t center[3],axis_u[3],axis_v[3],corners[4][3];
    int32_t radius,target_radius;
    uint16_t rotation,age;
    uint8_t alive,drip,rgb[3];
} SmN64WebSplat;
typedef int (*SmN64WebVisualMarker)(void *,const SmN64WebPlayer *,unsigned,int32_t out[3]);
typedef struct SmN64WebVisuals {
    SmN64WebVisualStrand strands[SMN64_WEB_VISUAL_CAPACITY];
    SmN64WebSplat splats[SMN64_WEB_VISUAL_CAPACITY];
    int32_t active_swing,active_zip;
    int32_t camera[3]; /* original camera whole units, not fixed12 */
    SmN64WebVisualMarker marker;
    void *marker_context;
    int32_t strand_order[SMN64_WEB_VISUAL_CAPACITY],splat_order[SMN64_WEB_VISUAL_CAPACITY];
    uint32_t strand_count,splat_count; /* source newest-first effect-list order */
    /* Actual graphical allocations, independent of effect-list/slot order.
     * Primary line=base, knots=base+1+i, secondary line=base+count+1.
     * Empty zip shells have base0. Metadata survives fading and deletion;
     * slot reuse replaces it only at the next actual constructor. */
    uint64_t strand_graphical_base[SMN64_WEB_VISUAL_CAPACITY];
    uint64_t splat_graphical_serial[SMN64_WEB_VISUAL_CAPACITY];
    uint64_t graphical_clock; /* standalone fallback; shared by ordered API */
} SmN64WebVisuals;
enum {SMN64_VISUAL_SWING=1,SMN64_VISUAL_ZIP=2,SMN64_VISUAL_RELEASED=3};
void smn64_web_visuals_init(SmN64WebVisuals *,SmN64WebVisualMarker,void *);
/* Directly compatible with services.lifecycle. Context must be a PENDING visual
 * state copy. On a negative frame result the owner rolls that copy back too.
 * Only ordinary latched web_type0 is supported; fire graphics return-2. */
int smn64_web_visual_lifecycle(void *,SmN64WebEventKind,SmN64WebPlayer *,const SmN64Swinger *);
/* Same synchronous operation, with a shared transactional allocation clock.
 * Export only on success. Any failure requires the caller to discard the
 * pending visuals/player/RNG/clock together; this does not roll them back. */
int smn64_web_visual_lifecycle_ordered(void *,SmN64WebEventKind,SmN64WebPlayer *,
    const SmN64Swinger *,uint64_t *shared_clock);
/* Called after numeric swinger_step, before contacts/state: source AF8E4/B4068.
 * Does not consume RNG or step phase a second time. */
int smn64_web_visuals_swing_endpoint(SmN64WebVisuals *,const SmN64Swinger *);
/* Source93894..939C8 player tail. Re-evaluates authored marker0/1 at current pose,
 * preserves the source last-point attachment offset and updates live strands. */
int smn64_web_visuals_attach(SmN64WebVisuals *,const SmN64WebPlayer *);
/* Source AF254 actor-list phase, including full 82708 clock: first update2,
 * later wrapped tick difference capped6. Separate from effect-list geometry.
 * It must run in the source actor phase, not on each host display refresh. */
int smn64_web_visuals_zip_actors(SmN64WebVisuals *,uint32_t absolute_tick);
/* Source effect-list phase. effect_seconds is source float800FBF38, not display
 * FPS. Updates ordinary strand/released/splat effects once; no zip actor clock. */
int smn64_web_visuals_effects(SmN64WebVisuals *,const int32_t player_position[3],
                             uint32_t absolute_tick,float effect_seconds,
                             uint32_t shared_rng[3]);
/* Source AE318/62D78 numeric splat constructor and full AE4C0 effect update.
 * Rotation is already selected by the caller's single original RNG(4096).
 * Init includes the constructor's first update (radius16, age1). */
void smn64_web_splat_init(SmN64WebSplat *,const int32_t target[3],const int32_t normal[3],uint16_t rotation);
int smn64_web_splat_step(SmN64WebSplat *,float effect_seconds);
#endif
