#ifndef GFX_PC_H
#define GFX_PC_H

#include "types.h"
#include "pc/gfx/gfx.h"

enum ShaderFlag {
    SHADER_FLAG_HUE,
    SHADER_FLAG_SATURATION,
    SHADER_FLAG_BRIGHTNESS,
    SHADER_FLAG_CONTRAST,
    SHADER_FLAG_EXPOSURE,
    SHADER_FLAG_DITHERING,
    SHADER_FLAG_POSTERIZATION,
    SHADER_FLAG_SCANLINES,
    SHADER_FLAG_MAX
};

struct GfxRenderingAPI;
struct GfxWindowManagerAPI;

/* Host-only display-list boundary: finish 3D characters before native 2D UI.
 * Other NOOP tags retain their native no-op meaning. */
#define GFX_CODEX_WORLD_CHARACTERS_TAG 0x524F434Bu
typedef void (*GfxCodexWorldDraw)(const float view[16],const float projection[16],const int viewport[4]);

extern Vec3f gLightingDir;
extern Color gLightingColor[2];
extern Color gVertexColor;
extern Color gFogColor;
extern f32 gFogIntensity;

extern bool gFullbright;

extern int gShaderFlags[SHADER_FLAG_MAX];
extern f32 gDefaultShaderFlagValues[SHADER_FLAG_MAX];
extern f32 gShaderFlagValues[SHADER_FLAG_MAX];
extern bool gShaderFlagsEnabled;

#ifdef __cplusplus
extern "C" {
#endif

void gfx_init(struct GfxWindowManagerAPI *wapi, struct GfxRenderingAPI *rapi, const char *window_title);
struct GfxRenderingAPI *gfx_get_current_rendering_api(void);
/* Last real perspective camera in this frame; column-major OpenGL matrices. */
bool gfx_codex_get_camera(float view[16], float projection[16], int viewport[4]);
/* Only a backend with a current compatible GL context registers a callback. */
void gfx_codex_set_world_draw(GfxCodexWorldDraw draw);
/* Last real host RDP ENV alpha at the completed 3D-world pass in this frame.
 * False before that boundary; does not read ambient GL or change renderer state. */
bool gfx_codex_get_world_environment_alpha(uint8_t *alpha);
bool gfx_codex_get_world_material_state(uint8_t environment[4],uint8_t fog[4]);
void gfx_codex_set_interpolated_view(const float view[16]);
void gfx_start_frame(void);
void gfx_run(Gfx *commands);
void gfx_end_frame_render(void);
void gfx_display_frame(void);
void gfx_end_frame(void);
void gfx_shutdown(void);
void gfx_pc_precomp_shader(uint32_t rgb1, uint32_t alpha1, uint32_t rgb2, uint32_t alpha2, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
