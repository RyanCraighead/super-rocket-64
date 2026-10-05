#ifndef SM64_SPIDERMAN_WORLD_H
#define SM64_SPIDERMAN_WORLD_H
#include <stdint.h>
struct Surface;
/* Explicit SM64-world mapping, not an emulation of the Neversoft mesh engine.
 * Native source fixed12 X,Y-down,Z -> host X,-Y,-Z. Query endpoints are body
 * coordinates already: no foot/body offset is added here. */
typedef struct SpidermanWorldHit {
    int32_t present, position[3], distance;
    int16_t normal[3];
    uint16_t source_flags;
    const struct Surface *surface;
} SpidermanWorldHit;
/* Complete finite segment against actual host triangles. Nearest tangible
 * static triangle wins. Dynamic/object geometry is explicitly unsupported
 * (-4) when nearest, never silently transparent. No output mutation on errors.
 * Returns 1 for complete hit/miss, -1 invalid input, -3 corrupt/unbounded host
 * partition, -4 moving/object-owned surface. A zero-length segment is a miss.
 * camera collision flags, Lua hooks and global camera query mode are untouched. */
int spiderman_world_trace_ex(const int32_t from[3],const int32_t to[3],
                             int include_objects,SpidermanWorldHit *hit);
int spiderman_world_trace(const int32_t from[3], const int32_t to[3],
                          SpidermanWorldHit *hit);
#endif
