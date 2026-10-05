#ifndef SM64_SPIDERMAN_DOME_SCENE_H
#define SM64_SPIDERMAN_DOME_SCENE_H
#include "spiderman_dome_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
int spiderman_dome_scene_init(const char *original_dome_asset_directory);
int spiderman_dome_scene_ready(void);
int spiderman_dome_scene_copy_pool(uint16_t model_slot,uint16_t node,
    SmN64DomeVertex *out,size_t capacity,size_t *count);
int spiderman_dome_scene_copy_geometry(uint16_t model_slot,uint16_t node,SmN64DomeGeometry *out);
void spiderman_dome_scene_shutdown(void);
void spiderman_dome_scene_suspend(void);
/* Caller supplies exact source transparent-pass draw order; no distance sort.
 * Unique serials are verified, not reinterpreted as another source list. Copies
 * current source pools after the complete owner transaction commits. Failure
 * hides the old snapshot. NULL instances allowed for zero count. */
int spiderman_dome_scene_submit(const SpidermanDomeInstance *,size_t count,uint32_t source_tick);
/* In/out source render state is published only after complete success. Call at
 * the recovered transparent56CAC pass boundary and carry resultingENV to any
 * later ENV-consuming effects. Never independently draw at arbitrary end-frame.
 * Caller calls scroll_frame_end once per source frame, never per host display. */
int spiderman_dome_scene_draw(const float view[16],const float projection[16],
    const int viewport[4],SpidermanDomeRenderState *);
const char *spiderman_dome_scene_status(void);
unsigned spiderman_dome_scene_drawn_triangles(void);
typedef struct SpidermanDomeDrawCounts {
    uint32_t source_tick,held_bodies,piece_bodies,ring_bodies,triangles;
} SpidermanDomeDrawCounts;
/* Counts of submitted primitives, not framebuffer pixels. Zero on failure,
 * submit or suspend. Shatter counts belong to the combined F5540 effect scene. */
int spiderman_dome_scene_draw_counts(SpidermanDomeDrawCounts *);
#ifdef SPIDERMAN_TESTING
int spiderman_dome_scene_test_snapshot(SpidermanDomeSnapshot *,size_t capacity,size_t *count,uint32_t *tick);
#endif
#ifdef __cplusplus
}
#endif
#endif
