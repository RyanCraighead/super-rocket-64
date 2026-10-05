/* Windowless ordering test: real native display-list construction, real GBI
 * NOOP encoding, actual PC boundary/camera and SDL character dispatch bodies.
 * Render sinks record order; no OpenGL context, game, assets or actors run. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "game/area.h"
#include "pc/gfx/gfx_pc.h"
#include "pc/network/network.h"
static void test_behind_hook(void);
#define smlua_call_event_hooks(...) test_behind_hook()
#include "pc/cliopts.h"
#include "pc/character_net.h"
#include "game/character_presentation.h"

enum { TEST_WORLD=0x1001,TEST_BEHIND,TEST_HUD,TEST_LABEL,TEST_DIALOG,TEST_TRANSITION,TEST_CLEAR };
static Gfx commands[256];
Gfx *gDisplayListHead;
Vp gViewportFullscreen;
Vp *gViewportOverride,*gViewportClip;
u32 gFBSetColor,gWarpTransFBSetColor;
s16 gWarpTransDelay,gPauseScreenMode,gSaveOptSelectIndex;
struct Area *gCurrentArea;
struct WarpTransition gWarpTransition;
struct ServerSettings gServerSettings;
struct CLIOptions gCLIOpts;
bool gDjuiDisabled,gDjuiInMainMenu;
static struct Area area;
static char order[128];
static unsigned events,checks,localDraws,presentationDraws,remoteDraws,otherDraws,flushes;
static int pendingWorld,presentationAvailable,otherPresentation,worldHasCamera=1;
static GfxCodexWorldDraw sCodexWorldDraw;
static bool sCodexWorldDrawn,sCodexCameraValid,sCodexInterpolatedViewValid;
static bool sCodexWorldEnvironmentValid,sCodexWorldEnvironmentActive;
static Mat4 sCodexView,sCodexProjection,sCodexInterpolatedView;
static int sCodexViewport[4];
static void *ctx=(void*)1,*currentContext=(void*)1;
static const float *expectedView;

#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s; order=%s\n",__LINE__,#x,order);return 1;}}while(0)
static void event(char value){assert(events+1<sizeof order);order[events++]=value;order[events]=0;}
static void emit(unsigned tag){gDPNoOpTag(gDisplayListHead++,tag);}
static void gfx_flush(void){if(pendingWorld){event('W');pendingWorld=0;}flushes++;}
static void gfx_codex_capture_world_environment(void){sCodexWorldEnvironmentValid=true;sCodexWorldEnvironmentActive=false;}
SDL_GLContext SDL_GL_GetCurrentContext(void){return currentContext;}
static void camera_matches(const float *v,const float *p,const int *viewport){
    assert(memcmp(v,expectedView,sizeof(Mat4))==0);
    assert(memcmp(p,sCodexProjection,sizeof(Mat4))==0);
    assert(memcmp(viewport,sCodexViewport,sizeof sCodexViewport)==0);
    assert(!pendingWorld&&sCodexWorldEnvironmentValid);
}
int rocket_runtime_draw(const float v[16],const float p[16],const int viewport[4]){camera_matches(v,p,viewport);localDraws++;event('L');return 1;}
int character_presentation_car_snapshot(RocketSnapshot *out){(void)out;return presentationAvailable;}
void character_presentation_draw(const float *v,const float *p,const int *viewport){
    if(presentationAvailable){camera_matches(v,p,viewport);presentationDraws++;event('P');}
    else if(otherPresentation){otherDraws++;event('O');}
}
void character_net_draw(const float v[16],const float p[16],const int viewport[4]){camera_matches(v,p,viewport);remoteDraws++;event('R');}
void dynos_update_gfx(void){}
void geo_process_root(struct GraphNodeRoot *root,Vp *a,Vp *b,s32 color){(void)root;(void)a;(void)b;(void)color;emit(TEST_WORLD);}
void djui_reset_hud_params(void){}
void create_dl_ortho_matrix(void){}
void djui_gfx_displaylist_begin(void){}
void djui_gfx_displaylist_end(void){}
void nametags_render(void){}
static void test_behind_hook(void){emit(TEST_BEHIND);}
void render_hud(void){emit(TEST_HUD);}
void render_text_labels(void){emit(TEST_LABEL);}
void do_cutscene_handler(void){}
void print_displaying_credits_entry(void){}
s16 render_menus_and_dialogs(void){emit(TEST_DIALOG);return 0;}
void make_viewport_clip_rect(Vp *viewport){(void)viewport;}
s32 render_screen_transition(s8 timer,s8 type,u8 time,struct WarpTransitionData *data){(void)timer;(void)type;(void)time;(void)data;emit(TEST_TRANSITION);return 0;}
void clear_frame_buffer(s32 color){(void)color;emit(TEST_CLEAR);}
void clear_viewport(Vp *viewport,s32 color){(void)viewport;(void)color;emit(TEST_CLEAR);}
void set_warp_transition_rgb(u8 red,u8 green,u8 blue){(void)red;(void)green;(void)blue;}

#include "world_draw_native.inc"

static void replay(void){
    for(Gfx *cmd=commands;cmd<gDisplayListHead;cmd++){
        if((cmd->words.w0>>24)!=G_NOOP)continue;
        uintptr_t tag=cmd->words.w1;
        switch(tag){
            case TEST_WORLD:pendingWorld=1;sCodexCameraValid=worldHasCamera;sCodexWorldEnvironmentActive=true;break;
            case TEST_BEHIND:gfx_flush();event('B');break;
            case TEST_HUD:gfx_flush();event('H');break;
            case TEST_LABEL:gfx_flush();event('T');break;
            case TEST_DIALOG:gfx_flush();event('D');break;
            case TEST_TRANSITION:gfx_flush();event('F');break;
            case TEST_CLEAR:gfx_flush();event('C');break;
            default:test_native_noop_dispatch(cmd);break;
        }
    }
    gfx_flush();
}
static void fresh(void){
    memset(&gCLIOpts,0,sizeof gCLIOpts);memset(&gWarpTransition,0,sizeof gWarpTransition);
    memset(&gServerSettings,0,sizeof gServerSettings);
    gDisplayListHead=commands;gCurrentArea=&area;gDjuiDisabled=gDjuiInMainMenu=false;gWarpTransDelay=0;
    gViewportOverride=gViewportClip=NULL;
    events=localDraws=presentationDraws=remoteDraws=otherDraws=flushes=0;order[0]=0;
    pendingWorld=presentationAvailable=otherPresentation=0;worldHasCamera=1;ctx=currentContext=(void*)1;
    sCodexInterpolatedViewValid=false;
    for(int i=0;i<16;i++){(&sCodexView[0][0])[i]=i+.25f;(&sCodexProjection[0][0])[i]=i+40.f;(&sCodexInterpolatedView[0][0])[i]=i+80.f;}
    sCodexViewport[0]=15;sCodexViewport[1]=20;sCodexViewport[2]=1280;sCodexViewport[3]=720;
    expectedView=&sCodexView[0][0];gfx_codex_world_frame_begin();gfx_codex_set_world_draw(gfx_sdl_draw_rocket_world);
}
int main(void){
    fresh();gCLIOpts.rocketCar=gCLIOpts.characterNet=true;gWarpTransition.isActive=1;
    render_game();CHECK(events==0);replay();CHECK(!strcmp(order,"WLRBHTDF"));
    CHECK(localDraws==1&&remoteDraws==1&&presentationDraws==0);
    gfx_sdl_draw_other_presentation();CHECK(!strcmp(order,"WLRBHTDF"));
    /* Duplicate command/list invocations in this display must not redraw cars. */
    gfx_codex_world_noop(GFX_CODEX_WORLD_CHARACTERS_TAG);CHECK(localDraws==1&&remoteDraws==1);
    /* The same logic tick can produce another interpolated display. */
    gfx_codex_world_frame_begin();sCodexInterpolatedViewValid=true;expectedView=&sCodexInterpolatedView[0][0];
    events=0;order[0]=0;replay();CHECK(!strcmp(order,"WLRBHTDF"));CHECK(localDraws==2&&remoteDraws==2);

    fresh();gCLIOpts.rocketCar=gCLIOpts.characterNet=true;presentationAvailable=1;gWarpTransition.isActive=1;
    render_game();replay();gfx_sdl_draw_other_presentation();CHECK(!strcmp(order,"WPRBHTDF"));
    CHECK(localDraws==0&&presentationDraws==1&&remoteDraws==1);
    fresh();gCLIOpts.characterNet=true;render_game();replay();CHECK(!strcmp(order,"WRBHTD"));CHECK(remoteDraws==1&&localDraws==0);
    fresh();render_game();replay();CHECK(!strcmp(order,"WBHTD"));CHECK(localDraws+remoteDraws+presentationDraws==0);
    /* Null backend hook (dummy/DX), wrong context, camera and viewport gates. */
    fresh();gCLIOpts.rocketCar=true;gfx_codex_set_world_draw(NULL);render_game();replay();CHECK(!strcmp(order,"WBHTD"));CHECK(localDraws==0);
    fresh();gCLIOpts.rocketCar=true;currentContext=(void*)2;render_game();replay();CHECK(localDraws==0);
    fresh();gCLIOpts.rocketCar=true;ctx=NULL;render_game();replay();CHECK(localDraws==0);
    fresh();gCLIOpts.rocketCar=true;worldHasCamera=0;render_game();replay();CHECK(localDraws==0);
    fresh();gCLIOpts.rocketCar=true;sCodexViewport[2]=0;render_game();replay();CHECK(localDraws==0);
    /* A title/blank/paused world emits no boundary and cannot reuse a camera. */
    fresh();gCLIOpts.rocketCar=gCLIOpts.characterNet=true;gCurrentArea=NULL;render_game();replay();CHECK(!strcmp(order,"BTC"));CHECK(localDraws+remoteDraws==0);
    fresh();gCLIOpts.rocketCar=true;gWarpTransition.pauseRendering=1;render_game();replay();CHECK(localDraws==0);
    fresh();gCLIOpts.rocketCar=true;sCodexCameraValid=true;gfx_codex_world_noop(123);CHECK(!sCodexWorldDrawn&&localDraws==0);
    fresh();gCLIOpts.rocketCar=gCLIOpts.characterNet=true;render_game_n64();replay();CHECK(!strcmp(order,"WBHTD"));CHECK(localDraws+remoteDraws==0);
    /* Unrelated legacy presentation retains its original swap-stage path. */
    fresh();otherPresentation=1;render_game();replay();gfx_sdl_draw_other_presentation();CHECK(!strcmp(order,"WBHTDO"));CHECK(otherDraws==1);
    printf("PASS: %u world/HUD ordering, driving/presentation/remote dispatch, replay and backend checks\n",checks);
    return 0;
}
