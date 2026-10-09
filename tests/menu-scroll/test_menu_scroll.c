/* Actual config registry/serializer/parser, with an isolated filesystem shim. */
#include "../../src/pc/configfile.c"
static int checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
struct CLIOptions gCLIOpts;
struct Mods gLocalMods;
char **gBanAddresses, **gModeratorAddresses;
bool *gBanPerm, *gModerator;
u16 gBanCount, gModeratorCount;
f32 gMasterVolume;
struct PcDebug gPcDebug;
static const char *directory;
const char *fs_get_write_path(const char *name){static char path[2048];snprintf(path,sizeof path,"%s/%s",directory,name);return path;}
fs_file_t *fs_open(const char *name){
    FILE *f=fopen(fs_get_write_path(name),"r");if(!f)return NULL;
    fs_file_t *file=calloc(1,sizeof *file);file->handle=f;return file;
}
const char *fs_readline(fs_file_t *file,char *dst,uint64_t size){return fgets(dst,(int)size,file->handle);}
bool fs_eof(fs_file_t *file){return feof(file->handle);}
void fs_close(fs_file_t *file){fclose(file->handle);free(file);}
void ban_list_add(char *address,bool perm){(void)address;(void)perm;}
void moderator_list_add(char *address,bool perm){(void)address;(void)perm;}
void mods_enable(char *relativePath){(void)relativePath;}
bool network_player_name_valid(char *name){return name&&*name;}
int dynos_pack_get_count(void){return 0;}
bool dynos_pack_get_enabled(int i){(void)i;return false;}
const char *dynos_pack_get_name(int i){(void)i;return "";}

/* Production UI and config code; only OS, audio/GPU and gameplay services are
 * inert. No window, system input, network, installed config or save is opened. */
#include "../../src/pc/djui/djui_base.c"
#include "../../src/pc/djui/djui_flow_layout.c"
#include "../../src/pc/djui/djui_cursor.c"
#include "../../src/pc/djui/djui_interactable.c"
#include "../../src/pc/djui/djui_text.c"
#include "../../src/pc/djui/djui_unicode.c"
#include "../../src/pc/djui/djui_theme.c"
#include "../../src/pc/djui/djui_button.c"
#include "../../src/pc/djui/djui_selectionbox.c"
#include "../../src/pc/djui/djui_slider.c"
#include "../../src/pc/djui/djui_checkbox.c"
#include "../../src/pc/djui/djui_bind.c"
#include "../../src/pc/djui/djui_image.c"
#include "../../src/pc/djui/djui_three_panel.c"
#include "../../src/pc/djui/djui_panel.c"
#include "../../src/pc/djui/djui_panel_menu.c"
#include "../../src/pc/djui/djui_panel_options.c"
#include "../../src/pc/djui/djui_panel_controls.c"
#include "../../src/pc/djui/djui_panel_controls_n64.c"
#include "../../src/pc/djui/djui_panel_controls_extra.c"
#include "../../src/pc/djui/djui_panel_rocket_controls.c"
#include "../../src/pc/djui/djui_rocket_boost.c"
#include "../../src/pc/djui/djui_root.c"
#include "../../src/pc/controller/controller_sdl.c"
#include "font-widths.h"

struct DjuiRoot* gDjuiRoot;
struct DjuiText *gDjuiPauseOptions,*gDjuiModReload;
bool gDjuiInMainMenu,gDjuiInPlayerMenu,gDjuiDisabled,gDjuiPanelPauseCreated,gDjuiPanelMainCreated;
bool gDjuiPanelJoinMessageVisible,gDjuiShuttingDown,gDjuiChangingTheme,gDjuiChatBoxFocus,gDjuiConsoleFocus;
bool gAttemptingToOpenPlayerlist;
struct DjuiChatBox* gDjuiChatBox;
struct DjuiConsole* gDjuiConsole;
struct DjuiThreePanel* gDjuiPlayerList;
struct DjuiThreePanel* gDjuiModList;
const u8 sPlayerListSize=8;
u8 sPageIndex,gRenderingInterpolated;
enum NetworkType gNetworkType;
struct ServerSettings gServerSettings;
static bool test_has_focus(void){return true;}
static void test_cursor_visible(bool visible){(void)visible;}
static struct GfxWindowManagerAPI testWindow={.has_focus=test_has_focus,.set_cursor_visible=test_cursor_visible};
struct GfxWindowManagerAPI* gWindowApi=&testWindow;
Vec3f gGlobalSoundSource;
u32 mouse_window_buttons;
s32 mouse_window_x,mouse_window_y;
u8 gd_texture_hand_open[1],gd_texture_hand_closed[1];
u8 texture_selectionbox_back_icon[1],texture_selectionbox_forward_icon[1];
u8 texture_checkbox_check_icon[1];
Gfx testDisplayList[1000000],*gDisplayListHead=testDisplayList;
const Gfx dl_djui_simple_rect[1],dl_djui_menu_rect[1],dl_ia_text_end[1];
static u32 rawKey=VK_INVALID;
static float elapsed;
static unsigned screenWidth=1280,screenHeight=720;
static float uiScale=1;
static FILE* svg;
static float drawX,drawY,drawH;

void controller_mouse_read_window(void){}
void* hmap_get(void* map,int64_t key){(void)map;(void)key;return NULL;}
void controller_reconfigure(void){configfile_save(configfile_name());}
u32 controller_get_raw_key(void){u32 k=rawKey;rawKey=VK_INVALID;return k;}
const char* translate_bind_to_name(int key){return key==VK_INVALID?"Unbound":key==1?"Escape":"Test key";}
void play_sound(s32 id,f32* pos){(void)id;(void)pos;}
void newcam_init_settings(void){}
f32 smooth_step(f32 a,f32 b,f32 t){return (t-a)/(b-a);}
f32 clock_elapsed(void){return elapsed;}
void djui_console_toggle(void){}
void djui_chat_box_toggle(void){}
u8 network_player_connected_count(void){return 1;}
void gfx_get_dimensions(u32* w,u32* h){*w=screenWidth;*h=screenHeight;}
f32 djui_gfx_get_scale(void){return uiScale;}
void djui_gfx_position_translate(f32*x,f32*y){(void)x;(void)y;}
void djui_gfx_scale_translate(f32*x,f32*y){(void)x;(void)y;}
void djui_gfx_size_translate(f32*x){(void)x;}
void create_dl_translation_matrix(s8 push,f32 x,f32 y,f32 z){(void)push;(void)x;(void)y;(void)z;}
void create_dl_scale_matrix(s8 push,f32 x,f32 y,f32 z){(void)push;(void)x;(void)y;(void)z;}
bool djui_gfx_add_clipping(struct DjuiBase*base){(void)base;return false;}
void djui_gfx_render_texture(const Texture*t,u32 w,u32 h,u8 f,u8 s,bool filter){(void)t;(void)w;(void)h;(void)f;(void)s;(void)filter;}
bool djui_gfx_add_clipping_specific(struct DjuiBase* b,f32 x,f32 y,f32 w,f32 h){
    drawX=x;drawY=y;drawH=h;
    return x+w<=b->clip.x||y+h<=b->clip.y||x>=b->clip.x+b->clip.width||y>=b->clip.y+b->clip.height;
}
static void draw_char(const char*c){
    if(svg && *c!=' ') fprintf(svg,"<text x='%g' y='%g' font-family='monospace' font-size='%g' fill='rgb(%u,%u,%u)'>&#%u;</text>\n",drawX,drawY+drawH*.78,drawH*.7,sDjuiTextCurrentColor.r,sDjuiTextCurrentColor.g,sDjuiTextCurrentColor.b,(unsigned char)*c);
}
static f32 char_width(const char*c){return *c==' '?0.30f:djui_unicode_get_sprite_width(c,font_normal_widths,32);}
static void font_noop(void){}
static const struct DjuiFont testFont={.charWidth=.5,.charHeight=1,.lineHeight=.8125,.defaultFontScale=32,.render_begin=font_noop,.render_end=font_noop,.render_char=draw_char,.char_width=char_width};
const struct DjuiFont* gDjuiFonts[]={&testFont,&testFont,&testFont,&testFont};
bool djui_rect_render(struct DjuiBase*b){
    if(svg&&b->color.a&&b->clip.width>0&&b->clip.height>0) fprintf(svg,"<rect x='%g' y='%g' width='%g' height='%g' fill='rgb(%u,%u,%u)' fill-opacity='%g'/>\n",b->clip.x,b->clip.y,b->clip.width,b->clip.height,b->color.r,b->color.g,b->color.b,b->color.a/255.0);
    return true;
}
static void rect_destroy(struct DjuiBase*b){free(b);}
struct DjuiRect* djui_rect_create(struct DjuiBase*p){struct DjuiRect*r=calloc(1,sizeof*r);djui_base_init(p,&r->base,djui_rect_render,rect_destroy);return r;}
struct DjuiRect* djui_rect_container_create(struct DjuiBase*p,f32 h){struct DjuiRect*r=djui_rect_create(p);djui_base_set_size_type(&r->base,DJUI_SVT_RELATIVE,DJUI_SVT_ABSOLUTE);djui_base_set_size(&r->base,1,h);r->base.color.a=0;return r;}
char* djui_language_get(const char*section,const char*key){(void)section;return (char*)key;}
#define INERT_PANEL(name) void name(struct DjuiBase*b){(void)b;}
INERT_PANEL(djui_panel_player_create)
INERT_PANEL(djui_panel_dynos_create)
INERT_PANEL(djui_panel_camera_create)
INERT_PANEL(djui_panel_display_create)
INERT_PANEL(djui_panel_sound_create)
INERT_PANEL(djui_panel_misc_create)
void rocket_audio_stop(void){}
/* Exercise real offline rule setters. Any network operation is a test failure. */
void network_send(struct Packet*p){(void)p;CHECK(false);}
void packet_init(struct Packet*p,enum PacketType type,bool reliable,enum PacketLevelMatchType match){(void)p;(void)type;(void)reliable;(void)match;CHECK(false);}
void packet_write(struct Packet*p,void*data,u16 length){(void)p;(void)data;(void)length;CHECK(false);}
void djui_panel_pause_disconnect_key_update(int code){(void)code;}
f64 clock_elapsed_f64(void){return elapsed;}
bool gDjuiHudLockMouse,mouse_init_ok;
NewCamera gNewCamera;
s16 gMenuMode=-1,sCurrPlayMode;
s32 gDialogID=DIALOG_NONE;
u32 mouse_buttons;
int mouse_x,mouse_y;
int rocket_runtime_enabled(void){return 1;}
void rocket_runtime_gamepad(const RocketGamepad*p){(void)p;}
void rocket_runtime_interrupt(void){}
int character_switch_enabled(void){return 1;}
int character_switch_accepts(enum CharacterSwitchId id){return id==CHARACTER_OCTANE;}
enum CharacterSwitchId character_switch_active(void){return CHARACTER_OCTANE;}
int thps_runtime_enabled(void){return 0;}
int thps_adapter_controller_active(void){return 0;}
void thps_adapter_set_gamepad_spin(uint8_t spin){(void)spin;}
void thps_adapter_pause_inputs(void){}
int character_wheel_is_open(void){return 0;}
int character_wheel_blocks_gameplay(void){return 0;}
void character_wheel_gamepad(int a,int b,int c,int d,int16_t x,int16_t y){(void)a;(void)b;(void)c;(void)d;(void)x;(void)y;}
bool get_first_person_enabled(void){return false;}
bool is_game_paused(void){return false;}
void controller_mouse_enter_relative(void){}
void controller_mouse_leave_relative(void){}
void controller_mouse_read_relative(void){}
void *fs_load_file(const char*name,uint64_t*size){(void)name;(void)size;return NULL;}

static void frame(void){
    elapsed+=.3f;gDisplayListHead=testDisplayList;
    djui_panel_update();djui_base_render(&gDjuiRoot->base);
    if(sInputControlledBase&&!sCursorMouseControlled)sInputControlledBase->get_cursor_hover_location(sInputControlledBase,&gCursorX,&gCursorY);
}
static void settle(void){for(int i=0;i<15;i++)frame();}
static void focus(struct DjuiBase*b){djui_cursor_input_controlled_center(b);settle();}
static void visible(struct DjuiBase*b){
    struct DjuiFlowLayout*layout=djui_flow_layout_scroll_parent(b);
    if(layout && (b->elem.y<layout->base.clip.y-.1 || b->elem.y+b->elem.height>layout->base.clip.y+layout->base.clip.height+.1))fprintf(stderr,"viewport %ux%u scale %g center %d: elem y=%g height=%g body y=%g height=%g\n",screenWidth,screenHeight,uiScale,configDjuiThemeCenter,b->elem.y,b->elem.height,layout->base.clip.y,layout->base.clip.height);
    CHECK(b->elem.y>=0 && b->elem.y+b->elem.height<=screenHeight/uiScale+.1);
    CHECK(layout&&b->elem.y>=layout->base.clip.y-.1&&b->elem.y+b->elem.height<=layout->base.clip.y+layout->base.clip.height+.1);
    CHECK(b->clip.width>0);
}
static void snapshot(const char*name){
    char path[2048];snprintf(path,sizeof path,"%s/%s.svg",directory,name);svg=fopen(path,"w");CHECK(svg);
    fprintf(svg,"<svg xmlns='http://www.w3.org/2000/svg' width='%u' height='%u' viewBox='0 0 %g %g'><rect width='100%%' height='100%%' fill='#253044'/>",screenWidth,screenHeight,screenWidth/uiScale,screenHeight/uiScale);
    frame();fputs("</svg>",svg);fclose(svg);svg=NULL;
}
static int collect(struct DjuiBase*b,struct DjuiBase**items,int n){
    if(!b->visible||!b->enabled)return n;
    if(b->interactable&&b->interactable->enabled){CHECK(n<256);items[n++]=b;}
    for(struct DjuiBaseChild*c=b->child;c;c=c->next)n=collect(c->base,items,n);
    return n;
}
static void sweep(void){
    gInteractablePad=(OSContPad){0};djui_interactable_update();djui_interactable_update();
    struct DjuiBase*items[256];int n=collect(sPanelList->base,items,0);CHECK(n>0);
    for(int i=0;i<n;i++){focus(items[i]);visible(items[i]);}
    // Traverse offscreen rows via the actual cursor path, not only direct focus.
    focus(items[0]);int steps=0;
    do {djui_cursor_move(0,1);settle();visible(sInputControlledBase);CHECK(++steps<=n+1);} while(sInputControlledBase!=items[0]);
    CHECK(steps>=n/3);
    // Last/first wrap and both paths through the real input-direction handler.
    focus(items[n-1]);gInteractablePad=(OSContPad){0};djui_interactable_update_pad();
    gInteractablePad.button=D_JPAD;djui_interactable_update_pad();settle();if(sInputControlledBase!=items[0])fprintf(stderr,"wrap n=%d steps=%d first=%p got=%p last=%p ignore=%d binding=%p keyboard=%d focus=%p\n",n,steps,(void*)items[0],(void*)sInputControlledBase,(void*)items[n-1],sIgnoreAllInputsWhenBinding,(void*)gInteractableBinding,sKeyboardHoldDirection,(void*)gInteractableFocus);CHECK(sInputControlledBase==items[0]);visible(items[0]);
    gInteractablePad=(OSContPad){0};djui_interactable_update_pad();
    djui_interactable_on_key_down(SCANCODE_UP);djui_interactable_update_pad();settle();CHECK(sInputControlledBase==items[n-1]);visible(items[n-1]);djui_interactable_on_key_up(SCANCODE_UP);
    sKeyboardHoldDirection=PAD_HOLD_DIR_NONE;gInteractablePad=(OSContPad){0};djui_interactable_update_pad();
}
static void controller_frame(void){
    gInteractablePad=(OSContPad){0};controller_sdl_read(&gInteractablePad);
    djui_interactable_update();frame();
}
static void keyboard_frame(void){gInteractablePad=(OSContPad){0};djui_interactable_update();frame();}
static void mouse_difficulty_press(struct DjuiBase*base,bool forward){
    struct DjuiBase*rect=&((struct DjuiSelectionbox*)base)->rect->base;
    sCursorMouseControlled=true;gCursorX=rect->elem.x+rect->elem.width*(forward?.75:.25);gCursorY=rect->elem.y+rect->elem.height*.5;
    mouse_window_buttons=0;keyboard_frame();
    mouse_window_buttons=L_MOUSE_BUTTON;keyboard_frame();unsigned after=rocket_difficulty();
    for(int held=0;held<8;held++){keyboard_frame();CHECK(rocket_difficulty()==after);}
    mouse_window_buttons=0;keyboard_frame();keyboard_frame();sCursorMouseControlled=false;
}
static void difficulty_input_tests(void){
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));init_ok=true;
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    CHECK(index>=0);configGamepadNumber=index;configBackgroundGamepad=true;
    SDL_Joystick*device=SDL_JoystickOpen(index);CHECK(device);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
    configKeyA[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_A;controller_sdl_bind();
    CHECK(rocket_difficulty_set(ROCKET_MEDIUM));
    djui_panel_options_create(NULL);settle();
    struct DjuiBase*body=djui_three_panel_get_body((struct DjuiThreePanel*)sPanelList->base);
    struct DjuiBase*difficulty=NULL;
    for(struct DjuiBaseChild*c=body->child;c;c=c->next)
        if(c->base->interactable&&c->base->interactable->on_value_change==difficulty_changed)difficulty=c->base;
    CHECK(difficulty);focus(difficulty);controller_frame();controller_frame();
    for(unsigned press=0;press<8;press++){
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_A,1));controller_frame();
        fprintf(stderr,"controller Cross press %u: pad=%04x difficulty=%u speed=%u jump=%u\n",press+1,gInteractablePad.button,rocket_difficulty(),rocket_speed_percent(),rocket_jump_percent());
        CHECK(gInteractablePad.button&PAD_BUTTON_A);
        CHECK(rocket_difficulty()==(ROCKET_MEDIUM+press+1)%3);
        unsigned after=rocket_difficulty();
        for(int held=0;held<8;held++){controller_frame();CHECK(rocket_difficulty()==after);}
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_A,0));controller_frame();controller_frame();
        CHECK(!(gInteractablePad.button&PAD_BUTTON_A));CHECK(sInputControlledBase==difficulty);visible(difficulty);
    }
    snapshot("difficulty-controller-easy-1280x720");
    /* SDL standardized south is PlayStation X/Cross (the saved native A bind).
     * Also exercise a user who assigns Xbox X/the west position to menu A. */
    configKeyA[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_X;controller_sdl_bind();
    for(unsigned press=0;press<6;press++){
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_X,1));controller_frame();
        CHECK(rocket_difficulty()==(press+1)%3);controller_frame();CHECK(rocket_difficulty()==(press+1)%3);
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_X,0));controller_frame();controller_frame();
    }
    for(unsigned press=0;press<6;press++){
        djui_interactable_on_key_down(SCANCODE_ENTER);keyboard_frame();CHECK(rocket_difficulty()==(press+1)%3);
        for(int held=0;held<8;held++){keyboard_frame();CHECK(rocket_difficulty()==(press+1)%3);}
        djui_interactable_on_key_up(SCANCODE_ENTER);keyboard_frame();keyboard_frame();
    }
    for(int forward=0;forward<2;forward++)for(unsigned press=0;press<6;press++){
        unsigned before=rocket_difficulty();mouse_difficulty_press(difficulty,forward);
        CHECK(rocket_difficulty()==(before+(forward?1:2))%3);
    }
    CHECK(rocket_speed_set_percent(88)&&rocket_jump_set_percent(67));settle();
    CHECK(rocket_difficulty()==ROCKET_CUSTOM&&difficultySelection==ROCKET_CUSTOM);
    /* A derived Custom pair survives opening, rendering and save/reload. */
    configfile_load();settle();CHECK(rocket_speed_percent()==88&&rocket_jump_percent()==67);
    mouse_difficulty_press(difficulty,false);CHECK(rocket_difficulty()==ROCKET_HARD);
    CHECK(rocket_speed_set_percent(88)&&rocket_jump_set_percent(67));settle();
    mouse_difficulty_press(difficulty,true);CHECK(rocket_difficulty()==ROCKET_EASY);
    /* Failed persistence must restore the shown value and allow a later retry. */
    CHECK(rocket_difficulty_set(ROCKET_HARD));settle();
    char blocked[2048];snprintf(blocked,sizeof blocked,"%s/%s.tmp",directory,configfile_name());CHECK(!mkdir(blocked,0700));
    mouse_difficulty_press(difficulty,true);CHECK(rocket_difficulty()==ROCKET_HARD&&difficultySelection==ROCKET_HARD);
    CHECK(strstr(rocket_difficulty_scope_label(),"Could not save"));CHECK(!rmdir(blocked));
    mouse_difficulty_press(difficulty,true);CHECK(rocket_difficulty()==ROCKET_EASY);
    configfile_load();settle();CHECK(rocket_difficulty()==ROCKET_EASY);
    struct DjuiBase*surface=NULL;
    for(struct DjuiBaseChild*c=body->child;c;c=c->next)
        if(c->base->interactable&&c->base->interactable->on_value_change==surface_changed)surface=c->base;
    CHECK(surface);CHECK(rocket_surface_set_mode(2));settle();focus(surface);visible(surface);controller_frame();controller_frame();
    for(unsigned press=0;press<9;press++){
        unsigned expected=2-(press+1)%3;
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_X,1));controller_frame();
        fprintf(stderr,"surface controller press=%u pad=%04x mode=%u selection=%u expected=%u\n",press,gInteractablePad.button,rocket_surface_mode(),surfaceSelection,expected);
        CHECK(rocket_surface_mode()==expected&&surfaceSelection==2-expected);
        for(int held=0;held<8;held++){controller_frame();CHECK(rocket_surface_mode()==expected);}
        CHECK(!SDL_JoystickSetVirtualButton(device,SDL_CONTROLLER_BUTTON_X,0));controller_frame();controller_frame();
        CHECK(sInputControlledBase==surface);visible(surface);
    }
    for(unsigned press=0;press<6;press++){
        unsigned expected=2-(press+1)%3;
        djui_interactable_on_key_down(SCANCODE_ENTER);keyboard_frame();CHECK(rocket_surface_mode()==expected);
        djui_interactable_on_key_up(SCANCODE_ENTER);keyboard_frame();keyboard_frame();
        configfile_load();settle();CHECK(rocket_surface_mode()==expected);visible(surface);
    }
    puts("PASS all three surface modes: actual SDL controller and keyboard cycles, hold/release, focus and persistence");
    configKeyA[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_A;controller_sdl_bind();
    focus(difficulty);visible(difficulty);
    puts("PASS production SDL -> DJUI -> difficulty setter: Cross/X cycles, held/release debounce, keyboard Enter, mouse both directions, Custom and save failure/retry");
    controller_sdl_shutdown();SDL_JoystickClose(device);SDL_JoystickDetachVirtual(index);SDL_Quit();
    djui_cursor_input_controlled_center(NULL);djui_panel_shutdown();
}
int main(int argc,char**argv){
    CHECK(argc==2);directory=argv[1];configDjuiTheme=1;configDjuiThemeFont=0;configExCoopTheme=false;
    gCLIOpts.rocketCar="test";gCLIOpts.offline=true;gDjuiRoot=djui_root_create();rocket_bindings_reset();
    difficulty_input_tests();
    unsigned dims[][2]={{640,480},{1280,720},{1920,1080},{2560,1080},{800,1280},{320,240},{640,360}};
    for(int center=0;center<2;center++)for(unsigned d=0;d<sizeof dims/sizeof dims[0];d++)for(int scale=0;scale<4;scale++){
        screenWidth=dims[d][0];screenHeight=dims[d][1];uiScale=(float[]){.5,1,1.5,.75}[scale];
        uiScale=fminf(uiScale,fmaxf(.5,fminf(screenWidth/320.f,screenHeight/240.f)));
        configDjuiThemeCenter=center;gDjuiInMainMenu=false;djui_panel_options_create(NULL);settle();sweep();
        struct DjuiBase*body=djui_three_panel_get_body((struct DjuiThreePanel*)sPanelList->base);
        struct DjuiBase*surfaces=NULL,*controls=NULL,*back=NULL;
        for(struct DjuiBaseChild*c=body->child;c;c=c->next){
            if(c->base->measure==djui_selectionbox_measure && !strcmp(((struct DjuiSelectionbox*)c->base)->text->message,"Octane surfaces"))surfaces=c->base;
            if(c->base->interactable&&c->base->interactable->on_click==djui_panel_controls_create)controls=c->base;
            back=c->base;
        }
        CHECK(surfaces&&controls&&back);focus(surfaces);visible(surfaces);
        struct DjuiFlowLayout*layout=(struct DjuiFlowLayout*)body;
        bool overflow=layout->contentHeight>layout->viewportHeight;
        CHECK(djui_flow_layout_scroll(surfaces,-100)==overflow);frame();CHECK(layout->scrollOffset<=fmaxf(0,layout->contentHeight-layout->viewportHeight)+.1);
        focus(surfaces);visible(surfaces);CHECK(!layout->manualScroll);
        unsigned old=configRocketSurfaceMode;surfaces->interactable->on_cursor_down_begin(surfaces,true);CHECK(configRocketSurfaceMode!=old);
        configfile_save(configfile_name());unsigned saved=configRocketSurfaceMode;configRocketSurfaceMode=99;configfile_load();CHECK(configRocketSurfaceMode==saved);
        if(d==1&&scale==1&&!center)snapshot("settings-surfaces-1280x720");
        focus(controls);controls->interactable->on_click(controls);settle();sweep();djui_panel_back();settle();CHECK(sInputControlledBase==controls);visible(controls);
        djui_panel_rocket_controls_create(controls);settle();sweep();
        struct DjuiBase *carBody=djui_three_panel_get_body((struct DjuiThreePanel*)sPanelList->base),*cameraBind=NULL;
        for(struct DjuiBaseChild*c=carBody->child;c;c=c->next)
            if(c->base->measure==djui_selectionbox_measure&&((struct DjuiSelectionbox*)c->base)->value==&configRocketBindings.action[RA_CAMERA])cameraBind=c->base;
        CHECK(cameraBind);focus(cameraBind);visible(cameraBind);
        unsigned cameraBefore=configRocketBindings.action[RA_CAMERA];cameraBind->interactable->on_cursor_down_begin(cameraBind,true);
        CHECK(configRocketBindings.action[RA_CAMERA]!=cameraBefore);unsigned cameraSaved=configRocketBindings.action[RA_CAMERA];
        configRocketBindings.action[RA_CAMERA]=RB_NONE;configfile_load();CHECK(configRocketBindings.action[RA_CAMERA]==cameraSaved);
        if(d==0&&scale==1&&center)snapshot("camera-binding-640x480");
        if(d==0&&scale==1&&center)snapshot("car-controls-640x480");
        struct DjuiBase*caller=sInputControlledBase;driving(caller);settle();sweep();djui_panel_back();settle();CHECK(sInputControlledBase==caller);
        air_controls(caller);settle();sweep();djui_panel_back();settle();
        djui_panel_controls_n64_create(caller);settle();sweep();
        struct DjuiBase*items[256];int n=collect(sPanelList->base,items,0);CHECK(n>=44);
        struct DjuiBase*lastBind=items[n-3];focus(lastBind);visible(lastBind);
        struct DjuiBind*bind=(struct DjuiBind*)lastBind->parent->parent;unsigned previous=bind->configKey[lastBind->tag];
        lastBind->interactable->on_click(lastBind);CHECK(gInteractableBinding==lastBind);
        struct DjuiPanel*beforeCapture=sPanelList;
        gInteractablePad=(OSContPad){.button=PAD_BUTTON_B|D_JPAD};djui_interactable_update();CHECK(sPanelList==beforeCapture&&sInputControlledBase==lastBind);
        rawKey=1;lastBind->interactable->on_bind(lastBind);CHECK(!gInteractableBinding&&bind->configKey[lastBind->tag]==previous);
        gInteractablePad=(OSContPad){0};djui_interactable_update();djui_interactable_update();
        lastBind->interactable->on_click(lastBind);rawKey=0x31;lastBind->interactable->on_bind(lastBind);CHECK(!gInteractableBinding&&bind->configKey[lastBind->tag]==0x31);
        gInteractablePad=(OSContPad){0};djui_interactable_update();djui_interactable_update();
        configfile_load();CHECK(bind->configKey[lastBind->tag]==0x31);
        if(d==1&&scale==1&&!center)snapshot("keyboard-bindings-bottom-1280x720");
        unsigned oldW=screenWidth,oldH=screenHeight;float oldScale=uiScale;
        screenWidth=640;screenHeight=360;uiScale=1;settle();visible(lastBind);CHECK(sInputControlledBase==lastBind);
        screenWidth=oldW;screenHeight=oldH;uiScale=oldScale;settle();visible(lastBind);
        djui_cursor_input_controlled_center(NULL);djui_panel_shutdown();
    }
    screenWidth=640;screenHeight=480;uiScale=1;configDjuiThemeCenter=false;
    struct DjuiThreePanel*longPanel=djui_panel_menu_create("Long label test",false);
    struct DjuiBase*longBody=djui_three_panel_get_body(longPanel);
    unsigned choice=0;char*longChoices[]={"Left shoulder plus right shoulder (shared actions)","Unbound"};
    struct DjuiSelectionbox*longBox=djui_selectionbox_create(longBody,"An unusually long car control label that wraps",longChoices,2,&choice,NULL);
    djui_button_create(longBody,"Back",DJUI_BUTTON_STYLE_BACK,djui_panel_menu_back);
    djui_panel_add(NULL,longPanel,NULL);settle();focus(&longBox->base);visible(&longBox->base);CHECK(longBox->base.height.value>32);
    f32 stableHeight=longBox->base.height.value;longBox->base.interactable->on_cursor_down_begin(&longBox->base,true);settle();CHECK(longBox->base.height.value==stableHeight);
    snapshot("long-labels-640x480");djui_cursor_input_controlled_center(NULL);djui_panel_shutdown();
    djui_base_destroy(&gDjuiRoot->base);
    printf("menu scrolling, nested binds, controller/keyboard wrap, capture/cancel, back/focus and config persistence: %d checks passed\n",checks);
    return 0;
}
