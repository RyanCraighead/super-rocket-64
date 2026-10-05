#ifndef SM64_SPIDERMAN_WEB_ATTACK_GL_H
#define SM64_SPIDERMAN_WEB_ATTACK_GL_H
#include "spiderman_web_gl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source38/111/108 vertex snapshot plus the independent RDP ENV alpha consumed
 * by111. That material does not consume the submitted SHADE color or alpha.
 * ENV must come from a documented original render-state boundary; the backend
 * never guesses it from lifetime RGB or adopts current host GL state. */
typedef struct SpidermanWebAttackQuad {
    SpidermanWebQuad submitted;
    uint8_t environment_alpha;
} SpidermanWebAttackQuad;
/* Exact original6B0DC submitted line Vtx. Model is identity, widthfield0.
 * Host coordinates are source_s16 *16 with Y/Z reflection. These are not the
 * quarter-scale/signed-rounded web-strand producer's coordinates. */
typedef struct SpidermanWebAttackLine {
    int16_t xyz[2][3];
    uint8_t rgba[2][4];
} SpidermanWebAttackLine;
typedef SpidermanWebGL SpidermanWebAttackGL;
SpidermanWebAttackGL *spiderman_web_attack_gl_create(SpidermanWebErrorFn,void *);
int spiderman_web_attack_gl_load_textures(SpidermanWebAttackGL *,const char *directory);
int spiderman_web_attack_gl_textures_loaded(const SpidermanWebAttackGL *);
/* Explicit host-camera embedding of sourceCE0D4(camera,2): scale a standard
 * perspective frustum's near/left/right/top/bottom by float32(1.05*1.05),
 * preserving its far plane. Reconstructs near/far to update depth cells10/14.
 * This preserves host view/FOV and reproduces the source frustum-scaling effect,
 * not its N64 matrix quantization. Non-perspective matrices fail closed.
 * Draw applies this only to108; all old web/trail APIs remain unchanged. */
int spiderman_web_attack_decal_projection(const float projection[16],float biased[16]);
/* Source pass ordering is caller-visible: caller supplies sprites first, then
 * decals; all line objects render after those quads. Each list is newest first.
 * Validates complete frame before issuing any draw. No simulation/RNG. */
int spiderman_web_attack_gl_draw(SpidermanWebAttackGL *,const float view[16],
    const float projection[16],const int viewport[4],const int native_viewport[2],
    const SpidermanWebAttackQuad *,size_t quads,const SpidermanWebAttackLine *,size_t lines);
const char *spiderman_web_attack_gl_last_error(const SpidermanWebAttackGL *);
void spiderman_web_attack_gl_destroy(SpidermanWebAttackGL *);
#ifdef SPIDERMAN_TESTING
int spiderman_web_attack_test_texture(const SpidermanWebAttackGL *,unsigned slot,unsigned char *,size_t);
ptrdiff_t spiderman_web_attack_test_lines(const float view[16],const float projection[16],
    const int viewport[4],const int native_viewport[2],const SpidermanWebAttackLine *,size_t,
    float *clip_and_rgba,size_t capacity_floats);
#endif
#ifdef __cplusplus
}
#endif
#endif
