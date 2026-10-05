#ifndef SM64_SPIDERMAN_TRAIL_SCENE_H
#define SM64_SPIDERMAN_TRAIL_SCENE_H
#include <stddef.h>
#include "../../codex/spiderman/combat/trails_n64.h"
#ifdef __cplusplus
extern "C" {
#endif
int spiderman_trail_scene_init(const char *original_asset_directory);
void spiderman_trail_scene_shutdown(void);
void spiderman_trail_scene_suspend(void);
int spiderman_trail_scene_ready(void);
/* Immutable source order, copied only AFTER the whole source/host commit. */
int spiderman_trail_scene_submit(const SmN64Trail *,size_t count,unsigned tick);
int spiderman_trail_scene_draw(const float view[16],const float projection[16],const int viewport[4]);
unsigned spiderman_trail_scene_drawn_quads(void);
const char *spiderman_trail_scene_status(void);
#ifdef __cplusplus
}
#endif
#endif
