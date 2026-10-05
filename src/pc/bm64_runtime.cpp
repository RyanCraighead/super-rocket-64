#include "bm64_runtime.h"
#include "bm64_pose.h"
#include "gfx/bm64_bomberman_gl.h"
#include "utils/oot_asset_path.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

namespace {
struct Model {
    Bm64BombermanGL *renderer=nullptr;
    bm64::Rig rig;
    ~Model(){bm64_bomberman_gl_destroy(renderer);}
};
std::unique_ptr<Model> player,bombs[18];
Bm64RenderSnapshot current={};
bool loaded=false,drawable=false,drawn=false;
std::string status="Original Bomberman disabled";
std::unique_ptr<Model> loadModel(const std::string &base,const std::string &folder,int id){
    std::string path=oot_asset_path::child(base,folder+"/mesh.json",240);
    auto bytes=oot_asset_path::readFile(path,32*1024*1024);
    auto j=nlohmann::json::parse(bytes.begin(),bytes.end(),[](int depth,nlohmann::json::parse_event_t,nlohmann::json &){if(depth>32)throw std::runtime_error("BM64 JSON nesting limit");return true;});
    if(j.at("source").at("rom").at("normalized_sha1")!="8a7648d8105ac4fc1ad942291b2ef89aeca921c9"||j.at("source").at("model").at("model_id")!=id)throw std::runtime_error("Wrong original Bomberman source profile/model");
    std::unique_ptr<Model> model(new Model);
    model->rig=bm64::Rig::load(oot_asset_path::child(base,folder+"/animations.json",240));
    const auto &skeleton=j.at("skeleton"),&limbs=skeleton.at("limbs");
    if(!limbs.is_array()||limbs.size()!=model->rig.limbs.size()||skeleton.at("root")!=model->rig.root)throw std::runtime_error("Original mesh/animation skeleton mismatch");
    for(size_t i=0;i<limbs.size();++i){
        const auto &source=limbs[i];const auto &rig=model->rig.limbs[i];
        auto link=[](int value)->nlohmann::json{return value<0?nlohmann::json(nullptr):nlohmann::json(value);};
        if(source.at("index")!=i||source.at("parent")!=link(rig.parent)||source.at("child")!=link(rig.child)||source.at("sibling")!=link(rig.sibling))throw std::runtime_error("Original mesh/animation hierarchy mismatch");
        int k=0;for(const char *key:{"translation","rotation","scale"}){const auto &values=source.at(key);if(!values.is_array()||values.size()!=3)throw std::runtime_error("Invalid original mesh transform");for(const auto &value:values){if(!value.is_number()||value.get<float>()!=rig.bind[k++])throw std::runtime_error("Original mesh/animation bind transform mismatch");}}
    }
    for(size_t i=0;i<model->rig.animations.size();++i)if(model->rig.animations[i].id!=int(i))throw std::runtime_error("Original animation IDs differ from their table indices");
    model->renderer=bm64_bomberman_gl_create(nullptr,nullptr);
    if(!model->renderer||!bm64_bomberman_gl_load(model->renderer,path.c_str()))throw std::runtime_error(bm64_bomberman_gl_last_error(model->renderer));
    return model;
}
bool finite3(const float *v){return v&&std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
int draw(Model &m,const std::vector<bm64::Transform> &pose,const float view[16],const float projection[16],const int viewport[4],const float position[3],float yaw,float scale,float opacity=1){
    std::vector<bm64::Matrix> joints,billboards;
    if(!m.rig.palettes(pose,view,position,yaw,BM64_HOST_SCALE,scale,joints,billboards))return 0;
    bm64_bomberman_gl_set_opacity(m.renderer,opacity);
    return bm64_bomberman_gl_draw(m.renderer,view,projection,viewport,joints[0].data(),billboards[0].data(),joints.size());
}
}
extern "C" int bm64_runtime_init(const char *directory){
    bm64_runtime_shutdown();
    try{
        if(!directory||!*directory)throw std::runtime_error("missing original asset directory");
        std::string base=oot_asset_path::canonical(directory);player=loadModel(base,"player",73);
        if(player->rig.limbs.size()!=37||player->rig.animations.size()!=27)throw std::runtime_error("Incomplete original player animation rig");
        for(int i=0;i<6;++i){char folder[32];std::snprintf(folder,sizeof folder,"bomb_%02d",i);bombs[i]=loadModel(base,folder,i);}
        for(int i:{6,16,17}){char folder[32];std::snprintf(folder,sizeof folder,"effect_%02d",i);bombs[i]=loadModel(base,folder,i);}
        loaded=true;status="Original Bomberman64 assets loaded";return 1;
    }catch(const std::exception &e){std::string why=e.what();bm64_runtime_shutdown();status="Original Bomberman64 disabled: "+why;return 0;}
}
extern "C" void bm64_runtime_shutdown(void){loaded=drawable=drawn=false;player.reset();for(auto &b:bombs)b.reset();current={};status="Original Bomberman64 disabled";}
extern "C" int bm64_runtime_enabled(void){return loaded;}
extern "C" int bm64_runtime_visible(void){return loaded&&drawable&&drawn;}
extern "C" const char *bm64_runtime_status(void){return status.c_str();}
extern "C" void bm64_runtime_suspend(void){drawable=drawn=false;current={};}
extern "C" void bm64_runtime_submit(const Bm64RenderSnapshot *s){
    if(!loaded||!s||!finite3(s->position)||!std::isfinite(s->yaw)){bm64_runtime_suspend();return;}
    for(int i=0;i<2;++i)if(s->animation[i]<-1||s->animation[i]>=27||!std::isfinite(s->frame[i])||s->frame[i]<0){bm64_runtime_suspend();return;}
    for(const auto &b:s->bombs)if(b.active&&(!finite3(b.position)||!std::isfinite(b.scale)||b.scale<=0||b.scale>20||b.model<0||b.model>=18||!bombs[b.model]||!std::isfinite(b.yaw)||b.opacity<0||b.opacity>255)){bm64_runtime_suspend();return;}
    current=*s;drawable=true;
}
extern "C" int bm64_runtime_snapshot(Bm64RenderSnapshot *s){if(!loaded||!drawable||!s)return 0;*s=current;return 1;}
extern "C" int bm64_runtime_draw(const float view[16],const float projection[16],const int viewport[4]){
    if(!loaded||!drawable)return 0;
    /* The first scene camera can legitimately have a zero viewport. Defer it;
     * do not discard the actor pose before the next valid world frame. */
    if(!view||!projection||!viewport||viewport[2]<=0||viewport[3]<=0)return 0;
    for(int i=0;i<16;++i)if(!std::isfinite(view[i])||!std::isfinite(projection[i]))return 0;
    try{
        std::vector<std::pair<int,float>> channels;for(int i=0;i<2;++i)if(current.animation[i]>=0)channels.push_back({current.animation[i],current.frame[i]});
        if(!draw(*player,player->rig.sample_layers(channels),view,projection,viewport,current.position,current.yaw,1))throw std::runtime_error(bm64_bomberman_gl_last_error(player->renderer));
        for(const auto &b:current.bombs)if(b.active&&!draw(*bombs[b.model],bombs[b.model]->rig.sample(-1,0),view,projection,viewport,b.position,b.yaw,b.scale,b.model>=6?b.opacity/255.0f:1.0f))throw std::runtime_error(bm64_bomberman_gl_last_error(bombs[b.model]->renderer));
        drawn=true;status="Original Bomberman64 active";return 1;
    }catch(const std::exception &e){status="Original Bomberman draw disabled: "+std::string(e.what());loaded=drawable=drawn=false;std::fprintf(stderr,"%s\n",status.c_str());return 0;}
}
