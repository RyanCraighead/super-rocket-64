#ifndef SM64_BM64_RUNTIME_H
#define SM64_BM64_RUNTIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BM64_HOST_SCALE 1.0f /* Explicit SM64 world conversion, not original code. */
#define BM64_HOST_BOMBS 8
typedef struct Bm64RenderBomb {
    int active,model; float position[3],scale;
    float yaw; int opacity; /* source degrees and 0..255, effects only */
} Bm64RenderBomb;
typedef struct Bm64RenderSnapshot {
    float position[3],yaw;
    int animation[2]; float frame[2];
    Bm64RenderBomb bombs[BM64_HOST_BOMBS];
    uint64_t ticks;
    int held,pumped,explosions;
} Bm64RenderSnapshot;
int bm64_runtime_init(const char *asset_directory);
void bm64_runtime_shutdown(void);
int bm64_runtime_enabled(void);
int bm64_runtime_visible(void);
const char *bm64_runtime_status(void);
void bm64_runtime_suspend(void);
void bm64_runtime_submit(const Bm64RenderSnapshot *state);
int bm64_runtime_snapshot(Bm64RenderSnapshot *state);
int bm64_runtime_draw(const float view[16],const float projection[16],const int viewport[4]);
#ifdef __cplusplus
}
#endif
#endif
