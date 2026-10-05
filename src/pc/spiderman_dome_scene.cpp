#include "spiderman_dome_scene.h"
#include "gfx/spiderman_dome_gl.h"
#include <array>
#include <cstdio>
#include <memory>
namespace {
struct DeleteAssets{void operator()(SpidermanDomeAssets*a)const{spiderman_dome_assets_destroy(a);}};
struct DeleteGL{void operator()(SpidermanDomeGL*r)const{spiderman_dome_gl_destroy(r);}};
std::unique_ptr<SpidermanDomeAssets,DeleteAssets> assets;
std::unique_ptr<SpidermanDomeGL,DeleteGL> renderer;
std::array<SpidermanDomeSnapshot,SPIDERMAN_DOME_SCENE_CAPACITY> current={};
size_t count=0;uint32_t tick=0;unsigned triangles=0;bool valid=false;
SpidermanDomeDrawCounts counts={};bool drawnValid=false;
char status[512]="Original ordinary dome inactive";
int fail(const char*s){valid=false;count=0;triangles=0;counts={};drawnValid=false;std::snprintf(status,sizeof status,"%s",s);return 0;}
}
extern "C" int spiderman_dome_scene_ready(void){return spiderman_dome_assets_ready(assets.get())&&spiderman_dome_gl_ready(renderer.get());}
extern "C" void spiderman_dome_scene_suspend(void){valid=false;count=0;triangles=0;counts={};drawnValid=false;}
extern "C" void spiderman_dome_scene_shutdown(void){spiderman_dome_scene_suspend();renderer.reset();assets.reset();std::snprintf(status,sizeof status,"Original ordinary dome inactive");}
extern "C" int spiderman_dome_scene_init(const char*directory){spiderman_dome_scene_shutdown();assets.reset(spiderman_dome_assets_create());renderer.reset(spiderman_dome_gl_create());if(!assets||!renderer||!spiderman_dome_assets_load(assets.get(),directory)||!spiderman_dome_gl_load(renderer.get(),assets.get())){std::snprintf(status,sizeof status,"Original dome assets unavailable: %s",spiderman_dome_assets_ready(assets.get())?spiderman_dome_gl_error(renderer.get()):spiderman_dome_assets_error(assets.get()));renderer.reset();assets.reset();return 0;}std::snprintf(status,sizeof status,"Original ordinary dome assets ready");return 1;}
extern "C" int spiderman_dome_scene_copy_pool(uint16_t slot,uint16_t node,SmN64DomeVertex*out,size_t capacity,size_t*n){return spiderman_dome_assets_copy_pool(assets.get(),slot,node,out,capacity,n);}
extern "C" int spiderman_dome_scene_copy_geometry(uint16_t slot,uint16_t node,SmN64DomeGeometry*out){return spiderman_dome_assets_copy_geometry(assets.get(),slot,node,out);}
extern "C" int spiderman_dome_scene_submit(const SpidermanDomeInstance*instances,size_t n,uint32_t sourceTick){
    if(!spiderman_dome_scene_ready()||n>current.size()||(n&&!instances))return fail("Original dome snapshot unavailable or exceeds finite capacity");
    std::array<SpidermanDomeSnapshot,SPIDERMAN_DOME_SCENE_CAPACITY> next={};size_t used=0;
    for(size_t i=0;i<n;++i){for(size_t j=0;j<i;++j)if(instances[i].graphical_serial==instances[j].graphical_serial)return fail("Duplicate original dome render serial");const int result=spiderman_dome_snapshot(assets.get(),&instances[i],&next[used]);if(result<0)return fail("Invalid original dome body/current mutable pool");if(result==1)++used;}
    current=next;count=used;tick=sourceTick;valid=true;triangles=0;counts={};drawnValid=false;return 1;
}
extern "C" int spiderman_dome_scene_draw(const float view[16],const float projection[16],const int vp[4],SpidermanDomeRenderState*state){
    triangles=0;counts={};drawnValid=false;if(!valid||!spiderman_dome_scene_ready())return 0;if(!state)return fail("Missing explicit dome source material state");
    std::array<SpidermanDomePacket,SPIDERMAN_DOME_SCENE_CAPACITY> packets={};SpidermanDomeRenderState pending=*state;
    for(size_t i=0;i<count;++i)if(!spiderman_dome_prepare(&current[i],&pending,&packets[i],&pending))return fail("Original dome ordered source material rejected");
    if(!spiderman_dome_gl_draw(renderer.get(),view,projection,vp,packets.data(),count))return fail(spiderman_dome_gl_error(renderer.get()));
    for(size_t i=0;i<count;++i){triangles+=packets[i].draw.corner_count/3;switch(packets[i].draw.model_slot){case 248:++counts.held_bodies;break;case 249:++counts.piece_bodies;break;case 226:++counts.ring_bodies;break;}}
    counts.source_tick=tick;counts.triangles=triangles;drawnValid=true;
    *state=pending;std::snprintf(status,sizeof status,"Original ordinary dome source frame %u rendered",unsigned(tick));return 1;
}
extern "C" const char*spiderman_dome_scene_status(void){return status;}
extern "C" int spiderman_dome_scene_draw_counts(SpidermanDomeDrawCounts*out){if(!out)return 0;*out=counts;return drawnValid?1:0;}
extern "C" unsigned spiderman_dome_scene_drawn_triangles(void){return triangles;}
#ifdef SPIDERMAN_TESTING
extern "C" int spiderman_dome_scene_test_snapshot(SpidermanDomeSnapshot*out,size_t capacity,size_t*n,uint32_t*sourceTick){if(!valid||!n||!sourceTick||capacity<count||(count&&!out))return 0;for(size_t i=0;i<count;++i)out[i]=current[i];*n=count;*sourceTick=tick;return 1;}
#endif
