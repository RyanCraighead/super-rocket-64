/* Isolated original Spider-Man asset/pose bridge. No character selection,
 * gameplay adapter, substitute mesh, fallback pose, or inferred playback. */
#include "spiderman_runtime.h"
#include "gfx/spiderman_gl.h"
#include <cmath>
#include <memory>
#include <string>
namespace {
struct DeleteRenderer { void operator()(SpidermanGL *r) const { spiderman_gl_destroy(r); } };
std::unique_ptr<SpidermanGL, DeleteRenderer> renderer;
SpidermanRenderSnapshot current = {};
bool drawable = false, drawn = false;
std::string status = "Original Spider-Man disabled";
bool validTransform(const SpidermanRenderSnapshot &s) {
    for (float v : s.position) if (!std::isfinite(v) || std::fabs(v) > 1e7f) return false;
    if(!std::isfinite(s.host_scale)||s.host_scale<=0||s.host_scale>10000)return false;
    if(s.orientation_mode==SPIDERMAN_ORIENTATION_YAW)return std::isfinite(s.yaw_degrees)&&std::fabs(s.yaw_degrees)<=1e7f;
    return s.orientation_mode==SPIDERMAN_ORIENTATION_NATIVE_BASIS&&spiderman_gl_valid_native_body_matrix(s.native_body_matrix);
}
}
extern "C" int spiderman_runtime_init(const char *directory) {
    spiderman_runtime_shutdown();
    renderer.reset(spiderman_gl_create(nullptr, nullptr));
    if (!renderer || !spiderman_gl_load(renderer.get(), directory)) {
        const std::string reason = spiderman_gl_last_error(renderer.get());
        renderer.reset(); status = "Original Spider-Man disabled: " + reason; return 0;
    }
    status = "Original Spider-Man assets loaded; waiting for an exact source frame"; return 1;
}
extern "C" void spiderman_runtime_shutdown(void) {
    drawable = drawn = false; current = {}; renderer.reset(); status = "Original Spider-Man disabled";
}
extern "C" int spiderman_runtime_enabled(void) { return spiderman_gl_is_loaded(renderer.get()); }
extern "C" int spiderman_runtime_frame_count(int slot) { return spiderman_gl_frame_count(renderer.get(), slot); }
extern "C" int spiderman_runtime_pose_s16(int slot,int frame,int16_t *out,size_t capacity) {return spiderman_gl_pose_s16(renderer.get(),slot,frame,out,capacity);}
extern "C" int spiderman_runtime_load_markers(const char *path) {
    if(!spiderman_gl_load_markers(renderer.get(),path)){status="Original Spider-Man markers unavailable: "+std::string(spiderman_gl_last_error(renderer.get()));return 0;}
    status="Original Spider-Man nine original marker records loaded";return 1;
}
extern "C" int spiderman_runtime_marker_count(void){return spiderman_gl_marker_count(renderer.get());}
extern "C" int spiderman_runtime_marker_record(int index,int16_t xyz[3],uint16_t *joint){return spiderman_gl_marker_record(renderer.get(),index,xyz,joint);}
extern "C" int spiderman_runtime_visible(void) { return spiderman_runtime_enabled() && drawable && drawn; }
extern "C" const char *spiderman_runtime_status(void) { return status.c_str(); }
extern "C" void spiderman_runtime_suspend(void) {
    drawable = drawn = false; current = {};
    if (spiderman_runtime_enabled()) status = "Original Spider-Man suspended";
}
extern "C" int spiderman_runtime_submit(const SpidermanRenderSnapshot *s) {
    drawn = false;
    if (!spiderman_runtime_enabled() || !s || !validTransform(*s)
        || s->frame_index < 0 || s->frame_index >= spiderman_gl_frame_count(renderer.get(), s->clip_slot)) {
        spiderman_runtime_suspend(); status = "Original Spider-Man hidden: invalid or missing exact source frame/transform"; return 0;
    }
    current = *s; drawable = true; status = "Original Spider-Man exact source frame ready"; return 1;
}
extern "C" int spiderman_runtime_snapshot(SpidermanRenderSnapshot *s) {
    if (!spiderman_runtime_enabled() || !drawable || !s) return 0;
    *s = current; return 1;
}
extern "C" int spiderman_runtime_draw(const float view[16], const float projection[16], const int viewport[4]) {
    drawn = false;
    if (!spiderman_runtime_enabled() || !drawable) return 0;
    const int submitted=current.orientation_mode==SPIDERMAN_ORIENTATION_NATIVE_BASIS
        ?spiderman_gl_draw_native(renderer.get(),view,projection,viewport,current.clip_slot,current.frame_index,current.position,current.host_scale,current.native_body_matrix)
        :spiderman_gl_draw(renderer.get(),view,projection,viewport,current.clip_slot,current.frame_index,current.position,current.yaw_degrees,current.host_scale);
    if (!submitted) {
        status = "Original Spider-Man hidden: " + std::string(spiderman_gl_last_error(renderer.get()));
        drawable = false; current = {}; return 0;
    }
    drawn = true; status = "Original Spider-Man original pose rendered; host scale and rasterization explicit";
    const char *warning=spiderman_gl_last_host_warning(renderer.get());
    if(warning&&*warning)status+="; "+std::string(warning);
    return 1;
}
