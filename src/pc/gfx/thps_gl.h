#ifndef SM64_THPS_GL_H
#define SM64_THPS_GL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Separate original THPS1 draw backend. All calls use one owner thread.
 * Loading reads only user-extracted local assets; no replacement geometry,
 * generated pose, animation selection, cadence, or gameplay is provided. */
typedef struct ThpsGL ThpsGL;
typedef void (*ThpsErrorFn)(void *user, const char *message);
ThpsGL *thps_gl_create(ThpsErrorFn callback, void *user);
int thps_gl_load(ThpsGL *renderer, const char *model_path_or_directory);
int thps_gl_is_loaded(const ThpsGL *renderer);
const char *thps_gl_last_error(const ThpsGL *renderer);
/* Retained diagnostic for inherited host errors, separate from our draw result. */
const char *thps_gl_last_host_warning(const ThpsGL *renderer);
int thps_gl_frame_count(const ThpsGL *renderer, int clip_slot);
int thps_gl_copy_rotation_tables(const ThpsGL *renderer,int16_t *pitch,
    int16_t *yaw,size_t cells_per_table);
int thps_gl_valid_native_body_matrix(const int16_t matrix[9]);
/* Original THPS1 MIPS-evaluated, COLUMN-major affine poses, selected by exact frame.
 * Conversion: original rotation * raw vertex * (8/36), translation * (8/36);
 * reflect Y,Z, then apply host_scale, host yaw in degrees and host position.
 * host_scale is an explicit mapping from exporter units to host units, NOT a
 * claim of original collision/world scale. No default or guessed scale.
 * Draw uses the current SDL GL context and existing world depth and restores
 * all GL state it changes (core, compatibility, GL2.1 and GLES2/3).
 * Inherited host GL errors are drained/reported before our first GL call and
 * retained as host warnings; own probe/upload/draw/restore errors fail drawing.
 * GL error flags cannot be restored. A lost/unresponsive context fails closed.
 * Destroy/reload while the owning context is current, before destroying it. */
int thps_gl_draw(ThpsGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], int clip_slot,
    int frame_index, const float host_position[3], float host_yaw_degrees,
    float host_scale);
/* Native actor orientation: A * native_body_matrix * source_pose, with
 * A=diag(1,-1,-1). Each row-major signed16 basis cell is divided by4096 exactly;
 * no yaw, normalization or host trigonometry replaces the authored basis.
 * host_position/scale retain the same explicit mapping as the legacy API. */
int thps_gl_draw_native(ThpsGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], int clip_slot,
    int frame_index, const float host_position[3], float host_scale,
    const int16_t native_body_matrix[9]);
/* Source visibility mask: Tony's recovered bail owner hides joints0/1/2.
 * Body may be NULL for legacy yaw mode. Bits outside0x7 are rejected; no
 * vertices or original poses are modified to implement visibility. */
int thps_gl_draw_masked(ThpsGL *renderer,const float view[16],
    const float projection[16],const int viewport[4],int clip_slot,
    int frame_index,const float host_position[3],float host_yaw_degrees,
    float host_scale,const int16_t native_body_matrix[9],uint32_t hidden_joints);
void thps_gl_destroy(ThpsGL *renderer);
#ifdef THPS_TESTING
int thps_test_regular_file(const char *path, size_t limit);
void thps_test_inject_probe_error(ThpsGL *renderer);
size_t thps_test_position_floats(const ThpsGL *renderer);
size_t thps_test_visible_triangles(const ThpsGL *renderer,uint32_t hidden_joints);
int thps_test_pose(ThpsGL *renderer, int clip_slot, int frame_index,
    const float position[3], float yaw_degrees, float scale,
    float *positions, size_t capacity);
int thps_test_pose_native(ThpsGL *renderer, int clip_slot,
    int frame_index, const float position[3], float scale,
    const int16_t native_body_matrix[9], float *positions, size_t capacity);
int thps_test_vertex_native(ThpsGL *renderer, int clip_slot,
    int frame_index, const float position[3], float scale,
    const int16_t native_body_matrix[9], size_t vertex,
    float out_position[3], float shade[4]);
int thps_test_matrix(const ThpsGL *renderer, int clip_slot,
    int frame_index, int bone, float raw_matrix[16]);
int thps_test_uv(const ThpsGL *renderer, size_t vertex,
    size_t material, float uv[2]);
#endif
#ifdef __cplusplus
}
#endif
#endif
