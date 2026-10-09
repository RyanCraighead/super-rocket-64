#ifndef ROCKET_WELCOME_H
#define ROCKET_WELCOME_H
struct MarioState;
struct Object;
int rocket_welcome_pending(void);
void rocket_welcome_reset(struct Object *lakitu);
int rocket_welcome_ready(struct MarioState *m, struct Object *lakitu);
void rocket_welcome_started(struct Object *lakitu);
void rocket_welcome_spawn(struct Object *lakitu);
void rocket_welcome_observe(struct Object *lakitu);
void rocket_welcome_finished(struct Object *lakitu);
#endif
