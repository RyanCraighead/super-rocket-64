#include "spiderman_trail_gl.h"
extern "C" SpidermanTrailGL *spiderman_trail_gl_create(SpidermanWebErrorFn fn,void *user){return spiderman_web_gl_create(fn,user);}
extern "C" int spiderman_trail_gl_load_texture(SpidermanTrailGL *r,const char *directory){return spiderman_web_gl_load_trail_texture(r,directory);}
extern "C" int spiderman_trail_gl_texture_loaded(const SpidermanTrailGL *r){return spiderman_web_gl_trail_texture_loaded(r);}
extern "C" int spiderman_trail_gl_draw(SpidermanTrailGL *r,const float view[16],const float projection[16],const int viewport[4],const SpidermanWebQuad *quads,size_t count){return spiderman_web_gl_draw_trail_quads(r,view,projection,viewport,quads,count);}
extern "C" const char *spiderman_trail_gl_last_error(const SpidermanTrailGL *r){return spiderman_web_gl_last_error(r);}
extern "C" void spiderman_trail_gl_destroy(SpidermanTrailGL *r){spiderman_web_gl_destroy(r);}
