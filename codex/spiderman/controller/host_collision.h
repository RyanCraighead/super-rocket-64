#ifndef SMN64_HOST_COLLISION_H
#define SMN64_HOST_COLLISION_H
#include <stdint.h>
/* Deliberate SM64 hurt-cylinder adapter, not the original Neversoft mesh test.
 * Every coordinate/radius is native signed fixed12; Y axis can point either
 * way, but lower_y <= upper_y. Endpoints are supplied by original bone sweep.
 * Returns 1 hit,0 miss,-1 invalid input; no output mutation on invalid/miss.
 * Contact is the earliest segment center entering the cylinder's spherical
 * radius expansion. A stationary overlapping sample is a valid geometric hit. */
int smn64_host_sweep_cylinder(const int32_t from[3], const int32_t to[3],
    int32_t sweep_radius, int32_t center_x, int32_t center_z,
    int32_t lower_y, int32_t upper_y, int32_t cylinder_radius,
    int32_t contact[3]);
#endif
