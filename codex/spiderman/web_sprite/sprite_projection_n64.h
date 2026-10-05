#ifndef SMN64_SPRITE_PROJECTION_N64_H
#define SMN64_SPRITE_PROJECTION_N64_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Source submitted geometry, compatible by fields with SpidermanWebQuad.
 * Matrix cells are decoded signed16.16 guMtxF2L output, column-major. */
typedef struct SmN64SpriteQuad {
    int16_t xyz[4][3],st[4][2];
    uint8_t rgba[4][4],indices[6];
    int32_t model_s16_16[16];
    uint16_t texture_slot;
} SmN64SpriteQuad;
typedef struct SmN64SpriteInput {
    int32_t position_fixed12[3];
    int16_t size56,angle84;
    float divisor80;
    uint16_t texture_width,texture_height,texture_slot;
    uint8_t logical_rgb[3],alpha;
} SmN64SpriteInput;
/* Original64BEC -> BBCC8 -> BBE90 projector, including optional BDB28 rotation
 * and original libultra trig arithmetic. camera_world is the final native
 * camera+0x60 array; CB608 stores camera-to-world there, not its inverse.
 * The source copies its16cells, then replaces translation with position/65536.
 * All float ops use binary32; source trig/reduction uses its explicit binary64
 * operations. Caller must use round-to-nearest and no fast-math/contraction.
 * No source timing, RNG, camera simulation, allocation or material is invented.
 * Returns1, or0 on unsupported input, leaving output untouched on rejection. */
int smn64_sprite_project(const SmN64SpriteInput *,const float camera_world[16],
                        SmN64SpriteQuad *);
/* Ordinary/released B15AC knot: slot46,24x24,size56=90,divisor80=400,angle84=0.
 * Source constructor0.4/10 is overwritten by64BEC's per-frame size expression.
 * RGB is logical source RGB; alpha is the separately source-owned Vtx alpha. */
int smn64_web_knot_quad(const int32_t position_fixed12[3],
                       const float camera_world[16],const uint8_t logical_rgb[3],
                       uint8_t alpha,SmN64SpriteQuad *);
/* Explicit host embedding only, not a port of the original camera controller.
 * Input is a rigid host view matrix (column-major). Native source camera basis
 * is A*transpose(host_view rotation), A=diag(1,-1,-1). Translation is zeroed
 * because the projector replaces it. No normalization or guessed camera roll.
 * Renderer maps output by16 into host physics units, then reflects Y and Z.
 * No viewport or field-of-view term belongs in this world-space quad size. */
int smn64_sprite_camera_from_host_view(const float host_view[16],
                                     float native_camera_world[16]);
#ifdef SMN64_SPRITE_TESTING
void smn64_sprite_test_trig(int16_t angle,float out_sine_cosine[2]);
#endif
#ifdef __cplusplus
}
#endif
#endif
