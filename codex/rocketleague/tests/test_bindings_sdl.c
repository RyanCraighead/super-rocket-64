/* Real SDL and keyboard readers with inert game/UI services. Virtual joystick
 * events stay inside this test process; no video, audio or OS input is used. */
#ifndef CONTROLLER_SDL_SOURCE
#define CONTROLLER_SDL_SOURCE "../../../src/pc/controller/controller_sdl.c"
#endif
#include CONTROLLER_SDL_SOURCE
#include "../../../src/pc/controller/controller_keyboard.c"
#include <assert.h>
#ifdef _WIN32
/* Production's private SDL keymap hook; this fixture never initializes video. */
void WIN_UpdateKeymap(void){}
#endif

static RocketGamepad observed;
static int car_enabled=1,car_selected=1,car_drawable=1,wheel_enabled,tony_enabled,panel_active,panel_focus,wheel_open;
static bool focused=true;
static bool has_focus(void){return focused;}
static struct GfxWindowManagerAPI window_api={.has_focus=has_focus};
struct GfxWindowManagerAPI *gWindowApi=&window_api;
ConfigStick configStick;
unsigned int configStickDeadzone=20,configRumbleStrength,configGamepadNumber;
bool configBackgroundGamepad,configExtendedReports,configDisableGamepads;
bool gDjuiInMainMenu,gDjuiInPlayerMenu,gDjuiChatBoxFocus,gDjuiConsoleFocus,gDjuiPanelPauseCreated;
bool gInteractableOverridePad,gDjuiHudLockMouse,mouse_init_ok;
NewCamera gNewCamera;
s16 gMenuMode=-1;
s32 gDialogID=DIALOG_NONE;
s16 sCurrPlayMode;
u32 mouse_buttons;
int mouse_x,mouse_y;
#define KEY(name) unsigned int configKey##name[MAX_BINDS]={VK_INVALID,VK_INVALID,VK_INVALID}
KEY(A);KEY(B);KEY(X);KEY(Y);KEY(Z);KEY(L);KEY(R);KEY(Start);
KEY(CUp);KEY(CDown);KEY(CLeft);KEY(CRight);KEY(StickUp);KEY(StickDown);KEY(StickLeft);KEY(StickRight);
KEY(DUp);KEY(DDown);KEY(DLeft);KEY(DRight);
int rocket_runtime_enabled(void){return car_enabled;}
int rocket_runtime_owns_controls(void){return car_enabled&&car_selected&&car_drawable;}
void rocket_runtime_gamepad(const RocketGamepad *p){observed=p?*p:(RocketGamepad){0};}
void rocket_runtime_interrupt(void){}
int character_switch_enabled(void){return wheel_enabled;}
int character_switch_accepts(enum CharacterSwitchId id){(void)id;return car_selected;}
enum CharacterSwitchId character_switch_active(void){return car_selected?CHARACTER_OCTANE:CHARACTER_MARIO;}
int thps_runtime_enabled(void){return tony_enabled;}
int thps_adapter_controller_active(void){return tony_enabled;}
void thps_adapter_set_gamepad_spin(uint8_t spin){(void)spin;}
void thps_adapter_pause_inputs(void){}
int character_wheel_is_open(void){return wheel_open;}
int character_wheel_blocks_gameplay(void){return wheel_open;}
void character_wheel_gamepad(int a,int b,int c,int d,int16_t x,int16_t y){(void)a;(void)b;(void)c;(void)d;(void)x;(void)y;}
int codex_panel_gl_is_focused(void){return panel_focus;}
int codex_panel_gl_owns_pointer(void){return panel_focus;}
void codex_panel_gl_set_game_ui_state(int a,int b,int c){(void)a;(void)b;(void)c;}
bool djui_panel_is_active(void){return panel_active;}
bool djui_interactable_on_key_down(int code){(void)code;return false;}
void djui_interactable_on_key_up(int code){(void)code;}
void djui_interactable_on_text_input(char *text){(void)text;}
void djui_interactable_on_text_editing(char *text,int a){(void)text;(void)a;}
void djui_panel_pause_disconnect_key_update(int code){(void)code;}
bool get_first_person_enabled(void){return false;}
bool is_game_paused(void){return sCurrPlayMode==PLAY_MODE_PAUSED;}
void controller_mouse_enter_relative(void){}
void controller_mouse_leave_relative(void){}
void controller_mouse_read_relative(void){}
void *fs_load_file(const char *name,uint64_t *size){(void)name;(void)size;return NULL;}

static SDL_Joystick *device;
static int device_index;
static void attach(void){
    device_index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    assert(device_index>=0);configGamepadNumber=(unsigned)device_index;
    device=SDL_JoystickOpen(device_index);assert(device);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
}
static OSContPad host_pad;
static RocketInput poll(void){
    host_pad=(OSContPad){0};controller_sdl_read(&host_pad);keyboard_read(&host_pad);
    RocketInput keyboard={0};
    keyboard.throttle=host_pad.stick_y/127.f;keyboard.jump=!!(host_pad.button&A_BUTTON);
    return rocket_gamepad_merge(&keyboard,&observed);
}
static void button(SDL_GameControllerButton b,int down){assert(!SDL_JoystickSetVirtualButton(device,b,down));}
#include "test_door_input_handoff.inc.c"
#include "test_text_gamepad.inc.c"
int main(void){
    /* Ignore physical devices; the reader opens only our explicit virtual index. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    assert(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));init_ok=true;
    configKeyA[0]=0x26;configKeyA[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_A;
    configKeyStickUp[0]=0x11;controller_sdl_bind();keyboard_bindkeys();
    attach();poll();
    configRocketBindings.action[RA_BOOST]=RB_RB;
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);RocketInput out=poll();assert(out.boost&&!out.jump);
    assert(!(host_pad.button&(A_BUTTON|B_BUTTON|Z_TRIG)));
    panel_active=1;out=poll();assert(!out.boost&&observed.ui_blocked);
    panel_active=0;out=poll();assert(!out.boost); // held across closing settings
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);out=poll();assert(out.boost);
    for(int capture=0;capture<5;++capture) {
        switch(capture) {
            case 0:gDjuiConsoleFocus=true;break;
            case 1:gDjuiChatBoxFocus=true;break;
            case 2:gDialogID=0;break;
            case 3:sCurrPlayMode=PLAY_MODE_PAUSED;break;
            case 4:wheel_open=1;break;
        }
        assert(!poll().boost);
        gDjuiConsoleFocus=gDjuiChatBoxFocus=false;gDialogID=DIALOG_NONE;
        sCurrPlayMode=0;panel_focus=wheel_open=0;
        assert(!poll().boost);
        button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();
        button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(poll().boost);
    }
    focused=false;out=poll();assert(!out.boost);focused=true;assert(!poll().boost);
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(poll().boost);
    controller_sdl_rocket_bindings_changed();assert(!poll().boost);
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();
    configRocketBindings.stick=1;
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,20000);
    out=poll();assert(out.steer<-.5f&&host_pad.ext_stick_x==0&&!(host_pad.button&R_CBUTTONS));
    panel_active=1;poll();panel_active=0;assert(!poll().steer);
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,5000);assert(!poll().steer); // same car deadzone on resume
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,0);poll();
    button(SDL_CONTROLLER_BUTTON_START,1);poll();assert(host_pad.button&START_BUTTON);
    button(SDL_CONTROLLER_BUTTON_START,0);poll();
    /* Disabled/unplugged controllers must preserve the UI gate for keyboard. */
    configDisableGamepads=true;panel_active=1;keyboard_on_key_down(0x11);
    out=poll();assert(!out.throttle&&observed.ui_blocked&&!observed.connected);
    panel_active=0;assert(!poll().throttle);
    keyboard_on_key_up(0x11);poll();keyboard_on_key_down(0x11);assert(poll().throttle==1);
    keyboard_on_key_up(0x11);configDisableGamepads=false;poll();
    SDL_JoystickDetachVirtual(device_index);SDL_JoystickClose(device);device=NULL;
    panel_active=1;keyboard_on_key_down(0x26);out=poll();assert(!out.jump&&!observed.connected&&observed.ui_blocked);
    panel_active=0;assert(!poll().jump);keyboard_on_key_up(0x26);poll();
    keyboard_on_key_down(0x26);assert(poll().jump);keyboard_on_key_up(0x26);
    attach();button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(!poll().boost);
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(poll().boost);
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();
    test_door_input_handoff();
    test_text_gamepad();
    /* The car profile never changes the standard Mario / Tony bindings. */
    car_enabled=car_selected=0;configRocketBindings.action[RA_JUMP]=RB_RB;poll();
    button(SDL_CONTROLLER_BUTTON_A,1);poll();assert(host_pad.button&A_BUTTON);
    button(SDL_CONTROLLER_BUTTON_A,0);poll();tony_enabled=1;poll();
    button(SDL_CONTROLLER_BUTTON_A,1);poll();assert(host_pad.button&D_CBUTTONS);assert(!(host_pad.button&A_BUTTON));
    button(SDL_CONTROLLER_BUTTON_A,0);poll();
    controller_sdl_shutdown();SDL_JoystickClose(device);SDL_JoystickDetachVirtual(device_index);SDL_Quit();
    puts("PASS real SDL/keyboard readers: remaps, focus/menu/release gates, right-stick camera isolation, disabled/unplugged keyboard, held reconnect, Mario/Tony unchanged (windowless fixture)");
}
