#ifndef GAME_THPS_RAILS_H
#define GAME_THPS_RAILS_H

/* Explicit SM64-world ledge tags, NOT original THPS collision/level data.
 * All geometry is unoffset host X/Y-up/Z. The source actor/foot-origin
 * conversion belongs to the adapter, never the authored endpoints below.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THPS_HOST_RAIL_NONE 0u
#define THPS_CASTLE_RAIL_COLLISION_WORDS 4368u

typedef struct ThpsHostRail {
    uint32_t id;
    float start[3], end[3], top_normal[3];
    uint32_t previous_id, next_id; /* Zero: finite endpoint, no continuation. */
    const char *name;
} ThpsHostRail;

typedef struct ThpsHostRailQuery {
    float previous[3], current[3]; /* Swept contact-reference position. */
    float motion[3];              /* Travel velocity/direction; may differ from sweep. */
    float radius;                 /* Caller policy in host units, no default physics. */
    float minimum_alignment;      /* Absolute motion/tangent cosine, [0,1]. */
    uint32_t excluded_id;          /* Optional re-entry cooldown; zero excludes none. */
    uint8_t requested;             /* Explicit current C-up request. */
} ThpsHostRailQuery;

typedef struct ThpsHostRailHit {
    const ThpsHostRail *rail;
    float point[3], sweep_point[3], tangent[3];
    float rail_fraction, sweep_fraction, distance;
    int direction;                /* +1 start->end, -1 end->start. */
} ThpsHostRailHit;

/* Returns only the two deliberately tagged vanilla Castle Grounds area-1
 * parapet edges. Before using them in a native scene, the adapter must verify
 * the loaded collision with thps_rails_collision_matches and reject replacements.
 */
const ThpsHostRail *thps_rails_for_level(int level, int area, size_t *count);
const ThpsHostRail *thps_rail_by_id(int level, int area, uint32_t id);

/* Numerical-word fingerprint of the exact native ROM-loaded collision array.
 * Native-endian int16_t input; no ROM bytes are embedded in this implementation.
 */
int thps_rails_collision_matches(int level, int area,
                                const int16_t *collision, size_t word_count);

/* Deterministic HOST geometric helper, not a recovered THPS rail scan.
 * Closest finite sweep/rail pair; lower rail ID wins equal-distance ties.
 * No request, bad inputs, or no candidate returns 0 without modifying hit.
 * Perpendicular and zero-motion sweeps are valid when minimum_alignment is 0.
 * At zero motion/tangent dot, direction is a diagnostic canonical +1, not a
 * source entry decision. The source controller owns endpoint rejection, entry
 * gates, threshold, travel direction, balance, speed, and state transitions.
 */
int thps_rails_trace(int level, int area, const ThpsHostRailQuery *query,
                     ThpsHostRailHit *hit);

#ifdef __cplusplus
}
#endif
#endif
