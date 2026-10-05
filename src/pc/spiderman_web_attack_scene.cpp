#include "spiderman_web_attack_scene.h"
#include "spiderman_web_attack_runtime.h"
#include "../../codex/spiderman/web_sprite/sprite_projection_n64.h"
#include <array>
#include <memory>
#include <string>
#include <algorithm>
namespace {
struct DeleteRenderer{void operator()(SpidermanWebAttackGL*r)const{spiderman_web_attack_gl_destroy(r);}};
std::unique_ptr<SpidermanWebAttackGL,DeleteRenderer> renderer;
std::array<SmN64WebAttackObject,SMN64_WEB_ATTACK_CAPACITY> current={};size_t count=0;uint32_t tick=0;SpidermanWebAttackRenderState material={};bool valid=false;unsigned drawnQuads=0,drawnLines=0;
std::string status="Original web attacks inactive";
int fail(const std::string&message){valid=false;count=0;drawnQuads=drawnLines=0;status=message;return 0;}
bool live(const SmN64WebAttackObject&o){switch(o.kind){case SMN64_ATTACK_PROJECTILE:return o.state.projectile.alive;case SMN64_ATTACK_BURST:return o.state.burst.alive;case SMN64_ATTACK_DECAL:return o.state.decal.alive;case SMN64_ATTACK_FRAGMENT:return o.state.fragment.alive;case SMN64_ATTACK_SPARK:return o.state.spark.alive;default:return false;}}
}
extern "C" int spiderman_web_attack_scene_ready(void){return renderer&&spiderman_web_attack_gl_textures_loaded(renderer.get());}
extern "C" void spiderman_web_attack_scene_suspend(void){valid=false;count=0;drawnQuads=drawnLines=0;material={};}
extern "C" void spiderman_web_attack_scene_shutdown(void){spiderman_web_attack_scene_suspend();renderer.reset();status="Original web attacks inactive";}
extern "C" int spiderman_web_attack_scene_init(const char*directory){spiderman_web_attack_scene_shutdown();renderer.reset(spiderman_web_attack_gl_create(nullptr,nullptr));if(!renderer||!spiderman_web_attack_gl_load_textures(renderer.get(),directory)){status=std::string("Original attack assets unavailable: ")+spiderman_web_attack_gl_last_error(renderer.get());renderer.reset();return 0;}status="Original attack assets ready";return 1;}
extern "C" int spiderman_web_attack_scene_submit(const SmN64WebAttackObject*objects,size_t n,uint32_t source_tick,const SpidermanWebAttackRenderState*state){
    if(!spiderman_web_attack_scene_ready()||n>SMN64_WEB_ATTACK_CAPACITY||(n&&!objects))return fail("Original attack snapshot unavailable or exceeds capacity");
    for(size_t i=0;i<n;++i){const auto&o=objects[i];if(!o.id||(i&&objects[i-1].id<=o.id)||o.kind<SMN64_ATTACK_PROJECTILE||o.kind>SMN64_ATTACK_SPARK)return fail("Original attack snapshot ID/type/list ordering rejected");if(o.kind==SMN64_ATTACK_BURST&&live(o)&&(!state||!state->environment_alpha_known))return fail("Original burst requires recovered source ENV alpha");}
    if(n)std::copy(objects,objects+n,current.begin());
    count=n;tick=source_tick;material=state?*state:SpidermanWebAttackRenderState{};valid=true;drawnQuads=drawnLines=0;return 1;
}
extern "C" int spiderman_web_attack_scene_draw(const float view[16],const float projection[16],const int viewport[4]){
    drawnQuads=drawnLines=0;if(!valid||!spiderman_web_attack_scene_ready())return 0;
    float camera[16];if(!smn64_sprite_camera_from_host_view(view,camera))return fail("Original attack host camera rejected");
    std::array<SpidermanWebAttackQuad,SMN64_WEB_ATTACK_CAPACITY> quads;std::array<SpidermanWebAttackLine,SMN64_WEB_ATTACK_CAPACITY*2> lines;size_t nq=0,nl=0;
    for(unsigned pass=0;pass<3;++pass)for(size_t i=0;i<count;++i){const auto&o=current[i];if(!live(o))continue;int result=1;
        if(pass==0&&o.kind==SMN64_ATTACK_PROJECTILE){result=spiderman_web_attack_snapshot_projectile(&o.state.projectile,camera,&quads[nq]);if(result==1)++nq;}
        else if(pass==0&&o.kind==SMN64_ATTACK_BURST){result=spiderman_web_attack_snapshot_burst(&o.state.burst,camera,material.environment_alpha,&quads[nq]);if(result==1)++nq;}
        else if(pass==1&&o.kind==SMN64_ATTACK_DECAL){result=spiderman_web_attack_snapshot_decal(&o.state.decal,&quads[nq]);if(result==1)++nq;}
        else if(pass==2&&o.kind==SMN64_ATTACK_FRAGMENT){result=spiderman_web_attack_snapshot_fragment(&o.state.fragment,&lines[nl]);if(result==1)nl+=2;}
        else if(pass==2&&o.kind==SMN64_ATTACK_SPARK){result=spiderman_web_attack_snapshot_spark(&o.state.spark,&lines[nl]);if(result==1)++nl;}
        if(result!=1)return fail("Original attack source producer rejected committed state");
    }
    const int native[2]={320,240};if(!spiderman_web_attack_gl_draw(renderer.get(),view,projection,viewport,native,quads.data(),nq,lines.data(),nl))return fail(std::string("Original attack draw failed: ")+spiderman_web_attack_gl_last_error(renderer.get()));
    drawnQuads=unsigned(nq);drawnLines=unsigned(nl);status="Original attack source frame "+std::to_string(tick)+" rendered";return 1;
}
extern "C" const char*spiderman_web_attack_scene_status(void){return status.c_str();}
extern "C" unsigned spiderman_web_attack_scene_drawn_quads(void){return drawnQuads;}
extern "C" unsigned spiderman_web_attack_scene_drawn_lines(void){return drawnLines;}
#ifdef SPIDERMAN_TESTING
extern "C" int spiderman_web_attack_scene_test_snapshot(SmN64WebAttackObject*out,size_t capacity,size_t*n,uint32_t*source_tick){if(!valid||!n||!source_tick||capacity<count||(count&&!out))return 0;if(count)std::copy(current.begin(),current.begin()+count,out);*n=count;*source_tick=tick;return 1;}
#endif
