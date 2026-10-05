#ifndef ROCKET_PLATFORM_H
#define ROCKET_PLATFORM_H
struct MarioState;
struct Object;
struct SyncObject;
struct Packet;
/* Returns true even for a suspended/stale car: callers must clear its load,
 * never fall back to the proxy Mario's foot position. */
int rocket_platform_car(unsigned index);
int rocket_platform_support(struct MarioState *m);
void rocket_platform_refresh(void);
void rocket_platform_forget(struct Object *object);
/* Nonmutating transport-bound check; run before relays and again before apply. */
int rocket_platform_packet_allowed(const struct Packet *p);
/* Native behavior authority; called after each behavior's normal init. */
int rocket_platform_begin(struct Object *object);
int rocket_platform_managed(const struct SyncObject *so);
int rocket_platform_accept(const struct SyncObject *so, unsigned from);
/* Weighted center using actual wheel/floor contacts; native players weigh 1,
 * a fully supported 180-unit RocketSim car weighs 2 Mario equivalents.
 * Native-only groups retain their original averaged torque. */
float rocket_platform_load(struct Object *object, float center[3]);
#endif
