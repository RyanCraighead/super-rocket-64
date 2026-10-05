/* Strict original THPS1 USA Rev1 mesh/pose bridge. Independent of every other
 * character backend; no substitute assets or guessed pose/cadence. */
#include "thps_runtime.h"
#include "gfx/thps_gl.h"
#include <cmath>
#include <memory>
#include <string>
namespace {
struct DeleteRenderer { void operator()(ThpsGL *r) const { thps_gl_destroy(r); } };
std::unique_ptr<ThpsGL,DeleteRenderer> renderer;
ThpsRenderSnapshot current={};
bool drawable=false,drawn=false;
std::string status="Original THPS1 disabled";
bool valid(const ThpsRenderSnapshot &s) {
    for(float v:s.position)if(!std::isfinite(v)||std::fabs(v)>1e7f)return false;
    return std::isfinite(s.host_scale)&&s.host_scale>0&&s.host_scale<=10000
        &&std::isfinite(s.yaw_degrees)&&std::fabs(s.yaw_degrees)<=1e7f
        &&(s.hidden_joints&~7u)==0
        &&(s.use_body_basis==0 || (s.use_body_basis==1 && thps_gl_valid_native_body_matrix(s.body_basis)));
}
}
extern "C" int thps_runtime_init(const char *path) {
    thps_runtime_shutdown();renderer.reset(thps_gl_create(nullptr,nullptr));
    if(!renderer||!thps_gl_load(renderer.get(),path)){
        const std::string reason=thps_gl_last_error(renderer.get());renderer.reset();status="Original THPS1 disabled: "+reason;return 0;
    }
    status="Original THPS1 assets loaded; waiting for exact source frame";return 1;
}
extern "C" void thps_runtime_shutdown(void){drawable=drawn=false;current={};renderer.reset();status="Original THPS1 disabled";}
extern "C" int thps_runtime_enabled(void){return thps_gl_is_loaded(renderer.get());}
extern "C" int thps_runtime_frame_count(int slot){return thps_gl_frame_count(renderer.get(),slot);}
extern "C" int thps_runtime_rotation_tables(int16_t *pitch,int16_t *yaw,size_t cells){return thps_gl_copy_rotation_tables(renderer.get(),pitch,yaw,cells);}
extern "C" int thps_runtime_visible(void){return thps_runtime_enabled()&&drawable&&drawn;}
extern "C" const char *thps_runtime_status(void){return status.c_str();}
extern "C" void thps_runtime_suspend(void){drawable=drawn=false;current={};if(thps_runtime_enabled())status="Original THPS1 suspended";}
extern "C" int thps_runtime_submit(const ThpsRenderSnapshot *s){
    drawn=false;
    if(!thps_runtime_enabled()||!s||!valid(*s)||s->frame_index<0||s->frame_index>=thps_runtime_frame_count(s->clip_slot)){
        thps_runtime_suspend();status="Original THPS1 hidden: invalid exact source frame or transform";return 0;
    }
    current=*s;drawable=true;status="Original THPS1 exact source frame ready";return 1;
}
extern "C" int thps_runtime_snapshot(ThpsRenderSnapshot *s){if(!thps_runtime_enabled()||!drawable||!s)return 0;*s=current;return 1;}
extern "C" int thps_runtime_draw(const float view[16],const float projection[16],const int viewport[4]){
    drawn=false;if(!thps_runtime_enabled()||!drawable)return 0;
    const int rendered=thps_gl_draw_masked(renderer.get(),view,projection,viewport,
        current.clip_slot,current.frame_index,current.position,current.yaw_degrees,
        current.host_scale,current.use_body_basis?current.body_basis:nullptr,current.hidden_joints);
    if(!rendered){
        status="Original THPS1 hidden: "+std::string(thps_gl_last_error(renderer.get()));drawable=false;current={};return 0;
    }
    drawn=true;status="Original THPS1 hawk and skateboard rendered with original MIPS pose; host scale/rasterization explicit";
    const char *warning=thps_gl_last_host_warning(renderer.get());if(warning&&*warning)status+="; "+std::string(warning);return 1;
}
