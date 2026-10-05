#ifndef SM64_SPIDERMAN_WEB_GL_H
#define SM64_SPIDERMAN_WEB_GL_H
#include <stddef.h>
#include <stdint.h>
#include "../../../codex/spiderman/web/strand_n64.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SpidermanWebGL SpidermanWebGL;
typedef void (*SpidermanWebErrorFn)(void *,const char *);
typedef struct SpidermanWebStrandInstance {
    const SmN64Strand *geometry;
    uint8_t line_rgb[3]; /* actual source lifetime colors; alpha=max(R,G,B) */
} SpidermanWebStrandInstance;
/* Source GPU snapshot, not an instruction to invent sprite geometry. Producers
 * supply the exact original submitted Vtx fields and guMtxF2L matrix decoded
 * into signed16.16 cells (column-major). The renderer expands no billboard and
 * chooses no size, rotation, fade, ST extent, winding or attachment itself. */
typedef struct SpidermanWebQuad {
    int16_t xyz[4][3],st[4][2];
    uint8_t rgba[4][4],indices[6];
    int32_t model_s16_16[16];
    uint16_t texture_slot; /* only verified original46 and47;47 requires uniform
                            * Vtx alpha, equal to original B9DA8/B9DE8 ENV alpha */
} SpidermanWebQuad;
SpidermanWebGL *spiderman_web_gl_create(SpidermanWebErrorFn,void *);
/* Optional original sprite assets. Reads only the two bounded regular files
 * exported by export_web_textures.py; exact original RGBA hashes are pinned.
 * Failed load clears sprite availability. Ordinary line drawing needs no files.
 * Reload/destroy after drawing requires its owning SDL GL context current. */
int spiderman_web_gl_load_textures(SpidermanWebGL *,const char *directory);
int spiderman_web_gl_textures_loaded(const SpidermanWebGL *);
const char *spiderman_web_gl_last_error(const SpidermanWebGL *);
const char *spiderman_web_gl_last_host_warning(const SpidermanWebGL *);
/* Borrow immutable, already source-stepped strands for this synchronous draw.
 * count<=64. A zero count permits a null pointer and submits no geometry.
 * Source float32(fixed12)/4096*.25 is half-away rounded and narrowed to s16;
 * multiply that submitted vertex by4 to map source render units to the host's
 * one-physics-unit-per-host-unit policy. Y and Z are reflected. The original
 * 64-vertex cap (including anchor) and32-vertex chunk gaps are preserved.
 * Matrices are column-major. native_viewport explicitly defines source pixel
 * resolution; SDK widthfield0 is1.5 native pixels, scaled in each axis to the
 * host viewport. Host triangles provide portable thickness, not N64 coverage.
 * No GL line width dependence, animation, endpoint selection, RNG or physics.
 * Uses current host depth; GL state changed by this renderer is restored.
 * GL error flags cannot be restored: inherited errors are separately reported.
 * Invalid input/failure submits no fallback or retained previous geometry. */
int spiderman_web_gl_draw(SpidermanWebGL *,const float view[16],
    const float projection[16],const int viewport[4],
    const int native_viewport[2],const SmN64Strand *strands,size_t count);
/* Canonical lifetime-aware form. Direct draw above is only initial ordinary
 * RGB162 convenience. Each geometry pointer is borrowed synchronously. */
int spiderman_web_gl_draw_instances(SpidermanWebGL *,const float view[16],
    const float projection[16],const int viewport[4],const int native_viewport[2],
    const SpidermanWebStrandInstance *,size_t count);
/* Complete immutable frame boundary. All inputs and required exact textures
 * are checked before drawing anything; source quads<=4096. Source matrix/vertex
 * output is multiplied by16 to host physics units, then reflected Y/Z.
 * ST uses the explicit host nearest/half-texel sampling policy documented in
 * web_render/README.md. It is not an N64 texture-filter/coverage emulator. */
int spiderman_web_gl_draw_frame(SpidermanWebGL *,const float view[16],
    const float projection[16],const int viewport[4],const int native_viewport[2],
    const SpidermanWebStrandInstance *,size_t count,const SpidermanWebQuad *,size_t quads);
/* Separate source connected-ribbon material41. Exact private800-byte20x10 I4
 * decode is pinned. Existing load/draw web APIs continue to accept46/47 only.
 * Quads carry original6511C truncated vertices, reused connected edges, SHADE
 * RGBA and UVs. Same source texture*shade combiner and no-depth-write host policy.
 * No line expansion, inferred width, source update or fallback texture. */
int spiderman_web_gl_load_trail_texture(SpidermanWebGL *,const char *directory);
int spiderman_web_gl_trail_texture_loaded(const SpidermanWebGL *);
int spiderman_web_gl_draw_trail_quads(SpidermanWebGL *,const float view[16],
    const float projection[16],const int viewport[4],const SpidermanWebQuad *,size_t count);
void spiderman_web_gl_destroy(SpidermanWebGL *);
#ifdef SPIDERMAN_TESTING
/* CPU diagnostic output: x,y,z,w homogeneous host clip positions in groups of
 * six vertices per source segment, after clipping and native-pixel expansion.
 * Returns vertex count, or -1 on invalid input/capacity. No partial writes. */
ptrdiff_t spiderman_web_test_geometry(const float view[16],const float projection[16],
    const int viewport[4],const int native_viewport[2],const SmN64Strand *,size_t,
    float *xyzw,size_t capacity_floats);
int spiderman_web_test_texture(const SpidermanWebGL *,unsigned slot,
    unsigned char *rgba,size_t capacity);
int spiderman_web_test_trail_texture(const SpidermanWebGL *,unsigned char *,size_t capacity);
int spiderman_web_test_regular_file(const char *,size_t limit);
void spiderman_web_test_inject_probe_error(SpidermanWebGL *);
int16_t spiderman_web_test_source_coordinate(int32_t fixed12);
#endif
#ifdef __cplusplus
}
#endif
#endif
