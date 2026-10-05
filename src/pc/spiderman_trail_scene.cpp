#include "spiderman_trail_scene.h"
#include "spiderman_trail_runtime.h"
#include "spiderman_web_runtime.h"
#include "gfx/spiderman_trail_gl.h"
#include <array>
#include <memory>
#include <string>
namespace {
struct DeleteTrail {void operator()(SpidermanTrailGL *p)const{spiderman_trail_gl_destroy(p);}};
std::unique_ptr<SpidermanTrailGL,DeleteTrail> renderer;
std::array<SmN64Trail,32> current={};size_t count=0;unsigned tick=0,drawn=0;bool valid=false;
std::string status="Original aerial trails inactive";
}
extern "C" void spiderman_trail_scene_suspend(void){count=0;drawn=0;valid=false;}
extern "C" void spiderman_trail_scene_shutdown(void){spiderman_trail_scene_suspend();renderer.reset();status="Original aerial trails inactive";}
extern "C" int spiderman_trail_scene_ready(void){return renderer&&spiderman_trail_gl_texture_loaded(renderer.get());}
extern "C" int spiderman_trail_scene_init(const char *directory){
    spiderman_trail_scene_shutdown();renderer.reset(spiderman_trail_gl_create(nullptr,nullptr));
    if(!renderer||!spiderman_trail_gl_load_texture(renderer.get(),directory)){
        status=std::string("Original aerial trail texture unavailable: ")+spiderman_trail_gl_last_error(renderer.get());renderer.reset();return 0;
    }status="Original aerial trail texture ready";return 1;
}
extern "C" int spiderman_trail_scene_submit(const SmN64Trail *trails,size_t n,unsigned sourceTick){
    if(!spiderman_trail_scene_ready()||n>current.size()||(!trails&&n)){spiderman_trail_scene_suspend();return 0;}
    for(size_t i=0;i<n;++i)if(trails[i].head>=5||trails[i].delete_requested){spiderman_trail_scene_suspend();return 0;}
    for(size_t i=0;i<n;++i)current[i]=trails[i];count=n;tick=sourceTick;valid=true;return 1;
}
extern "C" int spiderman_trail_scene_draw(const float view[16],const float projection[16],const int viewport[4]){
    drawn=0;if(!valid||!spiderman_trail_scene_ready())return 0;
    float camera[16];if(!spiderman_web_camera_from_host_view(view,camera)){status="Original aerial trail camera rejected";return 0;}
    std::array<SpidermanWebQuad,128> quads;size_t total=0;
    for(size_t i=0;i<count;++i){size_t n=0;int rc=spiderman_trail_snapshot(&current[i],camera,quads.data()+total,&n);
        if(rc!=1||n>4||total+n>quads.size()){status="Original aerial trail source submission rejected";return 0;}total+=n;}
    if(!spiderman_trail_gl_draw(renderer.get(),view,projection,viewport,quads.data(),total)){
        status=std::string("Original aerial trail draw failed: ")+spiderman_trail_gl_last_error(renderer.get());valid=false;return 0;}
    drawn=unsigned(total);status="Original aerial trail source frame "+std::to_string(tick)+" rendered";return 1;
}
extern "C" unsigned spiderman_trail_scene_drawn_quads(void){return drawn;}
extern "C" const char *spiderman_trail_scene_status(void){return status.c_str();}
