#include "spiderman_effect_scene.h"
#include "spiderman_web_runtime.h"
#include "spiderman_trail_runtime.h"
#include "spiderman_web_attack_runtime.h"
#include "gfx/spiderman_effects_gl.h"
#include "../../codex/spiderman/graphics/allocation_order_n64.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <vector>
namespace {
struct DeleteRenderer{void operator()(SpidermanWebGL*r)const{spiderman_web_gl_destroy(r);}};
std::unique_ptr<SpidermanWebGL,DeleteRenderer> renderer;
SmN64WebVisuals web={};std::array<SmN64WebAttackObject,SMN64_WEB_ATTACK_CAPACITY> attacks={};std::array<SmN64Trail,32> trails={};std::array<SmN64DomeShatterFragment,512> shards={};size_t shardCount=0;size_t attackCount=0,trailCount=0;uint32_t tick=0;uint64_t clockValue=0;bool valid=false;unsigned drawnQuads=0,drawnChains=0,drawnLines=0;char status[512]="Original combined effects inactive";SpidermanEffectDrawCounts drawCounts={};bool drawnValid=false;
int fail(const char*message){valid=false;drawnQuads=drawnChains=drawnLines=0;drawCounts={};drawnValid=false;std::snprintf(status,sizeof status,"%s",message);return 0;}
bool live(const SmN64WebAttackObject&o){switch(o.kind){case SMN64_ATTACK_PROJECTILE:return o.state.projectile.alive;case SMN64_ATTACK_BURST:return o.state.burst.alive;case SMN64_ATTACK_DECAL:return o.state.decal.alive;case SMN64_ATTACK_FRAGMENT:return o.state.fragment.alive;case SMN64_ATTACK_SPARK:return o.state.spark.alive;default:return false;}}
struct Node{uint32_t list;uint64_t serial;SpidermanEffectPrimitiveKind kind;size_t index;};
struct Prepared {
    std::vector<SpidermanWebQuad> webQuads,trailQuads;
    std::vector<SpidermanWebAttackQuad> attackQuads;
    std::vector<SpidermanWebStrandInstance> strandInstances;
    std::vector<SpidermanWebAttackLine> lines;
    std::vector<Node> order;
    std::vector<SpidermanEffectPrimitive> references;
};
bool serialRange(uint64_t base,unsigned n){return base&&n&&base<=clockValue&&uint64_t(n-1)<=clockValue-base;}
bool prepare(const float view[16],Prepared&p){
    if(!valid)return false;
    float camera[16];if(!spiderman_web_camera_from_host_view(view,camera))return false;
    for(uint32_t at=0;at<web.strand_count;++at){const unsigned index=unsigned(web.strand_order[at]);const auto&s=web.strands[index];if(!s.alive||!s.has_geometry)continue;const unsigned n=unsigned(s.geometry.count);const uint64_t base=web.strand_graphical_base[index];if(n<1||n>40||!serialRange(base,n+2))return false;
        const size_t line=p.strandInstances.size();SpidermanWebStrandInstance instance={&s.geometry,{s.line_rgb[0],s.line_rgb[1],s.line_rgb[2]}};p.strandInstances.push_back(instance);
        p.order.push_back({SMN64_GRAPHICAL_STRAND_LINES,base,SPIDERMAN_EFFECT_PRIMARY_CHAIN,line});p.order.push_back({SMN64_GRAPHICAL_STRAND_LINES,base+n+1,SPIDERMAN_EFFECT_SECONDARY_CHAIN,line});
        for(unsigned i=0;i<n;++i){SpidermanWebQuad q={};if(spiderman_web_snapshot_knot(s.geometry.particles[i],camera,s.particle_rgb,s.particle_alpha,&q)!=1)return false;p.order.push_back({SMN64_GRAPHICAL_SPRITES,base+1+i,SPIDERMAN_EFFECT_WEB_QUAD,p.webQuads.size()});p.webQuads.push_back(q);}
    }
    for(uint32_t at=0;at<web.splat_count;++at){const unsigned index=unsigned(web.splat_order[at]);const auto&s=web.splats[index];if(!s.alive)continue;const uint64_t serial=web.splat_graphical_serial[index];if(!serialRange(serial,1))return false;SpidermanWebQuad q={};if(spiderman_web_snapshot_splat(&s,&q)!=1)return false;p.order.push_back({SMN64_GRAPHICAL_POLYGONS,serial,SPIDERMAN_EFFECT_WEB_QUAD,p.webQuads.size()});p.webQuads.push_back(q);}
    // F5538 is currently owned solely by this trail registry; source snapshot
    // order is already newest-first and each trail emits segments0..3. These
    // ordinals never compare with another source list's allocation serial.
    for(size_t i=0;i<trailCount;++i){SpidermanWebQuad quads[4];size_t count=0;if(spiderman_trail_snapshot(&trails[i],camera,quads,&count)!=1||count>4)return false;for(size_t j=0;j<count;++j){p.order.push_back({SMN64_GRAPHICAL_TRAILS,uint64_t((trailCount-i)*4-j),SPIDERMAN_EFFECT_TRAIL_QUAD,p.trailQuads.size()});p.trailQuads.push_back(quads[j]);}}
    for(size_t i=0;i<attackCount;++i){const auto&o=attacks[i];if(!live(o))continue;const uint64_t serial=o.graphical_serial;if(!serialRange(serial,o.kind==SMN64_ATTACK_FRAGMENT?2:1))return false;SpidermanWebAttackQuad q={};SpidermanWebAttackLine lines[2]={};int rc=-1;
        switch(o.kind){
            case SMN64_ATTACK_PROJECTILE:rc=spiderman_web_attack_snapshot_projectile(&o.state.projectile,camera,&q);break;
            case SMN64_ATTACK_BURST:rc=spiderman_web_attack_snapshot_burst(&o.state.burst,camera,0,&q);break; // actual ENV supplied at draw time by ordered backend
            case SMN64_ATTACK_DECAL:rc=spiderman_web_attack_snapshot_decal(&o.state.decal,&q);break;
            case SMN64_ATTACK_FRAGMENT:rc=spiderman_web_attack_snapshot_fragment(&o.state.fragment,lines);break;
            case SMN64_ATTACK_SPARK:rc=spiderman_web_attack_snapshot_spark(&o.state.spark,lines);break;
            default:return false;
        }
        if(rc!=1)return false;
        if(o.kind==SMN64_ATTACK_FRAGMENT){p.order.push_back({SMN64_GRAPHICAL_LINES,serial+1,SPIDERMAN_EFFECT_ATTACK_LINE,p.lines.size()});p.lines.push_back(lines[0]);p.order.push_back({SMN64_GRAPHICAL_LINES,serial,SPIDERMAN_EFFECT_ATTACK_LINE,p.lines.size()});p.lines.push_back(lines[1]);}
        else if(o.kind==SMN64_ATTACK_SPARK){p.order.push_back({SMN64_GRAPHICAL_LINES,serial,SPIDERMAN_EFFECT_ATTACK_LINE,p.lines.size()});p.lines.push_back(lines[0]);}
        else{p.order.push_back({o.kind==SMN64_ATTACK_DECAL?uint32_t(SMN64_GRAPHICAL_POLYGONS):uint32_t(SMN64_GRAPHICAL_SPRITES),serial,SPIDERMAN_EFFECT_ATTACK_QUAD,p.attackQuads.size()});p.attackQuads.push_back(q);}
    }
    for(size_t i=0;i<shardCount;++i){
        const auto&s=shards[i];if(!s.alive||!serialRange(s.graphical_serial,1))return false;
        SmN64DomeShatterDraw source={};if(smn64_dome_shatter_snapshot(&s,&source)!=1||source.submitted.texture_slot!=403||source.environment_rgba[0]||source.environment_rgba[1]||source.environment_rgba[2]||source.combiner[0]!=0xfc50d3ff||source.combiner[1]!=0xfffffe38||source.render_mode!=0x0c184b50)return false;
        SpidermanWebAttackQuad q={};std::copy(&source.submitted.xyz[0][0],&source.submitted.xyz[0][0]+12,&q.submitted.xyz[0][0]);std::copy(&source.submitted.st[0][0],&source.submitted.st[0][0]+8,&q.submitted.st[0][0]);std::copy(&source.submitted.rgba[0][0],&source.submitted.rgba[0][0]+16,&q.submitted.rgba[0][0]);std::copy(source.submitted.indices,source.submitted.indices+6,q.submitted.indices);std::copy(source.submitted.model_s16_16,source.submitted.model_s16_16+16,q.submitted.model_s16_16);q.submitted.texture_slot=403;q.environment_alpha=source.environment_rgba[3];
        p.order.push_back({SMN64_GRAPHICAL_POLYGONS,s.graphical_serial,SPIDERMAN_EFFECT_DOME_SHATTER,p.attackQuads.size()});p.attackQuads.push_back(q);
    }
    if(p.order.size()>4096)return false;
    std::sort(p.order.begin(),p.order.end(),[](const Node&a,const Node&b){return smn64_graphical_draw_compare(a.list,a.serial,b.list,b.serial)<0;});
    for(size_t i=0;i<p.order.size();++i){const auto&n=p.order[i];if(i&&n.list==p.order[i-1].list&&n.serial==p.order[i-1].serial)return false;SpidermanEffectPrimitive ref={};ref.kind=n.kind;switch(n.kind){case SPIDERMAN_EFFECT_WEB_QUAD:ref.source.quad=&p.webQuads[n.index];break;case SPIDERMAN_EFFECT_TRAIL_QUAD:ref.source.quad=&p.trailQuads[n.index];break;case SPIDERMAN_EFFECT_DOME_SHATTER:case SPIDERMAN_EFFECT_ATTACK_QUAD:ref.source.attack_quad=&p.attackQuads[n.index];break;case SPIDERMAN_EFFECT_PRIMARY_CHAIN:case SPIDERMAN_EFFECT_SECONDARY_CHAIN:ref.source.strand=&p.strandInstances[n.index];break;case SPIDERMAN_EFFECT_ATTACK_LINE:ref.source.line=&p.lines[n.index];break;}p.references.push_back(ref);}
    return true;
}
}
extern "C" int spiderman_effect_scene_ready(void){return renderer&&spiderman_web_gl_textures_loaded(renderer.get())&&spiderman_web_gl_trail_texture_loaded(renderer.get())&&spiderman_web_attack_gl_textures_loaded(renderer.get());}
extern "C" void spiderman_effect_scene_suspend(void){valid=false;attackCount=trailCount=shardCount=0;drawnQuads=drawnChains=drawnLines=0;drawCounts={};drawnValid=false;}
extern "C" void spiderman_effect_scene_shutdown(void){spiderman_effect_scene_suspend();renderer.reset();std::snprintf(status,sizeof status,"Original combined effects inactive");}
extern "C" int spiderman_effect_scene_init(const char*directory){spiderman_effect_scene_shutdown();renderer.reset(spiderman_web_gl_create(nullptr,nullptr));if(!renderer||!spiderman_web_gl_load_textures(renderer.get(),directory)||!spiderman_web_gl_load_trail_texture(renderer.get(),directory)||!spiderman_web_attack_gl_load_textures(renderer.get(),directory)){std::snprintf(status,sizeof status,"Original effect assets unavailable: %s",spiderman_web_gl_last_error(renderer.get()));renderer.reset();return 0;}std::snprintf(status,sizeof status,"Original combined effect assets ready");return 1;}
extern "C" int spiderman_effect_scene_enable_dome(const char*directory){return renderer&&spiderman_effects_gl_load_dome_texture(renderer.get(),directory);}
extern "C" int spiderman_effect_scene_dome_ready(void){return renderer&&spiderman_effects_gl_dome_texture_loaded(renderer.get());}
extern "C" int spiderman_effect_scene_submit(const SmN64WebVisuals*visuals,const SmN64WebAttackObject*objects,size_t na,const SmN64Trail*sourceTrails,size_t nt,uint32_t sourceTick,uint64_t sharedClock){
    if(!spiderman_effect_scene_ready()||na>attacks.size()||nt>trails.size()||(na&&!objects)||(nt&&!sourceTrails))return fail("Combined source effect snapshot unavailable or exceeds capacity");
    if(visuals){if(visuals->strand_count>32||visuals->splat_count>32||visuals->graphical_clock>sharedClock)return fail("Traversal source list/clock rejected");bool seenStrand[32]={},seenSplat[32]={};for(uint32_t i=0;i<visuals->strand_count;++i){const int index=visuals->strand_order[i];if(index<0||index>=32||seenStrand[index])return fail("Traversal strand list rejected");seenStrand[index]=true;}for(uint32_t i=0;i<visuals->splat_count;++i){const int index=visuals->splat_order[i];if(index<0||index>=32||seenSplat[index])return fail("Traversal polygon list rejected");seenSplat[index]=true;}}
    for(size_t i=0;i<na;++i)if(!objects[i].id||(i&&objects[i-1].id<=objects[i].id)||objects[i].kind<SMN64_ATTACK_PROJECTILE||objects[i].kind>SMN64_ATTACK_SPARK)return fail("Attack source object list rejected");
    for(size_t i=0;i<nt;++i)if(sourceTrails[i].head>=5||sourceTrails[i].delete_requested)return fail("Trail source object list rejected");
    shardCount=0;web=visuals?*visuals:SmN64WebVisuals{};web.marker=nullptr;web.marker_context=nullptr;if(na)std::copy(objects,objects+na,attacks.begin());if(nt)std::copy(sourceTrails,sourceTrails+nt,trails.begin());attackCount=na;trailCount=nt;clockValue=sharedClock;tick=sourceTick;valid=true;drawnQuads=drawnChains=drawnLines=0;drawCounts={};drawnValid=false;return 1;
}
extern "C" int spiderman_effect_scene_submit_dome_shards(const SmN64DomeShatterFragment*source,size_t count,uint32_t sourceTick){
    if(!valid||sourceTick!=tick||count>shards.size()||(count&&!source)||!renderer||!spiderman_effects_gl_dome_texture_loaded(renderer.get()))return fail("Original dome shatter snapshot/material unavailable");
    for(size_t i=0;i<count;i++)if(!source[i].alive||!source[i].graphical_serial||source[i].graphical_serial>clockValue||(i&&source[i-1].graphical_serial<=source[i].graphical_serial))return fail("Original dome shatter list/order rejected");
    if(count)std::copy(source,source+count,shards.begin());
    shardCount=count;drawnQuads=drawnChains=drawnLines=0;drawCounts={};drawnValid=false;return 1;
}
extern "C" int spiderman_effect_scene_draw(const float view[16],const float projection[16],const int viewport[4],uint8_t initialEnvironment,uint8_t*finalEnvironment){
    drawnQuads=drawnChains=drawnLines=0;drawCounts={};drawnValid=false;if(!valid||!spiderman_effect_scene_ready())return 0;
    try{Prepared p;if(!prepare(view,p))return fail("Combined original effect preparation/order rejected");const int native[2]={320,240};uint8_t finalValue=initialEnvironment;if(!spiderman_effects_gl_draw_ordered(renderer.get(),view,projection,viewport,native,p.references.data(),p.references.size(),initialEnvironment,&finalValue))return fail(spiderman_web_gl_last_error(renderer.get()));for(const auto&n:p.order){if(n.kind==SPIDERMAN_EFFECT_PRIMARY_CHAIN||n.kind==SPIDERMAN_EFFECT_SECONDARY_CHAIN){++drawnChains;++drawCounts.traversal_strand_chains;}else if(n.kind==SPIDERMAN_EFFECT_ATTACK_LINE){++drawnLines;++drawCounts.attack_lines;}else{++drawnQuads;if(n.kind==SPIDERMAN_EFFECT_TRAIL_QUAD)++drawCounts.trail_quads;else if(n.kind==SPIDERMAN_EFFECT_ATTACK_QUAD)++drawCounts.attack_quads;else if(n.kind==SPIDERMAN_EFFECT_DOME_SHATTER)++drawCounts.dome_shatter_triangles;else if(p.webQuads[n.index].texture_slot==46)++drawCounts.traversal_knot_quads;else ++drawCounts.traversal_splat_quads;}}drawCounts.source_tick=tick;drawnValid=true;if(finalEnvironment)*finalEnvironment=finalValue;std::snprintf(status,sizeof status,"Original combined source frame %u rendered",unsigned(tick));return 1;}catch(...){return fail("Combined original effect allocation/preparation failed");}
}
extern "C" int spiderman_effect_scene_draw_counts(SpidermanEffectDrawCounts*out){if(!out)return 0;*out=drawCounts;return drawnValid?1:0;}
extern "C" const char*spiderman_effect_scene_status(void){return status;}
extern "C" unsigned spiderman_effect_scene_drawn_quads(void){return drawnQuads;}
extern "C" unsigned spiderman_effect_scene_drawn_strand_chains(void){return drawnChains;}
extern "C" unsigned spiderman_effect_scene_drawn_lines(void){return drawnLines;}
#ifdef SPIDERMAN_TESTING
extern "C" int spiderman_effect_scene_test_order(const float view[16],SpidermanEffectOrderProbe*out,size_t capacity,size_t*count){try{if(!count)return 0;Prepared p;if(!prepare(view,p)||capacity<p.order.size()||(!out&&!p.order.empty()))return 0;for(size_t i=0;i<p.order.size();++i){const auto&n=p.order[i];uint32_t slot=0;if(n.kind==SPIDERMAN_EFFECT_WEB_QUAD)slot=p.webQuads[n.index].texture_slot;else if(n.kind==SPIDERMAN_EFFECT_TRAIL_QUAD)slot=41;else if(n.kind==SPIDERMAN_EFFECT_ATTACK_QUAD||n.kind==SPIDERMAN_EFFECT_DOME_SHATTER)slot=p.attackQuads[n.index].submitted.texture_slot;out[i]={n.list,n.serial,uint32_t(n.kind),slot};}*count=p.order.size();return 1;}catch(...){return 0;}}
#endif
