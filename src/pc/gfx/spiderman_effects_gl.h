#ifndef SM64_SPIDERMAN_EFFECTS_GL_H
#define SM64_SPIDERMAN_EFFECTS_GL_H
#include "spiderman_web_attack_gl.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum SpidermanEffectPrimitiveKind {
    SPIDERMAN_EFFECT_WEB_QUAD=1,SPIDERMAN_EFFECT_TRAIL_QUAD,
    SPIDERMAN_EFFECT_ATTACK_QUAD,SPIDERMAN_EFFECT_PRIMARY_CHAIN,
    SPIDERMAN_EFFECT_SECONDARY_CHAIN,SPIDERMAN_EFFECT_ATTACK_LINE,
    SPIDERMAN_EFFECT_DOME_SHATTER
} SpidermanEffectPrimitiveKind;
typedef struct SpidermanEffectPrimitive {
    uint32_t kind; /* one of SpidermanEffectPrimitiveKind; unknown rejects */
    union {
        const SpidermanWebQuad *quad;
        const SpidermanWebAttackQuad *attack_quad;
        const SpidermanWebStrandInstance *strand;
        const SpidermanWebAttackLine *line;
    } source;
} SpidermanEffectPrimitive;
/* Borrow source-ordered immutable references for one synchronous frame. No
 * sorting, simulation, allocation timestamps, ENV inference or fallback occurs
 * inside the backend. The caller supplies its explicit initial ENV alpha.
 * Source47/108 FB commands update that byte in order;111 consumes the current
 * byte, independently of the input quad's standalone environment_alpha field.
 * All producers/materials validate before any GL draw. final_environment_alpha
 * is optional and changes only on success, including a valid empty frame.
 * count<=4096 and at most131072 generated host triangle vertices. Empty input
 * accepts NULL. Resources loaded through the original web/trail/attack loaders
 * share one reviewed GL context/state guard. Old standalone APIs unchanged. */
int spiderman_effects_gl_draw_ordered(SpidermanWebGL *,const float view[16],
    const float projection[16],const int viewport[4],const int native_viewport[2],
    const SpidermanEffectPrimitive *,size_t count,uint8_t initial_environment_alpha,
    uint8_t *final_environment_alpha);
/* Additional ordinary dome shatter material403. Exact original64x64 private
 * texture, separately enabled; legacy standalone attack APIs remain unchanged. */
int spiderman_effects_gl_load_dome_texture(SpidermanWebGL *,const char *directory);
int spiderman_effects_gl_dome_texture_loaded(const SpidermanWebGL *);
#ifdef __cplusplus
}
#endif
#endif
