#ifndef SM64_ROCKET_ENEMY_H
#define SM64_ROCKET_ENEMY_H
struct Object;
/* Called at the shared pre-collision boundary after a native hitbox exists.
 * Returns true only for a newly committed, authoritative attack. */
int rocket_enemy_attack(struct Object *enemy);
/* Shared with ordinary Bob-omb contact policy; installs the same native
 * authority callbacks, preserving held ownership and existing callbacks. */
int rocket_enemy_authority(struct Object *enemy);
void rocket_enemy_forget(struct Object *enemy);
void rocket_enemy_interrupt(struct Object *enemy);
#endif
