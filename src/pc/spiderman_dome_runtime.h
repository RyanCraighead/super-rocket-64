#ifndef SM64_SPIDERMAN_DOME_RUNTIME_H
#define SM64_SPIDERMAN_DOME_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "../../codex/spiderman/combat/dome_actor_n64.h"
#include "../../codex/spiderman/dome_render/dome_mesh_n64.h"
#include "../../codex/spiderman/dome_render/dome_material_n64.h"
#define SPIDERMAN_DOME_SCENE_CAPACITY 32u
typedef struct SpidermanDomeAssets SpidermanDomeAssets;
typedef struct SpidermanDomeInstance {
    SmN64DomeBody body;
    uint16_t model_slot,node;
    uint64_t graphical_serial;
    const SmN64DomeVertex *current_pool;
    size_t current_count;
} SpidermanDomeInstance;
/* Pointer-free, current-pool face expansion. Source instance flags0x0001 hides
 * the held base until its recovered lifecycle clears that bit. */
typedef struct SpidermanDomeSnapshot {
    SmN64DomeDraw draw;
    uint64_t graphical_serial;
    uint8_t logical_rgb[3],instance_bc;
} SpidermanDomeSnapshot;
/* Source RDP owner boundary, independent of borrowed host GL state. Frame-end
 * is called only at a source-frame boundary; draw/submit never reset the guard.
 * If another source material selects, update/invalidate the texture cache here.
 * Global105034 override and reversed global101678 culling remain unsupported. */
typedef struct SpidermanDomeRenderState {
    smn64_dome_scroll_state scroll404;
    float source_delta_seconds;
    uint16_t texture_slot;
    uint8_t texture_cache_valid;
    uint8_t environment_rgba[4],fog_rgba[4];
    uint32_t render_mode;
} SpidermanDomeRenderState;
typedef struct SpidermanDomePacket {
    SmN64DomeDraw draw;
    smn64_dome_material material;
} SpidermanDomePacket;
SpidermanDomeAssets *spiderman_dome_assets_create(void);
int spiderman_dome_assets_load(SpidermanDomeAssets *,const char *directory);
int spiderman_dome_assets_ready(const SpidermanDomeAssets *);
const char *spiderman_dome_assets_error(const SpidermanDomeAssets *);
void spiderman_dome_assets_destroy(SpidermanDomeAssets *);
/* Copies exact host-endian source template. Output/count untouched on failure.
 * Owner must make its own authoritative mutable pool, then give that same pool
 * to synchronous51174 and snapshot capture. Only226,248,249 are accepted. */
int spiderman_dome_assets_copy_pool(const SpidermanDomeAssets *,uint16_t model_slot,
    uint16_t node,SmN64DomeVertex *out,size_t capacity,size_t *count);
int spiderman_dome_assets_copy_geometry(const SpidermanDomeAssets *,uint16_t model_slot,
    uint16_t node,SmN64DomeGeometry *out);
int spiderman_dome_assets_copy_texture(const SpidermanDomeAssets *,uint16_t slot,
    uint8_t *out,size_t capacity);
/* Return1 visible,0 hidden/dead,-1 invalid. Output unchanged unless1. Neither
 * simulation, resource ownership, mesh mutation nor source time advances. */
int spiderman_dome_snapshot(const SpidermanDomeAssets *,const SpidermanDomeInstance *,
    SpidermanDomeSnapshot *out);
/* One ordered source draw. Actual selection (cache miss) emits white ENV and
 * advances404 at most once per source frame; actorENV then overwrites it. BC254
 * emits fog/mode and invalidates the texture cache. Transactional outputs. */
int spiderman_dome_prepare(const SpidermanDomeSnapshot *,
    const SpidermanDomeRenderState *before,SpidermanDomePacket *packet,
    SpidermanDomeRenderState *after);
#ifdef __cplusplus
}
#endif
#endif
