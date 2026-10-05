#ifndef SMN64_DOME_SHATTER_N64_H
#define SMN64_DOME_SHATTER_N64_H
#include "../dome_render/dome_mesh_n64.h"
#include "../web_sprite/sprite_projection_n64.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SMN64_DOME_SHATTER_COUNT 30u
/* Original72838/729BC polygon, F5540. Pointer-free; retain with the complete
 * pending actor/RNG/allocation transaction. Corners are the jittered display
 * copy; base_corners retain the physical trajectory. All positions fixed12. */
typedef struct SmN64DomeShatterFragment {
    int32_t base_corners[4][3],corners[4][3],velocity[3],floor_y;
    int16_t age;
    uint16_t lifetime;
    uint8_t gray,alive;
    uint64_t graphical_serial;
} SmN64DomeShatterFragment;
typedef struct SmN64DomeShatterDraw {
    SmN64SpriteQuad submitted;
    uint8_t environment_rgba[4];
    uint32_t combiner[2],render_mode;
    uint64_t graphical_serial;
} SmN64DomeShatterDraw;
/* Ordinary release82F04->58560 only: model249/node4/texture403,30 triangles.
 * geometry must be identity-verified original data. Output is ORIGINAL CREATION
 * ORDER (oldest first); F5540 tick/draw needs descending graphical_serial.
 * render_translation is the SOURCE CACHED instance+98 native float translation,
 * not current actor position.58560 multiplies it by16; no actor rotation/scale.
 * floor_y is B4B6C's resolved source fixed12 floor; caller owns world query.
 * Source consumes Random(30) speed then Random(30) lifetime per triangle.
 * No first update here: later6701C F5540 must tick newborns this source frame.
 * Return1, or-1 without changing output/RNG/clock on rejection. */
int smn64_dome_shatter_spawn(const SmN64DomeGeometry *,
    const float render_translation[3],int32_t floor_y,uint32_t rng[3],
    uint64_t *shared_clock,SmN64DomeShatterFragment out[SMN64_DOME_SHATTER_COUNT]);
/* Exact72838 numeric constructor. xyz are three world-space binary32 source
 * positions (before fixed12 conversion); the fourth point duplicates point2.
 * Supplied velocity and floor are source fixed12. RNG only draws lifetime.
 * Returns1, or-1 transactionally on malformed/unsupported numeric inputs. */
int smn64_dome_shatter_fragment_init(SmN64DomeShatterFragment *,
    const float xyz[3][3],const int32_t velocity[3],int32_t floor_y,uint32_t rng[3]);
/* Full729BC update.8 Random(41) draws per live tick including terminal tick.
 * Returns1 live,0 newly/already dead,-1 invalid without mutation. */
int smn64_dome_shatter_fragment_tick(SmN64DomeShatterFragment *,uint32_t rng[3]);
/* Source62FEC/B9DE8: native .125 matrix, s16(fixed12>>13), full64x64 UV,
 * white SHADE, ENV=(0,0,0,2*((gray-1)&127)), texture403×ENV. Thus RGB is BLACK.
 * Both source windings emitted; submitted contains one pair for an explicit
 * two-sided host policy. Caller owns CE0D4 level2 and material caching/state.
 * Never tick/RNG at draw time. Return1 live,0 dead,-1 invalid. */
int smn64_dome_shatter_snapshot(const SmN64DomeShatterFragment *,SmN64DomeShatterDraw *);
#ifdef __cplusplus
}
#endif
#endif
