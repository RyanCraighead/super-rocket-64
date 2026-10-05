#ifndef SM64_SPIDERMAN_DOME_GL_H
#define SM64_SPIDERMAN_DOME_GL_H
#include "../spiderman_dome_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SpidermanDomeGL SpidermanDomeGL;
SpidermanDomeGL *spiderman_dome_gl_create(void);
int spiderman_dome_gl_load(SpidermanDomeGL *,const SpidermanDomeAssets *);
int spiderman_dome_gl_ready(const SpidermanDomeGL *);
/* Ordered packets from the source transparent56CAC pass only. All validation
 * precedes GL submission. Native signed16.16 output ×16, reflected Y/Z, uses
 * host view and level1 biased perspective. Ordinary403/404 consume ENV, never
 * vertex SHADE. Host GL linear-repeat filtering and straight-alpha source-over
 * are explicit adaptations: source two-cycle blender/fog/coverage are carried
 * as evidence metadata but are NOT N64 pixel emulation. No fire fallback.
 * All changed GL state is restored, including indexed state. Error flags cannot
 * be restored; inherited errors are reported separately. Current owner context
 * required for draw/reload/destroy after first draw. */
int spiderman_dome_gl_draw(SpidermanDomeGL *,const float view[16],
    const float projection[16],const int viewport[4],const SpidermanDomePacket *,size_t count);
const char *spiderman_dome_gl_error(const SpidermanDomeGL *);
const char *spiderman_dome_gl_host_warning(const SpidermanDomeGL *);
void spiderman_dome_gl_destroy(SpidermanDomeGL *);
#ifdef SPIDERMAN_TESTING
ptrdiff_t spiderman_dome_test_geometry(const float view[16],const float projection[16],
    const int viewport[4],const SpidermanDomePacket *,size_t count,float *,size_t capacity);
#endif
#ifdef __cplusplus
}
#endif
#endif
