#ifndef ROCKET_METAL_WATER_H
#define ROCKET_METAL_WATER_H
/* Native act_metal_water_falling uses stationary_slow_down/get_buoyancy:
 * approach -18 units/frame by +2/-1 each 30 Hz frame. Convert to host
 * units/second and divide acceleration across the four RocketSim substeps.
 * Horizontal controls, tires, boost, contacts and angular dynamics stay car
 * mechanics. This is a host cap adaptation, not Rocket League water physics. */
static inline float rocket_metal_water_velocity(float velocity) {
    const float target=-18.f*30.f;
    if(velocity<target){velocity+=2.f*30.f/4.f;return velocity>target?target:velocity;}
    velocity-=1.f*30.f/4.f;return velocity<target?target:velocity;
}
#endif
