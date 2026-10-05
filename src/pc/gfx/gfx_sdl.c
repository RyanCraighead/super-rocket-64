#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include <SDL2/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if FOR_WINDOWS
#define GLEW_STATIC
#include <GL/glew.h>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#else
#define GL_GLEXT_PROTOTYPES 1

#ifdef OSX_BUILD
#include <SDL2/SDL_opengl.h>
#else
#include <SDL2/SDL_opengles2.h>
#endif

#endif // End of OS-Specific GL defines

#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "gfx_window_manager_api.h"
#include "gfx_pc.h"
#include "wheel_overlay_gl.h"
#include "pc/character_wheel.h"
#include "pc/oot_link_runtime.h"
#include "pc/bm64_runtime.h"
#include "pc/rocket_runtime.h"
#include "pc/character_net.h"
#include "pc/bk_runtime.h"
#include "pc/spiderman_runtime.h"
#include "pc/thps_runtime.h"
#include "game/thps_adapter.h"
#include "game/character_presentation.h"
#include "pc/spiderman_web_scene.h"
#include "pc/spiderman_trail_scene.h"
#include "pc/spiderman_effect_scene.h"
#include "pc/spiderman_dome_scene.h"
#include "game/spiderman_dome_host.h"
#include "game/spiderman_web_attack_host.h"
#include "game/spiderman_combat_host.h"
#include "game/spiderman_adapter.h"
#include "gfx_screen_config.h"
#include "../pc_main.h"
#include "../configfile.h"
#include "../cliopts.h"

#include "pc/controller/controller_keyboard.h"
#include "pc/controller/controller_mouse.h"
#include "pc/controller/controller_sdl.h"
#include "pc/controller/controller_bind_mapping.h"
#include "pc/utils/misc.h"
#include "pc/mods/mod_import.h"
#include "pc/rom_checker.h"
#include "pc/djui/djui.h"
#include "pc/djui/djui_panel_pause.h"
#include "game/level_update.h"

#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif

// TODO: figure out if this shit even works
#ifdef VERSION_EU
# define FRAMERATE 25
#else
# define FRAMERATE 30
#endif

static SDL_Window *wnd;
static SDL_GLContext ctx = NULL;
static void gfx_sdl_draw_rocket_world(const float view[16],const float projection[16],const int viewport[4]);

static kb_callback_t kb_key_down = NULL;
static kb_callback_t kb_key_up = NULL;
static void (*kb_all_keys_up)(void) = NULL;
static void (*kb_text_input)(char*) = NULL;
static void (*kb_text_editing)(char*, int) = NULL;

static void (*m_scroll)(float, float) = NULL;

#define IS_FULLSCREEN() ((SDL_GetWindowFlags(wnd) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)

static inline void gfx_sdl_set_vsync(const bool enabled) {
    SDL_GL_SetSwapInterval(enabled);
}

static void gfx_sdl_set_fullscreen(void) {
    if (configWindow.reset)
        configWindow.fullscreen = false;
    if (configWindow.fullscreen == IS_FULLSCREEN())
        return;
    if (configWindow.fullscreen) {
        SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(wnd, 0);
        SDL_ShowCursor(1);
        configWindow.exiting_fullscreen = true;
    }
}

static void gfx_sdl_reset_dimension_and_pos(void) {
    if (configWindow.exiting_fullscreen) {
        configWindow.exiting_fullscreen = false;
        SDL_ShowCursor(0);
    }

    if (configWindow.reset) {
        configWindow.x = WAPI_WIN_CENTERPOS;
        configWindow.y = WAPI_WIN_CENTERPOS;
        configWindow.w = DESIRED_SCREEN_WIDTH;
        configWindow.h = DESIRED_SCREEN_HEIGHT;
        configWindow.reset = false;
    } else if (!configWindow.settings_changed) {
        return;
    }

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    SDL_SetWindowSize(wnd, configWindow.w, configWindow.h);
    SDL_SetWindowPosition(wnd, xpos, ypos);
    // in case vsync changed
    gfx_sdl_set_vsync(configWindow.vsync);
}

static void gfx_sdl_init(const char *window_title) {
#if defined(_WIN32)
    SetProcessDPIAware();
#endif

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_StartTextInput();

    if (configWindow.msaa > 0) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, configWindow.msaa);
    } else {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    }

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef USE_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);  // These attributes allow for hardware acceleration on RPis.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    unsigned visibility=SDL_WINDOW_SHOWN;
#ifdef ROCKET_CAR_QA
    if(character_net_qa_hidden()){visibility=SDL_WINDOW_HIDDEN;configWindow.fullscreen=false;}
#endif
    wnd = SDL_CreateWindow(
        window_title,
        xpos, ypos, configWindow.w, configWindow.h,
        SDL_WINDOW_OPENGL | visibility | SDL_WINDOW_RESIZABLE
    );
    ctx = SDL_GL_CreateContext(wnd);
    gfx_codex_set_world_draw(gfx_sdl_draw_rocket_world);

    gfx_sdl_set_vsync(configWindow.vsync);

    gfx_sdl_set_fullscreen();
    if (configWindow.fullscreen) {
        SDL_ShowCursor(SDL_DISABLE);
    }

    controller_bind_init();
}

bool gfx_sdl_check_opengl_compatibility(void) {
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            return false;
        }
    }

    // hidden window
    SDL_Window* window = SDL_CreateWindow(
        "",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );

    if (!window) {
        return false;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);

    if (!ctx) {
        SDL_DestroyWindow(window);
        return false;
    }

    SDL_GL_MakeCurrent(window, ctx);
    bool validVersion = gfx_opengl_check_compatibility();

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);

    return validVersion;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    run_one_game_iter();
}

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
    int w, h;
    SDL_GetWindowSize(wnd, &w, &h);
    if (width) *width = w;
    if (height) *height = h;
}

static void gfx_sdl_onkeydown(int scancode) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);

    if ((state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]) && state[SDL_SCANCODE_RETURN]) {
        configWindow.fullscreen = !configWindow.fullscreen;
        configWindow.settings_changed = true;
        return;
    }

    if (kb_key_down)
        kb_key_down(translate_sdl_scancode(scancode));
}

static void gfx_sdl_onkeyup(int scancode) {
    if (kb_key_up)
        kb_key_up(translate_sdl_scancode(scancode));
}

static void gfx_sdl_onscroll(float x, float y) {
    if (m_scroll)
        m_scroll(x, y);
}

static void gfx_sdl_ondropfile(char* path) {
#ifdef _WIN32
    char portable_path[SYS_MAX_PATH];
    if (sys_windows_short_path_from_mbs(portable_path, SYS_MAX_PATH, path)) {
        if (!gRomIsValid) {
            rom_on_drop_file(portable_path);
        } else if (gGameInited) {
            mod_import_file(portable_path);
        }
    }
#else
    if (!gRomIsValid) {
        rom_on_drop_file(path);
    } else if (gGameInited) {
        mod_import_file(path);
    }
#endif
}

static void gfx_sdl_codex_input_priority(void) {
    /* Single-player pause skips native actor updates, so cancel transient Link
       combat/input here too. This never advances animation or simulation. */
    if (gCLIOpts.ootLink && sCurrPlayMode == PLAY_MODE_PAUSED) oot_link_runtime_pause();
    if (rocket_runtime_enabled() && (sCurrPlayMode == PLAY_MODE_PAUSED || character_wheel_blocks_gameplay() || 0 || !(SDL_GetWindowFlags(wnd) & SDL_WINDOW_INPUT_FOCUS))) rocket_runtime_interrupt();
    if (gCLIOpts.thpsOriginal && sCurrPlayMode == PLAY_MODE_PAUSED) thps_adapter_pause_inputs();
}

static void gfx_sdl_handle_events(void) {
    character_wheel_update();
    gfx_sdl_codex_input_priority();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (character_wheel_handle_event(&event)) {
            /* Opening/closing is itself an input-owner transition. Refresh
             * before the next queued SDL event, not just before rendering, so
             * a following event observes the wheel input owner. */
            gfx_sdl_codex_input_priority();
            continue;
        }
        if(event.type==SDL_WINDOWEVENT){
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST||event.window.event==SDL_WINDOWEVENT_HIDDEN||event.window.event==SDL_WINDOWEVENT_MINIMIZED)controller_sdl_set_window_active(0);
            else if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)controller_sdl_set_window_active(1);
        }

        switch (event.type) {
            case SDL_TEXTINPUT:
                kb_text_input(event.text.text);
                break;
            case SDL_TEXTEDITING: //IME composition
                kb_text_editing(event.edit.text,event.edit.start);
                break;
            case SDL_KEYDOWN:
                gfx_sdl_onkeydown(event.key.keysym.scancode);
                break;
            case SDL_KEYUP:
                gfx_sdl_onkeyup(event.key.keysym.scancode);
                break;
            case SDL_MOUSEWHEEL:
                gfx_sdl_onscroll(event.wheel.preciseX, event.wheel.preciseY);
                break;
            case SDL_WINDOWEVENT:
                if(gCLIOpts.thpsOriginal) {
                    if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST || event.window.event==SDL_WINDOWEVENT_HIDDEN) {
                        thps_adapter_set_window_active(0);
                        if(kb_all_keys_up)kb_all_keys_up();
                    } else if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED || event.window.event==SDL_WINDOWEVENT_SHOWN)
                        thps_adapter_set_window_active(1);
                }
                if (!IS_FULLSCREEN()) {
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_MOVED:
                            if (!configWindow.exiting_fullscreen) {
                                if (event.window.data1 >= 0) configWindow.x = event.window.data1;
                                if (event.window.data2 >= 0) configWindow.y = event.window.data2;
                            }
                            break;
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            configWindow.w = event.window.data1;
                            configWindow.h = event.window.data2;
                            break;
                    }
                }
                break;
            case SDL_DROPFILE:
                gfx_sdl_ondropfile(event.drop.file);
                break;
            case SDL_QUIT:
                game_exit();
                break;
        }
    }

    if (configWindow.settings_changed) {
        gfx_sdl_set_fullscreen();
        gfx_sdl_reset_dimension_and_pos();
        configWindow.settings_changed = false;
    }
}

static void gfx_sdl_set_keyboard_callbacks(kb_callback_t on_key_down, kb_callback_t on_key_up,
void (*on_all_keys_up)(void), void (*on_text_input)(char*), void (*on_text_editing)(char*, int))
{
    kb_key_down = on_key_down;
    kb_key_up = on_key_up;
    kb_all_keys_up = on_all_keys_up;
    kb_text_input = on_text_input;
    kb_text_editing = on_text_editing;
}

static void gfx_sdl_set_scroll_callback(void (*on_scroll)(float, float)) {
    m_scroll = on_scroll;
}

static bool gfx_sdl_start_frame(void) {
    return true;
}

/* The host may publish its previous all-zero viewport on the first frame.
 * Treat unusable camera inputs as readiness, before invoking any renderer;
 * never replace them with a guessed viewport/matrix or advance unseen source. */
static bool spiderman_camera_usable(const float view[16],const float projection[16],const int viewport[4]) {
    if(viewport[2]<=0||viewport[3]<=0||viewport[2]>65536||viewport[3]>65536)return false;
    for(int i=0;i<16;++i)if(!isfinite(view[i])||fabsf(view[i])>1e7f||!isfinite(projection[i])||fabsf(projection[i])>1e7f)return false;
    return true;
}
static void gfx_sdl_draw_rocket_world(const float view[16],const float projection[16],const int viewport[4]) {
    if(!ctx||SDL_GL_GetCurrentContext()!=ctx)return;
    RocketSnapshot presentation;
    if(character_presentation_car_snapshot(&presentation))
        character_presentation_draw(view,projection,viewport);
    else if(rocket_runtime_owns_controls())
        rocket_runtime_draw(view,projection,viewport);
    if(gCLIOpts.characterNet)character_net_draw(view,projection,viewport);
}
static void gfx_sdl_draw_other_presentation(void) {
    RocketSnapshot car;
    /* Octane has already rendered at the world/UI boundary, including during
     * native actions that temporarily own its locomotion. */
    if(character_presentation_car_snapshot(&car))return;
    float view[16],projection[16];int viewport[4];
    if(gfx_codex_get_camera(view,projection,viewport))character_presentation_draw(view,projection,viewport);
}
static void gfx_sdl_swap_buffers_begin(void) {
    static SpidermanDomeRenderState dome_material;
    static uint32_t dome_tick;
    static bool dome_frame_valid;
    /* Pause may have begun after the event pump during this game frame. */
    gfx_sdl_codex_input_priority();
    if (gCLIOpts.thpsOriginal && thps_adapter_render_required()) {
        float view[16], projection[16]; int viewport[4];
        if (!gfx_codex_get_camera(view, projection, viewport) || !spiderman_camera_usable(view, projection, viewport))
            thps_adapter_render_wait();
        else if (!thps_runtime_draw(view, projection, viewport))
            thps_adapter_render_failure(thps_runtime_status());
        else thps_adapter_render_success();
    }
    if (gCLIOpts.bkDuo) {
        float view[16], projection[16]; int viewport[4];
        if (gfx_codex_get_camera(view, projection, viewport)) bk_runtime_draw(view, projection, viewport);
    }
    if (gCLIOpts.spidermanOriginal && spiderman_adapter_render_required()) {
        float view[16], projection[16]; int viewport[4];uint8_t environment[4],fog[4];SpidermanBehaviorSnapshot source;
        if (!gfx_codex_get_camera(view, projection, viewport) || !spiderman_camera_usable(view, projection, viewport) ||
            !gfx_codex_get_world_material_state(environment,fog)||!spiderman_adapter_snapshot(&source))
            spiderman_adapter_render_wait();
        else if (!spiderman_runtime_draw(view, projection, viewport))
            spiderman_adapter_render_failure(spiderman_runtime_status());
        else {
            /* Source57320 transparent body pass precedes6731C graphicals.
             * Host ENV/fog are explicit captured caller inputs. A display redraw
             * may reselect materials, but shared404 scroll advances only once
             * per committed source frame, including pause/camera waits. */
            if(!dome_frame_valid||source.ticks<dome_tick)memset(&dome_material,0,sizeof dome_material);
            if(!dome_frame_valid||source.ticks!=dome_tick)smn64_dome_scroll_frame_end(&dome_material.scroll404);
            memcpy(dome_material.environment_rgba,environment,4);memcpy(dome_material.fog_rgba,fog,4);dome_material.texture_cache_valid=0;dome_material.source_delta_seconds=2.f/60.f;
            if(!spiderman_dome_scene_draw(view,projection,viewport,&dome_material))spiderman_adapter_render_failure(spiderman_dome_scene_status());
            else if(!spiderman_effect_scene_draw(view,projection,viewport,dome_material.environment_rgba[3],NULL))spiderman_adapter_render_failure(spiderman_effect_scene_status());
            else {dome_tick=source.ticks;dome_frame_valid=true;spiderman_adapter_render_success();}
        }
    }else dome_frame_valid=false;
    if (gCLIOpts.bm64Bomberman) {
        float view[16], projection[16]; int viewport[4];
        if (gfx_codex_get_camera(view, projection, viewport)) bm64_runtime_draw(view, projection, viewport);
    }
    if (gCLIOpts.ootLink) {
        float view[16], projection[16]; int viewport[4];
        if (gfx_codex_get_camera(view, projection, viewport)) {
            oot_link_runtime_draw(view, projection, viewport);
        }
    }
    gfx_sdl_draw_other_presentation();

    { int width=0,height=0; SDL_GL_GetDrawableSize(wnd,&width,&height); character_wheel_render(width,height); }
#ifdef ROCKET_CAR_QA
    extern void rocket_qa_swap(void *window);
    rocket_qa_swap(wnd);
    extern void thps_native_qa_swap(SDL_Window *window);
    thps_native_qa_swap(wnd);
    character_net_qa_observe(wnd);
#endif
    SDL_GL_SwapWindow(wnd);
}

static void gfx_sdl_swap_buffers_end(void) {
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

static void gfx_sdl_delay(u32 ms) {
    SDL_Delay(ms);
}

static int gfx_sdl_get_max_msaa(void) {
    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (maxSamples > 16) { maxSamples = 16; }
    return maxSamples;
}

static void gfx_sdl_set_window_title(const char* title) {
    SDL_SetWindowTitle(wnd, title);
}

static void gfx_sdl_reset_window_title(void) {
    SDL_SetWindowTitle(wnd, TITLE);
}

static void gfx_sdl_shutdown(void) {
    gfx_codex_set_world_draw(NULL);
    character_wheel_shutdown();
    rocket_runtime_shutdown();
    oot_link_runtime_shutdown();
    bm64_runtime_shutdown();
    bk_runtime_shutdown();
    thps_runtime_shutdown();
    spiderman_dome_host_disable();
    spiderman_dome_scene_shutdown();
    spiderman_web_attack_host_disable();
    spiderman_effect_scene_shutdown();
    spiderman_combat_host_shutdown();
    spiderman_trail_scene_shutdown();
    spiderman_web_scene_shutdown();
    spiderman_runtime_shutdown();
    wheel_overlay_shutdown();
    if (SDL_WasInit(0)) {
        if (ctx) { SDL_GL_DeleteContext(ctx); ctx = NULL; }
        if (wnd) { SDL_DestroyWindow(wnd); wnd = NULL; }
        SDL_Quit();
    }
}

static bool gfx_sdl_has_focus(void) {
    return (SDL_GetWindowFlags(wnd) & SDL_WINDOW_INPUT_FOCUS);
}

static void gfx_sdl_start_text_input(void) { SDL_StartTextInput(); }
static void gfx_sdl_stop_text_input(void) { SDL_StopTextInput(); }

static char* gfx_sdl_get_clipboard_text(void) {
    static char clipboard_buf[WAPI_CLIPBOARD_BUFSIZ];

    char* text = SDL_GetClipboardText();
    strncpy(clipboard_buf, text, WAPI_CLIPBOARD_BUFSIZ - 1);
    SDL_free(text);

    clipboard_buf[WAPI_CLIPBOARD_BUFSIZ - 1] = '\0';
    return clipboard_buf;
}

static void gfx_sdl_set_clipboard_text(const char* text) { SDL_SetClipboardText(text); }
static void gfx_sdl_set_cursor_visible(bool visible) { SDL_ShowCursor((visible || 0 || character_wheel_owns_pointer()) ? SDL_ENABLE : SDL_DISABLE); }

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_scroll_callback,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time,
    gfx_sdl_shutdown,
    gfx_sdl_start_text_input,
    gfx_sdl_stop_text_input,
    gfx_sdl_get_clipboard_text,
    gfx_sdl_set_clipboard_text,
    gfx_sdl_set_cursor_visible,
    gfx_sdl_delay,
    gfx_sdl_get_max_msaa,
    gfx_sdl_set_window_title,
    gfx_sdl_reset_window_title,
    gfx_sdl_has_focus
};
