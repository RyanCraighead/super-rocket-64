#ifndef ROCKET_BULLY_H
#define ROCKET_BULLY_H
struct MarioState;
struct Object;
/* Only a deliberate local forward ram supplies a native fast-attack intent. */
int rocket_bully_ram(struct MarioState *m,struct Object *bully);
#endif
