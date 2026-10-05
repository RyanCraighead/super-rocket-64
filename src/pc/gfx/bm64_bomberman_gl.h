#ifndef SM64_BM64_BOMBERMAN_GL_H
#define SM64_BM64_BOMBERMAN_GL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Bm64BombermanGL Bm64BombermanGL;
typedef void (*Bm64BombermanErrorFn)(void *user, const char *message);
/* All calls belong to one owner thread. CPU load fails closed and accepts only
 * explicit original-mesh format; textures must stay within the asset directory. */
Bm64BombermanGL *bm64_bomberman_gl_create(Bm64BombermanErrorFn callback, void *user);
int bm64_bomberman_gl_load(Bm64BombermanGL *renderer, const char *mesh_path);
int bm64_bomberman_gl_is_loaded(const Bm64BombermanGL *renderer);
const char *bm64_bomberman_gl_last_error(const Bm64BombermanGL *renderer);
/* Original per-instance alpha mapped to host source-alpha blending. This is
 * an explicit approximation of the N64 blend/env-color render-state path. */
int bm64_bomberman_gl_set_opacity(Bm64BombermanGL *renderer, float opacity);
/* Palettes are column-major 4x4 affine matrices, already in host world units,
 * evaluated from original skeleton/animation. Both arrays need joint_count*16
 * floats. Original type5 vertices use billboard_world, others use joint_world.
 * Missing/nonfinite palettes fail; there is no fabricated pose fallback.
 * Draw requires current SDL GL context, respects existing world depth and
 * restores every GL state it changes. Returns1 only on successful submission. */
int bm64_bomberman_gl_draw(Bm64BombermanGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], const float *joint_world,
    const float *billboard_world, size_t joint_count);
void bm64_bomberman_gl_destroy(Bm64BombermanGL *renderer);
#ifdef BM64_BOMBERMAN_TESTING
size_t bm64_bomberman_test_palette_floats(const Bm64BombermanGL *renderer);
size_t bm64_bomberman_test_position_floats(const Bm64BombermanGL *renderer);
int bm64_bomberman_test_uv(const Bm64BombermanGL *renderer, size_t vertex,
                         size_t material, float uv[2]);
int bm64_bomberman_test_pose(Bm64BombermanGL *renderer, const float *joint_world,
    const float *billboard_world, size_t joint_count, float *positions, size_t capacity);
#endif
#ifdef __cplusplus
}
#endif
#endif
