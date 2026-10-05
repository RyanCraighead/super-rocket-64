/* Portable arithmetic recovered from unchanged original scene/material calls.
 * See test_material_oracle.py and MATERIAL_CONTRACT.md. No game asset payload. */
#include "dome_material_n64.h"
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <string.h>

#ifdef __FAST_MATH__
#error "Dome material source arithmetic requires strict IEEE floating point"
#endif
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
               FLT_MAX_EXP == 128, "Dome materials require IEEE binary32");

void smn64_dome_environment(const uint8_t rgb[3], uint8_t rgba[4]) {
    if (!rgb || !rgba) return;
    unsigned c[3], alpha = 0;
    for (unsigned i = 0; i < 3; ++i) {
        c[i] = 2u * rgb[i];
        if (c[i] > 255u) c[i] = 255u;
        if (c[i] > alpha) alpha = c[i];
    }
    for (unsigned i = 0; i < 3; ++i)
        rgba[i] = (uint8_t)(alpha ? c[i] * 255u / alpha : 255u);
    rgba[3] = (uint8_t)alpha;
}

void smn64_dome_scroll_init(smn64_dome_scroll_state *s) {
    if (s) memset(s, 0, sizeof(*s));
}

void smn64_dome_scroll_frame_end(smn64_dome_scroll_state *s) {
    if (s) s->updated = 0;
}

static int valid_scroll(const smn64_dome_scroll_state *s) {
    return s && fegetround() == FE_TONEAREST && isfinite(s->u) && isfinite(s->v) &&
           s->u >= 0.f && s->u <= 1.f && s->v == 0.f;
}

static void scroll_tile(const smn64_dome_scroll_state *s, uint32_t out[2]) {
    const float us = s->u * 32.f;
    const float vt = s->v * 32.f;
    const int32_t uls = (int32_t)(us * 4.f);
    const int32_t ult = (int32_t)(vt * 4.f);
    out[0] = 0xf2000000u | ((uint32_t)uls & 4095u) << 12 |
             ((uint32_t)ult & 4095u);
    out[1] = 0x01000000u | ((uint32_t)(128 + uls - 1) & 4095u) << 12 |
             ((uint32_t)(128 + ult - 1) & 4095u);
}

int smn64_dome_scroll_select(smn64_dome_scroll_state *s,
                            float delta, uint32_t tile[2]) {
    if (!tile || !valid_scroll(s) || !isfinite(delta) || delta < 0.f || delta > 60.f)
        return 0;
    if (!s->updated) {
        const float movement = 2.f * delta;
        float u = s->u + movement;
        while (u > 1.f) u = u - 1.f; /* source retains exact phase1 */
        s->u = u;
        s->updated = 1;
    }
    scroll_tile(s, tile);
    return 1;
}

int smn64_dome_material_for_actor(uint16_t slot, const uint8_t rgb[3],
        uint8_t bc, const uint8_t fog[3],
        const smn64_dome_scroll_state *scroll, smn64_dome_material *out) {
    if (!out || !rgb || (slot != 403 && slot != 404) ||
        (bc != 254 && bc != 255) || (bc == 254 && !fog) ||
        (slot == 404 && !valid_scroll(scroll))) return 0;
    smn64_dome_material value;
    memset(&value, 0, sizeof(value));
    value.texture_slot = slot;
    value.width = value.height = slot == 403 ? 64 : 32;
    value.combiner[0] = slot == 403 ? 0xfc50d3ffu : 0xfc129bffu;
    value.combiner[1] = 0xfffffe38u;
    value.render_mode = bc == 254 ? 0x01504a50u : 0x0c184b50u;
    value.depth_compare = 1;
    smn64_dome_environment(rgb, value.environment_rgba);
    if (bc == 254) {
        memcpy(value.fog_rgba, fog, 3);
        value.fog_rgba[3] = bc;
        value.writes_fog = 1;
    }
    if (slot == 404) {
        value.uses_scroll = 1;
        scroll_tile(scroll, value.tile_size);
    } else {
        value.tile_size[0] = 0xf2000000u;
        value.tile_size[1] = 0x010fc0fcu;
    }
    *out = value;
    return 1;
}
