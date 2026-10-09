/* Production SDL/keyboard, bindings, text encoder, tutorial and native dialog
 * state/pagination. Only device family and audiovisual services are fixtures. */
#define SDL_GameControllerGetType tutorial_device_type
#define main bindings_suite_main
#include "test_bindings_sdl.c"
#undef main
#undef SDL_GameControllerGetType
static SDL_GameControllerType family=SDL_CONTROLLER_TYPE_PS4;
SDL_GameControllerType tutorial_device_type(SDL_GameController *controller){assert(controller);return family;}
#include "game/hardcoded.h"
#include "game/character_switch.h"
#include "game/level_info.h"
#include "game/camera.h"
#include "game/segment2.h"
#include "audio/external.h"
#include "gfx_dimensions.h"
#include "pc/lua/smlua_hooks.h"
#include "pc/controller/controller_bind_mapping.h"
#include "level_table.h"
struct CLIOptions gCLIOpts;
struct BehaviorValues gBehaviorValues;
s16 gCurrLevelNum;
static int boostMode,surfaceMode=ROCKET_SURFACES_NATIVE_NO_WALLS;
int rocket_boost_mode(void){return boostMode;}
int rocket_surface_mode(void){return surfaceMode;}
static int longNames;
static const char *test_key_name(int code){return longNames?"AReallyLongKeyboardButtonNameThatMustWrapSafely":translate_bind_to_name(code);}
#define translate_bind_to_name test_key_name
#include "../../../src/game/rocket_tutorial.c"
#undef translate_bind_to_name
#include "tutorial_widths.inc.c"
enum {DIALOG_STATE_OPENING,DIALOG_STATE_VERTICAL,DIALOG_STATE_HORIZONTAL,DIALOG_STATE_CLOSING};
enum {DIALOG_PAGE_STATE_NONE,DIALOG_PAGE_STATE_SCROLL,DIALOG_PAGE_STATE_END};
enum {DIALOG_TYPE_ROTATE,DIALOG_TYPE_ZOOM};
enum {DIALOG_MARK_NONE,DIALOG_MARK_DAKUTEN,DIALOG_MARK_HANDAKUTEN};
#define DEFAULT_DIALOG_BOX_ANGLE 90.0f
#define DEFAULT_DIALOG_BOX_SCALE 19.0f
#define DIAG_VAL1 16
#define DIAG_VAL2 240
#define DIAG_VAL4 5
#define X_VAL3 0.0f
#define Y_VAL3 16
#define STRING_THE 0
#define STRING_YOU 1
s8 gDialogBoxState,gDialogBoxType,gDialogLineNum,gLastDialogResponse,gLastDialogLineNum;
s16 gLastDialogPageStrPos,gDialogTextPos,gDialogScrollOffsetY,gDialogOverrideY;
u8 gOverrideDialogPos;
s32 gDialogResponse;
f32 gDialogBoxOpenTimer=90,gDialogBoxScale=19;
static Gfx *sDialogOffsetPos;
static f32 sDialogOffset,sDialogOffsetPrev;
static u8 *sOverrideDialogHookString;
static struct Controller dialogController;
struct Controller *gPlayer1Controller=&dialogController;
static struct Camera testCamera;
struct Camera *gCamera=&testCamera;
Vec3f gGlobalSoundSource;
static Gfx commands[65536];
Gfx *gDisplayListHead=commands;
const Gfx dl_ia_text_begin[1]={0},dl_ia_text_end[1]={0};
static struct DialogEntry original;
static unsigned checks,closed,transitions,hookCalls,cutsceneCloses,pagePresses;
static int inverted;
static const char *hookText;
static bool hookAllowed=true;
static FILE *svg;
static float penX,penY,maxInk;
static uint8_t capturedGlyphs[ROCKET_TUTORIAL_CAPACITY];
static int captureGlyphs,capturedCount;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"tutorial line %d: %s\n",__LINE__,#x);abort();}}while(0)
void *segmented_to_virtual(const void *p){return (void*)p;}
struct DialogEntry *dialog_table_get(s32 id){return id==DIALOG_NONE?NULL:&original;}
void create_dl_translation_matrix(s8 op,f32 x,f32 y,f32 z){(void)z;if(op==MENU_MTX_PUSH){penX=x;penY=y;}else {penX+=x;penY+=y;}}
void render_generic_char(u8 c){
    if(captureGlyphs){CHECK(capturedCount<ROCKET_TUTORIAL_CAPACITY);capturedGlyphs[capturedCount++]=c;}
    if(gDialogBoxState==DIALOG_STATE_VERTICAL){CHECK(penX>=0&&penX+8<=136&&penY<=-14&&penY>=-94);if(penX+8>maxInk)maxInk=penX+8;}
    if(svg){u8 one[]={c,255};char name[8];convert_string_sm64_to_ascii(name,one);
        const char *s=!strcmp(name,"&")?"&amp;":!strcmp(name,"<")?"&lt;":!strcmp(name,">")?"&gt;":name;
        fprintf(svg,"<text x=\"%.0f\" y=\"%.0f\" textLength=\"%u\" lengthAdjust=\"spacingAndGlyphs\">%s</text>\n",30+penX,40-penY,gDialogCharWidths[c],s);}
}
static void change_and_flash_dialog_text_color_lines(s8 a,s8 b){(void)a;(void)b;}
static void render_multi_text_string_lines(s16 a,s8 b,s16 *c,s8 d,s8 e,s8 f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;CHECK(0);}
static void render_star_count_dialog_text(s8 *a,s16 *b){(void)a;(void)b;CHECK(0);}
void play_sound(s32 bits,f32 *position){(void)bits;(void)position;}
void play_dialog_sound(s32 id){(void)id;}
void handle_special_dialog_text(s32 id){CHECK(id==gDialogID);closed++;}
void level_set_transition(s16 frames,void (*callback)(s16 *)){(void)frames;(void)callback;transitions++;}
s32 trigger_cutscene_dialog(s32 id){CHECK(id==2);cutsceneCloses++;return 0;}
static void render_dialog_box_type(struct DialogEntry *entry,s8 lines){CHECK(entry->leftOffset==30&&entry->width==200&&lines==6);}
static void render_dialog_triangle_choice(void){CHECK(0);}
static void render_dialog_string_color(s8 lines){CHECK(lines==6);}
static u32 ensure_nonnegative(s32 value){return value<0?0:(u32)value;}
static void tutorial_hook(enum LuaHookedEventType event,s32 id,bool *allow,const char **text){(void)event;(void)id;hookCalls++;*allow=hookAllowed;*text=hookText;}
#undef smlua_call_event_hooks
#define smlua_call_event_hooks tutorial_hook
#include "../../../src/game/rocket_tutorial.inc.h"
#include "tutorial_native.inc.c"
#undef smlua_call_event_hooks
static void render(void){gDisplayListHead=commands;render_dialog_entries();CHECK(gDisplayListHead<commands+65536);}
static void begin(int level,int id){
    gCurrLevelNum=level;gDialogID=DIALOG_NONE;gDialogBoxState=DIALOG_STATE_OPENING;gDialogBoxOpenTimer=90;gDialogBoxScale=19;
    gDialogTextPos=gLastDialogPageStrPos=gDialogScrollOffsetY=gLastDialogResponse=0;dialogController.buttonPressed=0;
    if(inverted)create_dialog_inverted_box(id);else create_dialog_box(id);
    for(int i=0;i<12;i++){render();}CHECK(gDialogBoxState==DIALOG_STATE_VERTICAL);
}
static void page(void){
    dialogController.buttonPressed=(pagePresses++%2)?B_BUTTON:A_BUTTON;render();dialogController.buttonPressed=0;
    for(int i=0;i<12&&gDialogID!=DIALOG_NONE;i++)render();
}
static char *decoded(const uint8_t *text){
    static char buffer[ROCKET_TUTORIAL_CAPACITY*4];convert_string_sm64_to_ascii(buffer,text);
    int n=0;for(int i=0;buffer[i];i++){char c=buffer[i]=='\n'?' ':buffer[i];if(c!=' '||!n||buffer[n-1]!=' ')buffer[n++]=c;}buffer[n]=0;return buffer;
}
static void read_complete_dialog(void){
    uint8_t expected[ROCKET_TUTORIAL_CAPACITY];int count=0,pages=0;
    const uint8_t *text=rocket_tutorial_dialog(&original)->str;
    for(int i=0;text[i]!=DIALOG_CHAR_TERMINATOR;i++)
        if(text[i]!=DIALOG_CHAR_SPACE&&text[i]!=DIALOG_CHAR_NEWLINE)expected[count++]=text[i];
    capturedCount=0;
    while(gDialogID!=DIALOG_NONE&&pages<32){
        captureGlyphs=1;render();captureGlyphs=0;page();pages++;
    }
    CHECK(gDialogID==DIALOG_NONE&&pages<32);
    CHECK(capturedCount==count&&!memcmp(capturedGlyphs,expected,count));
}
static void snapshot(const char *directory,const char *name){
    char path[1024];snprintf(path,sizeof path,"%s/%s.svg",directory,name);svg=fopen(path,"w");CHECK(svg);
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"960\" height=\"720\" viewBox=\"0 0 320 240\"><rect width=\"320\" height=\"240\" fill=\"#46574b\"/><rect x=\"23\" y=\"34\" width=\"143\" height=\"106\" fill=\"#111\"/><g fill=\"white\" font-family=\"monospace\" font-size=\"10\">\n",svg);
    render();fputs("</g><text x=\"12\" y=\"224\" fill=\"white\" font-size=\"7\">Source-test render. Representative glyphs, not a game screenshot.</text></svg>\n",svg);fclose(svg);svg=NULL;
}
int main(int argc,char **argv){
    CHECK(argc==2);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));init_ok=true;attach();controller_bind_init();
    configKeyA[0]=0x26;configKeyB[0]=0x33;configKeyY[0]=0x32;configKeyStart[0]=0x39;configKeyR[0]=0x36;configKeyR[1]=0x100a;
    configKeyStickUp[0]=0x11;configKeyStickDown[0]=0x1f;configKeyStickLeft[0]=0x1e;configKeyStickRight[0]=0x20;configKeyZ[0]=0x25;
    keyboard_bindkeys();controller_sdl_bind();poll();gCLIOpts.rocketCar=true;
    gBehaviorValues.dialogs.LakituIntroDialog=DIALOG_034;gBehaviorValues.dialogs.KingWhompDialog=DIALOG_114;
    static u8 originalText[]={10,11,12,255};original=(struct DialogEntry){.linesPerBox=6,.leftOffset=30,.width=200,.str=originalText};
    struct DialogEntry preserved=original;uint8_t text[ROCKET_TUTORIAL_CAPACITY];
    button(SDL_CONTROLLER_BUTTON_A,1);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_PLAYSTATION);
    begin(LEVEL_CASTLE_GROUNDS,DIALOG_034);CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Super Rocket64"));snapshot(argv[1],"welcome-playstation");
    configRocketBindings.stick=1;render();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Steer with Right stick."));configRocketBindings.stick=0;render();
    page();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Triangle"));
    int retainedPage=rocket_tutorial_page(rocket_tutorial_dialog(&original)->str,gDialogTextPos,6);
    configRocketBindings.action[RA_CAMERA]=RB_RB;render();CHECK(rocket_tutorial_page(rocket_tutorial_dialog(&original)->str,gDialogTextPos,6)==retainedPage);
    CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"R1 to change the camera view."));
    // A keyboard press wins over a connected, held pad. Only fresh pad input returns it.
    keyboard_on_key_down(0x32);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_KEYBOARD);render();
    CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"M to change the camera view."));
    configKeyY[0]=0x22;render();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"G to change the camera view."));
    configKeyY[0]=0x27;render();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Semicolon to change the camera view."));
    configKeyY[0]=0x35;render();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Slash to change the camera view."));
    configKeyY[0]=0x22;
    keyboard_on_key_up(0x32);page();snapshot(argv[1],"welcome-keyboard-menu");
    unsigned beforeClosed=closed;for(int i=0;i<15&&gDialogID!=DIALOG_NONE;i++)page();CHECK(gDialogID==DIALOG_NONE&&closed==beforeClosed+1);
    CHECK(!memcmp(&original,&preserved,sizeof original));
    // SDL device family service is explicit; button events/readers are real.
    const SDL_GameControllerType types[]={SDL_CONTROLLER_TYPE_XBOXONE,SDL_CONTROLLER_TYPE_PS4,SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO,SDL_CONTROLLER_TYPE_UNKNOWN};
    const char *cameraNames[]={"Y","Triangle","X","North button"};
    for(int d=0;d<4;d++){
        family=types[d];button(SDL_CONTROLLER_BUTTON_A,0);poll();button(SDL_CONTROLLER_BUTTON_A,1);poll();rocket_bindings_reset();
        gCurrLevelNum=LEVEL_CASTLE_GROUNDS;CHECK(rocket_tutorial_text(DIALOG_034,6,text));CHECK(strstr(decoded(text),cameraNames[d]));
        for(int binding=0;binding<RB_COUNT;binding++)for(int which=0;which<4;which++){
            configRocketBindings.action[RA_JUMP]=configRocketBindings.action[RA_CAMERA]=configRocketBindings.action[RA_BOOST]=(unsigned)binding;
            const int levels[]={LEVEL_CASTLE_GROUNDS,LEVEL_BOB,LEVEL_WF,LEVEL_WF},ids[]={DIALOG_034,DIALOG_000,DIALOG_030,DIALOG_114};
            begin(levels[which],ids[which]);read_complete_dialog();
        }
    }
    rocket_bindings_reset();family=SDL_CONTROLLER_TYPE_PS4;begin(LEVEL_BOB,DIALOG_000);page();snapshot(argv[1],"first-world-coins");
    CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"each yellow coin adds 5 boost, each red coin adds 5, and each blue coin adds 5."));
    boostMode=ROCKET_BOOST_INFINITE;surfaceMode=ROCKET_SURFACES_CAR;render();CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"Infinite boost"));CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"normal car grip"));
    begin(LEVEL_WF,DIALOG_030);snapshot(argv[1],"whomp-four-wheels");CHECK(strstr(decoded(rocket_tutorial_dialog(&original)->str),"four wheels"));
    // Reflow is deferred during native horizontal transitions.
    dialogController.buttonPressed=A_BUTTON;render();dialogController.buttonPressed=0;CHECK(gDialogBoxState==DIALOG_STATE_HORIZONTAL);
    const uint8_t *held=rocket_tutorial_dialog(&original)->str;uint8_t previous[ROCKET_TUTORIAL_CAPACITY];memcpy(previous,held,sizeof previous);
    controller_sdl_note_keyboard_input();longNames=1;render();CHECK(!memcmp(previous,rocket_tutorial_dialog(&original)->str,sizeof previous));
    for(int i=0;i<8;i++){render();}CHECK(gDialogBoxState==DIALOG_STATE_VERTICAL);
    for(int i=0;i<20&&gDialogID!=DIALOG_NONE;i++){page();}CHECK(gDialogID==DIALOG_NONE);longNames=0;
    longNames=1;begin(LEVEL_CASTLE_GROUNDS,DIALOG_034);
    for(int i=0;i<20&&gLastDialogPageStrPos!=-1;i++){page();}CHECK(gDialogID!=DIALOG_NONE);
    longNames=0;render();CHECK(gDialogTextPos==rocket_tutorial_page_start(rocket_tutorial_dialog(&original)->str,100,6));
    CHECK(gLastDialogPageStrPos==-1);page();CHECK(gDialogID==DIALOG_NONE);
    // Fresh axes claim prompts; held axes, disabled pads and unfocused events do not.
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_LEFTX,25000);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_PLAYSTATION);
    keyboard_on_key_down(0x22);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_KEYBOARD);keyboard_on_key_up(0x22);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_LEFTX,-25000);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_PLAYSTATION);
    configDisableGamepads=true;poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_KEYBOARD);configDisableGamepads=false;
    controller_sdl_note_keyboard_input();focused=false;button(SDL_CONTROLLER_BUTTON_X,1);poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_KEYBOARD);
    focused=true;poll();CHECK(controller_sdl_prompt_device()==CONTROLLER_PROMPT_KEYBOARD);button(SDL_CONTROLLER_BUTTON_X,0);poll();
    // No edits to unrelated dialogs, classic Mario, Lua text or replacement tables.
    gCurrLevelNum=LEVEL_WF;CHECK(!rocket_tutorial_text(DIALOG_000,6,text));CHECK(!rocket_tutorial_text(DIALOG_115,6,text));CHECK(!rocket_tutorial_text(DIALOG_COUNT,6,text));
    gCLIOpts.rocketCar=false;CHECK(!rocket_tutorial_text(DIALOG_030,6,text));gCLIOpts.rocketCar=true;
    gCLIOpts.characterWheel=true;wheel_enabled=1;car_selected=0;CHECK(!rocket_tutorial_text(DIALOG_030,6,text));
    car_selected=1;CHECK(rocket_tutorial_text(DIALOG_030,6,text));gCLIOpts.characterWheel=false;wheel_enabled=0;
    original.replaced=true;gDialogID=DIALOG_030;CHECK(rocket_tutorial_dialog(&original)==&original);original.replaced=false;
    gLastDialogResponse=1;CHECK(rocket_tutorial_dialog(&original)==&original);gLastDialogResponse=0;
    inverted=1;begin(LEVEL_WF,DIALOG_030);for(int i=0;i<15&&gDialogID!=DIALOG_NONE;i++){page();}
    CHECK(gDialogID==DIALOG_NONE&&cutsceneCloses==1);inverted=0;
    hookText="Custom Lua dialog";begin(LEVEL_WF,DIALOG_030);CHECK(rocket_tutorial_dialog(&original)==&original);page();CHECK(gDialogID==DIALOG_NONE);hookText=NULL;
    hookAllowed=false;gDialogID=DIALOG_NONE;testCamera.cutscene=CUTSCENE_READ_MESSAGE;create_dialog_box(DIALOG_030);
    CHECK(gDialogID==DIALOG_NONE&&testCamera.cutscene==0);hookAllowed=true;
    CHECK(hookCalls>0&&transitions>0);CHECK(!memcmp(&original,&preserved,sizeof original));free(sOverrideDialogHookString);
    controller_sdl_shutdown();SDL_JoystickClose(device);SDL_JoystickDetachVirtual(device_index);SDL_Quit();
    printf("PASS tutorials: %u checks; actual SDL/keyboard/native text and dialog state machine, scopes, all bindings/families, remap/reflow, six-line pages, closure and Lua precedence. Max native glyph right edge %.0f. No game started.\n",checks,maxInk);
}
