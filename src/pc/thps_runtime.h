#ifndef SM64_THPS_RUNTIME_H
#define SM64_THPS_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original THPS1 USA Rev1 hawk model, including its original skateboard.
 * Caller selects a numeric original clip/frame; no playback is guessed here. */
typedef struct ThpsRenderSnapshot {
    float position[3];
    float yaw_degrees;
    float host_scale;
    int clip_slot;
    int frame_index;
    uint64_t ticks;
    /* Optional explicit host floor-aligned presentation transform. This is not
     * an assertion of original THPS body-orientation/collision equivalence. */
    int use_body_basis;
    int16_t body_basis[9];
    uint32_t hidden_joints; /* Original Tony bail mask: board joints0/1/2. */
} ThpsRenderSnapshot;
int thps_runtime_init(const char *asset_directory);
void thps_runtime_shutdown(void);
int thps_runtime_enabled(void);
int thps_runtime_frame_count(int clip_slot);
/* Copy into adapter-owned lifetime storage; each table has4096*9 signed16
 * cells. Invalid/overlapping outputs are rejected without writing either. */
int thps_runtime_rotation_tables(int16_t *pitch,int16_t *yaw,size_t cells_per_table);
int thps_runtime_visible(void);
const char *thps_runtime_status(void);
void thps_runtime_suspend(void);
int thps_runtime_submit(const ThpsRenderSnapshot *state);
int thps_runtime_snapshot(ThpsRenderSnapshot *state);
int thps_runtime_draw(const float view[16], const float projection[16], const int viewport[4]);
#ifdef __cplusplus
}
#endif
#endif
