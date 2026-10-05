#ifndef SMN64_DOME_MATERIAL_N64_H
#define SMN64_DOME_MATERIAL_N64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One shared state for source texture404, never one state per ring actor.
 * Reset updated once at the source-frame end, not on snapshot/draw capture.
 * The source selector cache can omit selection entirely; in that case do not
 * call scroll_select. A second actual selection in one frame only emits the
 * current tile bounds, without advancing the shared phase again. */
typedef struct {
    float u, v;
    uint32_t updated;
} smn64_dome_scroll_state;

typedef struct {
    uint32_t combiner[2];
    uint32_t render_mode;
    uint32_t tile_size[2];
    uint16_t texture_slot, width, height;
    uint8_t environment_rgba[4];
    uint8_t fog_rgba[4];
    uint8_t writes_fog, uses_scroll, depth_compare, depth_write;
} smn64_dome_material;

/* Null inputs are ignored without modifying output. */
void smn64_dome_environment(const uint8_t logical_rgb[3], uint8_t rgba[4]);
void smn64_dome_scroll_init(smn64_dome_scroll_state *state);
void smn64_dome_scroll_frame_end(smn64_dome_scroll_state *state);
/* Delta is the source float at800FBF38, in seconds. This bounded API accepts
 * finite 0..60 seconds and finite source phases0..1. Unsupported inputs return
 * zero without altering state or output. Arithmetic requires IEEE binary32 and
 * FE_TONEAREST; compile without fast math or floating-point contraction. */
int smn64_dome_scroll_select(smn64_dome_scroll_state *state,
                            float delta_seconds, uint32_t tile_size[2]);
/* Pure current-state snapshot; does not advance scrolling or modify ENV.
 * Describes the source scene order: preselect material (ENV white), then emit
 * actor ENV, then draw the cached material. Fog RGB comes from source1013C8.
 * BC254 requests source fog alpha254/mode01504A50; BC255 leaves mode0C184B50.
 * The independent105034 global override is outside this ordinary contract.
 * Fire texture402 is intentionally unsupported. No GL blending is invented. */
int smn64_dome_material_for_actor(uint16_t texture_slot,
        const uint8_t logical_rgb[3], uint8_t instance_bc,
        const uint8_t source_fog_rgb[3],
        const smn64_dome_scroll_state *current_scroll,
        smn64_dome_material *out);

#ifdef __cplusplus
}
#endif

#endif
