#ifndef _PC_MAIN_H
#define _PC_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gfx/gfx_pc.h"

#include "gfx/gfx_opengl.h"
#include "gfx/gfx_direct3d11.h"

#include "gfx/gfx_dxgi.h"
#include "gfx/gfx_sdl.h"
#include "gfx/gfx_dummy.h"

#include "audio/audio_api.h"
#include "audio/audio_sdl.h"
#include "audio/audio_null.h"

#define TITLE "Super Rocket 64"
#define AT_STARTUP __attribute__((constructor))

extern struct AudioAPI* gAudioApi;
extern struct GfxWindowManagerAPI* gWindowApi;
extern struct GfxRenderingAPI* gRenderApi;

extern bool gGameInited;
extern bool gGfxInited;

extern f32 gMasterVolume;

extern u8 gLuaVolumeMaster;
extern u8 gLuaVolumeLevel;
extern u8 gLuaVolumeSfx;
extern u8 gLuaVolumeEnv;

extern struct GfxWindowManagerAPI* wm_api;
void produce_one_dummy_frame(void (*callback)(), u8 clearColorR, u8 clearColorG, u8 clearColorB);
void game_deinit(void);
void game_exit(void);

#ifdef __cplusplus
}
#endif

#endif // _PC_MAIN_H
