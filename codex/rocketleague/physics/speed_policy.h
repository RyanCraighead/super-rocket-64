#ifndef ROCKET_SPEED_POLICY_H
#define ROCKET_SPEED_POLICY_H
/* One policy for saved preferences, authenticated rules and physical thresholds.
 * Gravity, jump height, angular control, time and world geometry are independent. */
#define ROCKET_SPEED_MIN 50u
#define ROCKET_SPEED_MAX 100u
#define ROCKET_SPEED_DEFAULT 100u
static inline int rocket_speed_valid(unsigned percent) {
    return percent >= ROCKET_SPEED_MIN && percent <= ROCKET_SPEED_MAX;
}
static inline unsigned rocket_speed_preference(unsigned percent) {
    return rocket_speed_valid(percent) ? percent : ROCKET_SPEED_DEFAULT;
}
static inline float rocket_speed_multiplier(unsigned percent) {
    return rocket_speed_preference(percent) / 100.f;
}
#endif
