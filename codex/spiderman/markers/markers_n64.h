#ifndef SMN64_MARKERS_H
#define SMN64_MARKERS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded records supplied by the user's exact ROM; no marker data bundled. */
typedef struct SmN64Marker {
    int16_t xyz[3];
    uint16_t joint;
} SmN64Marker;

/* Original 0x8004A720 post-pose path. The poses array is pose_count consecutive
 * 12-s16 records: nine row-major fixed12 rotation cells, then xyz translations
 * in authored integer units. body_matrix is nine row-major fixed12 cells.
 * body_translation is the RETAINED three integer matrix translations; it is
 * intentionally independent of the current fixed12 position. Original 49A50
 * refreshes it with position>>12, but motion may change position afterward.
 * mirror_x negates the body's first matrix column with signed16 wrapping.
 * Returns 1 on success; 0 for invalid shape/index/pointers, leaving out intact.
 * Inputs are unmodified, including under mirrored evaluation. */
int smn64_marker_world(const SmN64Marker *markers, size_t marker_count,
                      size_t marker_index, const int16_t *poses,
                      size_t pose_count, const int16_t body_matrix[9],
                      const int32_t body_translation[3],
                      const int32_t position_fixed12[3], int mirror_x,
                      int32_t out[3]);
#ifdef __cplusplus
}
#endif
#endif
