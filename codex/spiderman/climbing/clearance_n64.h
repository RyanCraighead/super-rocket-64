#ifndef SMN64_CLEARANCE_H
#define SMN64_CLEARANCE_H
#include "climbing_n64.h"

/* Original USA1.0 8009CF5C radial clearance owner. Position and basis are in
 * source fixed12/Y-down coordinates; distance is an unscaled source integer.
 * Eight full diameter segments are queried, in original order. Each contact
 * adds distance*normal to position before the next segment is constructed.
 * Only position changes; contact position, flags, and distance are ignored.
 *
 * Return 1 after eight completed queries, -1 for a null position/basis, or -3
 * when the trace is absent or does not return exactly 1. Negative returns leave
 * position unchanged. Trace callbacks must obey the read-only contract from
 * climbing_n64.h; earlier queries may be retried after an unavailable query.
 * There is no collision-free fallback and no substitute host-world geometry.
 */
int smn64_climb_clearance(int32_t position[3], const SmN64ClimbBasis *basis,
                         int32_t distance, SmN64ClimbTrace trace, void *context);
#endif
