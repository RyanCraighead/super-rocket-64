#ifndef ROCKET_SURFACE_POLICY_H
#define ROCKET_SURFACE_POLICY_H
/* Values 0/1 are persisted by older builds. Never reinterpret either choice. */
enum { ROCKET_SURFACES_CAR = 0, ROCKET_SURFACES_NATIVE = 1,
       ROCKET_SURFACES_NATIVE_NO_WALLS = 2 };
#define ROCKET_SURFACES_DEFAULT ROCKET_SURFACES_NATIVE_NO_WALLS
static inline int rocket_surface_valid(unsigned mode) { return mode <= 2; }
static inline unsigned rocket_surface_preference(unsigned mode) {
    return rocket_surface_valid(mode) ? mode : ROCKET_SURFACES_DEFAULT;
}
static inline int rocket_surface_native(unsigned mode) {
    return mode == ROCKET_SURFACES_NATIVE || mode == ROCKET_SURFACES_NATIVE_NO_WALLS;
}
/* SM64's floor/wall split, not a car tilt limit. Slopes remain floors. */
#define ROCKET_NATIVE_FLOOR_NORMAL_MIN .01f
#endif
