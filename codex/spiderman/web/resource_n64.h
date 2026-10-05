#ifndef SMN64_WEB_RESOURCE_H
#define SMN64_WEB_RESOURCE_H
#include <stdint.h>
/* Source fields retain their actual meaning/width. Full cartridge is 4096.
 * Suit 2 and 3 bypass spending in this N64 revision. No PC constants used. */
typedef struct SmN64WebResource {
    int32_t remaining;            /* player704 */
    int32_t cartridges;           /* player708 */
    int32_t web_type;             /* player718: cleared on refill/empty */
    int32_t player_2cc;           /* original nonzero exemption */
    int32_t infinite_web;         /* global800F5F50 */
    int32_t difficulty;           /* global800F5EF4: 0=>1/32, 1=>1/2 */
    int32_t allow_empty;          /* player1108: succeeds with no inventory */
    int32_t voice_busy;           /* global800ED8E8 suppresses empty voice */
    uint8_t suit;                 /* global800ECD7A */
} SmN64WebResource;
typedef struct SmN64WebResourceEvent {
    int32_t sound;                /* refill:0x1e; otherwise0 */
    int32_t voice_group;          /* empty:0x21; otherwise0 */
    int32_t voice_variant;        /* original rng(3)+2, or0 */
} SmN64WebResourceEvent;
/* Original RNG 800AB7F4. A single shared stream is owned by the actor runtime. */
uint32_t smn64_web_random(uint32_t state[3], uint32_t range);
/* Returns original success0/1, -1 on NULL inputs. Emitted audio is an explicit
 * host event. One cartridge at most is loaded, even for costs above4096; amounts
 * are source signed32 and scaling preserves original low-word wrap. */
int smn64_web_consume(SmN64WebResource *, int32_t amount,
                       uint32_t random_state[3], SmN64WebResourceEvent *);
/* Source800A2F00 inventory addition; source health is signed16. Cartridge cap
 * is2 for suits7..9, otherwise10. At most one cartridge is added per call.
 * `refill_tick` is source player70C and changes only on cartridge increment. */
int smn64_web_refill(SmN64WebResource *,int32_t amount,int16_t health,
                     uint32_t now,uint32_t *refill_tick);
/* Source8D1AC..8D218 regeneration before swinger update. Must run once per AI
 * pass, with source elapsed ticks. It is independent of consumption exemptions. */
int smn64_web_resource_tick(SmN64WebResource *,int16_t health,int32_t elapsed_ticks,
                            uint32_t now,uint32_t *refill_tick);
typedef struct SmN64WebStartInventory {
    int32_t remaining, cartridges;
    int16_t health, field_2c0;
} SmN64WebStartInventory;
/* Source constructor94924..94A44, fresh zeroed allocation. Explicit mode and
 * level are inputs, not guessed defaults. Zero/zero saved inventory selects a
 * fresh load; either nonzero restores both words. Returns1 or-1 unsupported mode. */
int smn64_web_inventory_start(int32_t difficulty,uint8_t suit,uint32_t level,
                              int32_t saved_remaining,int32_t saved_cartridges,
                              SmN64WebStartInventory *);
#endif
