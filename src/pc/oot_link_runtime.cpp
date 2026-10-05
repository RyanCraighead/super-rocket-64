#include "oot_link_runtime.h"
#include "gfx/oot_link_gl.h"
#include "../../codex/oot/movement/oot_movement.h"
#include "utils/json.hpp"
#include "utils/oot_asset_path.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

using nlohmann::json;
namespace {
constexpr const char *REVISION = "52a510f379afd143aaa0375be9f1e190369572e1";
constexpr float BINANG_RAD = 6.2831853071795864769f / 65536.0f;
constexpr int LEFT_HAND_LIMB = 15;
struct Pose { int16_t joint[22][3]; };
struct Track { std::vector<Pose> frames; };
struct Required { const char *name; int count; };
const Required required[] = {
    {"normal_wait",89}, {"normal_walk",29}, {"fighter_run",20},
    {"normal_jump",9}, {"normal_run_jump",13},
    {"fighter_normal_kiru",5}, {"fighter_normal_kiru_end",8}, {"fighter_normal_kiru_endR",9},
    {"fighter_normal_kiru_finsh",6}, {"fighter_normal_kiru_finsh_end",19}, {"anchor_normal_kiru_finsh_endR",19},
    {"fighter_Lside_kiru",5}, {"fighter_Lside_kiru_end",9}, {"anchor_Lside_kiru_endR",8},
    {"fighter_Lside_kiru_finsh",9}, {"fighter_Lside_kiru_finsh_end",15}, {"anchor_Lside_kiru_finsh_endR",15},
    {"fighter_Rside_kiru",5}, {"fighter_Rside_kiru_end",12}, {"anchor_Rside_kiru_endR",18},
    {"fighter_Rside_kiru_finsh",7}, {"fighter_Rside_kiru_finsh_end",18}, {"anchor_Rside_kiru_finsh_endR",19},
    {"fighter_pierce_kiru",4}, {"fighter_pierce_kiru_end",9}, {"anchor_pierce_kiru_endR",9},
    {"fighter_pierce_kiru_finsh",10}, {"fighter_pierce_kiru_finsh_end",19}, {"anchor_pierce_kiru_finsh_endR",32},
    {"fighter_rolling_kiru",13}, {"fighter_rolling_kiru_end",16}, {"anchor_rolling_kiru_endR",14}
};
struct State {
    OotLinkGL *renderer = nullptr;
    bool loaded = false, attached = false, drawable = false, drawn = false, paused = false;
    bool cameraDeferred = false;
    bool press = false, committed = true, sourceTick = false, wasGrounded = true;
    OotMoveState move{}, preActionMove{};
    OotLinkInput pendingInput{};
    bool finished = true;
    bool preActionMelee = false, lunge = false;
    OotMoveConfig config = oot_move_config_kokiri(false);
    OotMoveClock30 clock{};
    OotMoveGaitState gait{};
    OotSwordStickHistory sticks{};
    OotSwordCombo combo{};
    OotSwordEdgeHistory edges[3]{};
    OotSwordQuad quads[2]{};
    std::map<std::string,Track> tracks;
    std::string poseTrack, poseSecondaryTrack, assetProfile;
    float poseFrame=0, poseSecondaryFrame=0, poseBlend=0;
    std::string track, status = "Original OoT Link disabled";
    Pose pose{};
    float position[3]{}, frame = 0;
    float previousRoot[3]{-57,3377,0};
    int16_t previousYaw = 0;
    int attack = -1, weaponState = 0;
    bool recovery = false, animMovement = false;
    uint64_t ticks = 0, swing = 0;
} s;
void require(bool yes, const char *message) { if (!yes) throw std::runtime_error(message); }
std::string canonical(const std::string &path) { return oot_asset_path::canonical(path); }
std::string childPath(const std::string &root, const std::string &relative) {
    return oot_asset_path::child(root, relative, 2047);
}
std::vector<unsigned char> readFile(const std::string &path, size_t cap) {
    return oot_asset_path::readFile(path, cap);
}
int16_t signed16(int value) { unsigned v = static_cast<unsigned>(value)&65535; return static_cast<int16_t>(v < 32768 ? static_cast<int>(v) : static_cast<int>(v)-65536); }
std::string fullName(const char *shortName) { return std::string("gPlayerAnim_link_")+shortName; }
const Track &track(const std::string &name) {
    auto i=s.tracks.find(name); require(i!=s.tracks.end(),"Required original animation missing"); return i->second;
}
void sample(const std::string &name, float frame, Pose &pose) {
    const auto &t=track(name); size_t index=static_cast<size_t>(std::max(0.0f,frame));
    pose=t.frames[std::min(index,t.frames.size()-1)];
    if(&pose==&s.pose) {
        s.poseTrack=name; s.poseFrame=frame; s.poseSecondaryTrack.clear();
        s.poseSecondaryFrame=0; s.poseBlend=0;
    }
}
void blend(Pose &a, const Pose &b, float weight) {
    /* AnimTask_Interp uses signed-short deltas for every Vec3s including root. */
    for(int i=0;i<22;++i) for(int j=0;j<3;++j) {
        int16_t delta=signed16(static_cast<int>(b.joint[i][j])-a.joint[i][j]);
        a.joint[i][j]=signed16(a.joint[i][j]+static_cast<int>(delta*weight));
    }
}
void clearEdges() { std::memset(s.edges,0,sizeof(s.edges)); std::memset(s.quads,0,sizeof(s.quads)); s.weaponState=0; }
void cancelCombat() {
    clearEdges(); s.attack=-1; s.recovery=false; s.animMovement=false; s.press=false; s.lunge=false;
    oot_sword_combo_reset(&s.combo); oot_sword_stick_reset(&s.sticks);
}
void idle() { s.track=fullName("normal_wait"); s.frame=0; sample(s.track,s.frame,s.pose); }
void finishRoot() {
    if(s.animMovement) {
        s.move.shape_yaw=signed16(s.move.shape_yaw+s.pose.joint[1][1]);
        s.move.move_yaw=s.move.shape_yaw; s.pose.joint[1][1]=0;
    }
    s.animMovement=false;
}
void startAttack(bool targeting) {
    finishRoot();
    int requested=oot_sword_select_attack(&s.sticks,targeting);
    int attack=oot_sword_combo_start(&s.combo,requested);
    s.press=false;
    if(attack<0) { cancelCombat(); idle(); return; }
    s.lunge=requested!=OOT_SWORD_SPIN && s.sticks.directions[s.sticks.index]==0;
    s.attack=attack; s.recovery=false; s.frame=0; s.track=oot_sword_attack_info(attack)->track;
    s.move.speed_xz=0; s.move.move_yaw=s.move.shape_yaw;
    s.previousRoot[0]=-57; s.previousRoot[1]=3377; s.previousRoot[2]=0;
    s.previousYaw=s.move.shape_yaw; s.animMovement=true; ++s.swing; clearEdges();
    sample(s.track,0,s.pose);
}
void rootDelta(float delta[3]) {
    if(!s.animMovement) return;
    float sn=oot_move_sin(s.move.shape_yaw), cs=oot_move_cos(s.move.shape_yaw);
    float oldSn=oot_move_sin(s.previousYaw),oldCs=oot_move_cos(s.previousYaw);
    float x=s.pose.joint[0][0], z=s.pose.joint[0][2];
    delta[0]+=(x*cs+z*sn-s.previousRoot[0]*oldCs-s.previousRoot[2]*oldSn)*.01f*OOT_LINK_WORLD_SCALE;
    delta[2]+=(z*cs-x*sn-s.previousRoot[2]*oldCs+s.previousRoot[0]*oldSn)*.01f*OOT_LINK_WORLD_SCALE;
    s.previousRoot[0]=x; s.previousRoot[2]=z; s.previousYaw=s.move.shape_yaw;
    /* Horizontal animation movement belongs to the actor. Keeping it in the
       skin too would double movement and desynchronise the hand hitbox. */
    s.pose.joint[0][0]=-57; s.pose.joint[0][2]=0;
}
void values(OotLinkStep *out) {
    out->attacking=s.attack>=0 && !s.recovery;
    out->shape_yaw=s.move.shape_yaw;
    out->host_forward_velocity=oot_move_host30_velocity(s.move.speed_xz,OOT_LINK_WORLD_SCALE);
    out->host_y_velocity=oot_move_host30_velocity(s.move.velocity_y,OOT_LINK_WORLD_SCALE);
    out->swing_id=s.swing;
}
OotSwordVec3 transform(const float *m,OotSwordVec3 p) {
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}
void rendererError(void *,const char *message) {
    s.status=message && *message ? message : "Original Link renderer error (no detail)";
    std::fprintf(stderr,"[OoT Link renderer] %s\n",s.status.c_str());
}
}

extern "C" int oot_link_runtime_init(const char *directory) {
    oot_link_runtime_shutdown();
    if(!directory || !*directory) return 0;
    try {
        const auto root=canonical(directory);
        const auto bytes=readFile(childPath(root,"manifest.json"),8*1024*1024);
        json manifest=json::parse(bytes.begin(),bytes.end());
        require(manifest.at("schema_version")==1 && manifest.at("source_revision")==REVISION,"Unsupported OoT extraction manifest");
        const auto &rom=manifest.at("rom");
        const std::string md5=rom.at("normalized_md5").get<std::string>();
        const std::string profile=rom.value("profile",std::string());
        const bool original10=(profile.empty() || profile=="ntsc-1.0-US") &&
            (md5=="5bd1fe107bf8106b2ab6650abecd54d6" || md5=="6829a16db1a34e8ce989847cd8da8d9a");
        const bool original12=profile=="ntsc-1.2-US" &&
            md5=="57a9719ad547c516342e1a15d5c28c3d" &&
            rom.value("normalized_sha1",std::string())=="41b3bdc48d98c48529219919015a1af22f5057c2" &&
            rom.value("normalized_sha256",std::string())=="49acd3885f13b0730119b78fb970911cc8aba614fe383368015c21565983368d";
        require(original10 || original12,"Verified US OoT 1.0 or strict US 1.2 extraction profile required");
        s.assetProfile=original12 ? "ntsc-1.2-US" : "ntsc-1.0-US";
        const auto &segment=manifest.at("segments").at("link_animetion");
        auto animationBytes=readFile(childPath(root,segment.at("file").get<std::string>()),16*1024*1024);
        require(segment.at("size").get<size_t>()==animationBytes.size(),"Animation segment length mismatch");
        const auto &animations=manifest.at("animations");
        require(animations.is_array() && animations.size()<=1000,"Invalid animation manifest");
        std::map<std::string,json> specs;
        for(const auto &entry:animations) {
            std::string name=entry.at("name").get<std::string>();
            require(specs.emplace(name,entry).second,"Duplicate animation entry");
        }
        for(const auto &need:required) {
            std::string name=fullName(need.name),dataName=name+"_Data";
            auto found=specs.find(dataName); require(found!=specs.end(),("Missing original track: "+dataName).c_str());
            const auto &entry=found->second;
            require(entry.at("frame_count")==need.count && entry.at("frame_stride")==134,"Original animation frame count/stride mismatch");
            auto raw=readFile(childPath(root,entry.at("raw_file").get<std::string>()),4096*134);
            size_t offset=entry.at("offset").get<size_t>();
            require(raw.size()==static_cast<size_t>(need.count)*134 && offset<=animationBytes.size() && raw.size()<=animationBytes.size()-offset,"Original animation span mismatch");
            require(std::equal(raw.begin(),raw.end(),animationBytes.begin()+offset),"Animation bytes differ from original extracted segment");
            Track t; t.frames.resize(need.count);
            for(int frame=0;frame<need.count;++frame) for(int j=0;j<66;++j) {
                size_t p=static_cast<size_t>(frame)*134+j*2;
                t.frames[frame].joint[j/3][j%3]=signed16(raw[p]*256+raw[p+1]);
            }
            s.tracks.emplace(name,std::move(t));
        }
        s.renderer=oot_link_gl_create(rendererError,nullptr); require(s.renderer!=nullptr,"Cannot create original Link renderer");
        auto meshPath=childPath(root,"mesh-adult/mesh.json");
        if(!oot_link_gl_load(s.renderer,meshPath.c_str())) {
            const char *detail=oot_link_gl_last_error(s.renderer);
            throw std::runtime_error(std::string("Original Link mesh validation failed: ")+
                (detail && *detail ? detail : "renderer returned failure without detail"));
        }
        s.loaded=true; cancelCombat(); idle();
        s.status=std::string("Original adult OoT Link loaded: ")+(original12 ? "US NTSC 1.2 assets" : "US NTSC 1.0 assets")+
            "; NTSC 1.0-derived bounded ground/air/one-handed sword behavior; native SM64 collision";
        return 1;
    } catch(const std::exception &e) {
        std::string error=e.what(); oot_link_runtime_shutdown(); s.status="Original OoT Link disabled: "+error; return 0;
    }
}
extern "C" void oot_link_runtime_shutdown(void) {
    if(s.renderer) oot_link_gl_destroy(s.renderer);
    s=State{};
}
extern "C" int oot_link_runtime_enabled(void) { return s.loaded; }
extern "C" int oot_link_runtime_visible(void) { return s.loaded && s.drawable && s.drawn; }
extern "C" const char *oot_link_runtime_status(void) { return s.status.c_str(); }
extern "C" int oot_link_runtime_snapshot(OotLinkSnapshot *out) {
    if(!out) return 0;
    std::memset(out,0,sizeof(*out));
    out->enabled=s.loaded; out->attached=s.attached; out->drawable=s.drawable;
    out->visible=oot_link_runtime_visible(); out->paused=s.paused;
    out->source_tick=s.sourceTick; out->source_frame_complete=s.committed && s.finished;
    out->source_ticks=s.ticks; out->swing_id=s.swing;
    out->attack_id=s.attack; out->recovery=s.recovery; out->weapon_state=s.weaponState;
    out->active_quads=s.loaded && s.finished && s.sourceTick ? static_cast<int>(s.quads[0].valid)+static_cast<int>(s.quads[1].valid) : 0;
    std::copy(s.position,s.position+3,out->position);
    out->source_speed_xz=s.move.speed_xz; out->source_velocity_y=s.move.velocity_y;
    out->shape_yaw=s.move.shape_yaw; out->animation_frame=s.poseFrame;
    out->secondary_frame=s.poseSecondaryFrame; out->secondary_weight=s.poseBlend;
    auto text=[](char *to,size_t size,const std::string &from) {
        size_t n=std::min(size-1,from.size()); std::memcpy(to,from.data(),n); to[n]=0;
    };
    text(out->animation,sizeof(out->animation),s.poseTrack);
    text(out->secondary_animation,sizeof(out->secondary_animation),s.poseSecondaryTrack);
    text(out->asset_profile,sizeof(out->asset_profile),s.assetProfile);
    return 1;
}

extern "C" void oot_link_runtime_suspend(void) {
    cancelCombat(); s.attached=false; s.drawable=false; s.clock.phase=0; s.sourceTick=false; s.committed=true; s.finished=true;
}
extern "C" int oot_link_runtime_present(const float *position,int16_t yaw) {
    if(!s.loaded||!position)return 0;
    for(int k=0;k<3;k++)if(!std::isfinite(position[k]))return 0;
    std::copy(position,position+3,s.position);s.move.shape_yaw=yaw;s.drawable=true;
    return 1;
}
extern "C" void oot_link_runtime_pause(void) {
    cancelCombat(); s.clock.phase=0; s.paused=true; s.sourceTick=false; s.committed=true; s.finished=true;
}
extern "C" int oot_link_runtime_draw(const float view[16],const float projection[16],const int viewport[4]) {
    if(!s.loaded || !s.drawable) return 0;
    // Native transitions can temporarily lack a camera/viewport (for example
    // before the first world render). Defer drawing without destroying valid
    // loaded assets or source state; native Mario remains available meanwhile.
    bool finiteView=view!=nullptr, finiteProjection=projection!=nullptr;
    if(view) for(int i=0;i<16;++i) if(!std::isfinite(view[i])) finiteView=false;
    if(projection) for(int i=0;i<16;++i) if(!std::isfinite(projection[i])) finiteProjection=false;
    const bool cameraReady=finiteView && finiteProjection && viewport && viewport[2]>0 && viewport[3]>0;
    if(!cameraReady) {
        if(!s.cameraDeferred) std::fprintf(stderr,
            "[OoT Link runtime] Draw deferred: view=%s projection=%s viewport=%s %dx%d\n",
            !view ? "missing" : finiteView ? "finite" : "non-finite",
            !projection ? "missing" : finiteProjection ? "finite" : "non-finite",
            viewport ? "present" : "missing",viewport ? viewport[2] : 0,viewport ? viewport[3] : 0);
        s.cameraDeferred=true; s.drawn=false; return 0;
    }
    s.cameraDeferred=false;
    int result=oot_link_gl_draw(s.renderer,view,projection,viewport,s.position,s.move.shape_yaw*BINANG_RAD,OOT_LINK_WORLD_SCALE,s.pose.joint,s.pose.joint,0);
    if(result) s.drawn=true;
    else {
        const char *detail=oot_link_gl_last_error(s.renderer);
        const std::string cause=detail && *detail ? detail : "renderer returned failure without detail";
        s.loaded=false; s.drawn=false; s.drawable=false; cancelCombat();
        s.status="Original Link drawing failed; original-player override disabled: "+cause;
        std::fprintf(stderr,"[OoT Link runtime] %s\n",s.status.c_str());
    }
    return result;
}
extern "C" int oot_link_runtime_step(const OotLinkInput *input,OotLinkStep *out) {
    if(!s.loaded || !input || !out) return 0;
    std::memset(out,0,sizeof(*out));
    for(float value:input->position) if(!std::isfinite(value)) { oot_link_runtime_suspend(); return 0; }
    if(!std::isfinite(input->stick_magnitude) || !std::isfinite(input->host_forward_velocity) || !std::isfinite(input->host_y_velocity)) { oot_link_runtime_suspend(); return 0; }
    std::memset(s.quads,0,sizeof(s.quads));
    if(!s.attached) {
        oot_move_init(&s.move,input->facing_yaw); oot_move_gait_reset(&s.gait);
        s.move.speed_xz=input->host_forward_velocity/OOT_LINK_WORLD_SCALE;
        s.move.velocity_y=input->grounded ? 0 : input->host_y_velocity/OOT_LINK_WORLD_SCALE;
        s.attached=true; s.wasGrounded=input->grounded; idle();
    }
    if(s.paused) { s.paused=false; idle(); }
    std::copy(input->position,input->position+3,s.position); s.drawable=true;
    s.press=s.press || input->attack_pressed;
    s.sourceTick=oot_move_clock30_tick(&s.clock); out->source_tick=s.sourceTick; s.committed=false; s.finished=false; s.pendingInput=*input;
    if(!s.sourceTick) { values(out); return 1; }
    ++s.ticks;
    oot_sword_combo_pre_tick(&s.combo); oot_sword_combo_release(&s.combo,input->attack_held);
    oot_sword_stick_push(&s.sticks,input->stick_magnitude,input->stick_yaw,input->world_yaw,s.move.shape_yaw);
    s.preActionMove=s.move; s.preActionMelee=s.attack>=0 && !s.recovery;
    OotMoveDelta actor=oot_move_actor_delta(&s.move,&s.config);
    out->displacement[0]=actor.x*OOT_LINK_WORLD_SCALE;
    out->displacement[1]=input->grounded ? 0 : actor.y*OOT_LINK_WORLD_SCALE;
    out->displacement[2]=actor.z*OOT_LINK_WORLD_SCALE;
    values(out); return 1;
}
extern "C" void oot_link_runtime_commit(const OotLinkCollision *collision,OotLinkStep *out) {
    if(!s.loaded || !s.attached || !collision || !out || s.committed) return;
    s.committed=true;
    for(float value:collision->position) if(!std::isfinite(value)) { oot_link_runtime_suspend(); return; }
    std::copy(collision->position,collision->position+3,s.position);
    if(!s.sourceTick) return;
    const OotLinkInput *input=&s.pendingInput;
    const bool grounded=collision->grounded!=0;
    if(grounded) s.move.velocity_y=0;
    if(collision->hit_ceiling && s.move.velocity_y>0) s.move.velocity_y=0;
    if(collision->hit_wall) s.move.speed_xz=0;
    if(collision->left_ground) {
        OotAutojumpContext jump{true,false,s.preActionMelee,false,collision->floor_forbids_jump!=0,collision->distance_to_floor/OOT_LINK_WORLD_SCALE};
        if(oot_move_can_autojump(&s.preActionMove,&jump)) s.move.velocity_y=oot_move_autojump_impulse(&s.preActionMove,&s.config);
        // Source collision chooses air before ground input/action dispatch.
        // This also discards a B edge on the leaving-ground tick: no grounded
        // sword can begin, and therefore no canceled root movement can leak.
        cancelCombat();
        s.track=fullName(s.move.speed_xz>4 && std::abs(static_cast<int>(signed16(s.move.move_yaw-s.move.shape_yaw)))<0x1000 ? "normal_run_jump" : "normal_jump");
        s.frame=0; sample(s.track,s.frame,s.pose);
    }
    OotMoveInput movement{input->stick_magnitude,input->world_yaw,input->floor_pitch,s.config.speed_cap,false};
    if(grounded && s.press && (s.attack<0 || s.recovery)) startAttack(input->targeting);
    else if(s.attack>=0) {
        s.weaponState=s.recovery ? 0 : oot_sword_weapon_state(s.attack,s.frame);
        // STATE2_30 forward-input lunge in Player_Action_808502D0.
        if(!s.recovery && s.lunge && s.frame==0) { s.move.speed_xz=15; s.lunge=false; }
        s.move.speed_xz=std::max(0.0f,s.move.speed_xz-(s.recovery ? 1.5f : 5.0f));
        bool complete=oot_sword_animation_once(&s.frame,static_cast<float>(track(s.track).frames.size()-1),s.recovery ? 1.0f : 2.0f/3.0f);
        if(complete) {
            if(!s.recovery && s.press && grounded) startAttack(input->targeting);
            else if(!s.recovery) {
                s.recovery=true; clearEdges(); s.frame=0;
                const auto *attack=oot_sword_attack_info(s.attack);
                s.track=input->targeting ? attack->target_recovery : attack->recovery;
            } else { finishRoot(); cancelCombat(); idle(); }
        }
        sample(s.track,s.frame,s.pose);
    } else if(!grounded) {
        if(s.wasGrounded || (s.track!=fullName("normal_jump") && s.track!=fullName("normal_run_jump"))) {
            if(!collision->left_ground) s.track=fullName(s.move.speed_xz>4 && std::abs(static_cast<int>(signed16(s.move.move_yaw-s.move.shape_yaw)))<0x1000 ? "normal_run_jump" : "normal_jump");
            s.frame=0;
        } else oot_sword_animation_once(&s.frame,static_cast<float>(track(s.track).frames.size()-1),2.0f/3.0f);
        sample(s.track,s.frame,s.pose);
        oot_move_air_tick(&s.move,&s.config,&movement,false);
    } else {
        if(!s.wasGrounded) { s.move.velocity_y=0; s.move.action=OOT_MOVE_IDLE; idle(); }
        OotMoveAction before=s.move.action;
        if(before==OOT_MOVE_RUN) {
            auto gait=oot_move_gait_tick(&s.gait,s.move.speed_xz,false); Pose run;
            sample(fullName("normal_walk"),gait.walk_frame,s.pose); sample(fullName("fighter_run"),gait.run_frame,run); blend(s.pose,run,gait.run_weight);
            s.poseSecondaryTrack=fullName("fighter_run"); s.poseSecondaryFrame=gait.run_frame; s.poseBlend=gait.run_weight;
        } else { s.track=fullName("normal_wait"); s.frame=std::fmod(s.frame+1.0f,89.0f); sample(s.track,s.frame,s.pose); }
        oot_move_ground_tick(&s.move,&s.config,&movement);
        if(before!=OOT_MOVE_RUN && s.move.action==OOT_MOVE_RUN) oot_move_gait_reset(&s.gait);
    }
    /* A press is sampled on one source update only. Holding B is represented
       separately by the original combo hold timer; no long-lived input queue. */
    s.press=false;
    rootDelta(out->animation_displacement);
    s.wasGrounded=grounded; values(out);
}
extern "C" void oot_link_runtime_finish(const OotLinkCollision *collision,OotLinkStep *out) {
    if(!s.loaded || !s.attached || !collision || !out || !s.committed || s.finished) return;
    s.finished=true;
    for(float value:collision->position) if(!std::isfinite(value)) { oot_link_runtime_suspend(); return; }
    std::copy(collision->position,collision->position+3,s.position);
    if(!s.sourceTick) return;
    if(collision->grounded) s.move.velocity_y=0;
    if(collision->hit_ceiling && s.move.velocity_y>0) s.move.velocity_y=0;
    if(collision->hit_wall) s.move.speed_xz=0;
    if(collision->left_ground && s.attack>=0) cancelCombat();
    values(out);
    if(!s.weaponState || s.attack<0 || s.recovery) { clearEdges(); return; }
    float matrix[16];
    if(!oot_link_gl_limb_transform(s.renderer,s.position,s.move.shape_yaw*BINANG_RAD,OOT_LINK_WORLD_SCALE,s.pose.joint,LEFT_HAND_LIMB,matrix)) { clearEdges(); return; }
    OotSwordVec3 tips[3],bases[3]; oot_sword_local_edges(4000,&s.combo.repeat_count,tips,bases);
    for(int i=0;i<3;++i) {
        if(i && s.weaponState<=0) continue;
        OotSwordQuad q{}; oot_sword_sweep(&s.edges[i],transform(matrix,tips[i]),transform(matrix,bases[i]),&q);
        if(i && s.weaponState>0) s.quads[i-1]=q;
    }
}
extern "C" int oot_link_runtime_quads(OotSwordQuad out[2],uint64_t *swing) {
    if(!out || !s.loaded || !s.committed || !s.finished || !s.sourceTick) return 0;
    out[0]=s.quads[0]; out[1]=s.quads[1]; if(swing) *swing=s.swing;
    return (out[0].valid ? 1:0)+(out[1].valid ? 1:0);
}
#ifdef OOT_LINK_RUNTIME_TESTING
extern "C" int oot_link_runtime_test_pose(int16_t out[22][3]) { if(!out||!s.loaded)return 0;std::memcpy(out,s.pose.joint,sizeof(s.pose.joint));return 1; }
extern "C" float oot_link_runtime_test_frame(void) { return s.frame; }
extern "C" int oot_link_runtime_test_attack(void) { return s.attack; }
extern "C" uint64_t oot_link_runtime_test_ticks(void) { return s.ticks; }
#endif
