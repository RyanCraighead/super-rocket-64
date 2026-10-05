#ifndef ROCKET_BOSS_H
#define ROCKET_BOSS_H
#include <PR/ultratypes.h>
struct Object;
enum RocketBossKind { ROCKET_BOSS_KING_BOBOMB, ROCKET_BOSS_BOWSER };
int rocket_boss_impact_yaw(struct Object *boss,enum RocketBossKind kind,s16 *yaw);
#endif
