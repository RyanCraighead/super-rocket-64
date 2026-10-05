#ifndef SM64_SPIDERMAN_TRAIL_RUNTIME_H
#define SM64_SPIDERMAN_TRAIL_RUNTIME_H
#include <stddef.h>
#include "gfx/spiderman_web_gl.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "../../codex/spiderman/combat/trails_n64.h"
/* Pure 8006511C submission producer. Borrow a committed source trail and the
 * original camera+60 matrix (column-major). Use the existing explicit
 * spiderman_web_camera_from_host_view conversion when embedding a host camera.
 * Copies exact connected vertices/colors/UVs after source float32 arithmetic,
 * truncation and s16 narrowing. Does not step, allocate, choose width or use RNG.
 * out has capacity4. Returns1 valid live snapshot,0 deleted,-1 invalid; count
 * and output remain unchanged on non-success. Degenerate segments are skipped
 * exactly by source length<.001 and reset the chain. A live snapshot may count0.
 * Original global segment list draws newest allocated trail first. */
int spiderman_trail_snapshot(const SmN64Trail *,const float native_camera_world[16],
    SpidermanWebQuad out[4],size_t *count);
#ifdef __cplusplus
}
#endif
#endif
