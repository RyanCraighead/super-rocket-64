#include "clearance_n64.h"
#include <string.h>

/* Define source low-word arithmetic without signed overflow or relying on
 * implementation-defined unsigned-to-signed conversion/right shifts. */
static int32_t bits(uint32_t x) {
    return x <= INT32_MAX ? (int32_t)x : -1 - (int32_t)(UINT32_MAX - x);
}
static int32_t add(int32_t a, int32_t b) {
    return bits((uint32_t)a + (uint32_t)b);
}
static int32_t sub(int32_t a, int32_t b) {
    return bits((uint32_t)a - (uint32_t)b);
}
static int32_t mul(int32_t a, int32_t b) {
    return bits((uint32_t)a * (uint32_t)b);
}
static int32_t fixed12_floor(int32_t a) {
    return a >= 0 ? a / 4096 : -1 - (int32_t)((uint32_t)(-1 - a) >> 12);
}

int smn64_climb_clearance(int32_t position[3], const SmN64ClimbBasis *basis,
                         int32_t distance, SmN64ClimbTrace trace, void *context) {
    /* These are the mathematical fixed12 octants, not an extracted asset.
     * Diagonals truncate 4096/sqrt(2) to 2896. All sixteen values have been
     * checked against the unchanged source sine/cosine routines. */
    static const int16_t sine[8] = {0, 2896, 4096, 2896, 0, -2896, -4096, -2896};
    static const int16_t cosine[8] = {4096, 2896, 0, -2896, -4096, -2896, 0, 2896};
    int32_t next[3];
    if (!position || !basis) return -1;
    if (!trace) return -3;
    memcpy(next, position, sizeof next);
    for (unsigned ray = 0; ray < 8; ++ray) {
        SmN64ClimbQuery query;
        SmN64ClimbHit hit;
        memset(&query, 0, sizeof query);
        memset(&hit, 0, sizeof hit);
        for (unsigned axis = 0; axis < 3; ++axis) {
            int32_t forward = mul(fixed12_floor(mul(basis->forward[axis], sine[ray])), distance);
            int32_t right = mul(fixed12_floor(mul(basis->right[axis], cosine[ray])), distance);
            query.start[axis] = add(add(next[axis], forward), right);
            query.end[axis] = sub(sub(next[axis], forward), right);
        }
        query.arg1 = 1;
        query.arg4 = 1;
        if (trace(context, &query, &hit) != 1) return -3;
        if (hit.present) {
            for (unsigned axis = 0; axis < 3; ++axis)
                next[axis] = add(next[axis], mul(hit.normal[axis], distance));
        }
    }
    memcpy(position, next, sizeof next);
    return 1;
}
