#ifndef SM64_BK_RUNTIME_H
#define SM64_BK_RUNTIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BK_HOST_SCALE 1.0f /* Explicit SM64 world conversion, not original code. */
/* Values match BkMotion.model_flags. Original model direction reverses yaw in
 * talon trot; appendage branches come from core2/modelappendages.c. */
#define BK_RENDER_KAZOOIE_UPPER 1u
#define BK_RENDER_KAZOOIE_FEET 2u
#define BK_RENDER_KAZOOIE_DIRECTION 4u
typedef struct BkRenderSnapshot {
    float position[3],yaw; /* Host world position; original yaw in DEGREES. */
    int animation_asset; /* Original asset table ID, not exported table index. */
    int animation;       /* Resolved exported index returned by snapshot(). */
    float animation_time; /* Original normalized controller timer, 0..1. */
    uint64_t ticks;
    int action;          /* Original Banjo-Kazooie BS state ID. */
    unsigned model_flags;
} BkRenderSnapshot;
int bk_runtime_init(const char *asset_directory);
void bk_runtime_shutdown(void);
int bk_runtime_enabled(void);
int bk_runtime_visible(void);
const char *bk_runtime_status(void);
void bk_runtime_suspend(void);
void bk_runtime_submit(const BkRenderSnapshot *state);
int bk_runtime_snapshot(BkRenderSnapshot *state);
int bk_runtime_draw(const float view[16],const float projection[16],const int viewport[4]);
#ifdef __cplusplus
}
#endif
#endif
