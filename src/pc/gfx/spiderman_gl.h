#ifndef SM64_SPIDERMAN_GL_H
#define SM64_SPIDERMAN_GL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Separate original Spider-Man draw backend. All calls use one owner thread.
 * Loading reads only user-extracted local assets; no replacement geometry,
 * generated pose, animation selection, cadence, or gameplay is provided. */
typedef struct SpidermanGL SpidermanGL;
typedef void (*SpidermanErrorFn)(void *user, const char *message);
SpidermanGL *spiderman_gl_create(SpidermanErrorFn callback, void *user);
int spiderman_gl_load(SpidermanGL *renderer, const char *model_path_or_directory);
int spiderman_gl_is_loaded(const SpidermanGL *renderer);
const char *spiderman_gl_last_error(const SpidermanGL *renderer);
/* Retained diagnostic for inherited host errors, separate from our draw result. */
const char *spiderman_gl_last_host_warning(const SpidermanGL *renderer);
int spiderman_gl_frame_count(const SpidermanGL *renderer, int clip_slot);
int spiderman_gl_pose_s16(const SpidermanGL *renderer, int clip_slot,
    int frame_index, int16_t *poses, size_t capacity);
int spiderman_gl_load_markers(SpidermanGL *renderer, const char *path);
int spiderman_gl_marker_count(const SpidermanGL *renderer);
int spiderman_gl_marker_record(const SpidermanGL *renderer, int index,
    int16_t xyz[3], uint16_t *joint);
/* Source9D258 column guard only (absolute-component sum >=2048). This does
 * not certify orthonormality: source inward/forward may be non-unit.
 * Never normalizes, repairs or invents a fallback fixed12 matrix. */
int spiderman_gl_valid_native_body_matrix(const int16_t matrix[9]);
/* Original-MIPS-evaluated, COLUMN-major affine poses, selected by exact frame.
 * Conversion: original rotation * raw vertex * (8/36), translation /36;
 * reflect Y,Z, then apply host_scale, host yaw in degrees and host position.
 * host_scale is an explicit mapping from exporter units to host units, NOT a
 * claim of original collision/world scale. No default or guessed scale.
 * Draw uses the current SDL GL context and existing world depth and restores
 * all GL state it changes (core, compatibility, GL2.1 and GLES2/3).
 * Inherited host GL errors are drained/reported before our first GL call and
 * retained as host warnings; own probe/upload/draw/restore errors fail drawing.
 * GL error flags cannot be restored. A lost/unresponsive context fails closed.
 * Destroy/reload while the owning context is current, before destroying it. */
int spiderman_gl_draw(SpidermanGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], int clip_slot,
    int frame_index, const float host_position[3], float host_yaw_degrees,
    float host_scale);
/* Native actor orientation: A * native_body_matrix * source_pose, with
 * A=diag(1,-1,-1). Each row-major signed16 basis cell is divided by4096 exactly;
 * no yaw, normalization or host trigonometry replaces the authored basis.
 * host_position/scale retain the same explicit mapping as the legacy API. */
int spiderman_gl_draw_native(SpidermanGL *renderer, const float view[16],
    const float projection[16], const int viewport[4], int clip_slot,
    int frame_index, const float host_position[3], float host_scale,
    const int16_t native_body_matrix[9]);
void spiderman_gl_destroy(SpidermanGL *renderer);
#ifdef SPIDERMAN_TESTING
int spiderman_test_regular_file(const char *path, size_t limit);
void spiderman_test_inject_probe_error(SpidermanGL *renderer);
size_t spiderman_test_position_floats(const SpidermanGL *renderer);
int spiderman_test_pose(SpidermanGL *renderer, int clip_slot, int frame_index,
    const float position[3], float yaw_degrees, float scale,
    float *positions, size_t capacity);
int spiderman_test_pose_native(SpidermanGL *renderer, int clip_slot,
    int frame_index, const float position[3], float scale,
    const int16_t native_body_matrix[9], float *positions, size_t capacity);
int spiderman_test_vertex_native(SpidermanGL *renderer, int clip_slot,
    int frame_index, const float position[3], float scale,
    const int16_t native_body_matrix[9], size_t vertex,
    float out_position[3], float shade[4]);
int spiderman_test_matrix(const SpidermanGL *renderer, int clip_slot,
    int frame_index, int bone, float raw_matrix[16]);
int spiderman_test_uv(const SpidermanGL *renderer, size_t vertex,
    size_t material, float uv[2]);
#endif
#ifdef __cplusplus
}
#endif
#endif
