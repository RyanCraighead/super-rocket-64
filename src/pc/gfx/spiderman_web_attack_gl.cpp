#include "spiderman_web_attack_gl.h"
/* Deliberately thin: all GL dispatch, ownership, resource and state restoration
 * lives in the reviewed shared backend. These explicit material entry points
 * are implemented there; no legacy web/trail API accepts attack materials. */
extern "C" SpidermanWebAttackGL *spiderman_web_attack_gl_create(SpidermanWebErrorFn fn,void *user){return spiderman_web_gl_create(fn,user);}
extern "C" const char *spiderman_web_attack_gl_last_error(const SpidermanWebAttackGL*r){return spiderman_web_gl_last_error(r);}
extern "C" void spiderman_web_attack_gl_destroy(SpidermanWebAttackGL*r){spiderman_web_gl_destroy(r);}
