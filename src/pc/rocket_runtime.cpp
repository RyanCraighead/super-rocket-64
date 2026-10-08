/* Original private Octane geometry, approximate RocketSim physics and host paint.
 * No Psyonix source or decoded assets are present in this translation unit. */
#include "rocket_runtime.h"
#include "rocket_boost.h"
#include "player_bump.h"
#include "rocket_audio.h"
extern "C" {
#include "game/rocket_wing.h"
#include "game/rocket_penguin.h"
#include "game/rocket_squish_visual.h"
#include "game/level_update.h"
}
#include <string>
#ifdef ROCKET_CAR
#include "utils/json.hpp"
#include "utils/oot_asset_path.h"
#include "utils/rocket_sha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include "gfx/rocket_gl_state.h"
extern "C" {
#include "cliopts.h"
#include "configfile.h"
#include "sm64.h"
extern const unsigned char mario_texture_metal_shade[],mario_texture_metal_light[];
}
namespace {
struct Vertex {float p[3],n[3],color[4];};
struct DrawVertex {float p[3],uv[2],color[4];};
static_assert(sizeof(Vertex)==40,"Unexpected mesh vertex layout");
using Matrix=std::array<float,16>;
std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> world(nullptr,rocket_world_destroy);
std::vector<Vertex> body,wheel;
std::vector<DrawVertex> stream;
RocketSnapshot current={};
RocketGamepad gamepad={};
RocketInput lastInput={};
GLFunctions gl;
SDL_GLContext context=nullptr;
GLuint program=0,vbo=0,vao=0,metalTexture=0;
GLint uMVP=-1,uMetal=-1,uMetalTexture=-1;
GLint uCaps=-1;
bool drawable=false;
uint32_t epoch=0;
uint32_t capVisuals=0;
float spin[4]={};
std::string status="Rocket car disabled";
bool finite(const float *values,size_t count){if(!values)return false;for(size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
Matrix multiply(const Matrix &a,const Matrix &b){Matrix out={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];return out;}
void releaseGL(){
    if(context&&SDL_GL_GetCurrentContext()==context){if(program)gl.DeleteProgram(program);if(vbo)gl.DeleteBuffers(1,&vbo);if(vao)gl.DeleteVertexArrays(1,&vao);if(metalTexture)gl.DeleteTextures(1,&metalTexture);}
    program=vbo=vao=metalTexture=0;context=nullptr;
}
GLuint shader(GLenum type,const std::string &source){
    GLuint s=gl.CreateShader(type);const char *p=source.c_str();gl.ShaderSource(s,1,&p,nullptr);gl.CompileShader(s);GLint ok=0;gl.GetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024]={};gl.GetShaderInfoLog(s,1023,nullptr,log);gl.DeleteShader(s);throw std::runtime_error(std::string("Rocket shader: ")+log);}return s;
}
void initGL(){
    if(!SDL_GL_GetCurrentContext())throw std::runtime_error("Rocket renderer needs OpenGL");
    if(context==SDL_GL_GetCurrentContext()&&program)return;
    if(context)throw std::runtime_error("Rocket renderer context changed");
    if(!gl.load())throw std::runtime_error("Rocket OpenGL functions unavailable");
    GLState saved(gl);context=SDL_GL_GetCurrentContext();
    std::string prefix=gl.es?"#version 100\nprecision mediump float;\n":gl.modern?"#version 130\n":"#version 120\n";
    std::string vs=prefix+(gl.modern?"in vec3 aPosition;in vec2 aUV;in vec4 aColor;out vec4 vColor;out vec2 vUV;":"attribute vec3 aPosition;attribute vec2 aUV;attribute vec4 aColor;varying vec4 vColor;varying vec2 vUV;");
    vs+="uniform mat4 uMVP;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vColor=aColor;vUV=aUV;}";
    std::string fs=prefix+(gl.modern?"in vec4 vColor;in vec2 vUV;out vec4 outputColor;":"varying vec4 vColor;varying vec2 vUV;");
    fs+="uniform vec4 uCaps;uniform bool uMetal;uniform sampler2D uMetalTexture;void main(){if(uCaps.x>0.5 && mod(floor(gl_FragCoord.x)+floor(gl_FragCoord.y),2.0)>0.5)discard;vec4 color=vColor;if(uMetal){vec2 uv=clamp(vUV,0.0,1.0);uv=vec2((uv.x*63.0+0.5)/64.0,(uv.y*31.0+0.5)/64.0);vec3 shade=";
    fs+=(gl.modern?"texture":"texture2D");fs+="(uMetalTexture,uv).rgb;vec3 light=";
    fs+=(gl.modern?"texture":"texture2D");fs+="(uMetalTexture,uv+vec2(0.0,0.5)).rgb;color.rgb=clamp(shade*0.5+light,0.0,1.0);}";
    fs+=(gl.modern?"outputColor=color;}":"gl_FragColor=color;}");
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=0;
    try{f=shader(GL_FRAGMENT_SHADER,fs);}catch(...){gl.DeleteShader(v);throw;}
    program=gl.CreateProgram();gl.AttachShader(program,v);gl.AttachShader(program,f);gl.BindAttribLocation(program,0,"aPosition");gl.BindAttribLocation(program,1,"aUV");gl.BindAttribLocation(program,2,"aColor");gl.LinkProgram(program);gl.DeleteShader(v);gl.DeleteShader(f);
    GLint ok=0;gl.GetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Rocket shader link failed");
    uMVP=gl.GetUniformLocation(program,"uMVP");uCaps=gl.GetUniformLocation(program,"uCaps");gl.GenBuffers(1,&vbo);if(gl.vaoSupported)gl.GenVertexArrays(1,&vao);
    uMetal=gl.GetUniformLocation(program,"uMetal");uMetalTexture=gl.GetUniformLocation(program,"uMetalTexture");
    /* Reuse the native Mario 64x32 environment textures, packed vertically to
     * keep both lookups on unit 0 covered by the existing GL state guard.
     * Native metal combines TEXEL0 * SHADE + TEXEL1 (silver ambient = 0.5).
     * These linked host textures never enter the Octane asset export/package. */
    unsigned char pixels[64*64*4];
    for(int part=0;part<2;part++)for(int i=0;i<64*32;i++) {
        const unsigned char *source=part?mario_texture_metal_light:mario_texture_metal_shade;
        unsigned color=(source[i*2]<<8)|source[i*2+1];
        for(int channel=0;channel<3;channel++)pixels[(part*64*32+i)*4+channel]=
            (unsigned char)(((color>>(11-channel*5))&31)*255/31);
        pixels[(part*64*32+i)*4+3]=255;
    }
    gl.ActiveTexture(GL_TEXTURE0);if(gl.samplers)gl.BindSampler(0,0);
    if(gl.unpackBuffer)gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(gl.unpackRows){gl.PixelStorei(GL_UNPACK_ROW_LENGTH,0);gl.PixelStorei(GL_UNPACK_SKIP_ROWS,0);gl.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    gl.GenTextures(1,&metalTexture);gl.BindTexture(GL_TEXTURE_2D,metalTexture);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
}
std::vector<Vertex> loadPart(const std::string &base,const nlohmann::json &part,const char *name){
    if(part.at("name")!=name||!part.at("vertices").is_number_unsigned())throw std::runtime_error("Invalid Octane part");
    size_t count=part.at("vertices").get<size_t>();
    if(count<3||count>500000||count%3)throw std::runtime_error("Invalid Octane triangle count");
    auto data=oot_asset_path::readFile(oot_asset_path::child(base,part.at("file").get<std::string>(),240),count*40,count*40);
    const char *expected=std::strcmp(name,"body")==0?"e1162f1643ad34857fda1284b5ecac7af7d7172069c968b2eb9473b9fdb523df":"46adadb342a27e3b39309087c051d68efcccf7e6cfccf8341a7866062720ff28";
    if(part.at("sha256")!=expected||rocket_assets::sha256(data)!=expected)throw std::runtime_error("Original Octane geometry hash mismatch");
    std::vector<Vertex> result(count);std::memcpy(result.data(),data.data(),data.size());
    for(const auto &v:result){
        if(!finite(v.p,3)||!finite(v.n,3)||!finite(v.color,4))throw std::runtime_error("Nonfinite Octane vertex");
        float norm=0;for(int i=0;i<3;++i){if(std::fabs(v.p[i])>200||std::fabs(v.n[i])>1.01f)throw std::runtime_error("Octane vertex out of range");norm+=v.n[i]*v.n[i];}
        if(norm<.5f||norm>1.5f)throw std::runtime_error("Invalid Octane normal");
        for(float c:v.color)if(c<0||c>1)throw std::runtime_error("Invalid Octane color");
    }
    return result;
}
void append(const std::vector<Vertex> &vertices,const float position[3],const float basis[9],float scale,const float *view,const float pivot[3],const float squish[3],int which=-1){
    float roll=which<0?0:spin[which],c=std::cos(roll),s=std::sin(roll);
    for(const Vertex &v:vertices){
        float p[3]={v.p[0],v.p[1],v.p[2]},n[3]={v.n[0],v.n[1],v.n[2]};
        if(which>=0){ // Visual tire spin only; contact/steering/centers come from the physics backend.
            for(float *vec:{p,n}){float x=vec[0],z=vec[2];vec[0]=c*x+s*z;vec[2]=-s*x+c*z;}
        }
        DrawVertex out={};float dot=0,normal[3]={};const float light[3]={.26726124f,.80178373f,.53452248f};
        for(int k=0;k<3;++k){out.p[k]=position[k];for(int q=0;q<3;++q){out.p[k]+=basis[q*3+k]*p[q]*scale;normal[k]+=basis[q*3+k]*n[q];}}
        rocket_squish_vertex(out.p,normal,pivot,squish);
        for(int k=0;k<3;k++)dot+=normal[k]*light[k];
        /* Native G_TEXTURE_GEN uses view-space normal Y for S and X for T. */
        out.uv[0]=.5f+.5f*(view[1]*normal[0]+view[5]*normal[1]+view[9]*normal[2]);
        out.uv[1]=.5f+.5f*(view[0]*normal[0]+view[4]*normal[1]+view[8]*normal[2]);
        float brightness=.40f+.60f*std::max(0.f,dot);for(int k=0;k<3;++k)out.color[k]=v.color[k]*brightness;out.color[3]=1;
        stream.push_back(out);
    }
}
}
extern "C" int rocket_runtime_init(void){
    rocket_runtime_shutdown();
    try{
        std::string base=oot_asset_path::canonical(gCLIOpts.characterNet ? gCLIOpts.characterNetAssets : gCLIOpts.rocketAssets);
        auto bytes=oot_asset_path::readFile(oot_asset_path::child(base,"model.json",240),65536);
        auto j=nlohmann::json::parse(bytes.begin(),bytes.end(),[](int depth,nlohmann::json::parse_event_t,nlohmann::json &){if(depth>16)throw std::runtime_error("Octane JSON depth");return true;});
        if(j.at("schema")!="octane-host-mesh-v1"||j.at("body_sha256")!="bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6"||j.at("wheel_sha256")!="9b2582f69e6bf2fd06272b9b545dfd33cfc931f31d373b63a1078a747d902560"||j.at("parts").size()!=2)throw std::runtime_error("Unsupported original Octane profile");
        body=loadPart(base,j.at("parts")[0],"body");wheel=loadPart(base,j.at("parts")[1],"wheel");
        if(gCLIOpts.rocketCar||(gCLIOpts.characterNet&&gCLIOpts.characterWheel)){
            world.reset(rocket_world_create());if(!world)throw std::runtime_error(rocket_world_error());
        }
        if(!gCLIOpts.headless)rocket_audio_load(base.c_str());
        status="Original Octane geometry; approximate RocketSim physics and host materials";std::fprintf(stderr,"%s\n",status.c_str());return 1;
    }catch(const std::exception &e){std::string why=e.what();rocket_runtime_shutdown();status="Rocket car disabled: "+why;return 0;}
}
extern "C" void rocket_runtime_shutdown(void){rocket_audio_shutdown();releaseGL();world.reset();body.clear();wheel.clear();stream.clear();drawable=false;capVisuals=0;current={};gamepad={};lastInput={};status="Rocket car disabled";}
extern "C" int rocket_runtime_enabled(void){return world?1:0;}
extern "C" int rocket_runtime_boost_mode(void){return rocket_wing_boost_mode();}
extern "C" int rocket_runtime_set_boost_mode(int mode){
    if(!rocket_world_set_boost_mode(world.get(),mode))return 0;
    if(drawable)rocket_world_snapshot(world.get(),&current);
    return 1;
}
extern "C" int rocket_runtime_collect_coin(void){
    if(!rocket_world_collect_coin(world.get()))return 0;
    if(drawable)rocket_world_snapshot(world.get(),&current);
    return 1;
}
static uint32_t appliedRule;
extern "C" int rocket_runtime_rule_ready(void){
    return world&&rocket_world_speed(world.get())==rocket_speed_percent()&&
        rocket_world_jump_height(world.get())==rocket_jump_percent()&&appliedRule==rocket_rule_revision();
}
extern "C" uint32_t rocket_runtime_epoch(void){return epoch;}
extern "C" void rocket_runtime_selection_changed(void){
    rocket_audio_stop();
    if(++epoch==0)++epoch;
    rocket_world_interrupt(world.get());
}
extern "C" int rocket_runtime_owns_controls(void){return world&&drawable;}
extern "C" void rocket_runtime_gamepad(const RocketGamepad *pad){
    RocketGamepad next=pad?*pad:RocketGamepad{};
    if(next.ui_blocked||gamepad.connected!=next.connected||(next.connected&&gamepad.instance!=next.instance)){rocket_audio_stop();rocket_world_interrupt(world.get());}
    gamepad=next;
}
extern "C" void rocket_runtime_last_input(RocketInput *input){if(input)*input=lastInput;}
extern "C" int rocket_runtime_read_selected_input(const RocketInput *keyboard,RocketInput *input){
    if(!keyboard||!input)return 0;
    *input=rocket_gamepad_merge(keyboard,&gamepad);
    if(!world||!SDL_GetKeyboardFocus()||gamepad.ui_blocked){*input={};return 0;}
    return 1;
}
extern "C" int rocket_runtime_read_input(const RocketInput *keyboard,RocketInput *input){
    if(!drawable){if(input)*input={};return 0;}
    return rocket_runtime_read_selected_input(keyboard,input);
}
extern "C" void rocket_runtime_suspend(void){rocket_audio_stop();drawable=false;capVisuals=0;rocket_world_set_environment(world.get(),nullptr);rocket_world_set_water_query(world.get(),nullptr);rocket_world_set_water(world.get(),0,0,0);rocket_world_interrupt(world.get());}
extern "C" void rocket_runtime_set_cap_visuals(uint32_t flags){capVisuals=flags&MARIO_SPECIAL_CAPS;}
extern "C" int rocket_runtime_set_environment(const RocketEnvironment *environment){return rocket_world_set_environment(world.get(),environment);}
extern "C" void rocket_runtime_set_metal_water(int active){rocket_world_set_metal_water(world.get(),active);}
extern "C" void rocket_runtime_interrupt(void){rocket_audio_stop();rocket_world_interrupt(world.get());}
extern "C" void rocket_runtime_set_water(int present,float level,int metal){
    rocket_world_set_water(world.get(),present,level,metal);
    RocketSnapshot state;if(drawable&&rocket_world_snapshot(world.get(),&state))current.water_mode=state.water_mode;
}
extern "C" void rocket_runtime_set_water_current(const float velocity[3]){rocket_world_set_water_current(world.get(),velocity);}
extern "C" void rocket_runtime_set_water_query(RocketWaterQuery query){rocket_world_set_water_query(world.get(),query);}
extern "C" int rocket_runtime_mesh(int layer,const RocketTriangle *triangles,size_t count){return rocket_world_mesh(world.get(),layer,triangles,count);}
extern "C" int rocket_runtime_platforms(const RocketPlatform *platforms,size_t count){return rocket_world_platforms(world.get(),platforms,count);}
extern "C" int rocket_runtime_reset(const float *position,const float *velocity,float yaw){
    rocket_audio_reset();
    ++epoch;drawable=false;capVisuals=0;std::fill(spin,spin+4,0.f);return rocket_world_reset(world.get(),position,velocity,yaw);
}
extern "C" int rocket_runtime_recover(const RocketSnapshot *pose){
    rocket_audio_stop();
    if(!rocket_world_recover(world.get(),pose))return 0;
    ++epoch; // Remote interpolation must not sweep a recovered car through the gate.
    return rocket_world_snapshot(world.get(),&current);
}
extern "C" int rocket_runtime_frame(uint64_t frame,const RocketInput *input,int paused,int blocked){
    blocked=blocked||!SDL_GetKeyboardFocus()||gamepad.ui_blocked;
    if(!input)return -1;
    lastInput=rocket_gamepad_merge(input,&gamepad);
    rocket_penguin_filter_input(&lastInput);
    const unsigned percent=rocket_speed_percent(),jump=rocket_jump_percent();const uint32_t rule=rocket_rule_revision();
    if(world&&(rocket_world_speed(world.get())!=percent||rocket_world_jump_height(world.get())!=jump||appliedRule!=rule)){
        rocket_world_set_speed(world.get(),percent);rocket_world_set_jump_height(world.get(),jump);appliedRule=rule;
        if(!++epoch)++epoch;
        player_bump_clear(0); // Retire grants without resetting physical state or fuel.
    }
    rocket_world_set_boost_mode(world.get(), rocket_boost_mode());
    rocket_world_set_temporary_boost(world.get(), rocket_wing_active(0));
    rocket_world_set_surface_mode(world.get(), rocket_surface_mode());
    int result=rocket_world_frame(world.get(),frame,&lastInput,paused,blocked);
    if(result>=0&&rocket_world_snapshot(world.get(),&current)){
        drawable=true;
        if(result>0){float forward=0;for(int k=0;k<3;++k)forward+=current.velocity[k]*current.basis[k];for(int i=0;i<4;++i)if(current.wheel_radius[i]>0)spin[i]=std::fmod(spin[i]+forward/(30.f*current.wheel_radius[i]),6.28318530718f);}
    }else drawable=false;
    rocket_audio_update(drawable?&current:nullptr,configRocketSoundMode,drawable&&!paused&&!blocked&&!gCLIOpts.headless);
    return result;
}
extern "C" int rocket_runtime_bump(const float delta[3]){if(!drawable||!rocket_world_bump(world.get(),delta))return 0;return rocket_world_snapshot(world.get(),&current);}
extern "C" int rocket_runtime_snapshot(RocketSnapshot *snapshot){if(!drawable||!snapshot)return 0;*snapshot=current;return 1;}
extern "C" const char *rocket_runtime_status(void){return status.c_str();}
static int drawSnapshot(const RocketSnapshot *snapshot,uint32_t nativeFlags,const float squish[3],const float *view,const float *projection,const int *viewport){
    if(!snapshot)return 0;
    const RocketSnapshot &pose=*snapshot;
    if(body.empty()||wheel.empty()||!finite(view,16)||!finite(projection,16)||!viewport||viewport[2]<=0||viewport[3]<=0)return 0;
    try{
        initGL();GLState saved(gl);stream.clear();stream.reserve(body.size()+wheel.size()*4);
        const float pivot[3]={pose.position[0],pose.position[1]-40.f,pose.position[2]};
        append(body,pose.position,pose.basis,ROCKET_HOST_SCALE,view,pivot,squish);
        for(int i=0;i<4;++i){
            float basis[9];std::copy(pose.basis,pose.basis+9,basis);
            float c=std::cos(pose.wheel_steer[i]),s=std::sin(pose.wheel_steer[i]);
            for(int k=0;k<3;++k){basis[k]=pose.basis[k]*c+pose.basis[3+k]*s;basis[3+k]=-pose.basis[k]*s+pose.basis[3+k]*c;}
            append(wheel,pose.wheel_position[i],basis,pose.wheel_radius[i]/16.f,view,pivot,squish,snapshot==&current?i:-1);
        }
        Matrix v,p;std::copy(view,view+16,v.begin());std::copy(projection,projection+16,p.begin());Matrix mvp=multiply(p,v);
        gl.UseProgram(program);gl.UniformMatrix4fv(uMVP,1,GL_FALSE,mvp.data());if(gl.vaoSupported)gl.BindVertexArray(vao);
        const float caps[4]={nativeFlags&MARIO_VANISH_CAP?1.f:0.f,0,0,0};gl.Uniform4fv(uCaps,1,caps);
        gl.Uniform1i(uMetal,!!(nativeFlags&MARIO_METAL_CAP));gl.Uniform1i(uMetalTexture,0);
        gl.ActiveTexture(GL_TEXTURE0);if(gl.samplers)gl.BindSampler(0,0);gl.BindTexture(GL_TEXTURE_2D,metalTexture);
        gl.BindBuffer(GL_ARRAY_BUFFER,vbo);gl.BufferData(GL_ARRAY_BUFFER,stream.size()*sizeof(DrawVertex),stream.data(),GL_STREAM_DRAW);
        gl.EnableVertexAttribArray(0);gl.EnableVertexAttribArray(1);gl.EnableVertexAttribArray(2);
        gl.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,uv));
        gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,p));gl.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,color));
        gl.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);gl.DepthFunc(GL_LEQUAL);gl.DepthMask(GL_TRUE);gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(gl.es)gl.DepthRangef(0,1);else gl.DepthRange(0,1);
        for(GLenum cap:CAPABILITIES){if(cap==GL_DEPTH_TEST)gl.Enable(cap);else gl.Disable(cap);}if(gl.raster)gl.Disable(GL_RASTERIZER_DISCARD);if(!gl.es)gl.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        gl.DrawArrays(GL_TRIANGLES,0,(GLsizei)stream.size());return 1;
    }catch(const std::exception &e){status="Rocket draw failed: "+std::string(e.what());std::fprintf(stderr,"%s\n",status.c_str());rocket_runtime_shutdown();status="Rocket draw failed: "+std::string(e.what());return 0;}
}
extern "C" int rocket_runtime_draw_snapshot_player(const RocketSnapshot *snapshot,unsigned index,uint32_t flags,const float *view,const float *projection,const int *viewport){
    if(index>=MAX_PLAYERS)return 0;
    float scale[3];rocket_squish_visual_scale(&gMarioStates[index],scale);
    return drawSnapshot(snapshot,flags,scale,view,projection,viewport);
}
extern "C" int rocket_runtime_draw_snapshot_caps(const RocketSnapshot *snapshot,uint32_t flags,const float *view,const float *projection,const int *viewport){
    const float scale[3]={1,1,1};return drawSnapshot(snapshot,flags,scale,view,projection,viewport);
}
extern "C" int rocket_runtime_draw(const float *view,const float *projection,const int *viewport){
    return world&&drawable?rocket_runtime_draw_snapshot_player(&current,0,capVisuals,view,projection,viewport):0;
}
extern "C" int rocket_runtime_draw_snapshot(const RocketSnapshot *snapshot,const float *view,const float *projection,const int *viewport){return rocket_runtime_draw_snapshot_player(snapshot,0,capVisuals,view,projection,viewport);}

#else
extern "C" int rocket_runtime_init(void){return 0;}
extern "C" void rocket_runtime_shutdown(void){}
extern "C" int rocket_runtime_enabled(void){return 0;}
extern "C" int rocket_runtime_set_boost_mode(int){return 0;}
extern "C" int rocket_runtime_boost_mode(void){return ROCKET_BOOST_COIN_ONLY;}
extern "C" int rocket_runtime_collect_coin(void){return 0;}
extern "C" uint32_t rocket_runtime_epoch(void){return 0;}
extern "C" int rocket_runtime_rule_ready(void){return 0;}
extern "C" void rocket_runtime_selection_changed(void){}
extern "C" int rocket_runtime_draw_snapshot(const RocketSnapshot*,const float*,const float*,const int*){return 0;}
extern "C" int rocket_runtime_draw_snapshot_caps(const RocketSnapshot*,uint32_t,const float*,const float*,const int*){return 0;}
extern "C" int rocket_runtime_draw_snapshot_player(const RocketSnapshot*,unsigned,uint32_t,const float*,const float*,const int*){return 0;}
extern "C" void rocket_runtime_set_cap_visuals(uint32_t){}
extern "C" int rocket_runtime_owns_controls(void){return 0;}
extern "C" void rocket_runtime_gamepad(const RocketGamepad*){}
extern "C" void rocket_runtime_last_input(RocketInput *input){if(input)*input=RocketInput{};}
extern "C" int rocket_runtime_read_input(const RocketInput*,RocketInput *input){if(input)*input=RocketInput{};return 0;}
extern "C" int rocket_runtime_read_selected_input(const RocketInput*,RocketInput *input){if(input)*input=RocketInput{};return 0;}
extern "C" void rocket_runtime_suspend(void){}
extern "C" int rocket_runtime_set_environment(const RocketEnvironment*){return 0;}
extern "C" void rocket_runtime_interrupt(void){}
extern "C" void rocket_runtime_set_water(int,float,int){}
extern "C" void rocket_runtime_set_water_current(const float*){}
extern "C" void rocket_runtime_set_water_query(RocketWaterQuery){}
extern "C" void rocket_runtime_set_metal_water(int){}
extern "C" int rocket_runtime_mesh(int,const RocketTriangle*,size_t){return 0;}
extern "C" int rocket_runtime_platforms(const RocketPlatform*,size_t){return 0;}
extern "C" int rocket_runtime_reset(const float*,const float*,float){return 0;}
extern "C" int rocket_runtime_recover(const RocketSnapshot*){return 0;}
extern "C" int rocket_runtime_frame(uint64_t,const RocketInput*,int,int){return -1;}
extern "C" int rocket_runtime_bump(const float*){return 0;}
extern "C" int rocket_runtime_snapshot(RocketSnapshot*){return 0;}
extern "C" int rocket_runtime_draw(const float*,const float*,const int*){return 0;}
extern "C" const char *rocket_runtime_status(void){return "Rocket car is not compiled; build with ROCKET_CAR=1";}
#endif
