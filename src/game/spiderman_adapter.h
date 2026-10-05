#ifndef SM64_SPIDERMAN_ADAPTER_H
#define SM64_SPIDERMAN_ADAPTER_H
#include <stdint.h>
struct MarioState;
struct Object;
typedef struct SpidermanBehaviorSnapshot {
    uint32_t state,ticks;
    int32_t adhered,wall,ceiling,clip,frame,health,web_remaining,native_position[3];
    int32_t collision,side_present,side_normal[3],forward[3],approach_ticks;
    int32_t maximum_health,web_cartridges,glove_hits;
    uint32_t combat_target;
} SpidermanBehaviorSnapshot;
/* Acquire/validate ownership before native interaction arbitration, without
 * advancing source time, posing, rendering, or applying queued damage. */
int spiderman_adapter_prepare_interactions(struct MarioState *m);
int spiderman_adapter_update(struct MarioState *m);
int spiderman_adapter_render_required(void);
/* Host camera/viewport readiness is reversible: hold the committed frame and
 * clocks until that same snapshot can draw. Actual renderer failure is latched. */
void spiderman_adapter_render_wait(void);
void spiderman_adapter_render_success(void);
void spiderman_adapter_render_failure(const char *reason);
int spiderman_adapter_enemy_contact(struct MarioState *,struct Object *);
int spiderman_adapter_process_interaction(struct MarioState *,struct Object *);
void spiderman_adapter_suspend(void);
/* Called only by the opt-in wheel; enables shared host/source health tracking. */
void spiderman_adapter_set_selected(int selected);
const char *spiderman_adapter_switch_reason(void);
/* Host input arbitration only: buttons owned by Spider-Man during gameplay.
 * Camera wrapper restores original values before the next controller poll. */
uint16_t spiderman_adapter_reserved_buttons(void);
const char *spiderman_adapter_status(void);
const char *spiderman_adapter_unavailable_label(void);
int spiderman_adapter_snapshot(SpidermanBehaviorSnapshot *);
#endif
