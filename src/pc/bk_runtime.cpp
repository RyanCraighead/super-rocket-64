/* Original Banjo-Kazooie asset/pose bridge. Host conversion is explicit; source
 * animation IDs and normalized timers come directly from the BK movement port.
 * No fallback geometry, skeleton, or animation is generated at runtime. */
#include "bk_runtime.h"
#include "bk_pose.h"
#include "gfx/bk_duo_gl.h"
#include "utils/oot_asset_path.h"
#include "utils/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using Json=nlohmann::json;
struct Model {
    BkDuoGL *renderer=nullptr;
    bk::Rig rig;
    ~Model(){bk_duo_gl_destroy(renderer);}
};
std::unique_ptr<Model> player;
BkRenderSnapshot current={};
bool loaded=false,drawable=false,drawn=false;
std::string status="Original Banjo-Kazooie disabled";
Json readJson(const std::string &path){
    auto bytes=oot_asset_path::readFile(path,32*1024*1024);
    return Json::parse(bytes.begin(),bytes.end(),[](int depth,Json::parse_event_t,Json &){if(depth>32)throw std::runtime_error("BK JSON nesting limit");return true;});
}
void checkSource(const Json &source){
    const auto &rom=source.at("rom");
    if(rom.at("profile")!="bk-us-rev1"||rom.at("normalized_sha1")!="ded6ee166e740ad1bc810fd678a84b48e245ab80"||rom.at("normalized_sha256")!="f9ad43f64c3a38b0ca4067e24f570dcb8f3f8fec077c5c342484bcbec7ff6d22"||rom.at("size")!=16777216)throw std::runtime_error("Wrong original Banjo-Kazooie ROM profile");
}
std::unique_ptr<Model> loadModel(const std::string &base){
    std::string meshPath=oot_asset_path::child(base,"player/mesh.json",240);
    std::string animationPath=oot_asset_path::child(base,"player/animations.json",240);
    Json mesh=readJson(meshPath),animations=readJson(animationPath);
    checkSource(mesh.at("source"));checkSource(animations.at("source"));
    const auto &model=mesh.at("source").at("model");
    if(model.at("asset_id")!=0x34e||model.at("sha256")!="dc79df5e2efcb0c6e8e89727683919cf848a9f378180c6f99aa54f3ce3744bb8"||model.at("decoded_size")!=162264)throw std::runtime_error("Wrong original Banjo-Kazooie model asset");
    if(mesh.at("skeleton")!=animations.at("skeleton"))throw std::runtime_error("Original mesh/animation pivot skeleton mismatch");
    std::unique_ptr<Model> result(new Model);
    result->rig=bk::Rig::load(animationPath);
    const auto &skeleton=mesh.at("skeleton"),&bones=skeleton.at("bones");
    if(!bones.is_array()||bones.size()!=result->rig.bones.size()||skeleton.at("translation_scale").get<float>()!=result->rig.translation_scale)throw std::runtime_error("Original decoded skeleton changed while loading");
    for(size_t i=0;i<bones.size();++i){const auto &bone=bones[i];const auto &rig=result->rig.bones[i];
        if(bone.at("index")!=i||bone.at("parent")!=rig.parent||bone.at("bone_id")!=rig.bone_id)throw std::runtime_error("Original mesh/animation bone mismatch");
        const auto &pivot=bone.at("pivot");if(!pivot.is_array()||pivot.size()!=3)throw std::runtime_error("Invalid original pivot");
        for(int k=0;k<3;++k)if(!pivot[k].is_number()||pivot[k].get<float>()!=rig.pivot[k])throw std::runtime_error("Original mesh/animation pivot mismatch");
    }
    /* These are source asset IDs used by the bounded original on-foot slice. */
    const int required[]={0x6f,0x2,0x3,0xc,0x8,0xb0,0x1,0x10c,0x16,0x26,0x15,0x27,0x7,0x4b,0x4c,0x61,0x18,0x17,0x5,0x4f,0x1c,0x1a,0x19,0x1d};
    for(int id:required){size_t count=0;for(const auto &a:result->rig.animations)if(a.id==id)++count;if(count!=1)throw std::runtime_error("Missing or duplicate original movement animation asset "+std::to_string(id));}
    result->renderer=bk_duo_gl_create(nullptr,nullptr);
    if(!result->renderer||!bk_duo_gl_load(result->renderer,meshPath.c_str()))throw std::runtime_error(bk_duo_gl_last_error(result->renderer));
    return result;
}
bool finite3(const float *v){return v&&std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
}
extern "C" int bk_runtime_init(const char *directory){
    bk_runtime_shutdown();
    try{
        if(!directory||!*directory)throw std::runtime_error("missing original asset directory");
        player=loadModel(oot_asset_path::canonical(directory));
        loaded=true;status="Original Banjo-Kazooie assets loaded";return 1;
    }catch(const std::exception &e){std::string why=e.what();bk_runtime_shutdown();status="Original Banjo-Kazooie disabled: "+why;return 0;}
}
extern "C" void bk_runtime_shutdown(void){loaded=drawable=drawn=false;player.reset();current={};status="Original Banjo-Kazooie disabled";}
extern "C" int bk_runtime_enabled(void){return loaded;}
extern "C" int bk_runtime_visible(void){return loaded&&drawable&&drawn;}
extern "C" const char *bk_runtime_status(void){return status.c_str();}
extern "C" void bk_runtime_suspend(void){drawable=drawn=false;current={};}
extern "C" void bk_runtime_submit(const BkRenderSnapshot *s){
    if(!loaded||!s||!finite3(s->position)||!std::isfinite(s->yaw)||!std::isfinite(s->animation_time)||s->animation_time<0||s->animation_time>1||s->action<0||s->action>255||(s->model_flags&~7u)){bk_runtime_suspend();return;}
    int index=-1;for(size_t i=0;i<player->rig.animations.size();++i)if(player->rig.animations[i].id==s->animation_asset){index=int(i);break;}
    if(index<0){bk_runtime_suspend();return;}
    current=*s;current.animation=index;drawable=true;
}
extern "C" int bk_runtime_snapshot(BkRenderSnapshot *s){if(!loaded||!drawable||!s)return 0;*s=current;return 1;}
extern "C" int bk_runtime_draw(const float view[16],const float projection[16],const int viewport[4]){
    if(!loaded||!drawable)return 0;
    /* Startup can provide a zero viewport before the first world camera. */
    if(!view||!projection||!viewport||viewport[2]<=0||viewport[3]<=0)return 0;
    for(int i=0;i<16;++i)if(!std::isfinite(view[i])||!std::isfinite(projection[i]))return 0;
    try{
        int selectors[64]={};for(int i=18;i<=41;++i)selectors[i]=1;
        for(int i:{1,9,12,15})selectors[i]=(current.model_flags&BK_RENDER_KAZOOIE_UPPER)?1:0;
        for(int i:{2,10,13,16})selectors[i]=(current.model_flags&BK_RENDER_KAZOOIE_FEET)?1:0;
        if(!bk_duo_gl_set_selectors(player->renderer,selectors,64))throw std::runtime_error("Invalid original model appendage selection");
        float yaw=current.yaw+((current.model_flags&BK_RENDER_KAZOOIE_DIRECTION)?180.0f:0.0f);
        /* core2/ba/model.c adds two source units without moving collision. */
        float world[3]={current.position[0],current.position[1]+2.0f*BK_HOST_SCALE,current.position[2]};
        std::vector<bk::Matrix> joints,billboards;
        auto pose=player->rig.sample_progress(current.animation,current.animation_time);
        if(!player->rig.palettes(pose,view,world,yaw,BK_HOST_SCALE,1.0f,joints,billboards)||joints.empty()||billboards.size()!=joints.size())throw std::runtime_error("Original Banjo-Kazooie animation palette evaluation failed");
        if(!bk_duo_gl_draw(player->renderer,view,projection,viewport,joints[0].data(),billboards[0].data(),joints.size()))throw std::runtime_error(bk_duo_gl_last_error(player->renderer));
        drawn=true;status="Original Banjo-Kazooie active";return 1;
    }catch(const std::exception &e){status="Original Banjo-Kazooie draw disabled: "+std::string(e.what());loaded=drawable=drawn=false;std::fprintf(stderr,"%s\n",status.c_str());return 0;}
}
