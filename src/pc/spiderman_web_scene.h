#ifndef SM64_SPIDERMAN_WEB_SCENE_H
#define SM64_SPIDERMAN_WEB_SCENE_H
#include "../../codex/spiderman/web/lifecycle_n64.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native presentation owner. Copies source-stepped immutable state; no frame
 * advancement, lifetime, gameplay RNG or host actor writes occur here. */
int spiderman_web_scene_init(const char *original_asset_directory);
void spiderman_web_scene_shutdown(void);
void spiderman_web_scene_suspend(void);
int spiderman_web_scene_submit(const SmN64WebVisuals *,unsigned source_tick);
int spiderman_web_scene_draw(const float view[16],const float projection[16],const int viewport[4]);
const char *spiderman_web_scene_status(void);
unsigned spiderman_web_scene_drawn_quads(void);
unsigned spiderman_web_scene_drawn_strands(void);
#ifdef __cplusplus
}
#endif
#endif
