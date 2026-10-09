/* Executes the production renderer in an SDL offscreen context, never the game.
 * Optional owned inputs enable private screenshots; fixtures contain no assets. */
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include "src/pc/rocket_runtime.cpp"
#include <cstdlib>
#include <iostream>

extern "C" {
const unsigned char mario_texture_metal_shade[4096]={255,255};
const unsigned char mario_texture_metal_light[4096]={127,255};
struct CLIOptions gCLIOpts={};
void rocket_audio_load(const char *) {std::abort();}
void rocket_audio_shutdown(void) {}
void rocket_world_destroy(RocketWorld *) {}
RocketWorld *rocket_world_create(void) {std::abort();}
const char *rocket_world_error(void) {return "No physics world in renderer fixture";}
}
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(0)
constexpr int W=800,H=540;
static std::vector<unsigned char> pixels() {
    std::vector<unsigned char> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());CHECK(glGetError()==GL_NO_ERROR);return p;
}
static size_t visible(const std::vector<unsigned char> &p) {size_t n=0;for(size_t i=3;i<p.size();i+=4)n+=p[i]!=0;return n;}
static void save(const std::string &name,const std::vector<unsigned char> &p) {
    std::vector<unsigned char> flipped(p.size());for(int y=0;y<H;y++)std::copy_n(p.data()+(H-1-y)*W*4,W*4,flipped.data()+y*W*4);
    CHECK(stbi_write_png(name.c_str(),W,H,4,flipped.data(),W*4)!=0);
}
int main(int argc,char **argv) {
    // Hard requirement: this test may only use the non-windowed SDL backend.
    CHECK(std::getenv("SDL_VIDEODRIVER")&&std::string(std::getenv("SDL_VIDEODRIVER"))=="offscreen");
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    const bool es=argc>1&&std::string(argv[1])=="es";
    const bool legacy=argc>1&&std::string(argv[1])=="legacy";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,es||legacy?2:3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,legacy?1:0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,es?SDL_GL_CONTEXT_PROFILE_ES:SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_Window *window=SDL_CreateWindow("Offscreen material fixture",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    CHECK(window);SDL_GLContext ctx=SDL_GL_CreateContext(window);CHECK(ctx);
    // SDL's legacy-context capability queries can leave GL_INVALID_ENUM.
    // Establish a clean boundary before executing any production GL code.
    for(GLenum error=glGetError();error!=GL_NO_ERROR;error=glGetError())std::cout<<"SDL context bootstrap error: "<<error<<"\n";
    CHECK(gl.load());CHECK(glGetError()==GL_NO_ERROR);
    CHECK(gl.es==es);CHECK(gl.modern==(!es&&!legacy));
    const bool owned=argc==5;
    if(owned) {
        CHECK(std::strlen(argv[2])<sizeof(gCLIOpts.rocketAssets));std::strcpy(gCLIOpts.rocketAssets,argv[2]);gCLIOpts.headless=true;
        CHECK(rocket_runtime_init()==1);CHECK(body.size()==rocket_materials::BODY_VERTICES);CHECK(!world);
        materials=rocket_materials::loadDirectory(argv[3]);
    } else {
        body.resize(rocket_materials::BODY_VERTICES);wheel.resize(3);
        for(Vertex &v:body){v.n[2]=1;v.color[0]=.025f;v.color[1]=.36f;v.color[2]=.95f;v.color[3]=1;}
        for(size_t start:{size_t(0),rocket_materials::CHASSIS_VERTICES}) {
            body[start].p[0]=-40;body[start].p[1]=-30;body[start+1].p[0]=40;body[start+1].p[1]=-30;body[start+2].p[2]=40;
            if(start)for(int i=0;i<3;i++)body[start+i].p[0]+=70;
        }
        materials.uv.resize(rocket_materials::BODY_VERTICES,{.25f,.5f});
        for(auto &rgba:materials.rgba){rgba.resize(2048*2048*4);for(size_t i=0;i<rgba.size();i+=4){rgba[i]=rgba[i+1]=rgba[i+2]=((i/4)%2048<1024)?255:30;rgba[i+3]=255;}}
    }
    CHECK(materials.ready());const auto material=materials;
    RocketSnapshot pose={};pose.basis[0]=1;pose.basis[5]=1;pose.basis[7]=1;
    for(int i=0;i<4;i++){pose.wheel_radius[i]=16*ROCKET_HOST_SCALE;pose.wheel_position[i][0]=(i<2?49.f:-33.f)*ROCKET_HOST_SCALE;pose.wheel_position[i][2]=(i%2?25.f:-25.f)*ROCKET_HOST_SCALE;pose.wheel_position[i][1]=-6.f*ROCKET_HOST_SCALE;}
    // Orthographic front/side view, matching model-space forward/right/up.
    float view[16]={-.86f,-.18f,.48f,0, 0,.94f,.35f,0, .51f,-.30f,.81f,0, 0,0,0,1};
    const float projection[16]={1.f/190,0,0,0,0,1.f/128,0,0,0,0,-1.f/350,0,0,-.10f,0,1};
    const int vp[4]={0,0,W,H};const float squish[3]={1,1,1};
    auto render=[&](uint32_t flags,bool clearDepth=true) {
        CHECK(glGetError()==GL_NO_ERROR);
        initGL();CHECK(glGetError()==GL_NO_ERROR);
        gl.Disable(GL_SCISSOR_TEST);gl.ColorMask(1,1,1,1);gl.DepthMask(1);glClearColor(.125f,.145f,.177f,0);
        if(es){auto clear=(void(APIENTRY *)(float))SDL_GL_GetProcAddress("glClearDepthf");CHECK(clear);clear(clearDepth?1.f:0.f);}
        else glClearDepth(clearDepth?1:0);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        // These deliberately hostile host states must be restored afterwards.
        gl.UseProgram(0);gl.Viewport(2,3,53,67);gl.Enable(GL_BLEND);gl.Enable(GL_SCISSOR_TEST);gl.DepthMask(0);gl.ActiveTexture(GL_TEXTURE3);
        gl.BlendFuncSeparate(GL_DST_COLOR,GL_ONE_MINUS_DST_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA);
        gl.BlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_SUBTRACT);
        CHECK(glGetError()==GL_NO_ERROR);
        CHECK(drawSnapshot(&pose,flags,squish,view,projection,vp)==1);
        CHECK(glGetError()==GL_NO_ERROR);
        GLint value=0,v[4];gl.GetIntegerv(GL_ACTIVE_TEXTURE,&value);CHECK(value==GL_TEXTURE3);gl.GetIntegerv(GL_CURRENT_PROGRAM,&value);CHECK(value==0);
        gl.GetIntegerv(GL_VIEWPORT,v);CHECK(v[0]==2&&v[1]==3&&v[2]==53&&v[3]==67);CHECK(gl.IsEnabled(GL_BLEND));CHECK(gl.IsEnabled(GL_SCISSOR_TEST));
        GLboolean mask=1;gl.GetBooleanv(GL_DEPTH_WRITEMASK,&mask);CHECK(!mask);
        gl.GetIntegerv(GL_BLEND_SRC_RGB,&value);CHECK(value==GL_DST_COLOR);
        gl.GetIntegerv(GL_BLEND_DST_RGB,&value);CHECK(value==GL_ONE_MINUS_DST_ALPHA);
        gl.GetIntegerv(GL_BLEND_SRC_ALPHA,&value);CHECK(value==GL_ONE_MINUS_SRC_ALPHA);
        gl.GetIntegerv(GL_BLEND_DST_ALPHA,&value);CHECK(value==GL_SRC_ALPHA);
        gl.GetIntegerv(GL_BLEND_EQUATION_RGB,&value);CHECK(value==GL_FUNC_REVERSE_SUBTRACT);
        gl.GetIntegerv(GL_BLEND_EQUATION_ALPHA,&value);CHECK(value==GL_FUNC_SUBTRACT);
        return pixels();
    };
    auto textured=render(0);CHECK(visible(textured)>1000);CHECK(visible(render(0,false))==0);
    auto vanish=render(MARIO_VANISH_CAP);CHECK(visible(vanish)>visible(textured)*.48&&visible(vanish)<visible(textured)*.52);
    auto metal=render(MARIO_METAL_CAP),metalVanish=render(MARIO_METAL_CAP|MARIO_VANISH_CAP);
    releaseGL();materials={};auto flat=render(0);CHECK(flat!=textured);CHECK(visible(flat)==visible(textured));
    CHECK(render(MARIO_METAL_CAP)==metal);CHECK(render(MARIO_METAL_CAP|MARIO_VANISH_CAP)==metalVanish);
    if(owned){save(std::string(argv[4])+"-flat.png",flat);save(std::string(argv[4])+"-textured.png",textured);}
    // Re-initialization and repeated local/remote drawing retain UV/material state.
    releaseGL();materials=material;CHECK(render(0)==textured);CHECK(render(0)==textured);
    // Actual thrust renders at the owned sockets for local AND remote poses.
    pose.boosting=1;pose.ticks=37;auto before=pose;
    auto flame=render(0);CHECK(flame!=textured);CHECK(std::memcmp(&before,&pose,sizeof pose)==0);
    CHECK(visible(flame)>visible(textured));CHECK(render(0)==flame); // frozen simulation time
    CHECK(visible(render(0,false))==0); // world depth occludes flame too
    pose.ticks=44;CHECK(render(0)!=flame);pose.ticks=37;
    auto flameVanish=render(MARIO_VANISH_CAP);CHECK(visible(flameVanish)<visible(flame)*.53);
    CHECK(visible(flameVanish)>visible(flame)*.47);
    CHECK(render(MARIO_METAL_CAP)!=metal); // exhaust keeps its emissive color
    pose.boosting=0;CHECK(render(0)==textured); // no stale flame after release
    pose.boost=100;CHECK(render(0)==textured);pose.boost=0;pose.boosting=1;
    CHECK(render(0)==flame); // backend flag includes last finite-fuel thrust tick
    if(owned){save(std::string(argv[4])+"-boost-side.png",flame);
        view[0]=.86f;view[1]=.18f;view[2]=-.48f;
        save(std::string(argv[4])+"-boost-rear.png",render(0));
        pose.boosting=0;save(std::string(argv[4])+"-rear-idle.png",render(0));}
    rocket_runtime_shutdown();CHECK(!materials.ready());CHECK(program==0&&materialTextures[0]==0&&materialTextures[1]==0);
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"Material production renderer "<<(es?"ES2":legacy?"GL2.1":"GL3")<<": "<<checks<<" checks passed\n";
}
