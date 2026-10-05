#ifndef SM64_SPIDERMAN_WEB_RUNTIME_H
#define SM64_SPIDERMAN_WEB_RUNTIME_H
#include "gfx/spiderman_web_gl.h"
#include "../../codex/spiderman/web/lifecycle_n64.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure source submission adapter. No lifetime update, allocation, source RNG,
 * or mutation of gameplay input. Returns1 for a live snapshot,0 for inactive,
 * -1 invalid input; non-success leaves output unchanged. */
int spiderman_web_snapshot_splat(const SmN64WebSplat *,SpidermanWebQuad *);
/* Source-proven ordinary knot projector. native_camera_world is the original
 * final camera-to-world matrix; use the explicit host conversion below when
 * embedding it in a host camera. Returns1 or-1, never alters output on failure. */
int spiderman_web_snapshot_knot(const int32_t position_fixed12[3],
    const float native_camera_world[16],const uint8_t logical_rgb[3],
    uint8_t source_alpha,SpidermanWebQuad *);
int spiderman_web_camera_from_host_view(const float view[16],float native_camera_world[16]);
#ifdef __cplusplus
}
#endif
#endif
