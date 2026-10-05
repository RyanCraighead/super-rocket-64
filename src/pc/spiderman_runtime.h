#ifndef SM64_SPIDERMAN_RUNTIME_H
#define SM64_SPIDERMAN_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#define SPIDERMAN_ORIGINAL_POSE_VALUES 216
#define SPIDERMAN_ORIGINAL_MARKER_COUNT 9
enum SpidermanOrientationMode {
    SPIDERMAN_ORIENTATION_YAW = 0,
    SPIDERMAN_ORIENTATION_NATIVE_BASIS = 1
};
#ifdef __cplusplus
extern "C" {
#endif
/* The future source-backed controller must provide a numeric original clip and
 * exact decoded frame. No invented idle/run roles, timer, 30fps, looping or
 * interpolation are inferred by this bridge. All calls use one owner thread. */
typedef struct SpidermanRenderSnapshot {
    float position[3];        /* Explicit host world position. */
    float yaw_degrees;        /* Explicit host yaw, degrees about host +Y. */
    float host_scale;         /* Exporter -> host units; finite and >0. */
    int clip_slot;            /* Original numeric animation slot, 0..299. */
    int frame_index;          /* Exact original decoded frame; no wrapping. */
    uint64_t ticks;           /* Opaque source-controller diagnostic. */
    /* Appended: zero initialization preserves the original yaw-only mode.
     * Native mode ignores yaw and retains each signed16 fixed12 basis cell.
     * Row-major columns are native right/inward/forward, BEFORE Y/Z reflection. */
    int orientation_mode;
    int16_t native_body_matrix[9];
} SpidermanRenderSnapshot;
int spiderman_runtime_init(const char *asset_directory);
void spiderman_runtime_shutdown(void);
int spiderman_runtime_enabled(void);
int spiderman_runtime_frame_count(int clip_slot); /* Validated source count, or 0. */
/* Exact original 18x12 signed16 poses, copied without host reconstruction.
 * capacity is in s16 values (>=216); rejection leaves output untouched. */
int spiderman_runtime_pose_s16(int clip_slot, int frame_index,
    int16_t *poses, size_t capacity);
/* Optional, explicit marker sidecar load; traversal must require count==9.
 * A rejected load clears prior marker records, leaving model/poses available.
 * Pass original-markers.json or its containing asset directory. */
int spiderman_runtime_load_markers(const char *path);
int spiderman_runtime_marker_count(void);
int spiderman_runtime_marker_record(int index, int16_t xyz[3], uint16_t *joint);
/* Feed records and exact poses to the verified marker evaluator with the
 * caller's RETAINED integer body_translation and separate CURRENT fixed12
 * position. This runtime does not derive or recompute either translation. */
int spiderman_runtime_visible(void); /* Successfully drawn since latest submit. */
const char *spiderman_runtime_status(void);
void spiderman_runtime_suspend(void);
int spiderman_runtime_submit(const SpidermanRenderSnapshot *state);
int spiderman_runtime_snapshot(SpidermanRenderSnapshot *state);
int spiderman_runtime_draw(const float view[16], const float projection[16],
    const int viewport[4]);
#ifdef __cplusplus
}
#endif
#endif
