#include "spiderman_web_scene.h"
#include "spiderman_web_runtime.h"
#include <array>
#include <memory>
#include <string>
#include <cstring>
namespace {
struct DeleteWeb {void operator()(SpidermanWebGL *r)const{spiderman_web_gl_destroy(r);}};
std::unique_ptr<SpidermanWebGL,DeleteWeb> renderer;
SmN64WebVisuals current={};bool valid=false;unsigned tick=0,drawnQuads=0,drawnStrands=0;
std::string status="Original web graphics inactive";
struct StrandDraw {SpidermanWebStrandInstance line;std::array<SpidermanWebQuad,40> knots;size_t count;};
}
extern "C" void spiderman_web_scene_suspend(void){valid=false;drawnQuads=drawnStrands=0;current={};}
extern "C" void spiderman_web_scene_shutdown(void){spiderman_web_scene_suspend();renderer.reset();status="Original web graphics inactive";}
extern "C" int spiderman_web_scene_init(const char *directory){
    spiderman_web_scene_shutdown();renderer.reset(spiderman_web_gl_create(nullptr,nullptr));
    if(!renderer||!spiderman_web_gl_load_textures(renderer.get(),directory)){
        status=std::string("Original web textures unavailable: ")+spiderman_web_gl_last_error(renderer.get());renderer.reset();return 0;
    }status="Original web textures loaded";return 1;
}
extern "C" const char *spiderman_web_scene_status(void){return status.c_str();}
extern "C" unsigned spiderman_web_scene_drawn_quads(void){return drawnQuads;}
extern "C" unsigned spiderman_web_scene_drawn_strands(void){return drawnStrands;}
extern "C" int spiderman_web_scene_submit(const SmN64WebVisuals *v,unsigned sourceTick){
    if(!renderer||!v||v->strand_count>32||v->splat_count>32){spiderman_web_scene_suspend();return 0;}
    bool seenStrand[32]={},seenSplat[32]={};
    for(unsigned order=0;order<v->strand_count;++order){int i=v->strand_order[order];if(i<0||i>=32||seenStrand[i]){spiderman_web_scene_suspend();return 0;}seenStrand[i]=true;
        if(v->strands[i].alive&&v->strands[i].has_geometry&&(v->strands[i].geometry.count<1||v->strands[i].geometry.count>40)){spiderman_web_scene_suspend();return 0;}}
    for(unsigned order=0;order<v->splat_count;++order){int i=v->splat_order[order];if(i<0||i>=32||seenSplat[i]){spiderman_web_scene_suspend();return 0;}seenSplat[i]=true;}
    current=*v;current.marker=nullptr;current.marker_context=nullptr;tick=sourceTick;valid=true;return 1;
}
extern "C" int spiderman_web_scene_draw(const float view[16],const float projection[16],const int viewport[4]){
    drawnQuads=drawnStrands=0;if(!valid||!renderer)return 0;
    float camera[16];if(!spiderman_web_camera_from_host_view(view,camera)){status="Original web host camera rejected";return 0;}
    std::array<SpidermanWebQuad,32> splats;size_t ns=0;
    std::array<StrandDraw,32> strands;size_t nl=0;
    for(unsigned order=0;order<current.splat_count;++order){const auto &s=current.splats[current.splat_order[order]];if(!s.alive)continue;
        if(spiderman_web_snapshot_splat(&s,&splats[ns])!=1){status="Original web splat submission rejected";return 0;}++ns;}
    for(unsigned order=0;order<current.strand_count;++order){const auto &s=current.strands[current.strand_order[order]];if(!s.alive||!s.has_geometry)continue;
        auto &d=strands[nl++];d.line.geometry=&s.geometry;std::memcpy(d.line.line_rgb,s.line_rgb,3);d.count=size_t(s.geometry.count);
        for(size_t k=0;k<d.count;++k)if(spiderman_web_snapshot_knot(s.geometry.particles[k],camera,s.particle_rgb,s.particle_alpha,&d.knots[k])!=1){status="Original web knot submission rejected";return 0;}}
    // Source global list order: newest splats, then newest strands and their
    // source point sprites. The immutable prepass validates all producers first.
    const int sourceViewport[2]={320,240};
    auto failed=[&](){status=std::string("Original web draw failed: ")+spiderman_web_gl_last_error(renderer.get());valid=false;return 0;};
    for(size_t i=0;i<ns;++i){if(!spiderman_web_gl_draw_frame(renderer.get(),view,projection,viewport,sourceViewport,nullptr,0,&splats[i],1))return failed();++drawnQuads;}
    for(size_t i=0;i<nl;++i){const auto &d=strands[i];if(!spiderman_web_gl_draw_frame(renderer.get(),view,projection,viewport,sourceViewport,&d.line,1,d.knots.data(),d.count))return failed();++drawnStrands;drawnQuads+=unsigned(d.count);}
    status="Original web source frame "+std::to_string(tick)+" rendered";return 1;
}
