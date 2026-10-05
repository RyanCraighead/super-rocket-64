#ifndef THPS1_AIR_ANIMATION_H
#define THPS1_AIR_ANIMATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure fields recovered from original object offsets 0x21c..0x22b.
 * `rate` is Q16 clip frames per nominal 30-Hz source update; run preserves it.
 */
typedef struct ThpsAnim {
    int16_t frame;
    uint16_t id;
    uint8_t mode;
    int8_t direction, target, continuation;
    uint16_t fraction;
    uint8_t count, finished;
    uint32_t rate;
} ThpsAnim;
/* frame_counts is original per-clip header byte +0x0b at stride8.
 * Array must hold all clip_count entries AND fallback clip46, even if the
 * requested ID is outside clip_count. No original asset bytes are bundled.
 */
typedef struct ThpsAnimBank {
    const uint8_t *frame_counts;
    uint32_t clip_count;
} ThpsAnimBank;
void thps1_anim_run(ThpsAnim *, const ThpsAnimBank *, int32_t id,
                    int32_t from, int32_t to, int32_t continuation);
void thps1_anim_advance(ThpsAnim *, const ThpsAnimBank *, int32_t dt8);
#ifdef __cplusplus
}
#endif
#endif
