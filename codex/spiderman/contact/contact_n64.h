#ifndef SMN64_FREE_CONTACT_H
#define SMN64_FREE_CONTACT_H
#include "../climbing/climbing_n64.h"
/* Original 800841D0 ordinary non-adhered/non-swinger/non-active-zip owner.
 * Positions/velocities are native fixed12, Y down. Normals are fixed12 s16.
 * Source body radius11A2 is70, body offset11A0 is96. */
typedef enum SmN64FreeRay {
    SMN64_FREE_HELD=1, SMN64_FREE_FORWARD, SMN64_FREE_RIGHT,
    SMN64_FREE_LEFT, SMN64_FREE_IDLE, SMN64_FREE_UP, SMN64_FREE_DOWN
} SmN64FreeRay;
typedef struct SmN64FreeQuery {
    SmN64ClimbQuery ray;
    SmN64FreeRay kind;
    unsigned pass; /* wall-deflection pass0 or1 */
} SmN64FreeQuery;
typedef struct SmN64FreeHit {
    SmN64ClimbHit hit;
    /* Source downward query returns a 60-byte collision record at line+F8.
     * Numeric words are opaque retained source metadata, never host pointers.
     * Geometry adapters must explicitly provide it (or return unavailable).
     * Other rays do not consume this record. */
    uint32_t ground_record[15];
    uint8_t has_ground_record;
} SmN64FreeHit;
typedef int (*SmN64FreeTrace)(void *, const SmN64FreeQuery *, SmN64FreeHit *);
typedef struct SmN64FreeContact {
    int32_t position[3], velocity[3], acceleration[3];
    uint8_t drag[3];
    int32_t elapsed_ticks;
    uint32_t state;
    uint16_t animation;
    int16_t frame;
    int32_t adhered, swinger_present, radial_constraint;
    int32_t previous_platform_present;
    int32_t held_object;
    uint16_t body_offset, body_radius, collision;
    int16_t angles[3], normal[3];
    int32_t d48;
    uint8_t ground_grace;
    SmN64ClimbHit side, ceiling;
    SmN64ClimbQuery ceiling_query;
    uint16_t idle_probe_angle;
    int32_t idle_probe_offset[3];
    uint32_t random_state[3];
    int32_t contact_position[3];
    uint32_t ground_record[15];
    int32_t lighting_target; /* intent only; -1 means no new request */
} SmN64FreeContact;
/* Runs the original scalar integration AND all ordinary world contact queries.
 * Animation must already have advanced. This does not run player AI or basis.
 * Query callbacks are pure/read-only, strict1 success; no-hit is hit.present=0.
 * Return1 success; -1 invalid/other owner; -3 unavailable query;
 * -4 moving-platform dependency; -5 radial/script constraint dependency;
 * -6 missing surface/ground metadata. All negatives leave state unchanged.
 * No marker callback: this original owner uses body-offset rays, NOT markers.
 * Retained inactive hit payloads are intentionally preserved. */
int smn64_free_contact_run(SmN64FreeContact *, SmN64FreeTrace, void *,
                           uint16_t bright_target, uint16_t normal_target);
#endif
