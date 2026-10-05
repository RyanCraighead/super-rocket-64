#include <math.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "area.h"
#include "mario.h"
#include "mario_step.h"
#include "mario_actions_submerged.h"
#include "rocket_environment.h"
void apply_vertical_wind(struct MarioState *m);
void apply_water_current(struct MarioState *m, Vec3f step);

unsigned rocket_environment_material(struct MarioState *m, struct Surface *surface) {
    if (!m || !m->area || !surface) return ROCKET_MATERIAL_NORMAL;
    struct MarioState probe = *m;
    probe.floor = surface;
    probe.action = ACT_IDLE; /* No Mario-only crawling exemption for a car. */
    unsigned material = ROCKET_MATERIAL_NORMAL;
    switch (mario_get_floor_class(&probe)) {
        case SURFACE_CLASS_VERY_SLIPPERY: material = ROCKET_MATERIAL_VERY_SLIPPERY; break;
        case SURFACE_CLASS_SLIPPERY: material = ROCKET_MATERIAL_SLIPPERY; break;
    }
    /* Native slope thresholds include slide terrain and explicit grippy overrides.
     * Leave walls/ceilings to the car; floor materials govern climbing routes. */
    if (surface->normal.y > .01f && mario_floor_is_slippery(&probe)) material |= ROCKET_MATERIAL_SLIDING;
    return material;
}

void rocket_environment_sample(struct MarioState *m, const RocketSnapshot *pose, RocketEnvironment *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!m || !m->area || !pose) return;
    struct MarioState probe = *m;
    for (int i = 0; i < 3; ++i) {
        probe.pos[i] = pose->position[i];
        probe.vel[i] = pose->velocity[i] / 30.f;
    }
    /* Native water stepping adds current to displacement, not stored velocity.
     * Metal-water actions intentionally omit ACT_FLAG_SWIMMING: full resistance
     * to flowing water and whirlpools, not a made-up global wind immunity. */
    if ((m->action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER)) || probe.pos[1] < m->waterLevel - 100.f) {
        if (!(m->flags & MARIO_METAL_CAP)) {
            Vec3f step = {0, 0, 0};
            /* The native speed tables have four entries. Reject malformed
             * surface force indices before calling the native helper. */
            if (probe.floor && probe.floor->type == SURFACE_FLOWING_WATER &&
                ((unsigned)(u16)probe.floor->force >> 8) >= 4) probe.floor = NULL;
            apply_water_current(&probe, step);
            for (int i = 0; i < 3; ++i) out->drift[i] = step[i] * 30.f;
        }
    } else if (pose->grounded) {
        Vec3f before;
        memcpy(before, probe.vel, sizeof before);
        /* Ground wind has an idle gust path and a speed/heading-dependent moving
         * path. Derive signed forward speed from the actual car, not ACT_IDLE. */
        probe.forwardVel = 0;
        for (int i = 0; i < 3; ++i) probe.forwardVel += probe.vel[i] * pose->basis[i];
        probe.action = fabsf(probe.forwardVel) > .1f ? ACT_WALKING : ACT_IDLE;
        if (probe.floor && ((unsigned)(u16)probe.floor->force >> 8) < 4) mario_update_moving_sand(&probe);
        mario_update_windy_ground(&probe);
        for (int i = 0; i < 3; ++i) out->drift[i] = (probe.vel[i] - before[i]) * 30.f;
    } else {
        float before = probe.vel[1];
        apply_vertical_wind(&probe);
        out->acceleration[1] = (probe.vel[1] - before) * 900.f;
    }
}
