#ifndef SM64_SPIDERMAN_TRAIL_GL_H
#define SM64_SPIDERMAN_TRAIL_GL_H
#include "spiderman_web_gl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Shared reviewed GL ownership/guard implementation; distinct trail contract. */
typedef SpidermanWebGL SpidermanTrailGL;
SpidermanTrailGL *spiderman_trail_gl_create(SpidermanWebErrorFn,void *);
int spiderman_trail_gl_load_texture(SpidermanTrailGL *,const char *directory);
int spiderman_trail_gl_texture_loaded(const SpidermanTrailGL *);
int spiderman_trail_gl_draw(SpidermanTrailGL *,const float view[16],
    const float projection[16],const int viewport[4],const SpidermanWebQuad *,size_t count);
const char *spiderman_trail_gl_last_error(const SpidermanTrailGL *);
void spiderman_trail_gl_destroy(SpidermanTrailGL *);
#ifdef __cplusplus
}
#endif
#endif
