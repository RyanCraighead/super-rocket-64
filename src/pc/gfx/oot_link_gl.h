#ifndef SM64_OOT_LINK_GL_H
#define SM64_OOT_LINK_GL_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Independent, original-asset renderer. No game/runtime globals are consulted.
 * All functions must run on one owner thread. Assets remain caller-local.
 * No generated body or rest-pose fallback is ever used. */
typedef struct OotLinkGL OotLinkGL;
/* Callback also reports nonfatal appearance warnings, prefixed as such. */
typedef void (*OotLinkErrorFn)(void *user, const char *message);
OotLinkGL *oot_link_gl_create(OotLinkErrorFn on_error, void *user);
/* CPU-only validation/load, path is a directory containing mesh.json or its
 * explicit filename. On any failure the handle is unloaded. Texture paths
 * are relative to that file and may not escape its directory. */
int oot_link_gl_load(OotLinkGL *renderer, const char *asset_directory_or_mesh_json);
int oot_link_gl_is_loaded(const OotLinkGL *renderer);
const char *oot_link_gl_last_error(const OotLinkGL *renderer);
/* Requires a current SDL GL context. Column-major GL camera matrices;
 * viewport in bottom-left framebuffer pixels using the existing world depth.
 * joint_frame[0] = ORIGINAL signed root translation; [1..21] = ORIGINAL
 * signed binary-angle rotations, each XYZ. No missing-frame substitution.
 * next_frame may equal joint_frame; interpolation must be in [0,1].
 * World transform = T(world_position)*Ry(yaw_radians)*S(.01*world_scale).
 * world_scale converts one OoT actor unit to SM64 world units.
 * Depth-tested, depth-writing, alpha-cutout opaque mesh. Every GL state this
 * call modifies is restored, including initialization/upload paths.
 * Returns 1 only when a loaded mesh was submitted successfully. */
int oot_link_gl_draw(OotLinkGL *renderer, const float view[16], const float projection[16],
                     const int viewport[4], const float world_position[3],
                     float yaw_radians, float world_scale,
                     const int16_t joint_frame[22][3], const int16_t next_frame[22][3],
                     float interpolation);
/* CPU-only exact tick transform, zero-based skeleton limb index 0..20
 * (PLAYER_LIMB_* - 1). Maps raw original model coordinates into SM64 world.
 * No interpolation, GL context, or draw is required. */
int oot_link_gl_limb_transform(OotLinkGL *renderer, const float world_position[3],
                               float yaw_radians, float world_scale,
                               const int16_t joint_frame[22][3], int limb_index,
                               float matrix[16]);
/* Call before destroying the owning GL context. CPU-only handles need none.
 * If called after context loss, CPU state is freed without GL driver calls. */
void oot_link_gl_destroy(OotLinkGL *renderer);
#ifdef OOT_LINK_TESTING
/* ROM-free tests only: output column-major palettes and world-space positions.
 * Null output buffers return the required element counts through the sizes.
 * Capacity arguments are floats, not bytes; undersized output is rejected. */
int oot_link_test_pose(OotLinkGL *renderer, const float world_position[3], float yaw_radians,
                      float world_scale, const int16_t frame[22][3],
                      const int16_t next[22][3], float interpolation,
                      float *palette, size_t palette_capacity,
                      float *positions, size_t positions_capacity);
int oot_link_test_uv(const OotLinkGL *renderer, size_t vertex_index, size_t material_index, float uv[2]);
size_t oot_link_test_palette_floats(const OotLinkGL *renderer);
size_t oot_link_test_position_floats(const OotLinkGL *renderer);
#endif
#ifdef __cplusplus
}
#endif
#endif
