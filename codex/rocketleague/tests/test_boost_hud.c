/* Production HUD functions and native colorful text queue. Fill/icon sinks
 * record drawing commands; no game, window, audio, input or save runs. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hud-native-headers.inc.h"
#include "../../../src/game/print.c"

Gfx *gDisplayListHead;
const Gfx dl_hud_img_begin[]={gsSPEndDisplayList()},dl_hud_img_end[]={gsSPEndDisplayList()};
static Gfx commands[2048];
struct GfxDimensions gfx_current_dimensions;
struct GrowingPool *gDisplayListHeap;
struct MarioState gMarioStates[MAX_PLAYERS];
static struct {s16 status;} sCameraHUD;
static u8 icons[6];
u8 *main_hud_camera_lut[6]={icons,icons+1,icons+2,icons+3,icons+4,icons+5};
static struct TextLabel labels[32];
static RocketSnapshot state;
static int owned=1,ready=1,selected=1,presenting,wheel,mode,hint;
unsigned int configKeyB[MAX_BINDS]={VK_INVALID,VK_INVALID,VK_INVALID};
static struct Draw {int x,y,w,h,value,icon;} draws[64];
static unsigned drawCount,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"HUD line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void *growing_pool_alloc(struct GrowingPool *pool,u32 size){(void)pool;CHECK(size==sizeof(struct TextLabel)&&sTextLabelsCount<32);return &labels[sTextLabelsCount];}
void *segmented_to_virtual(const void *address){return (void *)address;}
int rocket_runtime_owns_controls(void){return owned;}
int rocket_runtime_snapshot(RocketSnapshot *car){if(!ready)return 0;*car=state;return 1;}
int rocket_adapter_car_selected(void){return selected;}
int character_presentation_car_snapshot(RocketSnapshot *car){if(!presenting)return 0;*car=state;return 1;}
int character_wheel_is_open(void){return wheel;}
int rocket_runtime_boost_mode(void){return mode;}
int rocket_penguin_hint(void){return hint;}
const char *translate_bind_to_name(int bind){(void)bind;return "SPACE";}
static void icon(int x,int y,int size,u8 *texture){CHECK(drawCount<64);draws[drawCount++]=(struct Draw){x,y,size,size,(int)(texture-icons),1};}
void render_hud_tex_lut(s32 x,s32 y,u8 *texture){icon(x,y,16,texture);}
void render_hud_small_tex_lut(s32 x,s32 y,u8 *texture){icon(x,y,8,texture);}
static void fill(Gfx *command,int x,int y,int right,int bottom){
    CHECK(drawCount<64);draws[drawCount++]=(struct Draw){x,y,right-x+1,bottom-y+1,(int)((command-1)->words.w1&0xffff),0};
}
#undef gDPFillRectangle
#define gDPFillRectangle(pkt,x,y,right,bottom) fill(pkt,x,y,right,bottom)
#include "hud-native.inc.c"

static void reset(void){sTextLabelsCount=0;drawCount=0;gDisplayListHead=commands;}
static void render(void){
    RocketSnapshot before=state;RocketBindings bindings=configRocketBindings;
    reset();render_rocket_boost_hud();render_hud_camera_status();
    CHECK(!memcmp(&before,&state,sizeof state)&&!memcmp(&bindings,&configRocketBindings,sizeof bindings));
}
static int has_icon(int value){for(unsigned i=0;i<drawCount;i++)if(draws[i].icon&&draws[i].value==value)return 1;return 0;}
static int color_count(int color){int n=0;for(unsigned i=0;i<drawCount;i++)n+=!draws[i].icon&&draws[i].value==color;return n;}
static int label_is(int index,const char *text){return labels[index].length==(int)strlen(text)&&!memcmp(labels[index].buffer,text,strlen(text));}
static void capture(const char *directory,const char *name){
    if(!directory)return;
    char path[1024];snprintf(path,sizeof path,"%s/%s.draw",directory,name);
    FILE *file=fopen(path,"w");CHECK(file);fprintf(file,"ASPECT %.9g\n",gfx_current_dimensions.aspect_ratio);
    for(unsigned i=0;i<drawCount;i++){struct Draw *d=&draws[i];fprintf(file,"%c %d %d %d %d %d\n",d->icon?'I':'R',d->x,d->y,d->w,d->h,d->value);}
    for(int i=0;i<sTextLabelsCount;i++)for(int j=0;j<sTextLabels[i]->length;j++){
        int glyph=char_to_glyph_index(sTextLabels[i]->buffer[j]);if(glyph<0)continue;
        fprintf(file,"G %u %u %d\n",sTextLabels[i]->x+j*12,224-sTextLabels[i]->y,glyph);
    }
    CHECK(!fclose(file));
}
int main(int argc,char **argv){
    const char *output=argc>1?argv[1]:NULL;
    rocket_bindings_reset();sCameraHUD.status=CAM_STATUS_LAKITU|CAM_STATUS_C_DOWN;
    const float ratios[]={4.f/3,16.f/9,16.f/10,21.f/9,32.f/9,1,9.f/16};
    const float boosts[]={-1,0,.01f,5,20,21,50,99.01f,100,200,NAN,INFINITY};
    for(unsigned ratio=0;ratio<sizeof ratios/sizeof *ratios;ratio++)for(unsigned b=0;b<sizeof boosts/sizeof *boosts;b++){
        gfx_current_dimensions.aspect_ratio=ratios[ratio];state.boost=boosts[b];render();
        CHECK(drawCount==25&&sTextLabelsCount==2);
        CHECK(label_is(0,"BOOST"));CHECK(labels[1].buffer[0]=='+');
        CHECK(has_icon(GLYPH_CAM_CAMERA)&&has_icon(GLYPH_CAM_ARROW_DOWN)&&!has_icon(GLYPH_CAM_LAKITU_HEAD));
        float left=GFX_DIMENSIONS_FROM_LEFT_EDGE(0),right=GFX_DIMENSIONS_FROM_RIGHT_EDGE(0);
        for(unsigned i=0;i<drawCount;i++){
            struct Draw *d=&draws[i];CHECK(d->x>=left&&d->x+d->w<=right&&d->y>=180&&d->y+d->h<=232);
        }
        for(int i=0;i<sTextLabelsCount;i++){
            CHECK(labels[i].x>=left&&labels[i].x+labels[i].length*12+4<=right);
            CHECK(labels[i].y==40||labels[i].y==22);
        }
        int expected=isfinite(boosts[b])?(int)ceilf(fmaxf(0,fminf(100,boosts[b]))):0;
        char value[16];snprintf(value,sizeof value,"+%d%%",expected);CHECK(label_is(1,value));
        int active=(expected+4)/5;int color=expected<=20?GPACK_RGBA5551(248,88,40,1):GPACK_RGBA5551(248,192,48,1);
        CHECK(color_count(color)==active&&color_count(GPACK_RGBA5551(56,64,72,1))==20-active);
    }
    gfx_current_dimensions.aspect_ratio=16.f/9;state.boost=65;render();capture(output,"finite-65-wide");
    gfx_current_dimensions.aspect_ratio=4.f/3;state.boost=5;render();capture(output,"low-5-classic");
    mode=ROCKET_BOOST_INFINITE;render();CHECK(label_is(1,"MAX"));capture(output,"infinite-classic");
    state.water_mode=ROCKET_WATER_JET;mode=ROCKET_BOOST_COIN_ONLY;render();
    CHECK(label_is(0,"JET")&&label_is(1,"MAX"));CHECK(color_count(GPACK_RGBA5551(40,152,248,1))==20);capture(output,"jet-classic");
    state.water_mode=0;owned=0;presenting=1;render();CHECK(drawCount==25&&sTextLabelsCount==2);capture(output,"native-injury-classic");
    selected=0;render();CHECK(!sTextLabelsCount&&has_icon(GLYPH_CAM_LAKITU_HEAD)&&drawCount==3);
    CHECK(draws[0].x==266&&draws[0].y==205&&draws[1].x==282); // Original Mario camera layout.
    selected=owned=1;wheel=1;render();CHECK(!sTextLabelsCount&&has_icon(GLYPH_CAM_LAKITU_HEAD));
    wheel=presenting=0;ready=0;render();CHECK(!sTextLabelsCount&&has_icon(GLYPH_CAM_LAKITU_HEAD));ready=1;
    sCameraHUD.status=CAM_STATUS_MARIO|CAM_STATUS_C_UP;render();CHECK(has_icon(GLYPH_CAM_MARIO_HEAD)&&has_icon(GLYPH_CAM_ARROW_UP));
    sCameraHUD.status=CAM_STATUS_FIXED;render();CHECK(has_icon(GLYPH_CAM_FIXED));
    sCameraHUD.status=CAM_STATUS_NONE;render();CHECK(drawCount==23);
    sCameraHUD.status=CAM_STATUS_LAKITU;
    for(hint=1;hint<=3;hint++){render();CHECK(sTextLabelsCount==(hint==3?4:5));CHECK(label_is(0,"BOOST"));}
    hint=0;gfx_current_dimensions.aspect_ratio=9.f/16;state.boost=100;render();capture(output,"portrait-100");
    printf("PASS boost HUD: %u checks; actual native labels/draw functions, seven aspects, finite/infinite/jet/injury, exact Lakitu-only removal, camera arrows and penguin hints.\n",checks);
    return 0;
}
