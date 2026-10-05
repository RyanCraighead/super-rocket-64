#ifndef SMN64_WEB_OWNER_H
#define SMN64_WEB_OWNER_H
#include "state_n64.h"
/* Source-shaped shared phase helpers. These do not call a host world or state
 * dispatcher and must NOT be combined with a second player_begin/finish pass.
 * Read OWNER_CONTRACT.md for the exact cross-module order and synchronization. */
typedef struct SmN64WebOwner {
    int32_t acceleration[3]; /* retained70/74/78: used before next selection */
    uint8_t drag[3];         /* retained7c/7d/7e: used before next selection */
    int32_t previous_velocity_y; /* source savedS1 before841D0 */
    uint32_t refill_tick;    /* player70c */
    int16_t health;          /* playerE2 */
} SmN64WebOwner;
/* Base actor animation advance(old rate), resource prepass, existing swinger
 * advance(absolute now), rate reset, previous-Y snapshot, contact-prefix reset.
 * Source also clears D48, EB4 and platform10B4, not represented by runtime;
 * owner must clear those in the shared climbing/contact actor at this boundary.
 * Existing side surface/normal fields are RETAINED; only side.hit is cleared.
 * Missing callbacks are handled by the subsequent transactional physics step. */
/* Production graphics composition uses external original animation advance,
 * then visuals_zip_actors(now), then this AI prepass without advancing twice. */
int smn64_web_owner_after_animation(SmN64WebRuntime *,SmN64WebOwner *,int32_t elapsed_ticks);
/* Convenience numeric composition when no zip actor list is present. */
int smn64_web_owner_begin(SmN64WebRuntime *,SmN64WebOwner *,int32_t elapsed_ticks);
/* Source8D2D8..8D32C plus8D45C..8D4BC. Call exactly after physics/contact,
 * before input processing/ramp/state dispatch. Degenerate basis returns-5
 * without mutation (source idle fallback is an explicit integration boundary). */
int smn64_web_owner_basis_drag(SmN64WebRuntime *,SmN64WebOwner *);
/* Sorted authored web successor subset, at8D8F4 before state dispatch.
 * Returns prior clip when transitioned,65535 when unchanged,-1 invalid count.
 * Retains original rate and animation run semantics. Global successor owners
 * can implement this once instead; do not invoke both on the same pass. */
int smn64_web_owner_successor(SmN64WebRuntime *,const uint16_t *,size_t);
/* Source92710..92750 retained acceleration selection. Called after state/turn;
 * selects for NEXT physics pass. Does not replace92750..92B74 target velocity. */
void smn64_web_owner_acceleration(const SmN64WebRuntime *,SmN64WebOwner *);
#endif
