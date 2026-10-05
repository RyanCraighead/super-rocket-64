#ifndef SMN64_TRAILS_N64_H
#define SMN64_TRAILS_N64_H
#include <stdint.h>

/* Player motion trails: four source connected-line objects and five retained
 * fixed12 world points. Material slot 2 resolves through private F51EC data.
 * The renderer owns camera-facing expansion (source 6511C), not this kernel. */
typedef struct SmN64TrailSegment {
    int32_t from[3], to[3]; /* source segment +84 and +90 */
    uint32_t color;        /* source +40: flags high byte, B/G/R low bytes */
    uint16_t fade, width;   /* source +3C=8, +56=400 */
    uint8_t initial_vertex_rgb[3]; /* 66970/ BBD58; renderer refreshes color */
    uint8_t first;          /* source +50 bit 0x10 for segment zero */
    uint8_t frame, fraction;/* source +52/+53, updated by 67830 */
} SmN64TrailSegment;
typedef struct SmN64Trail {
    uint32_t head;          /* source +48, initially zero */
    uint32_t stopping;      /* source +54 */
    uint32_t delete_requested; /* source +33; destroy after effects pass */
    int32_t points[5][3];   /* source +4C array */
    SmN64TrailSegment segment[4]; /* source +50 array order */
} SmN64Trail;
typedef struct SmN64TrailPair { SmN64Trail *trail[2]; } SmN64TrailPair;
typedef struct SmN64TrailHost {
    void *context;
    /* Original authored marker 5 then 6, actual currently retained pose. */
    int (*marker)(void *, uint32_t marker, int32_t out[3]);
    /* Allocate genuine independently retained state; NULL is failure. Host
     * keeps stopped objects alive and ticks them until delete_requested. */
    SmN64Trail *(*allocate)(void *);
} SmN64TrailHost;

/* 66970 + 66574 numeric construction. No RNG calls. */
int smn64_trail_init(SmN64Trail *, const int32_t point[3], uint32_t player_color_6b0);
/* 675D4, once per player final pose tail; returns -1 for invalid state. */
int smn64_trail_append(SmN64Trail *, const int32_t point[3]);
/* 674F8 + per-segment 67830, once per global effects pass, independent of
 * player elapsed. Returns 1 when deletion is requested, 0 otherwise. */
int smn64_trail_tick(SmN64Trail *);
/* A22C0 / inline 9A944. Existing handles are preserved. Returns 1 or -2 for
 * a failed real marker/allocation; earlier successful creation is retained. */
int smn64_trails_start(SmN64TrailPair *, uint32_t player_color_6b0, const SmN64TrailHost *);
/* A2290: mark stopped and clear handles, without destroying the effects. */
void smn64_trails_stop(SmN64TrailPair *);
/* 93344..9339C: append markers after final pose/body translation refresh. */
int smn64_trails_retain(SmN64TrailPair *, const SmN64TrailHost *);
#endif
