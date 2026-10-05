#ifndef SM64_BK_DUO_GL_H
#define SM64_BK_DUO_GL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BkDuoGL BkDuoGL;
typedef void (*BkDuoErrorFn)(void *user, const char *message);
/* All calls belong to one owner thread. CPU load fails closed and accepts only
 * explicit original-mesh format; textures must stay within the asset directory. */
BkDuoGL *bk_duo_gl_create(BkDuoErrorFn callback, void *user);
int bk_duo_gl_load(BkDuoGL *renderer, const char *mesh_path);
int bk_duo_gl_is_loaded(const BkDuoGL *renderer);
const char *bk_duo_gl_last_error(const BkDuoGL *renderer);
/* Original appendage selector branches, 0 hides all branches. 64 entries.
 * Defaults match modelAppendages_reset(): eyes and footwear branch 1,
 * Kazooie upper/feet/rear hidden. Values are checked atomically. */
int bk_duo_gl_set_selectors(BkDuoGL *renderer, const int *selectors, size_t count);
/* Original per-instance alpha mapped to host source-alpha blending. This is
 * an explicit approximation of the N64 blend/env-color render-state path. */
int bk_duo_gl_set_opacity(BkDuoGL *renderer, float opacity);
/* Palettes are column-major 4x4 affine matrices, already in host world units,
 * evaluated from original skeleton/animation. Both arrays need joint_count*16
 * floats. BK source vertices use joint_world. The second palette is retained for explicit
 * billboard source vertices; ordinary source duo vertices are not billboards.
 * Missing/nonfinite palettes fail; there is no fabricated pose fallback.
 * Draw requires current SDL GL context, respects existing world depth and
 * restores every GL state it changes. Returns1 only on successful submission. */
int bk_duo_gl_draw(BkDuoGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], const float *joint_world,
    const float *billboard_world, size_t joint_count);
void bk_duo_gl_destroy(BkDuoGL *renderer);
#ifdef BK_DUO_TESTING
size_t bk_duo_test_visible_triangles(const BkDuoGL *renderer);
size_t bk_duo_test_palette_floats(const BkDuoGL *renderer);
size_t bk_duo_test_position_floats(const BkDuoGL *renderer);
int bk_duo_test_uv(const BkDuoGL *renderer, size_t vertex,
                         size_t material, float uv[2]);
int bk_duo_test_pose(BkDuoGL *renderer, const float *joint_world,
    const float *billboard_world, size_t joint_count, float *positions, size_t capacity);
#endif
#ifdef __cplusplus
}
#endif
#endif
